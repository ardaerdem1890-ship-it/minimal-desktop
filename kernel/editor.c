/* Minimal Desktop - full-screen text editor and pager for the terminal */
#include "editor.h"
#include "console.h"
#include "ramfs.h"
#include "keyboard.h"
#include "pit.h"
#include "string.h"

#define SCR_ROWS 25
#define SCR_COLS 80
#define EDIT_TOP 1
#define EDIT_ROWS 21          /* rows 1..21 */
#define TEXT_W   76
#define BAR_ROW  (SCR_ROWS - 2)
#define HINT_ROW (SCR_ROWS - 1)

static char ebuf[FS_MAX_FILE + 1];
static int  elen;
static int  epos;
static bool dirty;
static char epath[FS_NAME_MAX];
static int  etop;

static char rbuf[FS_MAX_FILE + 1];
static int  rlen;
static int  rtop;

static void wait_ms(u32 ms) { pit_sleep_ms(ms); }

/* ------------------------------------------------------------------------- */
/* small shared helpers                                                      */
/* ------------------------------------------------------------------------- */
static int line_of(const char *b, int n, int p) {
    int line = 0;
    if (p > n) p = n;
    for (int i = 0; i < p; i++) if (b[i] == '\n') line++;
    return line;
}

static int line_start(const char *b, int n, int line) {
    int cur = 0;
    for (int i = 0; i < n; i++) {
        if (cur == line) return i;
        if (b[i] == '\n') cur++;
    }
    return n;
}

static int line_end(const char *b, int n, int line) {
    int i = line_start(b, n, line);
    while (i < n && b[i] != '\n') i++;
    return i;
}

static int line_count(const char *b, int n) {
    int c = 1;
    for (int i = 0; i < n; i++) if (b[i] == '\n') c++;
    return c;
}

static void draw_bar(const char *title, const char *right) {
    char line[SCR_COLS + 1];
    int n = 0;
    line[n++] = ' ';
    for (const char *p = title; *p && n < SCR_COLS - 1; p++) line[n++] = *p;
    int rl = (int)strlen(right);
    while (n < SCR_COLS - rl - 1) line[n++] = ' ';
    for (const char *p = right; *p && n < SCR_COLS; p++) line[n++] = *p;
    while (n < SCR_COLS) line[n++] = ' ';
    line[SCR_COLS] = 0;
    console_write_at(BAR_ROW, 0, line, console_make_attr(VGA_BLACK, VGA_LIGHT_GREY));
    console_write_at(HINT_ROW, 0,
                     " F2/Ctrl+S save   Esc exit   arrows move   PgUp/PgDn scroll        ",
                     console_make_attr(VGA_LIGHT_GREY, VGA_BLUE));
}

static void screen_begin(const char *title) {
    console_clear();
    console_set_cursor(false);
    console_write_at(0, 0, "                                                                                ",
                     console_make_attr(VGA_WHITE, VGA_BLUE));
    char head[SCR_COLS + 1];
    ksnprintf(head, sizeof(head), " %-76s", title);
    console_write_at(0, 0, head, console_make_attr(VGA_WHITE, VGA_BLUE));
}

static void screen_end(void) {
    console_set_cursor(true);
    console_clear();
}

/* ------------------------------------------------------------------------- */
/* editor                                                                    */
/* ------------------------------------------------------------------------- */
static void edit_ensure_visible(void) {
    int line = line_of(ebuf, elen, epos);
    if (line < etop) etop = line;
    if (line >= etop + EDIT_ROWS) etop = line - EDIT_ROWS + 1;
    if (etop < 0) etop = 0;
}

static void edit_render(void) {
    char right[48];
    ksnprintf(right, sizeof(right), "Ln %d Col %d  %d bytes%s ",
              line_of(ebuf, elen, epos) + 1,
              epos - line_start(ebuf, elen, line_of(ebuf, elen, epos)) + 1,
              elen, dirty ? " *" : "");
    char title[80];
    ksnprintf(title, sizeof(title), "EDIT  %s", epath);
    draw_bar(title, right);

    int total = line_count(ebuf, elen);
    for (int r = 0; r < EDIT_ROWS; r++) {
        int line = etop + r;
        char rowbuf[SCR_COLS + 1];
        memset(rowbuf, ' ', SCR_COLS);
        rowbuf[SCR_COLS] = 0;
        if (line < total) {
            int s = line_start(ebuf, elen, line);
            int e = line_end(ebuf, elen, line);
            int shown = 0;
            for (int i = s; i < e && shown < TEXT_W; i++) {
                char ch = ebuf[i];
                rowbuf[shown++] = (ch == '\t') ? ' ' : (ch >= 32 ? ch : '.');
            }
        }
        console_write_at(EDIT_TOP + r, 0, rowbuf, console_make_attr(VGA_LIGHT_GREY, VGA_BLACK));
    }
    int cline = line_of(ebuf, elen, epos);
    int ccol = epos - line_start(ebuf, elen, cline);
    console_gotoxy(EDIT_TOP + (cline - etop), ccol);
}

