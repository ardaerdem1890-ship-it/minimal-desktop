/* Minimal Desktop - Ethernet / ARP / IPv4 / ICMP / UDP / DHCP / DNS / TCP-HTTP
 *
 * One blocking TCP client connection is supported at a time, which is enough
 * for curl, the package store and the GUI browser. All waits poll the NIC so
 * no network work ever runs in interrupt context.
 */
#include "net.h"
#include "console.h"
#include "string.h"
#include "io.h"
#include "pit.h"
#include "mm.h"

static bool dhcp_client(void);

#define ETH_ARP  0x0806
#define ETH_IPV4 0x0800

#define IP_ICMP 1
#define IP_TCP  6
#define IP_UDP  17

#define ARP_REQUEST 1
#define ARP_REPLY   2

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DNS_PORT 53

#define TCP_RX_CAP 8192
#define ARP_CACHE_SIZE 16
#define DNS_CACHE_SIZE 8

typedef struct PACKED {
    u8  dst[6];
    u8  src[6];
    u16 ethertype;
} eth_hdr_t;

typedef struct PACKED {
    u16 hw_type;
    u16 proto;
    u8  hw_len;
    u8  proto_len;
    u16 op;
    u8  sha[6];
    u32 spa;
    u8  tha[6];
    u32 tpa;
} arp_pkt_t;

typedef struct PACKED {
    u8  ver_ihl;
    u8  tos;
    u16 total_len;
    u16 id;
    u16 frag;
    u8  ttl;
    u8  proto;
    u16 checksum;
    u32 src;
    u32 dst;
} ip_hdr_t;

typedef struct PACKED {
    u8  type;
    u8  code;
    u16 checksum;
    u16 id;
    u16 seq;
} icmp_hdr_t;

typedef struct PACKED {
    u16 sport;
    u16 dport;
    u16 len;
    u16 checksum;
} udp_hdr_t;

typedef struct PACKED {
    u16 sport;
    u16 dport;
    u32 seq;
    u32 ack;
    u8  offset_flags;
    u16 window;
    u16 checksum;
    u16 urgent;
} tcp_hdr_t;

static net_info_t info;
static bool dhcp_got_lease;
static u8 frame_rx[1600];
static u8 mac_broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static u32 ip_counter;
static int last_http_status;
static u32 dhcp_pending_xid;
static u16 dns_pending_id;

static int udp_send(u32 dst, u16 sport, u16 dport, const void *data, u32 len);
static bool arp_resolve(u32 ip, u8 *mac_out);

/* ------------------------------------------------------------------------- */
/* small byte-order helpers (all wire fields are big-endian)                 */
/* ------------------------------------------------------------------------- */
static inline u16 htons16(u16 v) { return __builtin_bswap16(v); }
static inline u32 htonl32(u32 v) { return __builtin_bswap32(v); }
#define ntohs16 htons16
#define ntohl32 htonl32

static u16 rd16(const u8 *p) { return (u16)(((u16)p[0] << 8) | p[1]); }
static u32 rd32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}
static void wr16(u8 *p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static void wr32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }

static u32 csum_add(u32 sum, const u8 *data, int len) {
    while (len > 1) {
        sum += (u32)data[0] | ((u32)data[1] << 8);
        data += 2;
        len -= 2;
    }
    if (len) sum += data[0];
    return sum;
}

static u16 csum_fold(u32 sum) {
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)(~sum & 0xFFFF);
}

static u16 ip_checksum(const u8 *data, int len) {
    return csum_fold(csum_add(0, data, len));
}

static u16 udp_tcp_checksum(u32 src, u32 dst, u8 proto, const u8 *seg, int len) {
    u32 sum = 0;
    u8 pseudo[12];
    wr32(pseudo + 0, src);
    wr32(pseudo + 4, dst);
    pseudo[8] = 0;
    pseudo[9] = proto;
    wr16(pseudo + 10, (u16)len);
    sum = csum_add(sum, pseudo, 12);
    sum = csum_add(sum, seg, len);
    return csum_fold(sum);
}

u32 net_parse_ip(const char *s) {
    u32 parts[4] = { 0, 0, 0, 0 };
    int part = 0;
    const char *p = s;
    while (*p && part < 4) {
        if (*p == '.') { part++; p++; continue; }
        if (*p < '0' || *p > '9') return 0;
        u32 v = 0;
        while (*p >= '0' && *p <= '9') { v = v * 10 + (u32)(*p - '0'); p++; }
        if (v > 255) return 0;
        parts[part] = v;
        if (*p && *p != '.') return 0;
    }
    if (part != 3) return 0;
    return NET_IP(parts[0], parts[1], parts[2], parts[3]);
}

void net_format_ip(u32 ip, char *buf) {
    ksnprintf(buf, 20, "%u.%u.%u.%u",
              (unsigned)(ip & 0xFF), (unsigned)((ip >> 8) & 0xFF),
              (unsigned)((ip >> 16) & 0xFF), (unsigned)((ip >> 24) & 0xFF));
}

/* ------------------------------------------------------------------------- */
/* ARP cache                                                                 */
/* ------------------------------------------------------------------------- */
typedef struct {
    bool valid;
    u32 ip;
    u8 mac[6];
    u32 age;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];

static void arp_cache_add(u32 ip, const u8 *mac) {
    int slot = -1;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) { slot = i; break; }
        if (!arp_cache[i].valid && slot < 0) slot = i;
    }
    if (slot < 0) slot = (int)(ip_counter % ARP_CACHE_SIZE);
    arp_cache[slot].valid = true;
    arp_cache[slot].ip = ip;
    memcpy(arp_cache[slot].mac, mac, 6);
    arp_cache[slot].age = pit_ticks();
}

