/* Minimal Desktop - composited GUI desktop on VGA mode 13h
 *
 * Double-buffered compositor with windows, taskbar/tray, and built-in apps:
 * terminal, notepad, tasks, network, minimal web browser, file explorer.
 */
#include "gui.h"
#include "gfx.h"
#include "icons.h"
#include "mouse.h"
#include "keyboard.h"
#include "pit.h"
#include "rtc.h"
#include "task.h"
#include "net.h"
#include "store.h"
#include "shell.h"
#include "console.h"
#include "string.h"
#include "mm.h"
#include "ramfs.h"
#include "bmp.h"
#include "io.h"

#define TASKBAR_Y 178
#define TASKBAR_H 22
#define TITLE_H   10

enum { WIN_TERM = 0, WIN_NOTE, WIN_TASKS, WIN_NET, WIN_BROWSER, WIN_EXPLORER, WIN_IMGVIEW, WIN_COUNT };

typedef struct {
    bool open;
    int  x, y, w, h;
    int  kind;
    const char *title;
} gui_win_t;

static gui_win_t wins[WIN_COUNT] = {
    { false,  10,  14, 214, 124, WIN_TERM,     "terminal"  },
    { false,  40,  18, 190, 116, WIN_NOTE,     "notepad"   },
    { false, 140,  24, 168, 116, WIN_TASKS,    "tasks"     },
    { false,  30,  24, 206, 126, WIN_NET,      "network"   },
    { false,  18,  10, 280, 150, WIN_BROWSER,  "minimalweb"},
    { false,  26,  16, 240, 140, WIN_EXPLORER, "explorer"  },
    { false,  50,  12, 220, 160, WIN_IMGVIEW,  "imgview"   }
};

static int zorder[WIN_COUNT] = { 0, 1, 2, 3, 4, 5, 6 };
static int focus = WIN_TERM;
static bool start_open;
static int drag_win = -1;
/* forward declarations for app key handlers (used by click dispatch) */
static void terminal_key(int key);
static void notepad_key(int key);
static void network_key(int key);
static void browser_key(int key);
static void explorer_key(int key);
static void imgview_key(int key);

static int drag_dx, drag_dy;
static bool prev_left;
static bool running;
static int launch_req = GUI_LAUNCH_NONE;
static bool speaker_muted = false;
static int speaker_volume = 2; /* 0..3 */
static int shot_counter;
static int maximized_win = -1;
static u32 super_last;

/* terminal state */
static char term_buf[1400];
static int  term_len;
static char term_cmd[80];
static int  term_cmd_len;

/* notepad state */
static char note_buf[2048];
static int  note_len;

/* browser state */
static char br_url[128];
static char br_input[128];
static char br_content[4096];
static int  br_scroll;
static bool br_loading_msg;

/* image viewer state */
static char iv_files[32][128];
static int  iv_count;
static int  iv_index;
static u8   iv_frame[320 * 200];

/* explorer state */
static char ex_path[160];
static char ex_names[32][FS_NAME_MAX];
static u8   ex_types[32];
static u32  ex_sizes[32];
static int  ex_count;
static int  ex_sel;
static int  ex_scroll;

/* ------------------------------------------------------------------------- */
/* helpers                                                                   */
/* ------------------------------------------------------------------------- */
static void buf_append(char *buf, int *len, int max, const char *s) {
    while (*s && *len < max - 1) buf[(*len)++] = *s++;
    buf[*len] = 0;
}
static void buf_append_char(char *buf, int *len, int max, char c) {
    if (*len < max - 1) { buf[(*len)++] = c; buf[*len] = 0; }
}

static void draw_text_tail(int x, int y, int cols, int max_lines,
                           const char *text, u8 color) {
    if (cols <= 0 || max_lines <= 0) return;
    int starts[128];
    int total = 0, col = 0;
    const char *p = text;
    if (total < 128) starts[total++] = 0;
    while (*p) {
        if (*p == '\n') {
            p++; col = 0;
            if (total < 128) starts[total] = (int)(p - text);
            total++; continue;
        }
        if (col >= cols) {
            col = 0;
            if (total < 128) starts[total] = (int)(p - text);
            total++;
        }
        p++; col++;
    }
    if (total < 128) starts[total] = (int)(p - text);
    int first = total - max_lines;
    if (first < 0) first = 0;
    for (int l = first; l < total && l < 128; l++) {
        int start = starts[l];
        int end = (l + 1 < total && l + 1 < 128) ? starts[l + 1] : (int)(p - text);
        int n = end - start;
        if (n > cols) n = cols;
        while (n > 0 && (text[start + n - 1] == '\n' || text[start + n - 1] == '\r')) n--;
        char line[80];
        if (n > 79) n = 79;
        memcpy(line, text + start, (size_t)n);
        line[n] = 0;
        gfx_text(x, y + (l - first) * 8, line, color);
    }
}

static bool point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void bring_to_front(int idx) {
    int pos = -1;
    for (int i = 0; i < WIN_COUNT; i++) if (zorder[i] == idx) { pos = i; break; }
    if (pos < 0) return;
    for (int i = pos; i < WIN_COUNT - 1; i++) zorder[i] = zorder[i + 1];
    zorder[WIN_COUNT - 1] = idx;
    focus = idx;
}

static void open_window(int idx) {
    wins[idx].open = true;
    bring_to_front(idx);
}

/* speaker tone helper reserved for future tray audio controls */static int doom_icon_sin(int deg);
static int doom_icon_cos(int deg);

static void set_desktop_wallpaper(void) {
    /* dithered vertical gradient sky */
    for (int y = 0; y < TASKBAR_Y; y++) {
        u8 c = (y < 44) ? GUI_BLACK : (y < 90 ? GUI_DESK1 : GUI_DESK2);
        gfx_hline(0, y, GFX_WIDTH, c);
    }
    /* horizon glow band */
    for (int y = 84; y < 100; y++) {
        for (int x = 0; x < GFX_WIDTH; x++)
            if (((x + y) & 3) < 2) gfx_pixel(x, y, GUI_DESK3);
    }
    /* perspective grid floor */
    for (int y = 100; y < TASKBAR_Y; y++) {
        int step = 16 + (y - 100) / 2;
        for (int x = ((y * 7) % step); x < GFX_WIDTH; x += step)
            gfx_pixel(x, y, GUI_DESK3);
    }
    for (int x = 0; x < GFX_WIDTH; x += 40)
        for (int y = 100; y < TASKBAR_Y; y++) gfx_pixel(x, y, GUI_DESK3);
    /* sun / logo */
    for (int r = 0; r < 14; r++) {
        for (int a = 0; a < 360; a += 8) {
            int sx = 270 + (r * doom_icon_cos(a)) / 1024;
            int sy = 40 + (r * doom_icon_sin(a)) / 1024;
            if (sx >= 0 && sx < GFX_WIDTH && sy >= 0 && sy < TASKBAR_Y)
                gfx_pixel(sx, sy, r < 9 ? GUI_BLUE : 2);
        }
    }
}

/* tiny cached trig used by the wallpaper (kept local to avoid exposing it) */
static int doom_icon_sin(int deg) {
    deg %= 360; if (deg < 0) deg += 360;
    int q = deg % 90, sign = 1, x;
    if (deg < 90)       { x = q;      sign = 1; }
    else if (deg < 180) { x = 90 - q; sign = 1; }
    else if (deg < 270) { x = q;      sign = -1; }
    else                { x = 90 - q; sign = -1; }
    return sign * (4 * x * (180 - x) * 1024) / (40500 - x * (180 - x));
}
static int doom_icon_cos(int deg) { return doom_icon_sin(deg + 90); }

