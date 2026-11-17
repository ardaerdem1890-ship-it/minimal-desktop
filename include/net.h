#pragma once
#include "types.h"

#define NET_IP(a,b,c,d) ((u32)(a) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

typedef struct {
    char name[24];
    u8   mac[6];
    u32  ip;
    u32  netmask;
    u32  gateway;
    u32  dns;
    bool link_up;
    bool up;
    bool dhcp;
    bool present;
    u16  io_base;
    u8   irq;
    u32  rx_packets;
    u32  tx_packets;
    u32  rx_bytes;
    u32  tx_bytes;
} net_info_t;

bool net_init(void);
void net_poll(void);
bool net_is_up(void);
net_info_t *net_info(void);
u32  net_parse_ip(const char *s);
void net_format_ip(u32 ip, char *buf);
int  net_ping(u32 ip, int count);
u32  net_resolve(const char *host);
int  net_http_get(const char *url, char *out, int max, int *out_len);
void ifconfig_print(void);
bool net_selftest(char *err, int errlen);
bool net_dhcp_acquired(void);

int  net_http_status(void);

/* RTL8139 driver entry points */
bool rtl8139_init(void);
bool rtl8139_present(void);
void rtl8139_get_mac(u8 *mac);
void rtl_hw_reset(void);
int  rtl8139_send(const void *data, u32 len);
int  rtl8139_receive(void *buf, u32 max);
void rtl8139_poll(void);
u16  rtl8139_io_base(void);
u8   rtl8139_irq(void);
u32  rtl8139_rx_packets(void);
u32  rtl8139_tx_packets(void);
u32  rtl8139_rx_bytes(void);
u32  rtl8139_tx_bytes(void);
u16  rtl8139_isr(void);
u8   rtl8139_cr(void);
u16  rtl8139_cbr(void);
u16  rtl8139_capr(void);

/* Intel 8254x PRO/1000 (VirtualBox "Intel PRO/1000", QEMU -device e1000) */
bool e1000_init(void);
bool e1000_present(void);
void e1000_get_mac(u8 *mac);
int  e1000_send(const void *data, u32 len);
int  e1000_receive(void *buf, u32 max);
u32  e1000_rx_packets(void);
u32  e1000_tx_packets(void);
u32  e1000_rx_bytes(void);
u32  e1000_tx_bytes(void);
u32  e1000_mmio_base(void);
u8   e1000_irq(void);

/* AMD PCnet (VirtualBox/QEMU virtual NIC) */
bool pcnet_init(void);
bool pcnet_present(void);
void pcnet_get_mac(u8 *mac);
int  pcnet_send(const void *data, u32 len);
int  pcnet_receive(void *buf, u32 max);
u32  pcnet_rx_packets(void);
u32  pcnet_tx_packets(void);
u32  pcnet_rx_bytes(void);
u32  pcnet_tx_bytes(void);
u16  pcnet_io_base(void);
u8   pcnet_irq(void);

/* raw packet API used by the stack */
int  net_send_raw(const void *data, u32 len);
int  net_recv_raw(void *buf, u32 max);
