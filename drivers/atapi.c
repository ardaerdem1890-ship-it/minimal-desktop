/* Minimal Desktop - ATAPI CD-ROM driver (secondary IDE master, PIO)
 *
 * QEMU/VirtualBox attach the boot CD to the secondary IDE bus. This driver
 * issues READ(10) via the ATA PACKET command and reads 2048-byte sectors.
 */
#include "atapi.h"
#include "io.h"
#include "console.h"
#include "pit.h"
#include "string.h"
#include "mm.h"

#define IO  0x170u
#define CTRL 0x376u

static bool present;

static void delay400(void) {
    for (int i = 0; i < 4; i++) (void)inb(CTRL);
}

static bool wait_bsy(void) {
    for (u32 i = 0; i < 5000000; i++) {
        if (!(inb(IO + 7) & 0x80)) return true;
    }
    return false;
}

static bool wait_drq(void) {
    for (u32 i = 0; i < 5000000; i++) {
        u8 s = inb(IO + 7);
        if (s & 0x01) return false;
        if ((s & 0x08) && !(s & 0x80)) return true;
    }
    return false;
}

bool atapi_present(void) { return present; }

bool atapi_init(void) {
    present = false;
    outb(CTRL, 0x02);
    delay400();
    outb(CTRL, 0x00);
    delay400();

    outb(IO + 6, 0xA0);              /* secondary master */
    delay400();
    outb(IO + 2, 0);
    outb(IO + 3, 0);
    outb(IO + 4, 0);
    outb(IO + 5, 0);
    outb(IO + 7, 0xEC);              /* IDENTIFY */
    delay400();
    u8 st = inb(IO + 7);
    if (st == 0 || st == 0xFF) return false;
    if (!wait_bsy()) return false;
    u8 cyl = inb(IO + 4), head = inb(IO + 5);
    /* ATAPI devices report cyl == 0x14, head == 0xEB on IDENTIFY */
    if (!(cyl == 0x14 && head == 0xEB)) return false;
    if (!wait_drq()) return false;
    u16 ident[256];
    for (int i = 0; i < 256; i++) ident[i] = inw(IO);
    if (!((ident[0] >> 8) & 0x80)) return false;   /* bit 15 = ATAPI */
    present = true;
    return true;
}

bool atapi_read_sector(u32 lba, u8 *buf) {
    if (!present) return false;
    /* PACKET command setup */
    outb(IO + 6, 0xA0);
    delay400();
    outb(IO + 1, 0);                 /* features */
    outb(IO + 2, 0);                 /* sector count */
    outb(IO + 3, (u8)(lba & 0xFF));
    outb(IO + 4, (u8)((lba >> 8) & 0xFF));
    outb(IO + 5, (u8)((lba >> 16) & 0xFF));
    outb(IO + 6, 0xA0);              /* LBA, master */
    outb(IO + 7, 0xA0);              /* PACKET */

    if (!wait_bsy() || !wait_drq()) return false;

    /* READ(10) CDB */
    u8 cdb[12] = {0};
    cdb[0] = 0x28;
    cdb[2] = (u8)((lba >> 24) & 0xFF);
    cdb[3] = (u8)((lba >> 16) & 0xFF);
    cdb[4] = (u8)((lba >> 8) & 0xFF);
    cdb[5] = (u8)(lba & 0xFF);
    cdb[8] = 1;                      /* one block */
    for (int i = 0; i < 6; i++) outw(IO, (u16)(cdb[i * 2] | (cdb[i * 2 + 1] << 8)));

    if (!wait_bsy() || !wait_drq()) return false;
    /* 2048 bytes = 1024 words */
    for (int i = 0; i < 1024; i++) {
        u16 w = inw(IO);
        buf[i * 2] = (u8)(w & 0xFF);
        buf[i * 2 + 1] = (u8)(w >> 8);
    }
    return true;
}

bool atapi_selftest(char *err, int errlen) {
    if (!present) { ksnprintf(err, errlen, "no ATAPI device"); return false; }
    u8 *buf = (u8 *)kmalloc(2048);
    if (!buf) { ksnprintf(err, errlen, "out of memory"); return false; }
    bool ok = atapi_read_sector(16, buf);
    kfree(buf);
    if (!ok) { ksnprintf(err, errlen, "cannot read PVD"); return false; }
    return true;
}