/* ------------------------------------------------------------------------- */
/* screenshot to BMP in RAMFS                                                */
/* ------------------------------------------------------------------------- */
static int save_screenshot(void) {
    u8 *fb = gfx_framebuffer();
    if (!fb) return -1;
    u32 pixel_off = 54 + 256 * 4;
    u32 size = pixel_off + GFX_WIDTH * GFX_HEIGHT;
    u8 *bmp = (u8 *)kmalloc(size);
    if (!bmp) return -1;
    memset(bmp, 0, size);
    /* BMP header */
    bmp[0] = 'B'; bmp[1] = 'M';
    *(u32 *)(bmp + 2) = size;
    *(u32 *)(bmp + 10) = pixel_off;
    *(u32 *)(bmp + 14) = 40; /* DIB */
    *(s32 *)(bmp + 18) = GFX_WIDTH;
    *(s32 *)(bmp + 22) = -GFX_HEIGHT; /* top-down */
    *(u16 *)(bmp + 26) = 1;
    *(u16 *)(bmp + 28) = 8;
    *(u32 *)(bmp + 30) = 0;
    *(u32 *)(bmp + 34) = GFX_WIDTH * GFX_HEIGHT;
    *(u32 *)(bmp + 46) = 256;
    /* palette: approximate from our theme for used indices */
    static const u8 pal20[20][3] = {
        {0,0,0},{10,16,26},{30,58,95},{79,163,209},{192,57,43},{142,68,173},
        {39,174,96},{212,160,23},{27,36,48},{36,81,138},{60,120,192},{221,226,231},
        {26,32,40},{138,146,156},{82,210,115},{255,255,255},{12,16,24},{16,24,39},{21,32,52},{255,255,255}
    };
    for (int i = 0; i < 256; i++) {
        u8 r,g,b;
        if (i < 20) { r = pal20[i][0]; g = pal20[i][1]; b = pal20[i][2]; }
        else { r = (u8)i; g = (u8)i; b = (u8)i; }
        bmp[54 + i * 4 + 0] = b;
        bmp[54 + i * 4 + 1] = g;
        bmp[54 + i * 4 + 2] = r;
        bmp[54 + i * 4 + 3] = 0;
    }
    memcpy(bmp + pixel_off, fb, GFX_WIDTH * GFX_HEIGHT);
    char path[64];
    ksnprintf(path, sizeof(path), "/tmp/shot%d.bmp", shot_counter++);
    if (ramfs_lookup(path)) ramfs_unlink(path);
    fs_node_t *f = ramfs_create(path, FS_FILE);
    int rc = -1;
    if (f) {
        if (ramfs_write(f, bmp, size, 0) == (int)size) rc = 0;
        else { ramfs_unlink(path); }
    }
    kfree(bmp);
    if (rc == 0) {
        char msg[96];
        ksnprintf(msg, sizeof(msg), "screenshot saved: %s\n", path);
        buf_append(term_buf, &term_len, sizeof(term_buf), msg);
    }
    return rc;
}

/* ------------------------------------------------------------------------- */
/* browser                                                                   */
/* ------------------------------------------------------------------------- */
static void html_to_text(const char *in, char *out, int max) {
    int o = 0; bool tag = false; bool ws = false;
    for (int i = 0; in[i] && o < max - 1; i++) {
        char c = in[i];
        if (c == '<') {
            tag = true;
            /* detect some block tags for newlines */
            if (strncmp(in + i, "<br", 3) == 0 || strncmp(in + i, "<p", 2) == 0 ||
                strncmp(in + i, "<li", 3) == 0 || strncmp(in + i, "<div", 4) == 0 ||
                strncmp(in + i, "<h", 2) == 0 || strncmp(in + i, "</", 2) == 0) {
                if (!ws) { out[o++] = '\n'; ws = true; }
            }
            continue;
        }
        if (tag) { if (c == '>') tag = false; continue; }
        if (c == '\r') continue;
        if (c == '\t') c = ' ';
        if (c == ' ' || c == '\n') {
            if (!ws) { out[o++] = (c == '\n') ? '\n' : ' '; ws = (c == '\n'); }
            continue;
        }
        if (c == '&') {
            if (strncmp(in + i, "&amp;", 5) == 0) { c = '&'; i += 4; }
            else if (strncmp(in + i, "&lt;", 4) == 0) { c = '<'; i += 3; }
            else if (strncmp(in + i, "&gt;", 4) == 0) { c = '>'; i += 3; }
            else if (strncmp(in + i, "&nbsp;", 6) == 0) { c = ' '; i += 5; }
            else if (strncmp(in + i, "&quot;", 6) == 0) { c = '"'; i += 5; }
        }
        out[o++] = c; ws = false;
    }
    out[o] = 0;
}

static void browser_load(const char *url) {
    strncpy(br_url, url, sizeof(br_url) - 1);
    br_url[sizeof(br_url) - 1] = 0;
    strncpy(br_input, br_url, sizeof(br_input) - 1);
    br_scroll = 0;
    br_content[0] = 0;
    if (strcmp(url, "about:home") == 0 || strcmp(url, "home") == 0) {
        ksnprintf(br_content, sizeof(br_content),
            "Minimal Web - Home\n\n"
            "Welcome to Minimal Web browser.\n"
            "Offline pages: about:home, about:sys\n\n"
            "Try: http://10.0.2.2:8000/\n"
            "     http://example.com/\n\n"
            "Links: [about:sys] [about:home]\n");
        return;
    }
    if (strcmp(url, "about:sys") == 0) {
        char ip[20]; net_format_ip(net_info()->ip, ip);
        ksnprintf(br_content, sizeof(br_content),
            "Minimal Web - System\n\n"
            "OS: Minimal Desktop 1.0.0 (x86)\n"
            "Network: %s\nIP: %s\nStore packages: %d\n"
            "Keyboard: %s\n\n[about:home]\n",
            net_is_up() ? "up" : "down", ip, store_package_count(), kbd_layout_name());
        return;
    }
    if (!net_is_up()) {
        ksnprintf(br_content, sizeof(br_content),
            "Network unavailable.\nCannot fetch: %s\n\nTry about:home or about:sys\n", url);
        return;
    }
    static char body[4096];
    static char text[4096];
    int len = 0;
    br_loading_msg = true;
    int rc = net_http_get(url, body, sizeof(body), &len);
    br_loading_msg = false;
    if (rc != 0 || len <= 0) {
        ksnprintf(br_content, sizeof(br_content), "Failed to load %s\nHTTP status: %d\n", url, net_http_status());
        return;
    }
    html_to_text(body, text, sizeof(text));
    strncpy(br_content, text, sizeof(br_content) - 1);
    br_content[sizeof(br_content) - 1] = 0;
}

/* ------------------------------------------------------------------------- */
/* explorer                                                                  */
/* ------------------------------------------------------------------------- */
static void explorer_refresh(void) {
    ex_count = 0;
    fs_node_t *dir = ramfs_lookup(ex_path);
    if (!dir || dir->type != FS_DIR) {
        strncpy(ex_path, "/", sizeof(ex_path) - 1);
        dir = ramfs_root();
    }
    /* add parent entry */
    if (strcmp(ex_path, "/") != 0 && ex_count < 32) {
        strcpy(ex_names[ex_count], "..");
        ex_types[ex_count] = FS_DIR;
        ex_sizes[ex_count] = 0;
        ex_count++;
    }
    for (int i = 0; ex_count < 32; i++) {
        fs_node_t *c = NULL;
        if (ramfs_readdir(dir, i, &c) != 0) break;
        strncpy(ex_names[ex_count], c->name, FS_NAME_MAX - 1);
        ex_names[ex_count][FS_NAME_MAX - 1] = 0;
        ex_types[ex_count] = c->type;
        ex_sizes[ex_count] = c->size;
        ex_count++;
    }
    if (ex_sel >= ex_count) ex_sel = ex_count - 1;
    if (ex_sel < 0) ex_sel = 0;
}

static void explorer_open_selected(void) {
    if (ex_sel < 0 || ex_sel >= ex_count) return;
    if (strcmp(ex_names[ex_sel], "..") == 0) {
        char *slash = strrchr(ex_path, '/');
        if (slash && slash != ex_path) *slash = 0; else strcpy(ex_path, "/");
        explorer_refresh();
        ex_sel = 0;
        return;
    }
    char full[256];
    if (strcmp(ex_path, "/") == 0) ksnprintf(full, sizeof(full), "/%s", ex_names[ex_sel]);
    else ksnprintf(full, sizeof(full), "%s/%s", ex_path, ex_names[ex_sel]);
    fs_node_t *n = ramfs_lookup(full);
    if (!n) return;
    if (n->type == FS_DIR) {
        strncpy(ex_path, full, sizeof(ex_path) - 1);
        ex_path[sizeof(ex_path) - 1] = 0;
        explorer_refresh();
        ex_sel = 0; ex_scroll = 0;
    } else {
        /* open text file in notepad */
        note_len = 0; note_buf[0] = 0;
        char chunk[129]; u32 off = 0;
        while (off < n->size && note_len < (int)sizeof(note_buf) - 1) {
            u32 want = n->size - off; if (want > 128) want = 128;
            int got = ramfs_read(n, chunk, want, off);
            if (got <= 0) break;
            for (int i = 0; i < got && note_len < (int)sizeof(note_buf) - 1; i++)
                note_buf[note_len++] = chunk[i];
            off += (u32)got;
        }
        note_buf[note_len] = 0;
        open_window(WIN_NOTE);
    }
}

/* ------------------------------------------------------------------------- */
/* chrome & drawing                                                          */
/* ------------------------------------------------------------------------- */
static void draw_desktop(void) {
    set_desktop_wallpaper();
}

static void draw_desktop_icons(void) {
    static const char *names[] = { "Term", "Note", "Tasks", "Net", "Web", "Files", "DOOM" };
    static const int icons[] = { ICON_TERM, ICON_NOTEPAD, ICON_TASKS, ICON_NET,
                                 ICON_BROWSER, ICON_EXPLORER, ICON_DOOM };
    static const u8 colors[] = { GUI_GREEN, GUI_YELLOW, GUI_CYAN, GUI_BLUE, GUI_MAGENTA, GUI_TITLE_LT, GUI_RED };
    for (int i = 0; i < 7; i++) {
        int col = i / 4, row = i % 4;
        int x = 6 + col * 44;
        int y = 8 + row * 40;
        gfx_fill_rect(x + 1, y + 1, 20, 20, GUI_DESK3);
        gfx_frame(x + 1, y + 1, 20, 20, GUI_GREY);
        gfx_icon(x + 3, y + 3, icons[i], colors[i]);
        gfx_text(x + 2, y + 22, names[i], GUI_WHITE);
    }
}