static bool arp_cache_lookup(u32 ip, u8 *mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac, arp_cache[i].mac, 6);
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* raw framing                                                               */
/* ------------------------------------------------------------------------- */
/* Active NIC: 0 = none, 1 = RTL8139, 2 = PCnet (VirtualBox/QEMU) */
static int active_nic;

int net_send_raw(const void *data, u32 len) {
    if (active_nic == 2) return pcnet_send(data, len);
    if (active_nic == 3) return e1000_send(data, len);
    return rtl8139_send(data, len);
}

int net_recv_raw(void *buf, u32 max) {
    if (active_nic == 2) return pcnet_receive(buf, max);
    if (active_nic == 3) return e1000_receive(buf, max);
    return rtl8139_receive(buf, max);
}

static int eth_send(const u8 *dst, u16 type, const void *payload, u32 len) {
    u8 frame[1600];
    if (len + sizeof(eth_hdr_t) > sizeof(frame)) return -1;
    memset(frame, 0, sizeof(frame));
    eth_hdr_t *eth = (eth_hdr_t *)frame;
    memcpy(eth->dst, dst, 6);
    memcpy(eth->src, info.mac, 6);
    eth->ethertype = htons16(type);
    memcpy(frame + sizeof(eth_hdr_t), payload, len);
    u32 total = (u32)sizeof(eth_hdr_t) + len;
    if (total < 60) total = 60;       /* Ethernet minimum frame size */
    return net_send_raw(frame, total);
}

static int ip_send(u32 dst, u8 proto, const void *payload, u32 len) {
    u8 packet[1600];
    if (len + 20 > sizeof(packet)) return -1;
    ip_hdr_t *ip = (ip_hdr_t *)packet;
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons16((u16)(20 + len));
    ip->id = htons16((u16)(ip_counter++ & 0xFFFF));
    ip->frag = 0;
    ip->ttl = 64;
    ip->proto = proto;
    ip->checksum = 0;
    ip->src = info.ip;
    ip->dst = dst;
    memcpy(packet + 20, payload, len);
    ip->checksum = ip_checksum(packet, 20);

    u8 mac[6];
    if (!arp_resolve(dst, mac)) return -2;
    return eth_send(mac, ETH_IPV4, packet, 20 + len);
}

/* ------------------------------------------------------------------------- */
/* DHCP client                                                               */
/* ------------------------------------------------------------------------- */
typedef struct {
    bool valid;
    u8   type;
    u32  xid;
    u32  yiaddr;
    u32  server;
    u32  mask;
    u32  gateway;
    u32  dns;
} dhcp_result_t;

static dhcp_result_t dhcp_rx;

static u32 dhcp_build(u8 *buf, u32 xid, u8 type, u32 requested, u32 server) {
    memset(buf, 0, 400);
    eth_hdr_t *eth = (eth_hdr_t *)buf;
    memcpy(eth->dst, mac_broadcast, 6);
    memcpy(eth->src, info.mac, 6);
    eth->ethertype = htons16(ETH_IPV4);

    ip_hdr_t *ip = (ip_hdr_t *)(buf + 14);
    ip->ver_ihl = 0x45;
    ip->total_len = 0;                 /* patched below */
    ip->id = htons16((u16)(ip_counter++ & 0xFFFF));
    ip->ttl = 64;
    ip->proto = IP_UDP;
    ip->src = 0;
    ip->dst = 0xFFFFFFFF;

    udp_hdr_t *udp = (udp_hdr_t *)(buf + 34);
    udp->sport = htons16(DHCP_CLIENT_PORT);
    udp->dport = htons16(DHCP_SERVER_PORT);

    u8 *bootp = buf + 42;
    bootp[0] = 1;                      /* request */
    bootp[1] = 1;                      /* Ethernet */
    bootp[2] = 6;                      /* hlen */
    wr32(bootp + 4, xid);
    wr16(bootp + 10, 0x8000);          /* broadcast flag */
    memcpy(bootp + 28, info.mac, 6);
    wr32(bootp + 236, 0x63825363);

    u8 *opt = bootp + 240;
    int o = 0;
    opt[o++] = 53; opt[o++] = 1; opt[o++] = type;
    if (type == 3) {                   /* REQUEST */
        opt[o++] = 50; opt[o++] = 4; memcpy(opt + o, &requested, 4); o += 4;
        opt[o++] = 54; opt[o++] = 4; memcpy(opt + o, &server, 4); o += 4;
    }
    opt[o++] = 12; opt[o++] = 7; memcpy(opt + o, "minimal", 7); o += 7;
    opt[o++] = 55; opt[o++] = 3; opt[o++] = 1; opt[o++] = 3; opt[o++] = 6;
    opt[o++] = 255;

    u32 dhcp_len = 240 + (u32)o;
    if (dhcp_len < 300) dhcp_len = 300;      /* RFC 1542 minimum BOOTP size */
    udp->len = htons16((u16)(8 + dhcp_len));
    wr16(buf + 16, (u16)(20 + 8 + dhcp_len));
    udp->checksum = 0;
    ip->checksum = 0;
    ip->checksum = ip_checksum((u8 *)ip, 20);
    return 14 + 20 + 8 + dhcp_len;
}

