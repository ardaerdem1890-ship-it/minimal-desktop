/* Minimal Desktop - Intel 8254x (PRO/1000) Gigabit Ethernet driver
 *
 * VirtualBox's built-in NIC choices are AMD PCnet-FAST III, Intel PRO/1000 MT
 * Desktop and (for Linux guests) virtio-net. PCnet is handled in pcnet.c; this
 * file covers the Intel option, which is also QEMU's `-device e1000`.
 *
 * The driver uses the memory-mapped register window (BAR0) and the *legacy*
 * 16-byte descriptor format. Polled operation, no interrupts, matching the
 * other NICs in this kernel.
 */
#include "e1000.h"
#include "pci.h"
#include "io.h"
#include "console.h"
#include "string.h"
#include "pit.h"

#define PCI_VENDOR_INTEL 0x8086

/* control / status */
#define REG_CTRL   0x0000
#define REG_STATUS 0x0008
#define REG_EECD   0x0010
#define REG_EERD   0x0014
#define REG_ICR    0x00C0
#define REG_ITR    0x00C4
#define REG_RCTL   0x0100
#define REG_TCTL   0x0400
#define REG_RDBAL  0x2800
#define REG_RDBAH  0x2804
#define REG_RDLEN  0x2808
#define REG_RDH    0x2810
#define REG_RDT    0x2818
#define REG_TDBAL  0x3800
#define REG_TDBAH  0x3804
#define REG_TDLEN  0x3808
#define REG_TDH    0x3810
#define REG_TDT    0x3818
#define REG_RAL0   0x5400
#define REG_RAH0   0x5404

#define CTRL_FD      0x00000001
#define CTRL_ASDE    0x00000020
#define CTRL_SLU     0x00000040
#define CTRL_RST     0x04000000

#define RCTL_EN      0x00000002
#define RCTL_BAM     0x00008000
#define RCTL_SECRC   0x04000000
#define RCTL_BSIZE_2048 0x00000000

#define TCTL_EN      0x00000002
#define TCTL_PSP     0x00000008
#define TCTL_CT      0x00000100      /* collision threshold 0x10 */
#define TCTL_COLD    0x00040000      /* collision distance 0x40 */

#define RX_STATUS_DD  0x01
#define RX_STATUS_EOP 0x02

#define TX_CMD_EOP  0x01
#define TX_CMD_IFCS 0x02
#define TX_CMD_RS   0x08

#define NRX 32
#define NTX 8
#define RXBUF_SIZE 2048
#define TXBUF_SIZE 2048

typedef struct PACKED {
    u32 addr_lo;
    u32 addr_hi;
    u16 length;
    u16 checksum;
    u8  status;
    u8  errors;
    u16 special;
} rx_desc_t;

typedef struct PACKED {
    u32 addr_lo;
    u32 addr_hi;
    u16 length;
    u8  cso;
    u8  cmd;
    u8  status;
    u8  css;
    u16 special;
} tx_desc_t;

static volatile u32 *mmio;
static u32 mmio_phys;
static u8  pci_irq_line;
static bool present;
static u8  mac[6];
static int rx_cur, tx_cur;
static u32 rx_packets, tx_packets, rx_bytes, tx_bytes;

static rx_desc_t rx_ring[NRX] __attribute__((aligned(16)));
static tx_desc_t tx_ring[NTX] __attribute__((aligned(16)));
static u8 rx_bufs[NRX][RXBUF_SIZE] __attribute__((aligned(16)));
static u8 tx_bufs[NTX][TXBUF_SIZE] __attribute__((aligned(16)));

static inline void wr(u32 reg, u32 val) { mmio[reg >> 2] = val; }
static inline u32  rd(u32 reg)          { return mmio[reg >> 2]; }

static u16 eeprom_read(u8 addr) {
    wr(REG_EERD, ((u32)addr << 8) | 1u);
    for (int i = 0; i < 1000; i++) {
        u32 v = rd(REG_EERD);
        if (v & 0x10u) return (u16)(v >> 16);
        pit_sleep_ms(1);
    }
    return 0xFFFF;
}

bool e1000_present(void) { return present; }
void e1000_get_mac(u8 *out) { memcpy(out, mac, 6); }
u32  e1000_rx_packets(void) { return rx_packets; }
u32  e1000_tx_packets(void) { return tx_packets; }
u32  e1000_rx_bytes(void) { return rx_bytes; }
u32  e1000_tx_bytes(void) { return tx_bytes; }
u32  e1000_mmio_base(void) { return mmio_phys; }
u8   e1000_irq(void) { return pci_irq_line; }