static void draw_window_frame(gui_win_t *w, bool focused) {
    gfx_fill_rect(w->x + 2, w->y + 3, w->w, w->h, GUI_BLACK);
    gfx_fill_rect(w->x, w->y, w->w, w->h, GUI_PANEL);
    gfx_rect(w->x, w->y, w->w, w->h, GUI_BLACK);
    gfx_fill_rect(w->x + 1, w->y + 1, w->w - 2, TITLE_H - 1,
                  focused ? GUI_TITLE : GUI_GREY);
    gfx_text(w->x + 4, w->y + 2, w->title, GUI_WHITE);
    int cx = w->x + w->w - 10, cy = w->y + 2;
    gfx_fill_rect(cx, cy, 7, 7, GUI_PANEL);
    gfx_frame(cx, cy, 7, 7, GUI_BLACK);
    gfx_text(cx + 1, cy, "x", GUI_RED);
}

static void draw_terminal(gui_win_t *w) {
    int x = w->x + 4, y = w->y + TITLE_H + 3;
    int cols = (w->w - 8) / 8;
    int rows = (w->h - TITLE_H - 16) / 8;
    gfx_fill_rect(w->x + 2, w->y + TITLE_H + 1, w->w - 4, w->h - TITLE_H - 3, GUI_BLACK);
    draw_text_tail(x, y, cols, rows, term_buf, GUI_GREEN);
    int iy = w->y + w->h - 11;
    gfx_text(x, iy, "minimal@os:~$", GUI_WHITE);
    int px = x + 14 * 8;
    /* keep the typed command inside the window: the prompt takes 14 cells,
     * the remaining cells depend on the window width */
    int room = (w->x + w->w - 6 - px) / 8;
    if (room < 0) room = 0;
    if (room > 32) room = 32;
    int n = term_cmd_len;
    if (n > room) n = room;
    char cmd[40];
    memcpy(cmd, term_cmd + (term_cmd_len - n), (size_t)n);
    cmd[n] = 0;
    gfx_text(px, iy, cmd, GUI_CYAN);
    if (((pit_ticks() / 25) & 1) == 0) gfx_fill_rect(px + n * 8, iy, 5, 8, GUI_WHITE);
}

static void draw_notepad(gui_win_t *w) {
    int x = w->x + 4, y = w->y + TITLE_H + 3;
    int cols = (w->w - 8) / 8, rows = (w->h - TITLE_H - 8) / 8;
    gfx_fill_rect(w->x + 2, w->y + TITLE_H + 1, w->w - 4, w->h - TITLE_H - 3, GUI_WHITE);
    draw_text_tail(x, y, cols, rows, note_buf, GUI_TEXT);
    if (((pit_ticks() / 25) & 1) == 0) {
        int len = note_len, line = 0, col = 0;
        for (int i = 0; i < len; i++) { if (note_buf[i] == '\n' || col >= cols) { line++; col = 0; } else col++; }
        if (line >= rows) line = rows - 1;
        gfx_fill_rect(x + col * 8, y + line * 8, 5, 8, GUI_TEXT);
    }
}

static void draw_tasks(gui_win_t *w) {
    int x = w->x + 4, y = w->y + TITLE_H + 3;
    gfx_fill_rect(w->x + 2, w->y + TITLE_H + 1, w->w - 4, w->h - TITLE_H - 3, GUI_WHITE);
    gfx_text(x, y, "PID  NAME          MEM", GUI_TEXT);
    gfx_hline(w->x + 3, y + 9, w->w - 6, GUI_GREY);
    int row = 0;
    for (int i = 0; i < TASK_MAX && row < 8; i++) {
        task_info_t *t = task_get(i); if (!t) continue;
        char line[48]; ksnprintf(line, sizeof(line), "%-4d %-12s %3uK", t->pid, t->name, t->mem_kb);
        gfx_text(x, y + 13 + row * 9, line, t->killable ? GUI_TEXT : GUI_TITLE);
        row++;
    }
    char foot[48]; ksnprintf(foot, sizeof(foot), "%d tasks  %u ticks", task_count(), pit_ticks());
    gfx_text(x, w->y + w->h - 11, foot, GUI_GREY);
}

static void draw_network(gui_win_t *w) {
    int x = w->x + 4, y = w->y + TITLE_H + 3;
    gfx_fill_rect(w->x + 2, w->y + TITLE_H + 1, w->w - 4, w->h - TITLE_H - 3, GUI_WHITE);
    net_info_t *ni = net_info();
    net_poll();
    char line[72], ip[20], mask[20], gw[20], dns[20], mac[32];
    net_format_ip(ni->ip, ip); net_format_ip(ni->netmask, mask);
    net_format_ip(ni->gateway, gw); net_format_ip(ni->dns, dns);
    ksnprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
              ni->mac[0],ni->mac[1],ni->mac[2],ni->mac[3],ni->mac[4],ni->mac[5]);
    gfx_text(x, y, "Network Connector", GUI_TITLE);
    gfx_hline(w->x + 3, y + 9, w->w - 6, GUI_GREY);
    /* status badge */
    u8 stc = net_is_up() ? GUI_GREEN : GUI_RED;
    gfx_fill_rect(x, y + 12, 8, 8, stc);
    if (net_is_up()) ksnprintf(line, sizeof(line), "Connected  %s", ip);
    else ksnprintf(line, sizeof(line), "Disconnected / no lease");
    gfx_text(x + 12, y + 12, line, GUI_TEXT);
    ksnprintf(line, sizeof(line), "Mask %s  GW %s", mask, gw);
    gfx_text(x, y + 22, line, GUI_TEXT);
    ksnprintf(line, sizeof(line), "DNS  %s  %s", dns, ni->dhcp ? "DHCP" : "static");
    gfx_text(x, y + 31, line, GUI_TEXT);
    ksnprintf(line, sizeof(line), "MAC  %s", mac);
    gfx_text(x, y + 40, line, GUI_TEXT);
    ksnprintf(line, sizeof(line), "NIC  RTL8139  IRQ %u  IO 0x%X", ni->irq, ni->io_base);
    gfx_text(x, y + 49, line, GUI_GREY);
    ksnprintf(line, sizeof(line), "RX %u pkts/%u B  TX %u pkts/%u B", ni->rx_packets, ni->rx_bytes, ni->tx_packets, ni->tx_bytes);
    gfx_text(x, y + 58, line, GUI_GREY);
    /* action buttons */
    int by = y + 70;
    gfx_fill_rect(x, by, 44, 10, GUI_BLUE); gfx_text(x + 3, by + 1, "[P] Ping", GUI_WHITE);
    gfx_fill_rect(x + 48, by, 44, 10, GUI_TITLE); gfx_text(x + 50, by + 1, "[R] DHCP", GUI_WHITE);
    gfx_fill_rect(x + 96, by, 44, 10, GUI_DESK2); gfx_text(x + 98, by + 1, "[I] Info", GUI_WHITE);
    gfx_fill_rect(x, by + 12, 44, 10, GUI_BLUE); gfx_text(x + 3, by + 13, "[D] DNS", GUI_WHITE);
    gfx_text(x, by + 26, "Tip: use Browser for HTTP", GUI_GREY);
}

static void draw_browser(gui_win_t *w) {
    int cx = w->x + 2, cy = w->y + TITLE_H + 1, cw = w->w - 4, ch = w->h - TITLE_H - 3;
    gfx_fill_rect(cx, cy, cw, ch, GUI_WHITE);
    /* toolbar */
    gfx_fill_rect(cx, cy, cw, 12, GUI_PANEL);
    gfx_fill_rect(cx + 2, cy + 2, 14, 8, GUI_BLUE); gfx_text(cx + 4, cy + 2, "<", GUI_WHITE);
    gfx_fill_rect(cx + 18, cy + 2, 14, 8, GUI_BLUE); gfx_text(cx + 20, cy + 2, "R", GUI_WHITE);
    gfx_fill_rect(cx + 34, cy + 2, cw - 38, 8, GUI_WHITE);
    gfx_frame(cx + 34, cy + 2, cw - 38, 8, GUI_GREY);
    char udisp[40]; int ul = (cw - 46) / 8; if (ul > 39) ul = 39;
    strncpy(udisp, br_input, (size_t)ul); udisp[ul] = 0;
    gfx_text(cx + 36, cy + 2, udisp, GUI_TEXT);
    /* content */
    int tx = cx + 3, ty = cy + 14;
    int cols = (cw - 6) / 8, rows = (ch - 16) / 8;
    /* draw scrolled content lines */
    int line = 0, col = 0, drawn = 0, skip = br_scroll;
    const char *p = br_content;
    char lbuf[60];
    int li = 0;
    while (*p && drawn < rows) {
        if (*p == '\n' || col >= cols) {
            lbuf[li] = 0;
            if (skip > 0) skip--; else { gfx_text(tx, ty + drawn * 8, lbuf, GUI_TEXT); drawn++; }
            li = 0; col = 0; line++;
            if (*p == '\n') p++; else { /* wrap without consuming */ }
            continue;
        }
        if (li < cols && li < 59) { lbuf[li++] = *p; }
        col++; p++;
    }
    if (li > 0 && drawn < rows) { lbuf[li] = 0; if (skip <= 0) gfx_text(tx, ty + drawn * 8, lbuf, GUI_TEXT); }
    if (br_content[0] == 0) gfx_text(tx, ty, "(empty)", GUI_GREY);
}