static void dhcp_parse(const u8 *bootp, int len) {
    if (len < 240) return;
    if (bootp[0] != 2) return;
    if (rd32(bootp + 4) != dhcp_pending_xid) return;
    if (rd32(bootp + 236) != 0x63825363) return;

    dhcp_rx.xid = dhcp_pending_xid;
    dhcp_rx.yiaddr = htonl32(rd32(bootp + 16));
    const u8 *opt = bootp + 240;
    int end = len - 240;
    int i = 0;
    while (i < end) {
        u8 code = opt[i++];
        if (code == 0) continue;
        if (code == 255) break;
        if (i >= end) break;
        u8 olen = opt[i++];
        if (i + olen > end) break;
        switch (code) {
            case 53: if (olen >= 1) dhcp_rx.type = opt[i]; break;
            case 1:  if (olen >= 4) dhcp_rx.mask = htonl32(rd32(opt + i)); break;
            case 3:  if (olen >= 4) dhcp_rx.gateway = htonl32(rd32(opt + i)); break;
            case 6:  if (olen >= 4) dhcp_rx.dns = htonl32(rd32(opt + i)); break;
            case 54: if (olen >= 4) dhcp_rx.server = htonl32(rd32(opt + i)); break;
            default: break;
        }
        i += olen;
    }
    dhcp_rx.valid = true;
}

static bool dhcp_wait(u8 wanted, u32 xid, u32 timeout_ms) {
    u32 start = pit_ticks();
    while ((pit_ticks() - start) * 10 < timeout_ms) {
        net_poll();
        if (dhcp_rx.valid && dhcp_rx.type == wanted && dhcp_rx.xid == xid) return true;
        pit_sleep_ms(5);
    }
    return false;
}

static bool dhcp_client(void) {
    u8 packet[400];
    u32 xid = rand() ^ 0x4D444F53u;
    dhcp_pending_xid = xid;
    memset(&dhcp_rx, 0, sizeof(dhcp_rx));

    u32 len = dhcp_build(packet, xid, 1, 0, 0);     /* DISCOVER */
    if (net_send_raw(packet, len) != 0) return false;
    if (!dhcp_wait(2, xid, 2500)) return false;     /* OFFER */

    u32 offered = dhcp_rx.yiaddr;
    u32 server = dhcp_rx.server;
    info.ip = offered;
    info.netmask = dhcp_rx.mask ? dhcp_rx.mask : NET_IP(255, 255, 255, 0);
    if (dhcp_rx.gateway) info.gateway = dhcp_rx.gateway;
    if (dhcp_rx.dns) info.dns = dhcp_rx.dns;

    memset(&dhcp_rx, 0, sizeof(dhcp_rx));
    len = dhcp_build(packet, xid, 3, offered, server);  /* REQUEST */
    if (net_send_raw(packet, len) != 0) return false;
    if (!dhcp_wait(5, xid, 2500)) return false;         /* ACK */

    info.ip = dhcp_rx.yiaddr ? dhcp_rx.yiaddr : offered;
    if (dhcp_rx.mask) info.netmask = dhcp_rx.mask;
    if (dhcp_rx.gateway) info.gateway = dhcp_rx.gateway;
    if (dhcp_rx.dns) info.dns = dhcp_rx.dns;
    dhcp_got_lease = true;
    return true;
}

/* ------------------------------------------------------------------------- */
/* DNS client                                                                */
/* ------------------------------------------------------------------------- */
typedef struct {
    bool valid;
    u16 id;
    u32 ip;
} dns_result_t;

static dns_result_t dns_rx;

typedef struct {
    char name[64];
    u32 ip;
    bool valid;
} dns_cache_t;

static dns_cache_t dns_cache[DNS_CACHE_SIZE];

static u32 dns_cache_lookup(const char *name) {
    for (int i = 0; i < DNS_CACHE_SIZE; i++)
        if (dns_cache[i].valid && strcasecmp(dns_cache[i].name, name) == 0) return dns_cache[i].ip;
    return 0;
}

static void dns_cache_add(const char *name, u32 ip) {
    int slot = -1;
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (!dns_cache[i].valid) { slot = i; break; }
        if (strcasecmp(dns_cache[i].name, name) == 0) { slot = i; break; }
    }
    if (slot < 0) slot = 0;
    strncpy(dns_cache[slot].name, name, sizeof(dns_cache[slot].name) - 1);
    dns_cache[slot].ip = ip;
    dns_cache[slot].valid = true;
}

static void dns_parse(const u8 *pkt, int len, u16 id) {
    if (len < 12) return;
    if (rd16(pkt) != id) return;
    u16 flags = rd16(pkt + 2);
    if ((flags & 0x000F) != 0) return;              /* rcode */
    u16 qd = rd16(pkt + 4);
    u16 an = rd16(pkt + 6);
    int pos = 12;
    for (int q = 0; q < qd; q++) {
        while (pos < len) {
            u8 l = pkt[pos++];
            if (l == 0) break;
            if (l & 0xC0) { pos++; break; }
            pos += l;
        }
        pos += 4;
    }
    for (int a = 0; a < an && pos + 10 <= len; a++) {
        if ((pkt[pos] & 0xC0) == 0xC0) pos += 2;
        else { while (pos < len && pkt[pos]) pos += pkt[pos] + 1; pos++; }
        if (pos + 10 > len) break;
        u16 type = rd16(pkt + pos);
        u16 cls = rd16(pkt + pos + 2);
        u32 ttl = rd32(pkt + pos + 4);
        u16 rdlen = rd16(pkt + pos + 8);
        pos += 10;
        (void)ttl;
        if (type == 1 && cls == 1 && rdlen == 4 && pos + 4 <= len) {
            dns_rx.ip = htonl32(rd32(pkt + pos));
            dns_rx.id = id;
            dns_rx.valid = true;
            return;
        }
        pos += rdlen;
    }
}

