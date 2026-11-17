#pragma once
#include "types.h"

#define GFX_WIDTH  320
#define GFX_HEIGHT 200

bool gfx_enter(void);
void gfx_leave(void);
bool gfx_active(void);
u8  *gfx_framebuffer(void);
void gfx_present(void);
void gfx_clear(u8 color);
void gfx_pixel(int x, int y, u8 color);
u8   gfx_getpixel(int x, int y);
void gfx_hline(int x, int y, int w, u8 color);
void gfx_vline(int x, int y, int h, u8 color);
void gfx_rect(int x, int y, int w, int h, u8 color);
void gfx_fill_rect(int x, int y, int w, int h, u8 color);
void gfx_frame(int x, int y, int w, int h, u8 color);
void gfx_char(int x, int y, char c, u8 color);
void gfx_text(int x, int y, const char *s, u8 color);
void gfx_text_bg(int x, int y, const char *s, u8 fg, u8 bg);
int  gfx_text_width(const char *s);
void gfx_set_palette(u8 idx, u8 r, u8 g, u8 b);
void gfx_init_palette(void);
void gfx_init_text_palette(void);
void gfx_recover_text(void);
void gfx_hline_dither(int x, int y, int w, u8 a, u8 b);
extern const u8 gfx_font8x8[128][8];