static void draw_explorer(gui_win_t *w) {
    int cx = w->x + 2, cy = w->y + TITLE_H + 1, cw = w->w - 4, ch = w->h - TITLE_H - 3;
    gfx_fill_rect(cx, cy, cw, ch, GUI_WHITE);
    gfx_fill_rect(cx, cy, cw, 10, GUI_PANEL);
    char ph[40]; int pl = (cw - 8) / 8; if (pl > 39) pl = 39; strncpy(ph, ex_path, (size_t)pl); ph[pl] = 0;
    gfx_text(cx + 3, cy + 1, ph, GUI_TITLE);
    int tx = cx + 3, ty = cy + 12;
    int rows = (ch - 14) / 8;
    for (int i = 0; i < rows && ex_scroll + i < ex_count; i++) {
        int idx = ex_scroll + i;
        bool sel = (idx == ex_sel);
        if (sel) gfx_fill_rect(tx - 1, ty + i * 8, cw - 6, 8, GUI_CYAN);
        char line[48];
        if (ex_types[idx] == FS_DIR)
            ksnprintf(line, sizeof(line), "<DIR>  %s", ex_names[idx]);
        else
            ksnprintf(line, sizeof(line), "       %-12s %u", ex_names[idx], ex_sizes[idx]);
        gfx_text(tx, ty + i * 8, line, sel ? GUI_WHITE : GUI_TEXT);
    }
    if (ex_count == 0) gfx_text(tx, ty, "(empty)", GUI_GREY);
}

static void iv_refresh(void) {
    iv_count = 0;
    fs_node_t *dir = ramfs_lookup("/pictures");
    if (!dir || dir->type != FS_DIR) return;
    fs_node_t *child;
    for (int i = 0; ramfs_readdir(dir, i, &child) == 0 && iv_count < 32; i++) {
        if (child->type != FS_FILE) continue;
        const char *name = child->name;
        int len = (int)strlen(name);
        if (len >= 4 && strcasecmp(name + len - 4, ".bmp") == 0) {
            ksnprintf(iv_files[iv_count++], sizeof(iv_files[0]), "/pictures/%s", name);
        }
    }
}

static void iv_load(void) {
    memset(iv_frame, 0, sizeof(iv_frame));
    if (iv_count == 0) return;
    if (iv_index < 0) iv_index = 0;
    if (iv_index >= iv_count) iv_index = iv_count - 1;
    fs_node_t *f = ramfs_lookup(iv_files[iv_index]);
    if (!f) return;
    u8 *data = (u8 *)kmalloc(f->size);
    if (!data) return;
    if (ramfs_read(f, data, f->size, 0) == (int)f->size) {
        bmp_load(data, f->size, iv_frame, 212, 144);
    }
    kfree(data);
}

static void draw_imgview(gui_win_t *w) {
    int x = w->x + 4, y = w->y + TITLE_H + 3;
    gfx_fill_rect(w->x + 2, w->y + TITLE_H + 1, w->w - 4, w->h - TITLE_H - 3, GUI_BLACK);
    for (int row = 0; row < 144; row++) {
        for (int col = 0; col < 212; col++) {
            u8 c = iv_frame[row * 212 + col];
            if (c) gfx_pixel(x + col, y + row, c);
        }
    }
    char label[128];
    ksnprintf(label, sizeof(label), "%d/%d  %s", iv_index + 1, iv_count,
              iv_count ? iv_files[iv_index] : "(no images)");
    gfx_text(x, w->y + w->h - 10, label, GUI_WHITE);
    gfx_text(x, w->y + w->h - 18, "< > / [ ]  navigate    Esc  close", GUI_GREY);
}

static void imgview_key(int key) {
    if (key == KEY_LEFT || key == '[') {
        if (iv_index > 0) { iv_index--; iv_load(); }
    } else if (key == KEY_RIGHT || key == ']') {
        if (iv_index + 1 < iv_count) { iv_index++; iv_load(); }
    } else if (key == 'r' || key == 'R') {
        iv_refresh(); iv_load();
    }
}

static void draw_windows(void) {
    for (int i = 0; i < WIN_COUNT; i++) {
        gui_win_t *w = &wins[zorder[i]];
        if (!w->open) continue;
        draw_window_frame(w, zorder[i] == focus);
        switch (w->kind) {
            case WIN_TERM:     draw_terminal(w); break;
            case WIN_NOTE:     draw_notepad(w);  break;
            case WIN_TASKS:    draw_tasks(w);    break;
            case WIN_NET:      draw_network(w);  break;
            case WIN_BROWSER:  draw_browser(w);  break;
            case WIN_EXPLORER: draw_explorer(w); break;
            case WIN_IMGVIEW:  draw_imgview(w);  break;
            default: break;
        }
    }
}

static void draw_taskbar(void) {
    gfx_fill_rect(0, TASKBAR_Y, GFX_WIDTH, TASKBAR_H, GUI_TASKBAR);
    gfx_hline(0, TASKBAR_Y, GFX_WIDTH, GUI_TITLE_LT);
    gfx_fill_rect(2, TASKBAR_Y + 3, 30, 16, start_open ? GUI_TITLE_LT : GUI_BLUE);
    gfx_frame(2, TASKBAR_Y + 3, 30, 16, GUI_WHITE);
    gfx_icon(9, TASKBAR_Y + 3, ICON_START,
             start_open ? GUI_WHITE : GUI_CYAN);

    int bx = 34;
    for (int i = 0; i < WIN_COUNT; i++) {
        if (!wins[i].open) continue;
        char label[6]; int n = (int)strlen(wins[i].title); if (n > 4) n = 4;
        memcpy(label, wins[i].title, (size_t)n); label[n] = 0;
        gfx_fill_rect(bx, TASKBAR_Y + 3, 28, 16, focus == i ? GUI_TITLE : GUI_DESK3);
        gfx_text(bx + 2, TASKBAR_Y + 7, label, GUI_WHITE);
        bx += 29;
        if (bx > 220) break;
    }

    /* tray: network, RAM bar, audio, layout, clock */
    int tx = 186;
    /* network icon */
    gfx_fill_rect(tx, TASKBAR_Y + 5, 8, 8, net_is_up() ? GUI_GREEN : GUI_RED);
    gfx_text(tx + 1, TASKBAR_Y + 5, "N", GUI_WHITE);
    tx += 10;
    /* RAM bar */
    u32 tr, uf, ff, hk; mm_get_stats(&tr, &uf, &ff, &hk);
    u32 totalf = pmm_total_frames(); u32 usedf = pmm_used_frames();
    int pct = totalf ? (int)(usedf * 100 / totalf) : 0;
    gfx_frame(tx, TASKBAR_Y + 7, 20, 6, GUI_GREY);
    gfx_fill_rect(tx + 1, TASKBAR_Y + 8, (18 * pct) / 100, 4, pct > 80 ? GUI_RED : GUI_CYAN);
    tx += 22;
    /* audio */
    gfx_fill_rect(tx, TASKBAR_Y + 5, 10, 8, speaker_muted ? GUI_RED : GUI_GREEN);
    gfx_text(tx + 1, TASKBAR_Y + 5, speaker_muted ? "M" : "S", GUI_WHITE);
    tx += 12;
    /* keymap */
    const char *ln = kbd_layout_name();
    char ls[5]; strncpy(ls, ln, 4); ls[4] = 0;
    gfx_fill_rect(tx, TASKBAR_Y + 5, 22, 8, GUI_BLUE);
    gfx_text(tx + 1, TASKBAR_Y + 5, ls, GUI_WHITE);
    tx += 24;
    /* clock, right aligned inside the taskbar */
    rtc_time_t t; rtc_read(&t);
    char clock[16]; ksnprintf(clock, sizeof(clock), "%02d:%02d", t.hour, t.minute);
    gfx_text(GFX_WIDTH - (int)strlen(clock) * 8 - 3, TASKBAR_Y + 7, clock, GUI_WHITE);
}

static const int start_icons[12] = {
    ICON_TERM, ICON_NOTEPAD, ICON_TASKS, ICON_NET, ICON_BROWSER,
    ICON_EXPLORER, ICON_DOOM, ICON_SNAKE, ICON_PONG, ICON_DISK, ICON_START, ICON_FILE
};
static const char *start_labels[12] = {
    "Terminal", "Notepad", "Tasks", "Network", "Browser",
    "Explorer", "DOOM", "Snake", "Pong", "Screenshot", "Exit GUI", "Image Viewer"
};