static u32 dns_query(const char *name) {
    u32 cached = dns_cache_lookup(name);
    if (cached) return cached;
    if (!info.dns) return 0;

    u8 pkt[512];
    memset(pkt, 0, sizeof(pkt));
    u16 id = (u16)(rand() & 0xFFFF);
    wr16(pkt, id);
    wr16(pkt + 2, 0x0100);          /* recursion desired */
    wr16(pkt + 4, 1);               /* QDCOUNT */
    int pos = 12;
    const char *p = name;
    while (*p) {
        const char *dot = strchr(p, '.');
        int lab = dot ? (int)(dot - p) : (int)strlen(p);
        if (lab <= 0 || lab > 63 || pos + lab + 1 >= (int)sizeof(pkt)) return 0;
        pkt[pos++] = (u8)lab;
        memcpy(pkt + pos, p, (size_t)lab);
        pos += lab;
        if (!dot) break;
        p = dot + 1;
    }
    pkt[pos++] = 0;
    wr16(pkt + pos, 1); pos += 2;   /* A */
    wr16(pkt + pos, 1); pos += 2;   /* IN */

    dns_rx.valid = false;
    dns_pending_id = id;
    if (udp_send(info.dns, 53000 + (u16)(ip_counter++ % 100), DNS_PORT, pkt, (u32)pos) != 0) return 0;

    u32 start = pit_ticks();
    while ((pit_ticks() - start) * 10 < 2500) {
        net_poll();
        if (dns_rx.valid && dns_rx.id == id) {
            dns_cache_add(name, dns_rx.ip);
            return dns_rx.ip;
        }
        pit_sleep_ms(5);
    }
    return 0;
}

u32 net_resolve(const char *host) {
    u32 ip = net_parse_ip(host);
    if (ip) return ip;
    return dns_query(host);
}

/* ------------------------------------------------------------------------- */
/* UDP                                                                       */
/* ------------------------------------------------------------------------- */
static int udp_send(u32 dst, u16 sport, u16 dport, const void *data, u32 len) {
    u8 packet[1500];
    if (len + 8 > sizeof(packet)) return -1;
    udp_hdr_t *udp = (udp_hdr_t *)packet;
    udp->sport = htons16(sport);
    udp->dport = htons16(dport);
    udp->len = htons16((u16)(8 + len));
    udp->checksum = 0;
    memcpy(packet + 8, data, len);
    u16 c = udp_tcp_checksum(info.ip, dst, IP_UDP, packet, (int)(8 + len));
    if (c == 0) c = 0xFFFF;
    udp->checksum = c;
    return ip_send(dst, IP_UDP, packet, 8 + len);
}

/* ------------------------------------------------------------------------- */
/* ICMP                                                                      */
/* ------------------------------------------------------------------------- */
static struct {
    bool pending;
    u16 id;
    u16 seq;
    u32 from;
    u32 sent_tick;
    u32 rtt_ms;
    u8  ttl;
} ping_state;

static void icmp_handle(u32 src, const u8 *pkt, int len, const u8 *src_mac) {
    if (len < 8) return;
    u8 type = pkt[0];
    if (type == 8) {                            /* echo request -> reply */
        u8 reply[1500];
        if (len > (int)sizeof(reply)) len = (int)sizeof(reply);
        memcpy(reply, pkt, (size_t)len);
        reply[0] = 0;
        reply[2] = 0;
        reply[3] = 0;
        u16 c = ip_checksum(reply, len);
        reply[2] = (u8)(c & 0xFF);
        reply[3] = (u8)(c >> 8);

        u8 packet[1500];
        ip_hdr_t *ip = (ip_hdr_t *)packet;
        ip->ver_ihl = 0x45;
        ip->tos = 0;
        ip->total_len = htons16((u16)(20 + len));
        ip->id = htons16((u16)(ip_counter++ & 0xFFFF));
        ip->frag = 0;
        ip->ttl = 64;
        ip->proto = IP_ICMP;
        ip->checksum = 0;
        ip->src = info.ip;
        ip->dst = src;
        memcpy(packet + 20, reply, (size_t)len);
        ip->checksum = ip_checksum(packet, 20);
        eth_send(src_mac, ETH_IPV4, packet, 20 + (u32)len);
    } else if (type == 0) {                     /* echo reply */
        u16 id = rd16(pkt + 4);
        u16 seq = rd16(pkt + 6);
        if (ping_state.pending && id == ping_state.id && seq == ping_state.seq) {
            ping_state.pending = false;
            ping_state.from = src;
            ping_state.rtt_ms = (pit_ticks() - ping_state.sent_tick) * 10;
            if (ping_state.rtt_ms == 0) ping_state.rtt_ms = 1;
            ping_state.ttl = 64;
        }
    }
}

