/* Minimal Desktop - tiny BMP decoder + generated sample images */
#include "bmp.h"
#include "ramfs.h"
#include "mm.h"
#include "string.h"

static u16 rd16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 rd32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

int bmp_load(const u8 *data, u32 size, u8 *out, int out_w, int out_h) {
    if (!data || size < 54 || data[0] != 'B' || data[1] != 'M') return -1;
    u32 off = rd32(data + 10);
    int w = (int)rd32(data + 18);
    int h = (int)rd32(data + 22);
    if (h < 0) h = -h;
    u16 bpp = rd16(data + 28);
    u32 compression = rd32(data + 30);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || compression) return -1;
    if (bpp != 8 && bpp != 24) return -2;
    if (off + (u32)w * h * (bpp / 8) > size) return -1;

    int x0 = 0, y0 = 0, cw = w, ch = h;
    if (w > out_w) { x0 = (w - out_w) / 2; cw = out_w; }
    if (h > out_h) { y0 = (h - out_h) / 2; ch = out_h; }
    memset(out, 0, (size_t)(out_w * out_h));

    /* nearest-neighbour scale if the image is smaller than the window */
    int sx = (cw > 0 && cw <= out_w && cw < w) ? 1 : 1;
    (void)sx;

    u32 palette[256];
    if (bpp == 8) {
        for (int i = 0; i < 256; i++) {
            const u8 *p = data + 54 + i * 4;
            palette[i] = (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16);
        }
    }

    for (int row = 0; row < ch; row++) {
        int src_row = row + y0;
        const u8 *src = data + off + (u32)src_row * ((u32)w * (bpp / 8)) + x0 * (bpp / 8);
        u8 *dst = out + (size_t)row * out_w;
        for (int x = 0; x < cw; x++) {
            u32 rgb;
            if (bpp == 8) rgb = palette[src[x]];
            else {
                const u8 *p = src + x * 3;
                rgb = (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16);
            }
            /* map RGB to the 20-colour GUI palette */
            int r = (int)(rgb & 0xFF), g = (int)((rgb >> 8) & 0xFF), b = (int)((rgb >> 16) & 0xFF);
            int lum = (r + g + b) / 3;
            u8 c;
            if (r > 190 && g > 190 && b > 190) c = 15;
            else if (r > 150 && g < 90 && b < 90) c = 4;
            else if (g > 130 && r < 120 && b < 120) c = 6;
            else if (b > 130 && r < 120 && g < 150) c = 1;
            else if (b > 150 && g > 130 && r < 120) c = 3;
            else if (r > 140 && g > 120 && b < 100) c = 7;
            else if (r > 120 && b > 120 && g < 120) c = 5;
            else if (lum > 170) c = 11;
            else if (lum > 110) c = 13;
            else if (lum > 55) c = 1;
            else c = 0;
            dst[x] = c;
        }
    }
    return 0;
}

/* ------------------------- generated sample images ----------------------- */
typedef struct {
    const char *name;
    u8 r, g, b;
} swatch_t;

static const swatch_t SWATCHES[] = {
    { "sunset",  255, 128,  40 },
    { "ocean",    20,  90, 200 },
    { "forest",   30, 160,  70 },
    { "grape",   130,  60, 200 },
    { "rose",    220,  60, 120 },
    { "sky",      90, 180, 240 },
    { "gold",    230, 190,  40 },
    { "slate",    80,  90, 110 },
};

static void write_bmp(const char *path, int w, int h, u32 (*pixel)(int, int, u32)) {
    u32 size = 54 + (u32)w * h * 3;
    u8 *bmp = (u8 *)kmalloc(size);
    if (!bmp) return;
    memset(bmp, 0, size);
    bmp[0] = 'B'; bmp[1] = 'M';
    bmp[2] = (u8)size; bmp[3] = (u8)(size >> 8); bmp[4] = (u8)(size >> 16);
    bmp[10] = 54;
    bmp[14] = 40;
    bmp[18] = (u8)w; bmp[19] = (u8)(w >> 8);
    /* height is a full signed 32-bit value (negative = top-down) */
    s32 negh = -h;
    bmp[22] = (u8)(negh & 0xFF); bmp[23] = (u8)((negh >> 8) & 0xFF);
    bmp[24] = (u8)((negh >> 16) & 0xFF); bmp[25] = (u8)((negh >> 24) & 0xFF);
    /* planes (offset 26) = 1, bits-per-pixel (offset 28) = 24 */
    bmp[26] = 1;
    bmp[28] = 24;
    bmp[34] = (u8)(w * h * 3); bmp[35] = (u8)((w * h * 3) >> 8);
    for (int y = 0; y < h; y++) {
        u8 *row = bmp + 54 + (u32)y * w * 3;
        for (int x = 0; x < w; x++) {
            u32 rgb = pixel(x, y, 0);
            row[x * 3 + 0] = (u8)(rgb & 0xFF);
            row[x * 3 + 1] = (u8)((rgb >> 8) & 0xFF);
            row[x * 3 + 2] = (u8)((rgb >> 16) & 0xFF);
        }
    }
    if (ramfs_lookup(path)) ramfs_unlink(path);
    fs_node_t *f = ramfs_create(path, FS_FILE);
    if (f) ramfs_write(f, bmp, size, 0);
    kfree(bmp);
}

static u32 px_gradient(int x, int y, u32 seed) {
    const swatch_t *s = &SWATCHES[seed % 8];
    int t = (x * 255) / 159;
    int fade = (y * 255) / 119;
    int r = (s->r * t) / 255 + fade / 8;
    int g = (s->g * t) / 255 + fade / 8;
    int b = (s->b * t) / 255 + fade / 8;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (u32)((u32)r | ((u32)g << 8) | ((u32)b << 16));
}

