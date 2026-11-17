/* Minimal Desktop - AMD PCnet-PCI II / PCnet-FAST III driver
 *
 * This is the NIC VirtualBox exposes by default (Am79C973, PCI 1022:2020)
 * and QEMU's `pcnet` device. Polled descriptor rings, no interrupts used.
 * Layout follows the standard 32-bit software style (u-boot/Linux pcnet32).
 */
#include "pcnet.h"
#include "pci.h"
#include "io.h"
#include "pit.h"
#include "console.h"
#include "string.h"

#define PCI_VENDOR_AMD   0x1022
#define PCI_DEV_PCNET_II 0x2000
#define PCI_DEV_PCNET_FF 0x2020

/* I/O offsets from PCI BAR0 */
#define PCNET_RDP   0x10
#define PCNET_RAP   0x12
#define PCNET_RESET 0x14
#define PCNET_BDP   0x16

/* CSR0 bits */
#define CSR0_INIT  0x0001
#define CSR0_START 0x0002
#define CSR0_STOP  0x0004
#define CSR0_TXON  0x0008
#define CSR0_RXON  0x0010
#define CSR0_IDON  0x0100
#define CSR0_INTR  0x0200
#define CSR0_TINT  0x0400
#define CSR0_RINT  0x0800
#define CSR0_ERR   0x1000

#define NRX 16
#define NTX 4
#define RXBUF_SIZE 1544
#define TXBUF_SIZE 1544

/* 32-bit-style descriptors are 8 words (16 bytes) each */
typedef struct { u16 w[8]; } desc_t;

static u16 io_base;
static u8  pci_irq_line;
static bool present;
static u8  mac[6];

static desc_t rx_ring[NRX] __attribute__((aligned(16)));
static desc_t tx_ring[NTX] __attribute__((aligned(16)));
static u8 rx_bufs[NRX][RXBUF_SIZE] __attribute__((aligned(16)));
static u8 tx_bufs[NTX][TXBUF_SIZE] __attribute__((aligned(16)));
static u16 init_block[16] __attribute__((aligned(16)));
static int rx_cur, tx_cur;
static u32 rx_packets, tx_packets, rx_bytes, tx_bytes;

static inline void csr_write(u16 reg, u16 val) {
    outw((u16)(io_base + PCNET_RAP), reg);
    outw((u16)(io_base + PCNET_RDP), val);
}
static inline u16 csr_read(u16 reg) {
    outw((u16)(io_base + PCNET_RAP), reg);
    return inw((u16)(io_base + PCNET_RDP));
}
static inline void bcr_write(u16 reg, u16 val) {
    outw((u16)(io_base + PCNET_RAP), reg);
    outw((u16)(io_base + PCNET_BDP), val);
}
static inline u16 bcr_read(u16 reg) {
    outw((u16)(io_base + PCNET_RAP), reg);
    return inw((u16)(io_base + PCNET_BDP));
}

bool pcnet_present(void) { return present; }
void pcnet_get_mac(u8 *out) { memcpy(out, mac, 6); }
u16 pcnet_io_base(void) { return io_base; }
u8  pcnet_irq(void) { return pci_irq_line; }
u32 pcnet_rx_packets(void) { return rx_packets; }
u32 pcnet_tx_packets(void) { return tx_packets; }
u32 pcnet_rx_bytes(void) { return rx_bytes; }
u32 pcnet_tx_bytes(void) { return tx_bytes; }