static void draw_start_menu(void) {
    if (!start_open) return;
    int x = 2, y = TASKBAR_Y - 130, w = 150, h = 126;
    gfx_fill_rect(x + 3, y + 3, w, h, GUI_BLACK);
    gfx_fill_rect(x, y, w, h, GUI_PANEL);
    gfx_frame(x, y, w, h, GUI_CYAN);
    gfx_fill_rect(x + 1, y + 1, w - 2, 11, GUI_TITLE);
    gfx_text(x + 4, y + 2, "Minimal Desktop", GUI_WHITE);
    gfx_text(x + w - 24, y + 2, "SUPER", GUI_YELLOW);
    gfx_hline(x + 2, y + 13, w - 4, GUI_GREY);
    static const u8 cols[12] = {
        GUI_GREEN, GUI_YELLOW, GUI_CYAN, GUI_BLUE, GUI_MAGENTA,
        GUI_TITLE_LT, GUI_RED, GUI_GREEN, GUI_YELLOW, GUI_GREY, GUI_RED, GUI_CYAN
    };
    for (int i = 0; i < 12; i++) {
        int cellx = x + 3 + (i % 2) * 74;
        int celly = y + 16 + (i / 2) * 18;
        gfx_fill_rect(cellx, celly, 72, 17, (i & 1) ? GUI_DESK2 : GUI_DESK1);
        gfx_icon(cellx + 1, celly + 1, start_icons[i], cols[i]);
        gfx_text(cellx + 19, celly + 4, start_labels[i], GUI_WHITE);
    }
    gfx_text(x + 4, y + h - 10, "Ctrl+M max  Ctrl+Q close", GUI_GREY);
}

static void draw_cursor(int mx, int my) {    for (int i = 0; i < 10; i++) {
        int w = i < 7 ? i + 1 : 10 - i;
        gfx_hline(mx, my + i, w, GUI_CURSOR);
        gfx_hline(mx + 1, my + i, w, GUI_BLACK);
    }
    gfx_vline(mx, my, 10, GUI_CURSOR);
}

/* ------------------------------------------------------------------------- */
/* input/interaction                                                         */
/* ------------------------------------------------------------------------- */
static void alt_tab(void) {
    /* cycle to next open window */
    int start = focus;
    for (int step = 1; step <= WIN_COUNT; step++) {
        int idx = (start + step) % WIN_COUNT;
        if (wins[idx].open) { bring_to_front(idx); return; }
    }
}

static void snap_window(gui_win_t *w, int side) {
    /* side: -1 left, 1 right, 0 maximize */
    if (side == -1) { w->x = 0; w->y = 0; w->w = GFX_WIDTH / 2; w->h = TASKBAR_Y; }
    else if (side == 1) { w->x = GFX_WIDTH / 2; w->y = 0; w->w = GFX_WIDTH / 2; w->h = TASKBAR_Y; }
    else { w->x = 0; w->y = 0; w->w = GFX_WIDTH; w->h = TASKBAR_Y; }
}

static void menu_click(int mx, int my) {
    int x = 2, y = TASKBAR_Y - 130;
    if (mx < x || mx >= x + 150 || my < y || my >= y + 126) { start_open = false; return; }
    int col = (mx - x - 3) / 74;
    int row = (my - y - 16) / 18;
    int item = row * 2 + col;
    if (item < 0 || item > 10) { start_open = false; return; }
    switch (item) {
        case 0: open_window(WIN_TERM); break;
        case 1: open_window(WIN_NOTE); break;
        case 2: open_window(WIN_TASKS); break;
        case 3: open_window(WIN_NET); break;
        case 4: open_window(WIN_BROWSER); if (br_url[0] == 0) browser_load("about:home"); break;
        case 5: open_window(WIN_EXPLORER); explorer_refresh(); break;
        case 6: launch_req = GUI_LAUNCH_DOOM; running = false; break;
        case 7: launch_req = GUI_LAUNCH_SNAKE; running = false; break;
        case 8: launch_req = GUI_LAUNCH_PONG; running = false; break;
        case 9: save_screenshot(); break;
        case 10: running = false; break;
        case 11: iv_refresh(); iv_index = 0; iv_load(); open_window(WIN_IMGVIEW); break;
        default: break;
    }
    start_open = false;
}

static void handle_click(int mx, int my) {
    if (start_open) {
        menu_click(mx, my);
        return;
    }

    if (point_in(mx, my, 0, TASKBAR_Y, GFX_WIDTH, TASKBAR_H)) {
        if (point_in(mx, my, 2, TASKBAR_Y + 3, 30, 16)) { start_open = !start_open; return; }
        int bx = 34;
        for (int i = 0; i < WIN_COUNT; i++) {
            if (!wins[i].open) continue;
            if (point_in(mx, my, bx, TASKBAR_Y + 3, 28, 16)) { bring_to_front(i); return; }
            bx += 29; if (bx > 220) break;
        }
        /* tray clicks */
        int tx = 224;
        if (point_in(mx, my, tx, TASKBAR_Y + 4, 8, 10)) { open_window(WIN_NET); return; } /* net */
        tx += 10 + 22; /* ram bar not clickable */
        if (point_in(mx, my, tx, TASKBAR_Y + 4, 10, 10)) { speaker_muted = !speaker_muted; return; }
        tx += 12;
        if (point_in(mx, my, tx, TASKBAR_Y + 4, 22, 10)) {
            kbd_cycle_layout(1);
            return;
        }
        return;
    }

    for (int i = WIN_COUNT - 1; i >= 0; i--) {
        int idx = zorder[i];
        gui_win_t *w = &wins[idx];
        if (!w->open) continue;
        if (!point_in(mx, my, w->x, w->y, w->w, w->h)) continue;
        bring_to_front(idx);
        if (point_in(mx, my, w->x + w->w - 10, w->y + 2, 7, 7)) { w->open = false; return; }

        /* network connector buttons */
        if (idx == WIN_NET) {
            int nx = w->x + 4, ny = w->y + TITLE_H + 3;
            int by = ny + 70;
            if (point_in(mx, my, nx, by, 44, 10)) { network_key('p'); return; }
            if (point_in(mx, my, nx + 48, by, 44, 10)) { network_key('r'); return; }
            if (point_in(mx, my, nx + 96, by, 44, 10)) { network_key('i'); return; }
            if (point_in(mx, my, nx, by + 12, 44, 10)) { network_key('d'); return; }
        }

        /* browser toolbar interactions */
        if (idx == WIN_BROWSER) {
            int cy = w->y + TITLE_H + 1;
            if (point_in(mx, my, w->x + 4, cy + 2, 14, 8)) { browser_load("about:home"); return; }
            if (point_in(mx, my, w->x + 20, cy + 2, 14, 8)) { browser_load(br_url[0]?br_url:"about:home"); return; }
            /* clickable "links" in content: detect [about:...] tokens by click row approx */
            int content_y = cy + 14;
            int row = (my - content_y) / 8;
            if (row >= 0) {
                /* simple: if click in lower content and text contains about:sys/home, navigate */
                if (strstr(br_content, "about:sys") && strstr(br_content, "about:home")) {
                    /* heuristic: right side clicks -> sys */
                    if (mx > w->x + w->w / 2) browser_load("about:sys");
                    else browser_load("about:home");
                }
            }
        }
        /* explorer double-click open */
        if (idx == WIN_EXPLORER) {
            int cy = w->y + TITLE_H + 1;
            int ty = cy + 12;
            int row = (my - ty) / 8 + ex_scroll;
            if (row >= 0 && row < ex_count) { ex_sel = row; explorer_open_selected(); return; }
        }

        if (my < w->y + TITLE_H) { drag_win = idx; drag_dx = mx - w->x; drag_dy = my - w->y; }
        return;
    }

    /* desktop icons */
    for (int i = 0; i < 7; i++) {
        int col = i / 4, row = i % 4;
        int ix = 6 + col * 44, iy = 8 + row * 40;
        if (point_in(mx, my, ix, iy, 30, 32)) {
            switch (i) {
                case 0: open_window(WIN_TERM); break;
                case 1: open_window(WIN_NOTE); break;
                case 2: open_window(WIN_TASKS); break;
                case 3: open_window(WIN_NET); break;
                case 4: open_window(WIN_BROWSER); if (br_url[0]==0) browser_load("about:home"); break;
                case 5: open_window(WIN_EXPLORER); explorer_refresh(); break;
                case 6: launch_req = GUI_LAUNCH_DOOM; running = false; break;
            }
            return;
        }
    }
}