static u32 px_checker(int x, int y, u32 seed) {
    (void)seed;
    bool on = ((x / 16) + (y / 16)) & 1;
    return on ? 0x00F0F0F0 : 0x00102030;
}

static u32 px_smiley(int x, int y, u32 seed) {
    (void)seed;
    int dx = x - 80, dy = y - 60;
    if (dx * dx + dy * dy > 52 * 52) return 0x00081018;
    /* eyes */
    if ((x > 58 && x < 68 && y > 42 && y < 54) ||
        (x > 92 && x < 102 && y > 42 && y < 54)) return 0x00101010;
    /* mouth */
    int mx = x - 80, my = y - 74;
    if (my > 0 && my < 18 && mx * mx + my * my > 24 * 24 && mx * mx + my * my < 30 * 30)
        return 0x00101010;
    return 0x00F0D020;
}

static u32 px_maze(int x, int y, u32 seed) {
    (void)seed;
    int cx = (x / 20) % 3, cy = (y / 20) % 3;
    bool wall = (cx == 0 || cx == 2) && (cy == 0 || cy == 2);
    if (x % 20 < 2 || y % 20 < 2) wall = true;
    return wall ? 0x001A3A5A : 0x00C0D0D8;
}

static u32 px_textcard(int x, int y, u32 seed) {
    (void)seed;
    if (x < 6 || x > 153 || y < 6 || y > 113) return 0x00102030;
    /* 5x7 bitmap font for "HELLO" */
    static const u8 glyphs[5][7] = {
        {0x11,0x11,0x1F,0x11,0x11,0x11,0x11},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
        {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1F},
    };
    int lx = (x - 30) / 12;
    int ly = (y - 40) / 12;
    if (lx >= 0 && lx < 5 && ly >= 0 && ly < 7) {
        int bit = 6 - ((y - 40) % 12) / 2;
        if (bit >= 0 && bit < 7 && (glyphs[lx][bit] & (1 << (((x - 30) % 12) / 2))))
            return 0x00FFFFFF;
    }
    return 0x00004020;
}

static u32 px_mandel(int x, int y, u32 s) {
    (void)s;
    double cx = (x - 80) / 40.0, cy = (y - 60) / 40.0;
    double zx = 0, zy = 0;
    int it = 0;
    while (zx*zx + zy*zy < 4 && it < 40) {
        double t = zx*zx - zy*zy + cx;
        zy = 2*zx*zy + cy; zx = t; it++;
    }
    int v = it * 6;
    return (u32)(0xFF000000u | ((v & 0xFF) << 16) | ((v*2 & 0xFF) << 8) | (v*3 & 0xFF));
}

static u32 px_waves(int x, int y, u32 s) {
    (void)s;
    int v = (int)(40*((x*3)/16.0) + 30*((y*5)/16.0));
    int r = (v/2) & 0xFF, g = (v/3) & 0xFF, b = (v/4) & 0xFF;
    return 0xFF000000u | ((u32)b<<16) | ((u32)g<<8) | (u32)r;
}

static u32 px_check3d(int x, int y, u32 s) {
    (void)s;
    int cx = (x/20), cy = (y/20);
    int shade = ((cx+cy)&1) ? 180 : 40;
    return 0xFF000000u | ((u32)shade<<16) | ((u32)shade<<8) | (u32)shade;
}

static u32 px_logoc(int x, int y, u32 s) {
    (void)s;
    int dx=x-80, dy=y-60;
    if (dx*dx+dy*dy > 50*50) return 0xFF102030u;
    return 0xFF00FF80u;
}

static u32 px_grad0(int x, int y, u32 s) { (void)s; return px_gradient(x, y, 0); }
static u32 px_grad1(int x, int y, u32 s) { (void)s; return px_gradient(x, y, 1); }
static u32 px_grad2(int x, int y, u32 s) { (void)s; return px_gradient(x, y, 3); }
static u32 px_grad3(int x, int y, u32 s) { (void)s; return px_gradient(x, y, 4); }
static u32 px_check(int x, int y, u32 s) { (void)s; return px_checker(x, y, 0); }
static u32 px_smile(int x, int y, u32 s) { (void)s; return px_smiley(x, y, 0); }
static u32 px_mazex(int x, int y, u32 s) { (void)s; return px_maze(x, y, 0); }
static u32 px_text(int x, int y, u32 s) { (void)s; return px_textcard(x, y, 0); }

int bmp_sample_images(void) {
    if (!ramfs_lookup("/pictures")) ramfs_create("/pictures", FS_DIR);
    write_bmp("/pictures/sunset.bmp",  160, 120, px_grad0);
    write_bmp("/pictures/ocean.bmp",   160, 120, px_grad1);
    write_bmp("/pictures/forest.bmp",  160, 120, px_grad2);
    write_bmp("/pictures/grape.bmp",   160, 120, px_grad3);
    write_bmp("/pictures/checkers.bmp",160, 120, px_check);
    write_bmp("/pictures/smiley.bmp",  160, 120, px_smile);
    write_bmp("/pictures/maze.bmp",    160, 120, px_mazex);
    write_bmp("/pictures/hello.bmp",   160, 120, px_text);
    write_bmp("/pictures/mandel.bmp",  160, 120, px_mandel);
    write_bmp("/pictures/waves.bmp",   160, 120, px_waves);
    write_bmp("/pictures/check3d.bmp", 160, 120, px_check3d);
    write_bmp("/pictures/logo.bmp",    160, 120, px_logoc);
    return 0;
}