int net_ping(u32 ip, int count) {
    if (!net_is_up()) { kprintf("ping: network is down\n"); return -1; }
    char ipbuf[20];
    net_format_ip(ip, ipbuf);
    kprintf("PING %s: 56 data bytes\n", ipbuf);
    int replies = 0;
    for (int i = 1; i <= count; i++) {
        u8 pkt[64];
        memset(pkt, 0, sizeof(pkt));
        pkt[0] = 8;
        pkt[1] = 0;
        ping_state.id = (u16)(0x4D44 + (ip_counter & 0x0F));
        ping_state.seq = (u16)i;
        ping_state.pending = true;
        ping_state.sent_tick = pit_ticks();
        wr16(pkt + 4, ping_state.id);
        wr16(pkt + 6, ping_state.seq);
        for (int b = 8; b < 56; b++) pkt[b] = (u8)('a' + (b % 23));
        u16 c = ip_checksum(pkt, 56);
        pkt[2] = (u8)(c & 0xFF);
        pkt[3] = (u8)(c >> 8);

        if (ip_send(ip, IP_ICMP, pkt, 56) != 0) {
            kprintf("ping: send failed\n");
            continue;
        }
        u32 start = pit_ticks();
        while (ping_state.pending && (pit_ticks() - start) * 10 < 1200) {
            net_poll();
            pit_sleep_ms(2);
        }
        if (!ping_state.pending) {
            replies++;
            char from[20];
            net_format_ip(ping_state.from, from);
            kprintf("64 bytes from %s: icmp_seq=%d ttl=%u time=%u.%u ms\n",
                    from, i, ping_state.ttl, ping_state.rtt_ms / 10, ping_state.rtt_ms % 10);
        } else {
            kprintf("Request timeout for icmp_seq %d\n", i);
        }
        pit_sleep_ms(200);
    }
    kprintf("--- %s ping statistics ---\n", ipbuf);
    kprintf("%d packets transmitted, %d received, %d%% packet loss\n",
            count, replies, count ? (count - replies) * 100 / count : 0);
    return replies > 0 ? 0 : -1;
}

/* ------------------------------------------------------------------------- */
/* ARP                                                                       */
/* ------------------------------------------------------------------------- */
static void arp_send_request(u32 target) {
    arp_pkt_t arp;
    arp.hw_type = htons16(1);
    arp.proto = htons16(ETH_IPV4);
    arp.hw_len = 6;
    arp.proto_len = 4;
    arp.op = htons16(ARP_REQUEST);
    memcpy(arp.sha, info.mac, 6);
    arp.spa = info.ip;
    memset(arp.tha, 0, 6);
    arp.tpa = target;
    eth_send(mac_broadcast, ETH_ARP, &arp, sizeof(arp));
}

static void arp_handle(const u8 *pkt, int len) {
    if (len < (int)sizeof(arp_pkt_t)) return;
    const arp_pkt_t *a = (const arp_pkt_t *)pkt;
    u16 hw = ntohs16(a->hw_type);
    u16 proto = ntohs16(a->proto);
    u16 op = ntohs16(a->op);
    if (hw != 1 || proto != ETH_IPV4 || (op != ARP_REPLY && op != ARP_REQUEST)) return;
    arp_cache_add(a->spa, a->sha);
    if (op == ARP_REQUEST && a->tpa == info.ip) {
        arp_pkt_t reply;
        reply.hw_type = htons16(1);
        reply.proto = htons16(ETH_IPV4);
        reply.hw_len = 6;
        reply.proto_len = 4;
        reply.op = htons16(ARP_REPLY);
        memcpy(reply.sha, info.mac, 6);
        reply.spa = info.ip;
        memcpy(reply.tha, a->sha, 6);
        reply.tpa = a->spa;
        eth_send(a->sha, ETH_ARP, &reply, sizeof(reply));
    }
}