static void terminal_key(int key) {
    if (key == KEY_ENTER) {
        if (term_cmd_len == 0) return;
        term_cmd[term_cmd_len] = 0;
        buf_append(term_buf, &term_len, sizeof(term_buf), "\nminimal@os:~$ ");
        buf_append(term_buf, &term_len, sizeof(term_buf), term_cmd);
        buf_append(term_buf, &term_len, sizeof(term_buf), "\n");
        const char *blocked[] = { "startgui", "snake", "pong", "doom", "reboot", "shutdown", "clear" };
        bool deny = false;
        for (int i = 0; i < 7; i++) if (strcasecmp(term_cmd, blocked[i]) == 0) deny = true;
        if (deny) {
            buf_append(term_buf, &term_len, sizeof(term_buf), "gui: run this from text console\n");
        } else {
            static char cap[2048];
            console_capture_begin(cap, sizeof(cap));
            int rc = shell_execute(term_cmd);
            int n = console_capture_end();
            if (n > 0) buf_append(term_buf, &term_len, sizeof(term_buf), cap);
            if (rc != 0) { char msg[32]; ksnprintf(msg, sizeof(msg), "(exit %d)\n", rc); buf_append(term_buf,&term_len,sizeof(term_buf),msg); }
        }
        term_cmd_len = 0; term_cmd[0] = 0;
        if (term_len > 1000) { int cut = term_len / 2; memmove(term_buf, term_buf + cut, (size_t)(term_len - cut + 1)); term_len -= cut; }
    } else if (key == KEY_BACKSPACE) {
        if (term_cmd_len > 0) term_cmd[--term_cmd_len] = 0;
    } else if (key >= 0x20 && key < 0x7F) {
        buf_append_char(term_cmd, &term_cmd_len, sizeof(term_cmd), (char)key);
    }
}

static void notepad_key(int key) {
    if (key == KEY_ENTER) buf_append_char(note_buf, &note_len, sizeof(note_buf), '\n');
    else if (key == KEY_BACKSPACE) { if (note_len > 0) note_buf[--note_len] = 0; }
    else if (key == KEY_TAB) { for (int i = 0; i < 4; i++) buf_append_char(note_buf, &note_len, sizeof(note_buf), ' '); }
    else if (key >= 0x20 && key < 0x7F) buf_append_char(note_buf, &note_len, sizeof(note_buf), (char)key);
}

static void network_key(int key) {
    net_info_t *ni = net_info();
    if (key == '1' || key == 'p' || key == 'P') {
        char ip[20]; net_format_ip(ni->gateway ? ni->gateway : NET_IP(10,0,2,2), ip);
        buf_append(term_buf, &term_len, sizeof(term_buf), "[net] ping gateway...\n");
        net_ping(net_parse_ip(ip), 1);
    } else if (key == '2' || key == 'i' || key == 'I') {
        static char cap[1024];
        console_capture_begin(cap, sizeof(cap));
        ifconfig_print();
        console_capture_end();
        buf_append(term_buf, &term_len, sizeof(term_buf), cap);
    } else if (key == '3' || key == 'r' || key == 'R') {
        /* refresh/retry DHCP by reinitializing network stack */
        buf_append(term_buf, &term_len, sizeof(term_buf), "[net] reconnecting (DHCP)...\n");
        net_init();
    } else if (key == '4' || key == 'd' || key == 'D') {
        char host[64] = "example.com";
        u32 ip = net_resolve(host);
        char ib[20]; net_format_ip(ip, ib);
        char msg[80]; ksnprintf(msg, sizeof(msg), "[net] DNS %s -> %s\n", host, ip ? ib : "(failed)");
        buf_append(term_buf, &term_len, sizeof(term_buf), msg);
    }
}

static void browser_key(int key) {
    if (key == KEY_ENTER) { browser_load(br_input); }
    else if (key == KEY_BACKSPACE) { int l = (int)strlen(br_input); if (l > 0) br_input[l - 1] = 0; }
    else if (key == KEY_UP) { if (br_scroll > 0) br_scroll--; }
    else if (key == KEY_DOWN) { br_scroll++; }
    else if (key == KEY_PGUP) { br_scroll -= 5; if (br_scroll < 0) br_scroll = 0; }
    else if (key == KEY_PGDN) { br_scroll += 5; }
    else if (key >= 0x20 && key < 0x7F) {
        int l = (int)strlen(br_input);
        if (l < (int)sizeof(br_input) - 1) { br_input[l] = (char)key; br_input[l + 1] = 0; }
    }
}

static void explorer_key(int key) {
    int vis = 12;
    if (key == KEY_UP) { if (ex_sel > 0) ex_sel--; if (ex_sel < ex_scroll) ex_scroll = ex_sel; }
    else if (key == KEY_DOWN) { if (ex_sel < ex_count - 1) ex_sel++; if (ex_sel >= ex_scroll + vis) ex_scroll = ex_sel - vis + 1; }
    else if (key == KEY_ENTER) { explorer_open_selected(); }
    else if (key == KEY_BACKSPACE) { ex_sel = 0; explorer_open_selected(); /* .. if present */ }
}

/* ------------------------------------------------------------------------- */
/* public                                                                    */
/* ------------------------------------------------------------------------- */
int gui_run(void) {
    gfx_enter();
    mouse_set_bounds(GFX_WIDTH, GFX_HEIGHT);
    prev_left = false; drag_win = -1; start_open = false; running = true;
    focus = WIN_TERM; launch_req = GUI_LAUNCH_NONE;
    if (term_len == 0) {
        buf_append(term_buf, &term_len, sizeof(term_buf),
                   "Minimal Desktop GUI terminal\nType help, sysinfo, keymap, minipacket -I doom\n");
    }
    term_cmd_len = 0;
    if (br_url[0] == 0) { strcpy(br_input, "about:home"); }
    if (ex_path[0] == 0) { strcpy(ex_path, "/home/minimal"); explorer_refresh(); }
    iv_refresh(); iv_index = 0; iv_load();
    open_window(WIN_TERM);

    int pid = task_register("desktop", 320, true);
    task_set_state(pid, "running");

    while (running) {
        mouse_state_t m; mouse_get_state(&m);
        bool left = (m.buttons & 1) != 0;
        if (!prev_left && left) handle_click(m.x, m.y);

        if (drag_win >= 0) {
            if (left) {
                gui_win_t *w = &wins[drag_win];
                w->x = m.x - drag_dx; w->y = m.y - drag_dy;
                if (w->x < -w->w + 24) w->x = -w->w + 24;
                if (w->x > GFX_WIDTH - 24) w->x = GFX_WIDTH - 24;
                if (w->y < 0) w->y = 0;
                if (w->y > TASKBAR_Y - 12) w->y = TASKBAR_Y - 12;
            } else {
                /* snap on release near edges */
                gui_win_t *w = &wins[drag_win];
                if (w->x <= 2) snap_window(w, -1);
                else if (w->x + w->w >= GFX_WIDTH - 2) snap_window(w, 1);
                else if (w->y <= 1) snap_window(w, 0);
                drag_win = -1;
            }
        }
        prev_left = left;

        int key = kbd_getchar();
        while (key >= 0) {
            if (key == KEY_ESC) { running = false; break; }
            if (key == KEY_SUPER) {
                /* debounce: ignore typematic repeats so mashing Super cannot
                 * flood the event loop */
                if ((u32)(pit_ticks() - super_last) > 12) {
                    super_last = pit_ticks();
                    start_open = !start_open;
                }
                key = kbd_getchar(); continue;
            }
            if (key == KEY_TAB && kbd_alt_down()) { alt_tab(); key = kbd_getchar(); continue; }
            if (key == 'q' && kbd_ctrl_down() && focus >= 0 && focus < WIN_COUNT) {
                wins[focus].open = false; if (maximized_win == focus) maximized_win = -1;
                key = kbd_getchar(); continue;
            }
            if (key == 'm' && kbd_ctrl_down() && focus >= 0 && focus < WIN_COUNT) {
                gui_win_t *w = &wins[focus];
                if (maximized_win == focus) {
                    w->x = 10; w->y = 14; w->w = 214; w->h = 124;
                    maximized_win = -1;
                } else {
                    w->x = 0; w->y = 0; w->w = GFX_WIDTH; w->h = TASKBAR_Y;
                    maximized_win = focus;
                }
                key = kbd_getchar(); continue;
            }
            if (kbd_ctrl_down() && (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN)) {
                gui_win_t *w = &wins[focus];
                int step = 8;
                if (key == KEY_LEFT) w->x -= step;
                if (key == KEY_RIGHT) w->x += step;
                if (key == KEY_UP) w->y -= step;
                if (key == KEY_DOWN) w->y += step;
                if (w->x < 0) w->x = 0;
                if (w->y < 0) w->y = 0;
                key = kbd_getchar(); continue;
            }
            if (key == KEY_PRTSC && kbd_ctrl_down()) { save_screenshot(); key = kbd_getchar(); continue; }
            if (key == KEY_PRTSC) { save_screenshot(); key = kbd_getchar(); continue; }
            /* volume keys: F1/F2 as mute/volume toggle */
            if (key == KEY_F1) { speaker_muted = !speaker_muted; key = kbd_getchar(); continue; }
            if (key == KEY_F2) { speaker_volume = (speaker_volume + 1) % 4; if (speaker_volume==0) speaker_muted=true; else speaker_muted=false; key = kbd_getchar(); continue; }

            if (focus == WIN_TERM && wins[WIN_TERM].open) terminal_key(key);
            else if (focus == WIN_NOTE && wins[WIN_NOTE].open) notepad_key(key);
            else if (focus == WIN_NET && wins[WIN_NET].open) network_key(key);
            else if (focus == WIN_BROWSER && wins[WIN_BROWSER].open) browser_key(key);
            else if (focus == WIN_EXPLORER && wins[WIN_EXPLORER].open) explorer_key(key);
            else if (focus == WIN_IMGVIEW && wins[WIN_IMGVIEW].open) imgview_key(key);
            key = kbd_getchar();
        }

        net_poll();
        draw_desktop();
        draw_desktop_icons();
        draw_windows();
        draw_taskbar();
        draw_start_menu();
        draw_cursor(m.x, m.y);
        gfx_present();
        pit_sleep_ms(20);
    }

    task_unregister(pid);
    gfx_leave();
    if (launch_req == GUI_LAUNCH_NONE)
        kprintf("\n[gui] desktop closed, returning to console\n");
    return launch_req;
}

