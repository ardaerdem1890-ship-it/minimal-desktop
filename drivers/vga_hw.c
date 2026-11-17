/* Minimal Desktop - register-level VGA mode switching (text 80x25 <-> mode 13h) */
#include "vga_hw.h"
#include "io.h"
#include "string.h"

static void vga_seq(u8 idx, u8 val) {
    outb(0x3C4, idx);
    outb(0x3C5, val);
}

static void vga_crtc(u8 idx, u8 val) {
    outb(0x3D4, idx);
    outb(0x3D5, val);
}

static void vga_gc(u8 idx, u8 val) {
    outb(0x3CE, idx);
    outb(0x3CF, val);
}

static void vga_ac(u8 idx, u8 val) {
    (void)inb(0x3DA);       /* reset attribute-controller flip-flop */
    outb(0x3C0, idx);
    outb(0x3C0, val);
}

/* Standard 80x25 text mode (BIOS mode 3) register set. */
static const u8 text_seq[5]  = { 0x03, 0x00, 0x03, 0x00, 0x02 };
static const u8 text_crtc[25] = {
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF
};
static const u8 text_gc[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF };
static const u8 text_ac[21] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x0C, 0x00, 0x0F, 0x08, 0x00
};

/* Standard 320x200x256 mode 13h register set. */
static const u8 mode13_seq[5]  = { 0x03, 0x01, 0x0F, 0x00, 0x0E };
static const u8 mode13_crtc[25] = {
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
    0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF
};
static const u8 mode13_gc[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF };
static const u8 mode13_ac[21] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x41, 0x00, 0x0F, 0x00, 0x00
};

void vga_hw_set_text_mode(void) {
    int i;
    outb(0x3C2, 0x67);                 /* misc output: 25 MHz, text colour */

    vga_seq(0x00, 0x01);               /* synchronous reset */
    for (i = 0; i < 5; i++) vga_seq((u8)i, text_seq[i]);
    vga_seq(0x00, 0x03);

    vga_crtc(0x11, 0x00);              /* unlock CRTC 0-7 */
    for (i = 0; i < 25; i++) vga_crtc((u8)i, text_crtc[i]);

    for (i = 0; i < 9; i++) vga_gc((u8)i, text_gc[i]);
    for (i = 0; i < 21; i++) vga_ac((u8)i, text_ac[i]);

    outb(0x3C0, 0x20);                 /* enable video output */
    (void)inb(0x3DA);
    vga_hw_text_clear();
    vga_hw_set_text_cursor(0, 0);
    vga_hw_enable_cursor();
}

void vga_hw_set_mode13h(void) {
    int i;
    outb(0x3C2, 0x63);                 /* misc output: 25 MHz, 320x200 */

    vga_seq(0x00, 0x01);
    for (i = 0; i < 5; i++) vga_seq((u8)i, mode13_seq[i]);
    vga_seq(0x00, 0x03);

    vga_crtc(0x11, 0x00);              /* unlock CRTC */
    for (i = 0; i < 25; i++) vga_crtc((u8)i, mode13_crtc[i]);

    for (i = 0; i < 9; i++) vga_gc((u8)i, mode13_gc[i]);
    for (i = 0; i < 21; i++) vga_ac((u8)i, mode13_ac[i]);

    outb(0x3C0, 0x20);
    (void)inb(0x3DA);
    vga_hw_disable_cursor();
    memset((void *)0xA0000, 0, 64000);
}

/* The character generator lives in plane 2 of the 64 KiB VGA window. Mode 13h
 * writes linearly through all four planes, which destroys the font, so save it
 * before entering graphics mode and put it back on the way out. */
void vga_hw_read_font(u8 *dst, u32 bytes) {
    if (bytes > 8192) bytes = 8192;
    vga_seq(0x02, 0x04);              /* map mask: plane 2 only */
    vga_gc(0x04, 0x02);               /* read map select: plane 2 */
    vga_gc(0x05, 0x00);               /* write mode 0 */
    vga_gc(0x06, 0x00);               /* A0000 window, no odd/even */
    for (u32 i = 0; i < bytes; i++)
        dst[i] = *(volatile u8 *)(0xA0000u + i);
    for (int i = 0; i < 9; i++) vga_gc((u8)i, text_gc[i]);
    for (int i = 0; i < 5; i++) vga_seq((u8)i, text_seq[i]);
}

void vga_hw_write_font(const u8 *src, u32 bytes) {
    if (bytes > 8192) bytes = 8192;
    vga_seq(0x02, 0x04);              /* map mask: plane 2 only */
    vga_gc(0x04, 0x02);
    vga_gc(0x05, 0x00);
    vga_gc(0x06, 0x00);
    for (u32 i = 0; i < bytes; i++)
        *(volatile u8 *)(0xA0000u + i) = src[i];
    /* put the text-mode register set back (map mask, odd/even, read map) */
    for (int i = 0; i < 9; i++) vga_gc((u8)i, text_gc[i]);
    for (int i = 0; i < 5; i++) vga_seq((u8)i, text_seq[i]);
}

void vga_hw_set_text_cursor(int row, int col) {    u16 pos = (u16)(row * 80 + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

void vga_hw_enable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x0D);
    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0F);
}

void vga_hw_disable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_hw_text_clear(void) {
    u16 *vga = (u16 *)0xB8000;
    int i;
    for (i = 0; i < 80 * 25; i++) vga[i] = 0x0720;
    vga_hw_set_text_cursor(0, 0);
}
