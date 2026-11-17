/* Minimal Desktop - extra features: screensavers, music, backup, slideshow */
#include "extras.h"
#include "console.h"
#include "keyboard.h"
#include "pit.h"
#include "mm.h"
#include "ramfs.h"
#include "fat.h"
#include "string.h"
#include "gfx.h"
#include "io.h"
#include "bmp.h"

static u32 rng_state = 0x12345678u;
static u32 xr(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* ------------------------------------------------------------------------- */
/* Screensavers                                                               */
/* ------------------------------------------------------------------------- */
void screensaver_run(int mode, int seconds) {
    u8 ofg, obg;
    console_get_theme(&ofg, &obg);
    console_clear();
    u32 end = pit_ticks() + (u32)seconds * 100;

    if (mode == 0) {          /* matrix rain */
        static int col_y[80];
        for (int x = 0; x < 80; x++) col_y[x] = (int)(xr() % 25);
        while ((s32)(pit_ticks() - end) < 0) {
            for (int x = 0; x < 80; x++) {
                int y = col_y[x];
                char c = (char)('!' + (xr() % 90));
                console_put_at(y, x, c, console_make_attr(VGA_GREEN, VGA_BLACK));
                if (y > 0) console_put_at(y - 1, x, ' ', 0);
                col_y[x] = (y + 1) % 25;
            }
            pit_sleep_ms(80);
            if (kbd_getchar() == KEY_ESC) break;
        }
    } else if (mode == 1) {   /* starfield */
        typedef struct { float x, y, z; } star_t;
        static star_t stars[120];
        for (int i = 0; i < 120; i++) {
            stars[i].x = (float)((int)(xr() % 2000) - 1000) / 100;
            stars[i].y = (float)((int)(xr() % 2000) - 1000) / 100;
            stars[i].z = (float)(1 + (xr() % 100));
        }
        while ((s32)(pit_ticks() - end) < 0) {
            console_clear();
            for (int i = 0; i < 120; i++) {
                stars[i].z -= 0.4f;
                if (stars[i].z < 0.1f) {
                    stars[i].x = (float)((int)(xr() % 2000) - 1000) / 100;
                    stars[i].y = (float)((int)(xr() % 2000) - 1000) / 100;
                    stars[i].z = 10.0f;
                }
                int sx = 40 + (int)(stars[i].x / stars[i].z * 40);
                int sy = 12 + (int)(stars[i].y / stars[i].z * 12);
                if (sx >= 0 && sx < 80 && sy >= 0 && sy < 25) {
                    char ch = stars[i].z < 3 ? '*' : '.';
                    console_put_at(sy, sx, ch, console_make_attr(VGA_LIGHT_GREY, VGA_BLACK));
                }
            }
            pit_sleep_ms(60);
            if (kbd_getchar() == KEY_ESC) break;
        }
    } else if (mode == 2) {   /* bouncing logo */
        static const char *logo[] = {
            "  __  __ ___  ___  __   ___  _  _  ___  ___  ",
            " |  \\/  | _ \\/ __| \\ \\ / /_ \\| \\| |/ __|| _ \\ ",
            " | |\\/| |   /\\__ \\  \\ V /| | | .` |\\__ \\|   / ",
            " |_|  |_|_|_\\|___/   \\_/ |___|_|\\_||___/|_|_\\ "
        };
        int x = 0, y = 5, dx = 1, dy = 1;
        while ((s32)(pit_ticks() - end) < 0) {
            console_clear();
            for (int r = 0; r < 4; r++)
                console_write_at(y + r, x, logo[r], console_make_attr(VGA_LIGHT_GREEN, VGA_BLACK));
            x += dx; y += dy;
            if (x < 0 || x + 48 >= 80) dx = -dx;
            if (y < 0 || y + 5 >= 25) dy = -dy;
            pit_sleep_ms(90);
            if (kbd_getchar() == KEY_ESC) break;
        }
    } else {                  /* plasma */
        for (int frame = 0; frame < 200 && (s32)(pit_ticks() - end) < 0; frame++) {
            for (int row = 0; row < 25; row++) {
                for (int col = 0; col < 80; col++) {
                    int v = (int)(8 * (1 + (col * 8 / 80)) + 8 * (1 + (row * 8 / 25)) + frame);
                    u8 colors[] = { VGA_BLACK, VGA_BLUE, VGA_GREEN, VGA_CYAN,
                                    VGA_RED, VGA_MAGENTA, VGA_YELLOW, VGA_WHITE };
                    console_put_at(row, col, '#', console_make_attr(colors[v % 8], colors[(v / 8) % 8]));
                }
            }
            pit_sleep_ms(80);
            if (kbd_getchar() == KEY_ESC) break;
        }
    }
    console_clear();
    console_set_theme(ofg, obg);
}

/* ------------------------------------------------------------------------- */
/* Music: notes like "C4 E4 G4" separated by spaces/newlines                  */
/* ------------------------------------------------------------------------- */
static double pow2(double x) {
    /* crude 2^x via exp approximation: 2^x = e^(x ln2) */
    double e = 1.0, term = 1.0, a = x * 0.69314718056;
    for (int i = 1; i < 20; i++) {
        term *= a / i;
        e += term;
    }
    return e;
}

static int note_freq(char name, int oct) {
    static const int semi[7] = { 0, 2, 4, 5, 7, 9, 11 };
    int idx = -1;
    switch (name) {
        case 'C': case 'c': idx = 0; break;
        case 'D': case 'd': idx = 1; break;
        case 'E': case 'e': idx = 2; break;
        case 'F': case 'f': idx = 3; break;
        case 'G': case 'g': idx = 4; break;
        case 'A': case 'a': idx = 5; break;
        case 'B': case 'b': idx = 6; break;
    }
    if (idx < 0) return 0;
    int midi = 60 + 12 * (oct - 4) + semi[idx];
    return (int)(440.0 * pow2((double)(midi - 69) / 12.0));
}

int music_play(const char *path) {
    fs_node_t *f = ramfs_lookup(path);
    if (!f || f->type != FS_FILE) { kprintf("play: %s not found\n", path); return -1; }
    char *buf = (char *)kmalloc(f->size + 1);
    if (!buf) return -1;
    int got = ramfs_read(f, buf, f->size, 0);
    buf[got > 0 ? got : 0] = 0;

    char *p = buf;
    int notes = 0;
    while (*p) {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        if (!*p) break;
        char name = *p++;
        int oct = 4;
        if (*p >= '0' && *p <= '9') { oct = *p - '0'; p++; }
        int freq = note_freq(name, oct);
        if (freq > 0) {
            u32 div = 1193180u / (u32)freq;
            outb(0x43, 0xB6);
            outb(0x42, (u8)(div & 0xFF));
            outb(0x42, (u8)(div >> 8));
            u8 port = inb(0x61);
            outb(0x61, (u8)(port | 0x03));
            pit_sleep_ms(220);
            outb(0x61, (u8)(port & 0xFC));
            pit_sleep_ms(40);
            notes++;
        }
        kprintf(".");
    }
    kfree(buf);
    kprintf("\nplay: %d notes\n", notes);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Theme persistence                                                          */
/* ------------------------------------------------------------------------- */
void theme_save(void) {
    u8 fg, bg;
    console_get_theme(&fg, &bg);
    if (!ramfs_lookup("/disk")) return;
    if (ramfs_lookup("/disk/theme.cfg")) ramfs_unlink("/disk/theme.cfg");
    fs_node_t *f = ramfs_create("/disk/theme.cfg", FS_FILE);
    if (!f) return;
    char s[32];
    ksnprintf(s, sizeof(s), "%u %u\n", fg, bg);
    ramfs_write(f, s, (u32)strlen(s), 0);
}

void theme_load(void) {
    fs_node_t *f = ramfs_lookup("/disk/theme.cfg");
    if (!f) return;
    char s[32];
    int n = ramfs_read(f, s, sizeof(s) - 1, 0);
    s[n > 0 ? n : 0] = 0;
    int fg = atoi(s);
    const char *sp = strchr(s, ' ');
    int bg = sp ? atoi(sp + 1) : VGA_BLACK;
    if (fg >= 0 && fg < 16 && bg >= 0 && bg < 16)
        console_set_theme((u8)fg, (u8)bg);
}

/* ------------------------------------------------------------------------- */
/* Backup / restore                                                           */
/* ------------------------------------------------------------------------- */
int backup_create(const char *path) {
    fs_node_t *dst = ramfs_lookup(path);
    if (!dst) dst = ramfs_create(path, FS_FILE);
    if (!dst || dst->type != FS_FILE) return -1;
    ramfs_truncate(dst, 0);

    char header[16];
    ksnprintf(header, sizeof(header), "MDBACKUP1\n");
    ramfs_append(dst, header, (u32)strlen(header));

    u32 total_files = 0;
    /* walk all ramfs nodes via / recursion */
    fs_node_t *root = ramfs_root();
    fs_node_t *child;
    for (int i = 0; ramfs_readdir(root, i, &child) == 0; i++) {
        if (child->type != FS_FILE) continue;
        if (child == dst) continue;
        char p[128];
        ramfs_path(child, p, sizeof(p));
        u32 sz = child->size;
        ramfs_append(dst, p, (u32)strlen(p));
        ramfs_append(dst, "\n", 1);
        ramfs_append(dst, (const char *)&sz, 4);
        ramfs_append(dst, child->data, sz);
        total_files++;
    }
    kprintf("backup: %u files -> %s (%u bytes)\n", total_files, path, dst->size);
    return 0;
}

int backup_restore(const char *path) {
    fs_node_t *f = ramfs_lookup(path);
    if (!f || f->type != FS_FILE) { kprintf("restore: %s not found\n", path); return -1; }
    u8 *data = (u8 *)kmalloc(f->size);
    if (!data) return -1;
    int got = ramfs_read(f, data, f->size, 0);
    if (got <= 16 || memcmp(data, "MDBACKUP1\n", 10) != 0) {
        kprintf("restore: not a Minimal Desktop backup\n"); kfree(data); return -1;
    }
    int pos = 10;
    int files = 0;
    while (pos + 5 < got) {
        int end = pos;
        while (end < got && data[end] != '\n') end++;
        if (end >= got) break;
        char name[128];
        int nlen = end - pos;
        if (nlen <= 0 || nlen >= (int)sizeof(name)) break;
        memcpy(name, data + pos, (size_t)nlen);
        name[nlen] = 0;
        pos = end + 1;
        if (pos + 4 > got) break;
        u32 sz;
        memcpy(&sz, data + pos, 4);
        pos += 4;
        if (sz > (u32)(got - pos)) break;
        fs_node_t *dst = ramfs_lookup(name);
        if (!dst) dst = ramfs_create(name, FS_FILE);
        if (dst && dst->type == FS_FILE) {
            ramfs_truncate(dst, 0);
            ramfs_write(dst, data + pos, sz, 0);
            files++;
        }
        pos += (int)sz;
    }
    kfree(data);
    kprintf("restore: restored %d files from %s\n", files, path);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Slideshow                                                                  */
/* ------------------------------------------------------------------------- */
int slideshow_run(int seconds) {
    if (!gfx_enter()) { kprintf("slideshow: cannot enter graphics mode\n"); return -1; }
    fs_node_t *dir = ramfs_lookup("/pictures");
    if (!dir || dir->type != FS_DIR) { gfx_leave(); kprintf("slideshow: no /pictures\n"); return -1; }
    u8 *fb = gfx_framebuffer();
    char paths[16][128];
    int n = 0;
    fs_node_t *child;
    for (int i = 0; ramfs_readdir(dir, i, &child) == 0 && n < 16; i++) {
        if (child->type != FS_FILE) continue;
        int len = (int)strlen(child->name);
        if (len < 4 || strcmp(child->name + len - 4, ".bmp") != 0) continue;
        ksnprintf(paths[n++], sizeof(paths[0]), "/pictures/%s", child->name);
    }
    if (n == 0) { gfx_leave(); kprintf("slideshow: no images\n"); return -1; }
    u32 end = pit_ticks() + (u32)seconds * 100;
    int idx = 0;
    while ((s32)(pit_ticks() - end) < 0) {
        fs_node_t *f = ramfs_lookup(paths[idx]);
        if (f) {
            u8 *data = (u8 *)kmalloc(f->size);
            if (data) {
                int got = ramfs_read(f, data, f->size, 0);
                if (got == (int)f->size) bmp_load(data, (u32)got, fb, GFX_WIDTH, GFX_HEIGHT);
                kfree(data);
            }
        }
        gfx_present();
        u32 frame_end = pit_ticks() + 200;
        bool skip = false;
        while ((s32)(pit_ticks() - frame_end) < 0) {
            if (kbd_getchar() == KEY_ESC) { skip = true; break; }
            pit_sleep_ms(20);
        }
        if (skip) break;
        idx = (idx + 1) % n;
    }
    gfx_leave();
    kprintf("slideshow: done (%d images)\n", n);
    return 0;
}

int chkdsk_fat(void) {
    char errbuf[128];
    if (!fat_present()) {
        if (!fat_automount()) { kprintf("chkdsk: no FAT partition\n"); return -1; }
    }
    kprintf("chkdsk: checking FAT filesystem...\n");
    if (!fat_selftest(errbuf, sizeof(errbuf))) {
        kprintf("chkdsk: %s\n", errbuf);
        return -1;
    }
    kprintf("chkdsk: reserved clusters OK, no errors found\n");
    return 0;
}
