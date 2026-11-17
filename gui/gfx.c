/* Minimal Desktop - VGA mode 13h software renderer and 8x8 font */
#include "gfx.h"
#include "vga_hw.h"
#include "console.h"
#include "io.h"
#include "string.h"
#include "font8x8.h"

#define VRAM ((u8 *)0xA0000)
#define FB_SIZE (GFX_WIDTH * GFX_HEIGHT)

static u8 backbuf[FB_SIZE];
static bool active;
static u16 saved_text[80 * 25];
static u8  saved_font[8192];
static bool font_saved;
static int saved_row, saved_col;

/* ------------------------------------------------------------------------- */
/* palette                                                                   */
/* ------------------------------------------------------------------------- */
static void dac_write(u8 idx, u8 r, u8 g, u8 b) {
    outb(0x3C8, idx);
    outb(0x3C9, (u8)(r >> 2));
    outb(0x3C9, (u8)(g >> 2));
    outb(0x3C9, (u8)(b >> 2));
}

void gfx_set_palette(u8 idx, u8 r, u8 g, u8 b) {
    dac_write(idx, r, g, b);
}

void gfx_init_palette(void) {
    static const u8 pal[20][3] = {
        {   0,   0,   0 },   /*  0 black        */
        {  10,  16,  26 },   /*  1 dark         */
        {  30,  58,  95 },   /*  2 blue         */
        {  79, 163, 209 },   /*  3 cyan         */
        { 192,  57,  43 },   /*  4 red          */
        { 142,  68, 173 },   /*  5 magenta      */
        {  39, 174,  96 },   /*  6 green        */
        { 212, 160,  23 },   /*  7 yellow       */
        {  27,  36,  48 },   /*  8 taskbar      */
        {  36,  81, 138 },   /*  9 title        */
        {  60, 120, 192 },   /* 10 title light  */
        { 221, 226, 231 },   /* 11 panel        */
        {  26,  32,  40 },   /* 12 ink          */
        { 138, 146, 156 },   /* 13 grey         */
        {  82, 210, 115 },   /* 14 bright green */
        { 255, 255, 255 },   /* 15 white        */
        {  12,  16,  24 },   /* 16 desk 1       */
        {  16,  24,  39 },   /* 17 desk 2       */
        {  21,  32,  52 },   /* 18 desk 3       */
        { 255, 255, 255 }    /* 19 cursor       */
    };
    for (u8 i = 0; i < (u8)(sizeof(pal) / sizeof(pal[0])); i++)
        dac_write(i, pal[i][0], pal[i][1], pal[i][2]);
    /* Fill the rest with a neutral grey ramp so stray indices stay sane. */
    for (int i = 20; i < 256; i++)
        dac_write((u8)i, (u8)i, (u8)i, (u8)i);
}

/* Restore the standard 16-colour VGA text palette (mode 13h rewrites DAC). */
void gfx_init_text_palette(void) {
    static const u8 pal[16][3] = {
        {  0,  0,  0 }, {  0,  0, 170 }, {  0,170,  0 }, {  0,170,170 },
        {170,  0,  0 }, {170,  0,170 }, {170, 85,  0 }, {170,170,170 },
        { 85, 85, 85 }, { 85, 85,255 }, { 85,255, 85 }, { 85,255,255 },
        {255, 85, 85 }, {255, 85,255 }, {255,255, 85 }, {255,255,255 }
    };
    for (u8 i = 0; i < 16; i++) dac_write(i, pal[i][0], pal[i][1], pal[i][2]);
}

/* ------------------------------------------------------------------------- */
/* mode switching                                                            */
/* ------------------------------------------------------------------------- */
bool gfx_enter(void) {
    memcpy(saved_text, (const void *)0xB8000, sizeof(saved_text));
    vga_hw_read_font(saved_font, sizeof(saved_font));
    font_saved = true;
    console_get_cursor(&saved_row, &saved_col);
    vga_hw_set_mode13h();
    gfx_init_palette();
    memset(backbuf, 0, FB_SIZE);
    active = true;
    return true;
}