static int edit_save(void) {
    fs_node_t *n = ramfs_lookup(epath);
    if (!n) {
        n = ramfs_create(epath, FS_FILE);
        if (!n) return -1;
    }
    if (n->type != FS_FILE) return -1;
    if (ramfs_truncate(n, 0) != 0) return -1;
    if (elen > 0 && ramfs_write(n, ebuf, (u32)elen, 0) != (int)elen) return -1;
    dirty = false;
    return 0;
}

int editor_run(const char *path) {
    fs_node_t *n = ramfs_lookup(path);
    if (n && n->type != FS_FILE) {
        kprintf("edit: %s: is a directory\n", path);
        return -1;
    }
    elen = 0;
    if (n) {
        int got = ramfs_read(n, ebuf, FS_MAX_FILE, 0);
        if (got > 0) elen = got;
    }
    ebuf[elen] = 0;
    epos = elen;
    etop = 0;
    dirty = false;
    strncpy(epath, path, sizeof(epath) - 1);
    epath[sizeof(epath) - 1] = 0;

    screen_begin("EDIT");
    edit_ensure_visible();
    edit_render();

    for (;;) {
        wait_ms(4);
        int key = kbd_getchar();
        if (key < 0) continue;

        if (key == KEY_ESC) {
            if (dirty) {
                console_write_at(BAR_ROW, 0,
                    " Save changes?  Y = save   N = discard   Esc = cancel                    ",
                    console_make_attr(VGA_BLACK, VGA_YELLOW));
                for (;;) {
                    wait_ms(4);
                    int k = kbd_getchar();
                    if (k == 'y' || k == 'Y') { edit_save(); key = KEY_ESC; break; }
                    if (k == 'n' || k == 'N' || k == KEY_ESC) { key = KEY_ESC; break; }
                }
            }
            if (key == KEY_ESC) break;
        } else if (key == KEY_F2 || (key == 19)) {          /* F2 or Ctrl+S */
            if (edit_save() != 0)
                console_write_at(BAR_ROW, 0,
                    " Save failed: file could not be written                              ",
                    console_make_attr(VGA_WHITE, VGA_RED));
        } else if (key == KEY_LEFT) {
            if (epos > 0) epos--;
        } else if (key == KEY_RIGHT) {
            if (epos < elen) epos++;
        } else if (key == KEY_UP) {
            int line = line_of(ebuf, elen, epos);
            if (line > 0) {
                int col = epos - line_start(ebuf, elen, line);
                int s = line_start(ebuf, elen, line - 1);
                int e = line_end(ebuf, elen, line - 1);
                epos = s + (col < e - s ? col : e - s);
            }
        } else if (key == KEY_DOWN) {
            int line = line_of(ebuf, elen, epos);
            if (line + 1 < line_count(ebuf, elen)) {
                int col = epos - line_start(ebuf, elen, line);
                int s = line_start(ebuf, elen, line + 1);
                int e = line_end(ebuf, elen, line + 1);
                epos = s + (col < e - s ? col : e - s);
            }
        } else if (key == KEY_HOME) {
            epos = line_start(ebuf, elen, line_of(ebuf, elen, epos));
        } else if (key == KEY_END) {
            epos = line_end(ebuf, elen, line_of(ebuf, elen, epos));
        } else if (key == KEY_PGUP) {
            int line = line_of(ebuf, elen, epos) - EDIT_ROWS;
            if (line < 0) line = 0;
            epos = line_start(ebuf, elen, line);
        } else if (key == KEY_PGDN) {
            int line = line_of(ebuf, elen, epos) + EDIT_ROWS;
            int total = line_count(ebuf, elen);
            if (line >= total) line = total - 1;
            epos = line_start(ebuf, elen, line);
        } else if (key == KEY_BACKSPACE) {
            if (epos > 0) {
                memmove(ebuf + epos - 1, ebuf + epos, (size_t)(elen - epos));
                epos--; elen--; dirty = true;
            }
        } else if (key == KEY_DELETE) {
            if (epos < elen) {
                memmove(ebuf + epos, ebuf + epos + 1, (size_t)(elen - epos - 1));
                elen--; dirty = true;
            }
        } else if (key == KEY_ENTER) {
            if (elen < (int)FS_MAX_FILE) {
                memmove(ebuf + epos + 1, ebuf + epos, (size_t)(elen - epos));
                ebuf[epos] = '\n';
                epos++; elen++; dirty = true;
            }
        } else if (key == KEY_TAB) {
            for (int i = 0; i < 4 && elen < (int)FS_MAX_FILE; i++) {
                memmove(ebuf + epos + 1, ebuf + epos, (size_t)(elen - epos));
                ebuf[epos] = ' ';
                epos++; elen++;
            }
            dirty = true;
        } else if (key >= 0x20 && key < 0x7F) {
            if (elen < (int)FS_MAX_FILE) {
                memmove(ebuf + epos + 1, ebuf + epos, (size_t)(elen - epos));
                ebuf[epos] = (char)key;
                epos++; elen++; dirty = true;
            }
        } else {
            continue;
        }
        ebuf[elen] = 0;
        edit_ensure_visible();
        edit_render();
    }

    screen_end();
    kprintf("edit: %s %s (%d bytes)\n", epath, dirty ? "not saved" : "closed", elen);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* pager                                                                     */
/* ------------------------------------------------------------------------- */
static void pager_render(const char *path) {
    char right[24];
    int total = line_count(rbuf, rlen);
    int shown = rtop + ED_PAGER_ROWS;
    if (shown > total) shown = total;
    ksnprintf(right, sizeof(right), "%d-%d/%d ", rtop + 1, shown, total);
    char title[80];
    ksnprintf(title, sizeof(title), "READ  %s", path);
    draw_bar(title, right);

    for (int r = 0; r < ED_PAGER_ROWS; r++) {
        int line = rtop + r;
        char rowbuf[SCR_COLS + 1];
        memset(rowbuf, ' ', SCR_COLS);
        rowbuf[SCR_COLS] = 0;
        if (line < total) {
            int s = line_start(rbuf, rlen, line);
            int e = line_end(rbuf, rlen, line);
            int n = 0;
            for (int i = s; i < e && n < SCR_COLS - 1; i++) {
                char ch = rbuf[i];
                rowbuf[n++] = (ch == '\t') ? ' ' : (ch >= 32 ? ch : '.');
            }
        }
        console_write_at(EDIT_TOP + r, 0, rowbuf,
                         console_make_attr(VGA_LIGHT_GREY, VGA_BLACK));
    }
}

int pager_run(const char *path) {
    fs_node_t *n = ramfs_lookup(path);
    if (!n) { kprintf("read: %s: no such file\n", path); return -1; }
    if (n->type != FS_FILE) { kprintf("read: %s: is a directory\n", path); return -1; }
    rlen = ramfs_read(n, rbuf, FS_MAX_FILE, 0);
    if (rlen < 0) rlen = 0;
    rbuf[rlen] = 0;
    rtop = 0;

    int total = line_count(rbuf, rlen);
    screen_begin("READ");
    pager_render(path);

    for (;;) {
        wait_ms(4);
        int key = kbd_getchar();
        if (key < 0) continue;
        if (key == KEY_ESC || key == 'q' || key == 'Q') break;
        if (key == KEY_DOWN || key == KEY_ENTER) {
            if (rtop < total - 1) rtop++;
        } else if (key == KEY_UP) {
            if (rtop > 0) rtop--;
        } else if (key == KEY_PGDN || key == ' ') {
            rtop += ED_PAGER_ROWS;
            if (rtop > total - 1) rtop = total - 1;
        } else if (key == KEY_PGUP) {
            rtop -= ED_PAGER_ROWS;
            if (rtop < 0) rtop = 0;
        } else if (key == KEY_HOME || key == 'g') {
            rtop = 0;
        } else if (key == KEY_END || key == 'G') {
            rtop = total - 1;
            if (rtop < 0) rtop = 0;
        } else {
            continue;
        }
        pager_render(path);
    }

    screen_end();
    return 0;
}