static bool arp_resolve(u32 ip, u8 *mac_out) {
    if (arp_cache_lookup(ip, mac_out)) return true;
    if (ip == 0xFFFFFFFFu) { memcpy(mac_out, mac_broadcast, 6); return true; }

    for (int attempt = 0; attempt < 3; attempt++) {
        arp_send_request(ip);
        u32 start = pit_ticks();
        while ((pit_ticks() - start) * 10 < 700) {
            net_poll();
            if (arp_cache_lookup(ip, mac_out)) return true;
            pit_sleep_ms(5);
        }
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* TCP client                                                                */
/* ------------------------------------------------------------------------- */
enum { TCP_CLOSED = 0, TCP_SYN_SENT, TCP_ESTABLISHED, TCP_FIN_WAIT, TCP_CLOSE_WAIT };

static struct {
    bool active;
    u8   state;
    u32  remote_ip;
    u16  remote_port;
    u16  local_port;
    u32  snd_nxt;
    u32  snd_una;
    u32  rcv_nxt;
    u32  isn;
    u8   rx[TCP_RX_CAP];
    u32  rx_len;
    bool rx_closed;
    bool reset;
} tcp;

static u32 tcp_build(u8 *seg, u32 seq, u32 ack, u8 flags, const void *data, u32 len) {
    memset(seg, 0, 20);
    wr16(seg + 0, tcp.local_port);
    wr16(seg + 2, tcp.remote_port);
    wr32(seg + 4, seq);
    wr32(seg + 8, ack);
    seg[12] = (u8)((5 << 4) | 0);       /* data offset 5 words */
    seg[13] = flags;
    wr16(seg + 14, (u16)(TCP_RX_CAP - tcp.rx_len));
    if (len) memcpy(seg + 20, data, len);
    u16 c = udp_tcp_checksum(info.ip, tcp.remote_ip, IP_TCP, seg, (int)(20 + len));
    seg[16] = (u8)(c & 0xFF);
    seg[17] = (u8)(c >> 8);
    return 20 + len;
}

static int tcp_transmit(u8 flags, const void *data, u32 len) {
    u8 seg[1500];
    if (len + 20 > sizeof(seg)) return -1;
    u32 seq = tcp.snd_nxt;
    u32 ack = (flags & TCP_ACK) ? tcp.rcv_nxt : 0;
    u32 seglen = tcp_build(seg, seq, ack, flags, data, len);
    tcp.snd_nxt += len;
    if (flags & TCP_SYN) tcp.snd_nxt++;
    if (flags & TCP_FIN) tcp.snd_nxt++;
    return ip_send(tcp.remote_ip, IP_TCP, seg, seglen);
}

static void tcp_input(u32 src, const u8 *pkt, int len) {
    if (!tcp.active || len < 20) return;
    if (src != tcp.remote_ip) return;
    const tcp_hdr_t *h = (const tcp_hdr_t *)pkt;
    u16 sport = ntohs16(h->sport);
    u16 dport = ntohs16(h->dport);
    if (dport != tcp.local_port || sport != tcp.remote_port) return;
    u8 off = (u8)((h->offset_flags >> 4) & 0x0F);
    if (off < 5) return;
    int hdr_len = off * 4;
    if (hdr_len > len) return;
    u8 flags = h->offset_flags & 0x0F;
    u32 seq = ntohl32(h->seq);
    u32 ack = ntohl32(h->ack);
    const u8 *payload = pkt + hdr_len;
    int plen = len - hdr_len;

    if (flags & TCP_RST) {
        tcp.reset = true;
        tcp.rx_closed = true;
        tcp.state = TCP_CLOSED;
        return;
    }

    if (flags & TCP_SYN) {
        tcp.rcv_nxt = seq + 1;
        tcp.snd_una = ack;
        if (tcp.state == TCP_SYN_SENT) {
            tcp.state = TCP_ESTABLISHED;
            tcp_transmit(TCP_ACK, NULL, 0);
            return;
        }
    }

    if (ack > tcp.snd_una) tcp.snd_una = ack;

    if (plen > 0) {
        if (seq == tcp.rcv_nxt) {
            u32 space = TCP_RX_CAP - tcp.rx_len;
            u32 n = (u32)plen;
            if (n > space) n = space;
            memcpy(tcp.rx + tcp.rx_len, payload, n);
            tcp.rx_len += n;
            tcp.rcv_nxt += (u32)plen;
        }
        tcp_transmit(TCP_ACK, NULL, 0);
    }

    if (flags & TCP_FIN) {
        tcp.rcv_nxt++;
        tcp.rx_closed = true;
        tcp.state = TCP_CLOSE_WAIT;
        tcp_transmit(TCP_ACK, NULL, 0);
    }
}

static bool tcp_connect(u32 ip, u16 port) {
    memset(&tcp, 0, sizeof(tcp));
    tcp.active = true;
    tcp.remote_ip = ip;
    tcp.remote_port = port;
    tcp.local_port = (u16)(49152 + (ip_counter++ % 1000));
    tcp.isn = rand();
    tcp.snd_nxt = tcp.isn;
    tcp.snd_una = tcp.isn;
    tcp.state = TCP_SYN_SENT;

    for (int attempt = 0; attempt < 2; attempt++) {
        tcp.snd_nxt = tcp.isn;
        if (tcp_transmit(TCP_SYN, NULL, 0) != 0) return false;
        tcp.snd_nxt = tcp.isn + 1;
        u32 start = pit_ticks();
        while ((pit_ticks() - start) * 10 < 2000) {
            net_poll();
            if (tcp.state == TCP_ESTABLISHED) return true;
            if (tcp.reset) return false;
            pit_sleep_ms(2);
        }
    }
    tcp.active = false;
    return false;
}

static void tcp_close(void) {
    if (!tcp.active) return;
    if (tcp.state == TCP_ESTABLISHED || tcp.state == TCP_CLOSE_WAIT) {
        tcp_transmit(TCP_FIN | TCP_ACK, NULL, 0);
        for (int i = 0; i < 30; i++) {
            net_poll();
            pit_sleep_ms(2);
        }
    }
    tcp.active = false;
    tcp.state = TCP_CLOSED;
}

/* ------------------------------------------------------------------------- */
/* HTTP                                                                      */
/* ------------------------------------------------------------------------- */
int net_http_status(void) { return last_http_status; }

int net_http_get(const char *url, char *out, int max, int *out_len) {
    if (out_len) *out_len = 0;
    last_http_status = 0;
    if (!net_is_up()) {
        kprintf("curl: network is down (no DHCP lease)\n");
        return -1;
    }
    if (strncmp(url, "http://", 7) != 0) {
        kprintf("curl: only http:// is supported\n");
        return -1;
    }
    const char *host_start = url + 7;
    const char *slash = strchr(host_start, '/');
    const char *colon = strchr(host_start, ':');
    int host_len;
    u16 port = 80;
    if (colon && (!slash || colon < slash)) {
        host_len = (int)(colon - host_start);
        port = (u16)atoi(colon + 1);
    } else {
        host_len = slash ? (int)(slash - host_start) : (int)strlen(host_start);
    }
    if (host_len <= 0 || host_len >= 96) {
        kprintf("curl: invalid URL\n");
        return -1;
    }
    char host[96];
    memcpy(host, host_start, (size_t)host_len);
    host[host_len] = 0;
    const char *path = slash ? slash : "/";

    u32 ip = net_resolve(host);
    if (!ip) {
        kprintf("curl: cannot resolve host %s\n", host);
        return -1;
    }
    char ipbuf[20];
    net_format_ip(ip, ipbuf);
    kprintf("* Trying %s (%s):%u...\n", host, ipbuf, port);

    if (!arp_resolve(ip, (u8 *)frame_rx)) {     /* scratch MAC buffer */
        kprintf("curl: ARP resolution failed\n");
        return -1;
    }
    if (!tcp_connect(ip, port)) {
        kprintf("curl: connect failed\n");
        return -1;
    }
    kprintf("* Connected, sending GET %s HTTP/1.0\n", path);

    char req[512];
    int rlen = ksnprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: MinimalDesktop/1.0\r\n"
        "Accept: */*\r\nConnection: close\r\n\r\n", path, host);
    if (tcp_transmit(TCP_PSH | TCP_ACK, req, (u32)rlen) != 0) {
        tcp_close();
        return -1;
    }

    u32 start = pit_ticks();
    int total = 0;
    bool closed = false;
    while ((pit_ticks() - start) * 10 < 10000) {
        net_poll();
        if (tcp.rx_len) {
            u32 n = tcp.rx_len;
            if (total + (int)n > max - 1) n = (u32)(max - 1 - total);
            if (n) {
                memcpy(out + total, tcp.rx, n);
                total += (int)n;
            }
            tcp.rx_len = 0;
            if (total >= max - 1) { closed = true; break; }
        }
        if (tcp.rx_closed || tcp.reset) { closed = true; break; }
        pit_sleep_ms(2);
    }
    out[total] = 0;
    tcp_close();

    if (total <= 0) {
        kprintf("curl: empty response\n");
        return -1;
    }
    (void)closed;

    /* Parse the status line. */
    char *nl = strchr(out, '\n');
    if (nl && strncmp(out, "HTTP/", 5) == 0) {
        last_http_status = atoi(strchr(out, ' ') ? strchr(out, ' ') + 1 : "0");
        kprintf("HTTP status: %d\n", last_http_status);
    }

    /* Strip headers in place. */
    char *body = strstr(out, "\r\n\r\n");
    int skip = 0;
    if (body) skip = (int)(body - out) + 4;
    else {
        body = strstr(out, "\n\n");
        if (body) skip = (int)(body - out) + 2;
    }
    if (skip > 0 && skip < total) {
        memmove(out, out + skip, (size_t)(total - skip));
        total -= skip;
        out[total] = 0;
    }
    if (out_len) *out_len = total;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* receive dispatch                                                          */
/* ------------------------------------------------------------------------- */
static u16 ip_handle(u32 src, const u8 *pkt, int len, const u8 *src_mac) {
    if (len < 20) return 0;
    const ip_hdr_t *ip = (const ip_hdr_t *)pkt;
    if ((ip->ver_ihl >> 4) != 4) return 0;
    int ihl = (ip->ver_ihl & 0x0F) * 4;
    if (ihl < 20 || ihl > len) return 0;
    u16 total = ntohs16(ip->total_len);
    if (total > len) total = (u16)len;
    const u8 *payload = pkt + ihl;
    int plen = total - ihl;

    switch (ip->proto) {
        case IP_ICMP:
            icmp_handle(src, payload, plen, src_mac);
            break;
        case IP_UDP: {
            if (plen < 8) break;
            u16 sport = rd16(payload);
            u16 dport = rd16(payload + 2);
            u16 ulen = rd16(payload + 4);
            if (ulen < 8 || ulen > plen) break;
            if (dport == DHCP_CLIENT_PORT && sport == DHCP_SERVER_PORT)
                dhcp_parse(payload + 8, ulen - 8);
            else if (sport == DNS_PORT)
                dns_parse(payload + 8, ulen - 8, dns_pending_id);
            break;
        }
        case IP_TCP:
            tcp_input(src, payload, plen);
            break;
        default:
            break;
    }
    return total;
}

/* DNS queries use a random transaction ID; the parser checks it. */

void net_poll(void) {
    if (!info.present) return;
    for (int guard = 0; guard < 32; guard++) {
        int n = net_recv_raw(frame_rx, sizeof(frame_rx));
        if (n < (int)sizeof(eth_hdr_t)) break;
        const eth_hdr_t *eth = (const eth_hdr_t *)frame_rx;
        u16 type = ntohs16(eth->ethertype);
        const u8 *payload = frame_rx + sizeof(eth_hdr_t);
        int plen = n - (int)sizeof(eth_hdr_t);

        if (type == ETH_ARP) {
            arp_handle(payload, plen);
        } else if (type == ETH_IPV4) {
            if (plen >= 20) {
                const ip_hdr_t *ip = (const ip_hdr_t *)payload;
                ip_handle(ip->src, payload, plen, eth->src);
            }
        }
    }
    if (active_nic == 2) {
        info.rx_packets = pcnet_rx_packets();
        info.tx_packets = pcnet_tx_packets();
        info.rx_bytes = pcnet_rx_bytes();
        info.tx_bytes = pcnet_tx_bytes();
    } else if (active_nic == 3) {
        info.rx_packets = e1000_rx_packets();
        info.tx_packets = e1000_tx_packets();
        info.rx_bytes = e1000_rx_bytes();
        info.tx_bytes = e1000_tx_bytes();
    } else {
        info.rx_packets = rtl8139_rx_packets();
        info.tx_packets = rtl8139_tx_packets();
        info.rx_bytes = rtl8139_rx_bytes();
        info.tx_bytes = rtl8139_tx_bytes();
    }
    info.link_up = info.present;
}

/* ------------------------------------------------------------------------- */
/* init / status                                                             */
/* ------------------------------------------------------------------------- */
bool net_init(void) {
    memset(&info, 0, sizeof(info));
    memset(arp_cache, 0, sizeof(arp_cache));
    memset(dns_cache, 0, sizeof(dns_cache));
    memset(&tcp, 0, sizeof(tcp));
    memset(&ping_state, 0, sizeof(ping_state));
    active_nic = 0;
    strncpy(info.name, "eth0", sizeof(info.name) - 1);

    /* Try every NIC this kernel knows about. The order only matters when a VM
     * exposes more than one card: realtek first (QEMU default), then the two
     * VirtualBox favourites, then Intel PRO/1000. */
    if (rtl8139_init()) {
        info.present = true;
        active_nic = 1;
        rtl8139_get_mac(info.mac);
        info.io_base = rtl8139_io_base();
        info.irq = rtl8139_irq();
        strncpy(info.name, "rtl8139", sizeof(info.name) - 1);
    } else if (pcnet_init()) {
        info.present = true;
        active_nic = 2;
        pcnet_get_mac(info.mac);
        info.io_base = pcnet_io_base();
        info.irq = pcnet_irq();
        strncpy(info.name, "pcnet", sizeof(info.name) - 1);
    } else if (e1000_init()) {
        info.present = true;
        active_nic = 3;
        e1000_get_mac(info.mac);
        info.io_base = (u16)(e1000_mmio_base() & 0xFFFF);
        info.irq = e1000_irq();
        strncpy(info.name, "e1000", sizeof(info.name) - 1);
    }
    if (!info.present) {
        kprintf("[net] no supported NIC found (RTL8139 / PCnet / e1000)\n");
        return false;
    }

    info.link_up = true;
    info.up = true;
    info.gateway = NET_IP(10, 0, 2, 2);
    info.dns = NET_IP(10, 0, 2, 3);
    info.netmask = NET_IP(255, 255, 255, 0);

    kprintf("[net] %s MAC %02x:%02x:%02x:%02x:%02x:%02x IO 0x%X IRQ %u\n",
            info.name, info.mac[0], info.mac[1], info.mac[2], info.mac[3], info.mac[4], info.mac[5],
            info.io_base, info.irq);

    if (dhcp_client()) {
        info.dhcp = true;
        char ipb[20], gwb[20], dnsb[20];
        net_format_ip(info.ip, ipb);
        net_format_ip(info.gateway, gwb);
        net_format_ip(info.dns, dnsb);
        kprintf("[net] DHCP lease %s/24 gateway %s dns %s\n", ipb, gwb, dnsb);
    } else {
        info.up = false;
        kprintf("[net] DHCP failed (rx=%u tx=%u); interface left unconfigured\n",
                info.rx_packets, info.tx_packets);
    }
    return true;
}

bool net_is_up(void) { return info.present && info.up && info.ip != 0; }
net_info_t *net_info(void) { return &info; }
bool net_dhcp_acquired(void) { return dhcp_got_lease; }
bool net_dhcp_renew(void) {
    if (!info.present) return false;
    dhcp_got_lease = false;
    info.dhcp = false;
    info.up = true;
    info.ip = 0;
    bool ok = dhcp_client();
    if (ok) { info.dhcp = true; info.up = true; }
    return ok;
}

void net_set_static(u32 ip, u32 mask, u32 gw, u32 dns) {
    info.ip = ip;
    info.netmask = mask ? mask : NET_IP(255, 255, 255, 0);
    info.gateway = gw;
    info.dns = dns;
    info.up = (ip != 0);
    info.dhcp = false;
}


void ifconfig_print(void) {
    net_poll();
    kprintf("eth0: %s  flags=%s,BROADCAST,RUNNING  mtu 1500\n",
            info.name, info.link_up ? "UP" : "DOWN");
    kprintf("      ether %02x:%02x:%02x:%02x:%02x:%02x\n",
            info.mac[0], info.mac[1], info.mac[2], info.mac[3], info.mac[4], info.mac[5]);
    char ipb[20], maskb[20], gwb[20], dnsb[20];
    net_format_ip(info.ip, ipb);
    net_format_ip(info.netmask, maskb);
    net_format_ip(info.gateway, gwb);
    net_format_ip(info.dns, dnsb);
    kprintf("      inet %s  netmask %s\n", ipb, maskb);
    kprintf("      gateway %s  dns %s  %s\n", gwb, dnsb,
            info.dhcp ? "DHCP lease" : "static/unconfigured");
    kprintf("      RX packets %u  bytes %u   TX packets %u  bytes %u\n",
            info.rx_packets, info.rx_bytes, info.tx_packets, info.tx_bytes);
    kprintf("      IRQ %u  I/O 0x%X  link %s (100 Mbit full-duplex)\n",
            info.irq, info.io_base, info.link_up ? "UP" : "DOWN");
    kprintf("      ARP cache:");
    int shown = 0;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) continue;
        char b[20];
        net_format_ip(arp_cache[i].ip, b);
        kprintf(" %s -> %02x:%02x:%02x:%02x:%02x:%02x",
                b, arp_cache[i].mac[0], arp_cache[i].mac[1], arp_cache[i].mac[2],
                arp_cache[i].mac[3], arp_cache[i].mac[4], arp_cache[i].mac[5]);
        if (++shown >= 4) break;
    }
    if (!shown) kprintf(" (empty)");
    kprintf("\n");
}

bool net_selftest(char *err, int errlen) {
    if (!info.present || (active_nic != 1 && active_nic != 2 && active_nic != 3) ||
        (active_nic == 1 && !rtl8139_present()) ||
        (active_nic == 2 && !pcnet_present()) ||
        (active_nic == 3 && !e1000_present())) {
        ksnprintf(err, errlen, "no supported NIC detected (RTL8139 / PCnet / e1000)");
        return false;
    }
    if (!info.link_up) {
        ksnprintf(err, errlen, "link is down");
        return false;
    }
    if (!dhcp_got_lease || info.ip == 0) {
        ksnprintf(err, errlen, "no DHCP lease (ip=0)");
        return false;
    }
    u8 mac[6];
    if (!arp_resolve(info.gateway, mac)) {
        ksnprintf(err, errlen, "ARP for gateway failed");
        return false;
    }
    if (mac[0] == 0xFF) {
        ksnprintf(err, errlen, "gateway returned broadcast MAC");
        return false;
    }
    return true;
}