bool gui_selftest(char *err, int errlen) {
    /* Test the software renderer against its backbuffer only. Switching into
     * mode 13h and back is not part of what this test verifies, and some
     * emulated VGAs (VirtualBox's) cannot restore the text font afterwards. */
    gfx_fill_rect(0, 0, GFX_WIDTH, GFX_HEIGHT, GUI_BLACK);
    gfx_pixel(5, 5, GUI_CYAN);
    bool ok = gfx_getpixel(5, 5) == GUI_CYAN;
    gfx_fill_rect(20, 20, 40, 20, GUI_GREEN);
    if (gfx_getpixel(30, 30) != GUI_GREEN) ok = false;
    /* put the backbuffer back to black so the next GUI entry starts clean */
    gfx_fill_rect(0, 0, GFX_WIDTH, GFX_HEIGHT, GUI_BLACK);
    if (!ok) { ksnprintf(err, errlen, "framebuffer writes not visible"); return false; }
    return true;
}
/* ------------------------------------------------------------------------- */
/* Miniland - Hyprland-inspired tiling compositor (store module)              */
/* ------------------------------------------------------------------------- */

#define ML_BAR_H 12
#define ML_GAP   3
#define ML_WS    4

typedef struct {
    bool open;
    int  kind;
    bool floating;
} ml_win_t;

static void ml_layout(ml_win_t *mw, gui_win_t *tw, int area_x, int area_y, int area_w, int area_h) {
    int tiled_idx[WIN_COUNT]; int n = 0;
    for (int i = 0; i < WIN_COUNT; i++) if (mw[i].open && !mw[i].floating) tiled_idx[n++] = i;
    for (int i = 0; i < WIN_COUNT; i++) if (mw[i].open && mw[i].floating) {
        if (tw[i].w < 90) tw[i].w = 140;
        if (tw[i].h < 70) tw[i].h = 90;
        if (tw[i].x < area_x) tw[i].x = area_x + 4;
        if (tw[i].y < area_y) tw[i].y = area_y + 4;
        if (tw[i].x + tw[i].w > area_x + area_w) tw[i].x = area_x + area_w - tw[i].w - 2;
        if (tw[i].y + tw[i].h > area_y + area_h) tw[i].y = area_y + area_h - tw[i].h - 2;
    }
    if (n == 0) return;
    int gx = ML_GAP, gy = ML_GAP;
    if (n == 1) {
        int i = tiled_idx[0];
        tw[i].x = area_x + gx; tw[i].y = area_y + gy;
        tw[i].w = area_w - gx*2; tw[i].h = area_h - gy*2;
        return;
    }
    int master = tiled_idx[0];
    int mw_w = (area_w / 2) - gx - gx/2;
    if (mw_w < 80) mw_w = 80;
    tw[master].x = area_x + gx; tw[master].y = area_y + gy;
    tw[master].w = mw_w; tw[master].h = area_h - gy*2;
    int stack_n = n - 1;
    int sx = area_x + gx + mw_w + gx;
    int sw = area_w - (sx - area_x) - gx;
    if (sw < 60) sw = 60;
    int sh_total = area_h - gy*2 - gy*(stack_n - 1);
    int sy = area_y + gy;
    for (int s = 0; s < stack_n; s++) {
        int idx = tiled_idx[s + 1];
        int sh = sh_total / stack_n;
        if (s == stack_n - 1) sh = (area_y + area_h - gy) - sy;
        if (sh < 24) sh = 24;
        tw[idx].x = sx; tw[idx].y = sy;
        tw[idx].w = sw; tw[idx].h = sh;
        sy += sh + gy;
    }
}

static void ml_draw_wall(void) {
    for (int y = ML_BAR_H; y < GFX_HEIGHT; y++) {
        u8 c = (y < 70) ? GUI_DESK1 : (y < 130 ? GUI_DESK2 : GUI_DESK3);
        gfx_hline(0, y, GFX_WIDTH, c);
    }
    /* subtle grid for "rendered" feel */
    for (int y = ML_BAR_H; y < GFX_HEIGHT; y += 16)
        for (int x = 0; x < GFX_WIDTH; x += 32) gfx_pixel(x, y, GUI_DARK);
}

static void ml_draw_bar(int ws, const char *focus_title, bool floating) {
    gfx_fill_rect(0, 0, GFX_WIDTH, ML_BAR_H, GUI_TASKBAR);
    gfx_hline(0, ML_BAR_H - 1, GFX_WIDTH, GUI_TITLE_LT);
    gfx_fill_rect(2, 1, 10, 10, GUI_CYAN);
    gfx_text(4, 2, "M", GUI_BLACK);
    gfx_text(14, 2, "Miniland", GUI_WHITE);
    for (int i = 0; i < ML_WS; i++) {
        int x = 70 + i * 16;
        u8 bg = (i == ws) ? GUI_BLUE : GUI_DARK;
        gfx_fill_rect(x, 1, 14, 10, bg);
        gfx_frame(x, 1, 14, 10, (i == ws) ? GUI_CYAN : GUI_GREY);
        char n[2]; n[0] = (char)('1' + i); n[1] = 0;
        gfx_text(x + 4, 2, n, GUI_WHITE);
    }
    if (focus_title) {
        char t[28]; strncpy(t, focus_title, 27); t[27] = 0;
        gfx_text(140, 2, t, GUI_PANEL);
        if (floating) gfx_text(220, 2, "[F]", GUI_YELLOW);
    }
    gfx_text(240, 2, "SUPER", GUI_GREY);
    rtc_time_t tm; rtc_read(&tm);
    char clk[8]; ksnprintf(clk, sizeof(clk), "%02d:%02d", tm.hour, tm.minute);
    gfx_text(GFX_WIDTH - 32, 2, clk, GUI_YELLOW);
}

static void ml_draw_start_menu(bool open_menu) {
    if (!open_menu) return;
    int x = 2, y = ML_BAR_H + 2, w = 132, h = 110;
    gfx_fill_rect(x + 2, y + 2, w, h, GUI_BLACK);
    gfx_fill_rect(x, y, w, h, GUI_PANEL);
    gfx_frame(x, y, w, h, GUI_CYAN);
    gfx_fill_rect(x + 1, y + 1, w - 2, 10, GUI_TITLE);
    gfx_text(x + 4, y + 2, "Start (Super)", GUI_WHITE);
    static const char *items[] = {
        "Terminal   Alt+Enter", "Browser    Alt+B",
        "Explorer   Alt+E",    "Notepad    Alt+N",
        "Network",             "Tasks",
        "Toggle Float Alt+F",  "Close Win  Alt+Q",
        "Workspaces 1-4",      "Exit Miniland (Esc)"
    };
    for (int i = 0; i < 10; i++) gfx_text(x + 4, y + 14 + i * 9, items[i], GUI_TEXT);
}

static int ml_menu_pick(int mx, int my) {
    int x = 2, y = ML_BAR_H + 2, w = 132, h = 110;
    if (!point_in(mx, my, x, y, w, h)) return -1;
    int idx = (my - (y + 14)) / 9;
    if (idx < 0 || idx > 9) return -1;
    return idx;
}