bool pcnet_init(void) {
    pci_device_t dev;
    present = false;
    rx_packets = tx_packets = rx_bytes = tx_bytes = 0;

    if (!pci_find_device(PCI_VENDOR_AMD, PCI_DEV_PCNET_FF, &dev) &&
        !pci_find_device(PCI_VENDOR_AMD, PCI_DEV_PCNET_II, &dev)) return false;

    pci_enable_io(&dev);
    pci_enable_bus_mastering(&dev);
    if (!(dev.bar0 & 1)) {
        kprintf("[pcnet] BAR0 is MMIO, not supported\n");
        return false;
    }
    io_base = (u16)(dev.bar0 & ~3u);
    pci_irq_line = dev.irq;

    /* Hardware reset via the reset port. */
    (void)inw((u16)(io_base + PCNET_RESET));

    /* 32-bit software style, autoselect, no full-duplex forcing. */
    bcr_write(20, 0x0002);
    bcr_write(2, (u16)(bcr_read(2) | 0x0002));
    bcr_write(32, 0x0000);

    /* Read the station address from CSR12..14 (loaded from PROM on reset). */
    for (int i = 0; i < 3; i++) {
        u16 v = csr_read((u16)(12 + i));
        mac[i * 2]     = (u8)(v & 0xFF);
        mac[i * 2 + 1] = (u8)((v >> 8) & 0xFF);
    }

    memset(rx_ring, 0, sizeof(rx_ring));
    memset(tx_ring, 0, sizeof(tx_ring));
    memset(rx_bufs, 0, sizeof(rx_bufs));
    memset(tx_bufs, 0, sizeof(tx_bufs));
    memset(init_block, 0, sizeof(init_block));

    /* RX ring: OWN=1, negative buffer length. */
    for (int i = 0; i < NRX; i++) {
        u32 addr = (u32)(uintptr_t)rx_bufs[i];
        rx_ring[i].w[0] = (u16)(addr & 0xFFFF);
        rx_ring[i].w[1] = (u16)(addr >> 16);
        rx_ring[i].w[2] = (u16)(-RXBUF_SIZE);
        rx_ring[i].w[3] = 0x8000;
    }
    for (int i = 0; i < NTX; i++) {
        tx_ring[i].w[0] = 0;
        tx_ring[i].w[1] = 0;
        tx_ring[i].w[2] = 0;
        tx_ring[i].w[3] = 0;
    }
    rx_cur = 0; tx_cur = 0;

    /* 32-bit init block (16 words). */
    init_block[0] = 0x0000;                              /* mode */
    init_block[1] = (u16)((2 << 12) | (4 << 4));         /* TX=4, RX=16 */
    init_block[2] = (u16)(mac[0] | ((u16)mac[1] << 8));
    init_block[3] = (u16)(mac[2] | ((u16)mac[3] << 8));
    init_block[4] = (u16)(mac[4] | ((u16)mac[5] << 8));
    /* words 5..9: reserved + multicast filter = 0 (no multicast) */
    init_block[10] = (u16)((u32)(uintptr_t)rx_ring & 0xFFFF);
    init_block[11] = (u16)((u32)(uintptr_t)rx_ring >> 16);
    init_block[12] = (u16)((u32)(uintptr_t)tx_ring & 0xFFFF);
    init_block[13] = (u16)((u32)(uintptr_t)tx_ring >> 16);

    u32 addr = (u32)(uintptr_t)init_block;
    csr_write(1, (u16)(addr & 0xFFFF));
    csr_write(2, (u16)(addr >> 16));
    csr_write(4, 0x0915);
    csr_write(0, CSR0_INIT);
    for (int i = 0; i < 1000; i++) {
        if (csr_read(0) & CSR0_IDON) break;
        pit_sleep_ms(1);
    }
    if (!(csr_read(0) & CSR0_IDON)) {
        kprintf("[pcnet] init timeout (CSR0=%04x)\n", csr_read(0));
        return false;
    }
    csr_write(0, CSR0_START);

    present = true;
    kprintf("[net] PCnet MAC %02x:%02x:%02x:%02x:%02x:%02x IO 0x%X IRQ %u\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], io_base, pci_irq_line);
    return true;
}

int pcnet_send(const void *data, u32 len) {
    if (!present || len == 0 || len > TXBUF_SIZE) return -1;
    desc_t *d = &tx_ring[tx_cur];
    for (int i = 0; i < 500; i++) {
        if (!(d->w[3] & 0x8000)) break;
        pit_sleep_ms(1);
    }
    if (d->w[3] & 0x8000) return -1;

    memcpy(tx_bufs[tx_cur], data, len);
    d->w[0] = (u16)((u32)(uintptr_t)tx_bufs[tx_cur] & 0xFFFF);
    d->w[1] = (u16)((u32)(uintptr_t)tx_bufs[tx_cur] >> 16);
    d->w[2] = (u16)(-len);
    d->w[4] = 0; d->w[5] = 0; d->w[6] = 0; d->w[7] = 0;
    d->w[3] = (u16)(0x8000 | 0x0300);   /* OWN | STP | ENP */
    csr_write(0, CSR0_TXON);

    tx_cur = (tx_cur + 1) % NTX;
    tx_packets++;
    tx_bytes += len;
    return 0;
}

int pcnet_receive(void *buf, u32 max) {
    if (!present) return 0;
    desc_t *d = &rx_ring[rx_cur];
    u16 status = d->w[3];
    if (status & 0x8000) return 0;          /* NIC still owns */

    if (((status >> 8) & 0x03) == 0x03) {
        u32 total = d->w[4];                /* message length including FCS */
        if (total > 4 && total <= RXBUF_SIZE) {
            u32 payload = total - 4;
            if (payload > max) payload = max;
            memcpy(buf, rx_bufs[rx_cur], payload);
            rx_packets++;
            rx_bytes += payload;
            /* requeue descriptor */
            d->w[2] = (u16)(-RXBUF_SIZE);
            d->w[3] = 0x8000;
            rx_cur = (rx_cur + 1) % NRX;
            return (int)payload;
        }
    }
    /* error: requeue */
    d->w[2] = (u16)(-RXBUF_SIZE);
    d->w[3] = 0x8000;
    rx_cur = (rx_cur + 1) % NRX;
    return 0;
}

void pcnet_poll(void) { (void)csr_read(0); }
