/* Minimal Desktop - Realtek RTL8139 PCI Ethernet driver (polled DMA rings) */
#include "net.h"
#include "pci.h"
#include "io.h"
#include "console.h"
#include "string.h"
#include "pit.h"

#define RTL_VENDOR 0x10EC
#define RTL_DEVICE 0x8139

/* Register offsets from the I/O BAR */
#define RTL_IDR0     0x00
#define RTL_TSD0     0x10
#define RTL_TSAD0    0x20
#define RTL_RBSTART  0x30
#define RTL_CR       0x37
#define RTL_CAPR     0x38
#define RTL_CBR      0x3A
#define RTL_IMR      0x3C
#define RTL_ISR      0x3E
#define RTL_TCR      0x40
#define RTL_RCR      0x44
#define RTL_CONFIG1  0x52

#define RTL_RX_SIZE  (32768u + 16u + 1500u)
#define RTL_TX_COUNT 4
#define RTL_TX_SIZE  2048u

static u16 io_base;
static bool present;
static u8 mac[6];
static u8 rx_buffer[RTL_RX_SIZE] __attribute__((aligned(16)));
static u8 tx_buffer[RTL_TX_COUNT][RTL_TX_SIZE] __attribute__((aligned(16)));
static u32 tx_cur;
static u32 rx_offset;
static u8 pci_irq;
static u32 tx_packets, rx_packets;
static u32 tx_bytes, rx_bytes;

static inline void rtl_outb(u16 reg, u8 v) { outb((u16)(io_base + reg), v); }
static inline void rtl_outw(u16 reg, u16 v) { outw((u16)(io_base + reg), v); }
static inline u8  rtl_inb(u16 reg) { return inb((u16)(io_base + reg)); }
static inline void rtl_outl(u16 reg, u32 v) { outl((u16)(io_base + reg), v); }
static inline u32 rtl_inl(u16 reg) { return inl((u16)(io_base + reg)); }

bool rtl8139_present(void) { return present; }
void rtl8139_get_mac(u8 *out) { memcpy(out, mac, 6); }

void rtl_hw_reset(void) {
    /* Wake up / software reset */
    rtl_outb(RTL_CONFIG1, 0x00);
    rtl_outb(RTL_CR, 0x10);
    for (int i = 0; i < 1000; i++) {
        if (!(rtl_inb(RTL_CR) & 0x10)) break;
        pit_sleep_ms(1);
    }

    for (int i = 0; i < 6; i++) mac[i] = rtl_inb((u16)(RTL_IDR0 + i));

    /* Receive: accept broadcast + physical, 32 KiB ring, no wrap. */
    rtl_outl(RTL_RBSTART, (u32)rx_buffer);
    rtl_outb(RTL_CR, 0x0C);                 /* RX enable | TX enable */
    rtl_outl(RTL_RCR, 0x0BFu);               /* no-wrap, 8K ring, accept all (incl unicast->us) */
    rtl_outl(RTL_TCR, 0x03000700u);         /* IFG, 64-byte DMA burst */
    rtl_outl(RTL_IMR, 0x0000);              /* polled; NIC IRQ left masked */

    /* Clear pending status by writing ones. */
    outb((u16)(io_base + RTL_ISR), 0xFF);
    outb((u16)(io_base + RTL_ISR + 1), 0xFF);
    rx_offset = 0;
    rtl_outl(RTL_CAPR, 0);
}

bool rtl8139_init(void) {
    pci_device_t dev;
    present = false;
    memset(rx_buffer, 0, sizeof(rx_buffer));
    memset(tx_buffer, 0, sizeof(tx_buffer));

    if (!pci_find_device(RTL_VENDOR, RTL_DEVICE, &dev)) return false;
    pci_enable_io(&dev);
    pci_enable_bus_mastering(&dev);

    if (!(dev.bar0 & 1)) {
        kprintf("[net] RTL8139 BAR0 is MMIO (%p), expected I/O\n", (void *)dev.bar0);
        return false;
    }
    io_base = (u16)(dev.bar0 & ~3u);
    pci_irq = dev.irq;

    rtl_hw_reset();

    present = true;
    return true;
}

int rtl8139_send(const void *data, u32 len) {
    if (!present || len == 0 || len > RTL_TX_SIZE) return -1;
    u32 slot = tx_cur % RTL_TX_COUNT;
    memcpy(tx_buffer[slot], data, len);

    /* Fresh descriptors have the own bit set. Only wait when the ring wraps. */
    if (tx_cur >= RTL_TX_COUNT) {
        u32 guard = 0;
        while (rtl_inl((u16)(RTL_TSD0 + slot * 4)) & 0x2000) {
            if (++guard > 2000000) return -1;
            io_wait();
        }
    }

    rtl_outl((u16)(RTL_TSAD0 + slot * 4), (u32)tx_buffer[slot]);
    rtl_outl((u16)(RTL_TSD0 + slot * 4), len & 0x1FFF);
    tx_cur++;
    tx_packets++;
    tx_bytes += len;
    return 0;
}

int rtl8139_receive(void *buf, u32 max) {
    if (!present) return 0;
    u16 cbr = inw((u16)(io_base + RTL_CBR));
    if (cbr == rx_offset) return 0;

    u32 off = rx_offset;
    u16 status = (u16)(rx_buffer[off] | (rx_buffer[off + 1] << 8));
    u16 length = (u16)(rx_buffer[off + 2] | (rx_buffer[off + 3] << 8));

    if (!(status & 0x01) || length < 4 || length > 1800) {
        /* Out-of-sync: reset the read pointer to the current write pointer. */
        rx_offset = cbr;
        rtl_outl(RTL_CAPR, rx_offset);
        return 0;
    }

    u32 payload = (u32)length - 4;
    if (payload > max) payload = max;
    for (u32 i = 0; i < payload; i++) {
        u32 idx = (off + 4 + i) % RTL_RX_SIZE;
        ((u8 *)buf)[i] = rx_buffer[idx];
    }

    rx_offset = (off + 4 + length + 3) & ~3u;
    if (rx_offset >= RTL_RX_SIZE) rx_offset %= RTL_RX_SIZE;
    rtl_outl(RTL_CAPR, (rx_offset - 16u) & 0xFFFFu);

    rx_packets++;
    rx_bytes += payload;
    return (int)payload;
}

void rtl8139_poll(void) {
    if (!present) return;
    /* Status is cleared via ISR; polling happens through rtl8139_receive(). */
    (void)rtl_inb(RTL_ISR);
}

/* Accessors used by ifconfig / netstat */
u16 rtl8139_io_base(void) { return io_base; }
u8  rtl8139_irq(void) { return pci_irq; }
u32 rtl8139_rx_packets(void) { return rx_packets; }
u32 rtl8139_tx_packets(void) { return tx_packets; }
u32 rtl8139_rx_bytes(void) { return rx_bytes; }
u32 rtl8139_tx_bytes(void) { return tx_bytes; }
u16 rtl8139_isr(void) { return present ? inw((u16)(io_base + RTL_ISR)) : 0; }
u8  rtl8139_cr(void) { return present ? rtl_inb(RTL_CR) : 0; }
u16 rtl8139_cbr(void) { return present ? inw((u16)(io_base + RTL_CBR)) : 0; }
u16 rtl8139_capr(void) { return present ? inw((u16)(io_base + RTL_CAPR)) : 0; }