int gui_run_miniland(void) {
    gfx_enter();
    mouse_set_bounds(GFX_WIDTH, GFX_HEIGHT);
    prev_left = false; drag_win = -1; start_open = false; running = true; launch_req = GUI_LAUNCH_NONE;

    u16 ws_mask[ML_WS] = { 0, 0, 0, 0 };
    int ws = 0;
    ml_win_t mw[WIN_COUNT];
    gui_win_t tw[WIN_COUNT];
    for (int i = 0; i < WIN_COUNT; i++) {
        mw[i].open = false; mw[i].kind = wins[i].kind; mw[i].floating = false;
        tw[i] = wins[i]; tw[i].open = false;
    }
    int mlfocus = WIN_TERM;
    ws_mask[0] |= (1u << WIN_TERM);
    mw[WIN_TERM].open = true; tw[WIN_TERM].open = true; bring_to_front(WIN_TERM);

    int pid = task_register("miniland", 320, true);
    task_set_state(pid, "running");
    bool ml_menu = false;
    int resize_win = -1;

    while (running) {
        mouse_state_t m; mouse_get_state(&m);
        bool left = (m.buttons & 1) != 0;

        for (int i = 0; i < WIN_COUNT; i++) { mw[i].open = (ws_mask[ws] & (1u << i)) != 0; tw[i].open = mw[i].open; }
        ml_layout(mw, tw, 0, ML_BAR_H, GFX_WIDTH, GFX_HEIGHT - ML_BAR_H);

        if (!prev_left && left) {
            if (ml_menu) {
                int pick = ml_menu_pick(m.x, m.y);
                ml_menu = false;
                if (pick == 0) { mw[WIN_TERM].floating = true; ws_mask[ws] |= (1u<<WIN_TERM); mlfocus = WIN_TERM; bring_to_front(WIN_TERM); }
                else if (pick == 1) { mw[WIN_BROWSER].floating = true; ws_mask[ws] |= (1u<<WIN_BROWSER); mlfocus = WIN_BROWSER; bring_to_front(WIN_BROWSER); if (br_url[0]==0) browser_load("about:home"); }
                else if (pick == 2) { mw[WIN_EXPLORER].floating = true; ws_mask[ws] |= (1u<<WIN_EXPLORER); mlfocus = WIN_EXPLORER; bring_to_front(WIN_EXPLORER); explorer_refresh(); }
                else if (pick == 3) { mw[WIN_NOTE].floating = true; ws_mask[ws] |= (1u<<WIN_NOTE); mlfocus = WIN_NOTE; bring_to_front(WIN_NOTE); }
                else if (pick == 4) { mw[WIN_NET].floating = true; ws_mask[ws] |= (1u<<WIN_NET); mlfocus = WIN_NET; bring_to_front(WIN_NET); }
                else if (pick == 5) { mw[WIN_TASKS].floating = true; ws_mask[ws] |= (1u<<WIN_TASKS); mlfocus = WIN_TASKS; bring_to_front(WIN_TASKS); }
                else if (pick == 6) { mw[mlfocus].floating = !mw[mlfocus].floating; }
                else if (pick == 7) { ws_mask[ws] &= (u16)~(1u << mlfocus); }
                else if (pick == 9) { running = false; }
            } else {
                bool hit = false;
                for (int i = WIN_COUNT - 1; i >= 0; i--) {
                    int idx = zorder[i];
                    if (!mw[idx].open) continue;
                    gui_win_t *w = &tw[idx];
                    if (point_in(m.x, m.y, w->x, w->y, w->w, w->h)) {
                        mlfocus = idx; bring_to_front(idx);
                        if (point_in(m.x, m.y, w->x + w->w - 10, w->y + 2, 7, 7)) {
                            ws_mask[ws] &= (u16)~(1u << idx); mw[idx].open = false; tw[idx].open = false;
                        } else if (point_in(m.x, m.y, w->x + w->w - 10, w->y + w->h - 10, 10, 10)) {
                            mw[idx].floating = true; resize_win = idx;
                        } else if (m.y < w->y + TITLE_H) {
                            mw[idx].floating = true; drag_win = idx; drag_dx = m.x - w->x; drag_dy = m.y - w->y;
                        }
                        hit = true; break;
                    }
                }
                if (!hit) {
                    if (point_in(m.x, m.y, 2, 1, 10, 10)) ml_menu = !ml_menu;
                    else if (m.y <= ML_BAR_H) {
                        for (int i = 0; i < ML_WS; i++) {
                            int bx = 70 + i * 16;
                            if (point_in(m.x, m.y, bx, 1, 14, 10)) { ws = i; break; }
                        }
                    } else {
                        ml_menu = false;
                    }
                }
            }
        }

        if (drag_win >= 0) {
            if (left) {
                tw[drag_win].x = m.x - drag_dx; tw[drag_win].y = m.y - drag_dy;
                if (tw[drag_win].x < 0) tw[drag_win].x = 0;
                if (tw[drag_win].y < ML_BAR_H) tw[drag_win].y = ML_BAR_H;
            } else drag_win = -1;
        }
        if (resize_win >= 0) {
            if (left) {
                tw[resize_win].w = m.x - tw[resize_win].x;
                tw[resize_win].h = m.y - tw[resize_win].y;
                if (tw[resize_win].w < 90) tw[resize_win].w = 90;
                if (tw[resize_win].h < 60) tw[resize_win].h = 60;
            } else resize_win = -1;
        }
        prev_left = left;

        int key = kbd_getchar();
        while (key >= 0) {
            if (key == KEY_ESC) { running = false; break; }
            if (key == KEY_SUPER) {
                if ((u32)(pit_ticks() - super_last) > 12) {
                    super_last = pit_ticks();
                    ml_menu = !ml_menu;
                }
                key = kbd_getchar(); continue;
            }
            if (kbd_alt_down()) {
                if (key == KEY_ENTER) { ws_mask[ws] |= (1u<<WIN_TERM); mw[WIN_TERM].open=true; tw[WIN_TERM].open=true; mlfocus=WIN_TERM; bring_to_front(WIN_TERM); }
                else if (key == 'q' || key == 'Q') { ws_mask[ws] &= (u16)~(1u << mlfocus); }
                else if (key >= '1' && key <= '4') ws = key - '1';
                else if (key == 'j' || key == 'J' || key == KEY_TAB) {
                    for (int step = 1; step <= WIN_COUNT; step++) { int idx = (mlfocus + step) % WIN_COUNT; if (mw[idx].open) { mlfocus = idx; break; } }
                }
                else if (key == 'k' || key == 'K') {
                    for (int step = 1; step <= WIN_COUNT; step++) { int idx = (mlfocus + WIN_COUNT - step) % WIN_COUNT; if (mw[idx].open) { mlfocus = idx; break; } }
                }
                else if (key == 'b' || key == 'B') { ws_mask[ws] |= (1u<<WIN_BROWSER); mw[WIN_BROWSER].open=true; tw[WIN_BROWSER].open=true; mlfocus=WIN_BROWSER; bring_to_front(WIN_BROWSER); if (br_url[0]==0) browser_load("about:home"); }
                else if (key == 'e' || key == 'E') { ws_mask[ws] |= (1u<<WIN_EXPLORER); mw[WIN_EXPLORER].open=true; tw[WIN_EXPLORER].open=true; mlfocus=WIN_EXPLORER; bring_to_front(WIN_EXPLORER); explorer_refresh(); }
                else if (key == 'n' || key == 'N') { ws_mask[ws] |= (1u<<WIN_NOTE); mw[WIN_NOTE].open=true; tw[WIN_NOTE].open=true; mlfocus=WIN_NOTE; bring_to_front(WIN_NOTE); }
                else if (key == 'f' || key == 'F') { mw[mlfocus].floating = !mw[mlfocus].floating; }
                key = kbd_getchar(); continue;
            }
            if (!ml_menu && mw[mlfocus].open) {
                if (mlfocus == WIN_TERM) terminal_key(key);
                else if (mlfocus == WIN_NOTE) notepad_key(key);
                else if (mlfocus == WIN_NET) network_key(key);
                else if (mlfocus == WIN_BROWSER) browser_key(key);
                else if (mlfocus == WIN_EXPLORER) explorer_key(key);
            }
            key = kbd_getchar();
        }

        net_poll();
        ml_draw_wall();
        ml_draw_bar(ws, mw[mlfocus].open ? wins[mlfocus].title : "", mw[mlfocus].open && mw[mlfocus].floating);
        for (int i = 0; i < WIN_COUNT; i++) {
            int idx = zorder[i];
            if (!mw[idx].open) continue;
            gui_win_t *w = &tw[idx];
            /* draw window with drop shadow for floating */
            if (mw[idx].floating) gfx_fill_rect(w->x + 2, w->y + 2, w->w, w->h, GUI_BLACK);
            draw_window_frame(w, idx == mlfocus);
            /* resize grip */
            gfx_fill_rect(w->x + w->w - 6, w->y + w->h - 6, 5, 5, GUI_GREY);
            switch (w->kind) {
                case WIN_TERM:     draw_terminal(w); break;
                case WIN_NOTE:     draw_notepad(w);  break;
                case WIN_TASKS:    draw_tasks(w);    break;
                case WIN_NET:      draw_network(w);  break;
                case WIN_BROWSER:  draw_browser(w);  break;
                case WIN_EXPLORER: draw_explorer(w); break;
                default: break;
            }
        }
        ml_draw_start_menu(ml_menu);
        draw_cursor(m.x, m.y);
        gfx_present();
        pit_sleep_ms(20);
    }

    task_unregister(pid);
    gfx_leave();
    kprintf("\n[miniland] closed, returning to console\n");
    return GUI_LAUNCH_NONE;
}