void gfx_leave(void) {
    if (!active) return;
    vga_hw_set_text_mode();
    if (font_saved) vga_hw_write_font(saved_font, sizeof(saved_font));
    memcpy((void *)0xB8000, saved_text, sizeof(saved_text));
    gfx_init_text_palette();
    console_gotoxy(saved_row, saved_col);
    active = false;
}

/* Called from the exception handler: a fault may have happened inside mode 13h,
 * so the text-mode registers, the character generator in plane 2 and the DAC
 * palette all have to be put back before anything can be printed. */
void gfx_recover_text(void) {
    vga_hw_set_text_mode();
    if (font_saved) vga_hw_write_font(saved_font, sizeof(saved_font));
    gfx_init_text_palette();
    console_gotoxy(saved_row, saved_col);
    active = false;
}

bool gfx_active(void) { return active; }
u8 *gfx_framebuffer(void) { return backbuf; }

void gfx_present(void) {
    if (!active) return;
    memcpy(VRAM, backbuf, FB_SIZE);
}

/* ------------------------------------------------------------------------- */
/* primitives                                                                */
/* ------------------------------------------------------------------------- */
void gfx_clear(u8 color) {
    memset(backbuf, color, FB_SIZE);
}

void gfx_pixel(int x, int y, u8 color) {
    if ((unsigned)x >= GFX_WIDTH || (unsigned)y >= GFX_HEIGHT) return;
    backbuf[y * GFX_WIDTH + x] = color;
}

u8 gfx_getpixel(int x, int y) {
    if ((unsigned)x >= GFX_WIDTH || (unsigned)y >= GFX_HEIGHT) return 0;
    return backbuf[y * GFX_WIDTH + x];
}

void gfx_hline(int x, int y, int w, u8 color) {
    if ((unsigned)y >= GFX_HEIGHT) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > GFX_WIDTH) w = GFX_WIDTH - x;
    if (w <= 0) return;
    memset(backbuf + y * GFX_WIDTH + x, color, (size_t)w);
}

void gfx_vline(int x, int y, int h, u8 color) {
    if ((unsigned)x >= GFX_WIDTH) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > GFX_HEIGHT) h = GFX_HEIGHT - y;
    u8 *p = backbuf + y * GFX_WIDTH + x;
    for (int i = 0; i < h; i++) { *p = color; p += GFX_WIDTH; }
}

void gfx_fill_rect(int x, int y, int w, int h, u8 color) {
    for (int i = 0; i < h; i++) gfx_hline(x, y + i, w, color);
}

void gfx_rect(int x, int y, int w, int h, u8 color) {
    gfx_hline(x, y, w, color);
    gfx_hline(x, y + h - 1, w, color);
    gfx_vline(x, y, h, color);
    gfx_vline(x + w - 1, y, h, color);
}

void gfx_frame(int x, int y, int w, int h, u8 color) {
    gfx_rect(x, y, w, h, color);
}

void gfx_hline_dither(int x, int y, int w, u8 a, u8 b) {
    for (int i = 0; i < w; i++) gfx_pixel(x + i, y, ((x + i + y) & 1) ? b : a);
}

/* ------------------------------------------------------------------------- */
/* text                                                                      */
/* ------------------------------------------------------------------------- */
void gfx_char(int x, int y, char c, u8 color) {
    u8 ch = (u8)c;
    if (ch >= 128) ch = '?';
    const u8 *g = gfx_font8x8[ch];
    for (int row = 0; row < 8; row++) {
        u8 bits = g[row];
        for (int col = 0; col < 8; col++)
            if (bits & (1u << col)) gfx_pixel(x + col, y + row, color);
    }
}

void gfx_text(int x, int y, const char *s, u8 color) {
    while (*s) {
        gfx_char(x, y, *s++, color);
        x += 8;
    }
}

void gfx_text_bg(int x, int y, const char *s, u8 fg, u8 bg) {
    int w = gfx_text_width(s);
    gfx_fill_rect(x, y, w, 8, bg);
    gfx_text(x, y, s, fg);
}

int gfx_text_width(const char *s) {
    return (int)strlen(s) * 8;
}
