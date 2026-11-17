/* Minimal Desktop - ATA/IDE PIO driver (primary master) */
#include "ata.h"
#include "io.h"
#include "string.h"
#include "console.h"
#include "pit.h"

static ata_disk_t disk;
static bool initialized;

static void ata_wait_400ns(void) {
    for (int i = 0; i < 4; i++) (void)inb(ATA_PRIMARY_CTRL);
}

static bool ata_wait_bsy(void) {
    for (u32 i = 0; i < 1000000; i++) {
        u8 s = inb(ATA_PRIMARY_DATA + 7);
        if (!(s & 0x80)) return true;
    }
    return false;
}

static bool ata_wait_drq(void) {
    for (u32 i = 0; i < 1000000; i++) {
        u8 s = inb(ATA_PRIMARY_DATA + 7);
        if (s & 0x01) return false;          /* error */
        if (!(s & 0x80) && (s & 0x08)) return true;
    }
    return false;
}

static void ata_string(u8 *dst, u16 *src, int words) {
    for (int i = 0; i < words; i++) {
        dst[i * 2] = (u8)(src[i] >> 8);
        dst[i * 2 + 1] = (u8)(src[i] & 0xFF);
    }
    dst[words * 2] = 0;
    /* trim trailing spaces */
    for (int i = words * 2 - 1; i >= 0; i--) {
        if (dst[i] == ' ') dst[i] = 0;
        else break;
    }
}

void ata_init(void) {
    memset(&disk, 0, sizeof(disk));
    initialized = true;

    outb(ATA_PRIMARY_CTRL, 0x02);       /* soft reset / nIEN */
    ata_wait_400ns();
    outb(ATA_PRIMARY_CTRL, 0x00);

    outb(ATA_PRIMARY_DATA + 6, 0xA0);   /* master, LBA */
    ata_wait_400ns();
    outb(ATA_PRIMARY_DATA + 2, 0);
    outb(ATA_PRIMARY_DATA + 3, 0);
    outb(ATA_PRIMARY_DATA + 4, 0);
    outb(ATA_PRIMARY_DATA + 5, 0);
    outb(ATA_PRIMARY_DATA + 7, 0xEC);   /* IDENTIFY */

    ata_wait_400ns();
    u8 status = inb(ATA_PRIMARY_DATA + 7);
    if (status == 0x00 || status == 0xFF) {
        disk.present = false;
        return;
    }
    if (!ata_wait_bsy()) { disk.present = false; return; }
    if (inb(ATA_PRIMARY_DATA + 4) != 0 || inb(ATA_PRIMARY_DATA + 5) != 0) {
        disk.present = false;
        return;
    }
    if (!ata_wait_drq()) { disk.present = false; return; }

    u16 ident[256];
    for (int i = 0; i < 256; i++) ident[i] = inw(ATA_PRIMARY_DATA);
    ata_string((u8 *)disk.model, &ident[27], 20);
    ata_string((u8 *)disk.serial, &ident[10], 10);
    disk.sectors = ((u32)ident[61] << 16) | ident[60];
    disk.size_mb = disk.sectors / 2048;
    disk.io_base = ATA_PRIMARY_DATA;
    disk.present = disk.sectors > 0;
}

bool ata_present(void) { return initialized && disk.present; }
const ata_disk_t *ata_get_disk(void) { return &disk; }

bool ata_read_sector(u32 lba, u8 *buf) {
    if (!disk.present) return false;
    outb(ATA_PRIMARY_DATA + 6, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_PRIMARY_DATA + 2, 1);
    outb(ATA_PRIMARY_DATA + 3, (u8)(lba & 0xFF));
    outb(ATA_PRIMARY_DATA + 4, (u8)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_DATA + 5, (u8)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_DATA + 7, 0x20);
    if (!ata_wait_bsy()) return false;
    if (!ata_wait_drq()) return false;
    for (int i = 0; i < 256; i++) {
        u16 w = inw(ATA_PRIMARY_DATA);
        buf[i * 2] = (u8)(w & 0xFF);
        buf[i * 2 + 1] = (u8)(w >> 8);
    }
    return true;
}

bool ata_write_sector(u32 lba, const u8 *buf) {
    if (!disk.present) return false;
    outb(ATA_PRIMARY_DATA + 6, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_PRIMARY_DATA + 2, 1);
    outb(ATA_PRIMARY_DATA + 3, (u8)(lba & 0xFF));
    outb(ATA_PRIMARY_DATA + 4, (u8)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_DATA + 5, (u8)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_DATA + 7, 0x30);
    if (!ata_wait_bsy()) return false;
    if (!ata_wait_drq()) return false;
    for (int i = 0; i < 256; i++) {
        outw(ATA_PRIMARY_DATA, (u16)(buf[i * 2] | (buf[i * 2 + 1] << 8)));
    }
    outb(ATA_PRIMARY_DATA + 7, 0xE7);   /* flush cache */
    return ata_wait_bsy();
}

void ata_print_info(void) {
    if (disk.present) {
        kprintf("ATA primary master: %s\n", disk.model[0] ? disk.model : "(unknown)");
        kprintf("  serial %s, %u sectors, %u MiB, I/O base 0x%X\n",
                disk.serial[0] ? disk.serial : "-", disk.sectors, disk.size_mb, disk.io_base);
    } else {
        kprintf("ATA primary master: no disk detected (RAMFS is primary storage)\n");
    }
}