bool e1000_init(void) {
    pci_device_t dev;
    present = false;
    rx_packets = tx_packets = rx_bytes = tx_bytes = 0;

    static const u16 ids[] = { 0x100E, 0x100F, 0x10D3, 0x153A, 0x10EA, 0x10EB };
    bool found = false;
    for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        if (pci_find_device(PCI_VENDOR_INTEL, ids[i], &dev)) { found = true; break; }
    }
    if (!found) return false;

    pci_enable_io(&dev);
    pci_enable_bus_mastering(&dev);

    if (dev.bar0 & 1u) {
        kprintf("[e1000] BAR0 is I/O space, not supported\n");
        return false;
    }
    mmio_phys = dev.bar0 & ~0xFu;
    if (!mmio_phys) {
        kprintf("[e1000] BAR0 not assigned by firmware\n");
        return false;
    }
    /* Paging is disabled, so the physical MMIO window is directly usable. */
    mmio = (volatile u32 *)mmio_phys;
    pci_irq_line = dev.irq;

    wr(REG_CTRL, rd(REG_CTRL) | CTRL_RST);
    for (int i = 0; i < 100; i++) {
        if (!(rd(REG_CTRL) & CTRL_RST)) break;
        pit_sleep_ms(1);
    }
    pit_sleep_ms(5);

    /* Link up, auto-speed detection, full duplex. */
    wr(REG_CTRL, rd(REG_CTRL) | CTRL_SLU | CTRL_ASDE | CTRL_FD);

    /* MAC address: RAL0/RAH0 are preloaded from the EEPROM. */
    u32 ral = rd(REG_RAL0), rah = rd(REG_RAH0);
    if ((rah & 0x80000000u) && (ral || (rah & 0xFFFF))) {
        mac[0] = (u8)(ral);        mac[1] = (u8)(ral >> 8);
        mac[2] = (u8)(ral >> 16);  mac[3] = (u8)(ral >> 24);
        mac[4] = (u8)(rah);        mac[5] = (u8)(rah >> 8);
    } else {
        for (int i = 0; i < 3; i++) {
            u16 w = eeprom_read((u8)i);
            mac[i * 2]     = (u8)(w & 0xFF);
            mac[i * 2 + 1] = (u8)(w >> 8);
        }
    }
    bool any = false;
    for (int i = 0; i < 6; i++) if (mac[i]) any = true;
    if (!any) {
        kprintf("[e1000] could not read a MAC address\n");
        return false;
    }

    memset((void *)rx_ring, 0, sizeof(rx_ring));
    memset((void *)tx_ring, 0, sizeof(tx_ring));
    for (int i = 0; i < NRX; i++) {
        rx_ring[i].addr_lo = (u32)(uintptr_t)rx_bufs[i];
        rx_ring[i].addr_hi = 0;
        rx_ring[i].status = 0;
    }

    wr(REG_RDBAL, (u32)(uintptr_t)rx_ring);
    wr(REG_RDBAH, 0);
    wr(REG_RDLEN, (u32)(NRX * sizeof(rx_desc_t)));
    wr(REG_RDH, 0);
    wr(REG_RDT, NRX - 1);

    wr(REG_TDBAL, (u32)(uintptr_t)tx_ring);
    wr(REG_TDBAH, 0);
    wr(REG_TDLEN, (u32)(NTX * sizeof(tx_desc_t)));
    wr(REG_TDH, 0);
    wr(REG_TDT, 0);

    wr(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_2048);
    wr(REG_TCTL, TCTL_EN | TCTL_PSP | TCTL_CT | TCTL_COLD);

    rx_cur = 0;
    tx_cur = 0;
    present = true;
    return true;
}

int e1000_send(const void *data, u32 len) {
    if (!present || len == 0 || len > TXBUF_SIZE) return -1;

    tx_desc_t *d = &tx_ring[tx_cur];
    /* Reclaim the slot if the hardware is done with it. */
    for (int i = 0; i < 2000 && (d->status & 0x01); i++) pit_sleep_ms(1);
    d->status = 0;

    memcpy(tx_bufs[tx_cur], data, len);
    d->addr_lo = (u32)(uintptr_t)tx_bufs[tx_cur];
    d->addr_hi = 0;
    d->length = (u16)len;
    d->cso = 0;
    d->css = 0;
    d->special = 0;
    d->cmd = TX_CMD_EOP | TX_CMD_IFCS | TX_CMD_RS;

    tx_cur = (tx_cur + 1) % NTX;
    wr(REG_TDT, (u32)tx_cur);

    tx_packets++;
    tx_bytes += len;
    return 0;
}

int e1000_receive(void *buf, u32 max) {
    if (!present) return 0;

    rx_desc_t *d = &rx_ring[rx_cur];
    if (!(d->status & RX_STATUS_DD)) return 0;

    int copied = 0;
    if (d->status & RX_STATUS_EOP) {
        u32 total = d->length;
        if (total > max) total = max;
        memcpy(buf, rx_bufs[rx_cur], total);
        copied = (int)total;
        if (copied > 0) {
            rx_packets++;
            rx_bytes += (u32)copied;
        }
    }

    d->status = 0;
    wr(REG_RDT, (u32)rx_cur);
    rx_cur = (rx_cur + 1) % NRX;
    return copied;
}
