/* Minimal Desktop - VGA text console, COM1 serial mirror, kprintf */
#include "console.h"
#include "io.h"
#include "string.h"
#include "vga_hw.h"
#include "bootart.h"
#include <stdarg.h>

#define VGA_COLS 80
#define VGA_ROWS 25

static volatile u16 *const vga = (volatile u16 *)0xB8000;
static int cur_row = 0;
static int cur_col = 0;
static u8 cur_fg = VGA_LIGHT_GREY;
static u8 cur_bg = VGA_BLACK;
static u8 theme_fg = VGA_LIGHT_GREY;
static u8 theme_bg = VGA_BLACK;
static bool cursor_visible = true;
static bool serial_mirror = true;

/* Optional capture sink (GUI terminal, selftest transcript). */
static char *cap_buf;
static int cap_max;
static int cap_len;
static bool cap_on;

/* ------------------------------------------------------------------------- */
/* COM1 16550 serial port                                                    */
/* ------------------------------------------------------------------------- */
#define COM1 0x3F8
static bool serial_ok = false;

void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* disable interrupts */
    outb(COM1 + 3, 0x80);   /* DLAB */
    outb(COM1 + 0, 0x01);   /* divisor 1 = 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8 bits, no parity, one stop */
    outb(COM1 + 2, 0xC7);   /* FIFO enable + clear, 14-byte threshold */
    outb(COM1 + 4, 0x0B);   /* IRQs enabled, RTS/DSR set */

    /* Loopback presence test */
    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0xAE);
    if (inb(COM1 + 0) != 0xAE) {
        serial_ok = false;
    } else {
        serial_ok = true;
    }
    outb(COM1 + 4, 0x0F);
}

static bool serial_tx_ready(void) {
    return (inb(COM1 + 5) & 0x20) != 0;
}

void serial_putc(char c) {
    if (!serial_ok) return;
    int guard = 100000;
    while (!serial_tx_ready() && guard-- > 0) { }
    outb(COM1, (u8)c);
}

void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

int serial_rx_ready(void) {
    return serial_ok && (inb(COM1 + 5) & 0x01);
}

char serial_read(void) {
    while (!serial_rx_ready()) { }
    return (char)inb(COM1);
}

void serial_enable_mirror(bool on) {
    serial_mirror = on;
}

/* ------------------------------------------------------------------------- */
/* VGA text console                                                          */
/* ------------------------------------------------------------------------- */
u8 console_make_attr(u8 fg, u8 bg) {
    return (u8)((fg & 0x0F) | ((bg & 0x0F) << 4));
}

void console_init(void) {
    vga_hw_set_text_mode();
    cur_row = 0;
    cur_col = 0;
    cur_fg = theme_fg;
    cur_bg = theme_bg;
    console_clear();
    serial_mirror = true;
    serial_puts("\r\n[serial] Minimal Desktop COM1 online at 115200 8N1\r\n");
}

void console_clear(void) {
    u16 blank = (u16)(' ' | (console_make_attr(cur_fg, cur_bg) << 8));
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) vga[i] = blank;
    cur_row = 0;
    cur_col = 0;
    vga_hw_set_text_cursor(0, 0);
}

void console_scroll(void) {
    u16 blank = (u16)(' ' | (console_make_attr(cur_fg, cur_bg) << 8));
    for (int i = 0; i < (VGA_ROWS - 1) * VGA_COLS; i++)
        vga[i] = vga[i + VGA_COLS];
    for (int i = (VGA_ROWS - 1) * VGA_COLS; i < VGA_ROWS * VGA_COLS; i++)
        vga[i] = blank;
    cur_row = VGA_ROWS - 1;
}

static void console_update_cursor(void) {
    if (cursor_visible) vga_hw_set_text_cursor(cur_row, cur_col);
}

void console_putc(char c) {
    /* Defensive clamp: graphics-mode code can leave stale cursor state. */
    if (cur_row < 0 || cur_row >= VGA_ROWS) cur_row = VGA_ROWS - 1;
    if (cur_col < 0 || cur_col >= VGA_COLS) cur_col = 0;
    if (cap_on && cap_buf && cap_len < cap_max - 1) {
        if (c == '\r') {
            /* collapse carriage returns used by progress bars */
        } else if (c == '\t') {
            cap_buf[cap_len++] = ' ';
        } else if ((unsigned char)c >= 0x20 || c == '\n') {
            cap_buf[cap_len++] = c;
        }
        cap_buf[cap_len] = 0;
    }
    if (c == '\n') {
        cur_col = 0;
        cur_row++;
        if (serial_mirror) { serial_putc('\r'); serial_putc('\n'); }
    } else if (c == '\r') {
        cur_col = 0;
    } else if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
        } else if (cur_row > 0) {
            cur_row--;
            cur_col = VGA_COLS - 1;
        }
        vga[cur_row * VGA_COLS + cur_col] = (u16)(' ' | (console_make_attr(cur_fg, cur_bg) << 8));
        if (serial_mirror) serial_putc('\b');
    } else if (c == '\t') {
        int next = (cur_col + 8) & ~7;
        while (cur_col < next && cur_col < VGA_COLS) console_putc(' ');
    } else if ((unsigned char)c >= 0x20) {
        vga[cur_row * VGA_COLS + cur_col] = (u16)((unsigned char)c | (console_make_attr(cur_fg, cur_bg) << 8));
        cur_col++;
        if (cur_col >= VGA_COLS) { cur_col = 0; cur_row++; }
        if (serial_mirror) serial_putc(c);
    }
    if (cur_row >= VGA_ROWS) console_scroll();
    console_update_cursor();
}

void console_puts(const char *s) {
    while (*s) console_putc(*s++);
}

void console_write(const char *s, size_t n) {
    while (n--) console_putc(*s++);
}

void console_capture_begin(char *buf, int max) {
    cap_buf = buf;
    cap_max = max;
    cap_len = 0;
    cap_on = true;
    if (buf && max > 0) buf[0] = 0;
}

int console_capture_end(void) {
    cap_on = false;
    cap_buf = NULL;
    int n = cap_len;
    cap_len = 0;
    return n;
}

void console_set_color(u8 fg, u8 bg) {
    cur_fg = fg & 0x0F;
    cur_bg = bg & 0x0F;
}

void console_reset_color(void) {
    cur_fg = theme_fg;
    cur_bg = theme_bg;
}

void console_get_color(u8 *fg, u8 *bg) {
    if (fg) *fg = cur_fg;
    if (bg) *bg = cur_bg;
}

void console_set_theme(u8 fg, u8 bg) {
    theme_fg = fg & 0x0F;
    theme_bg = bg & 0x0F;
    console_reset_color();
}

void console_get_theme(u8 *fg, u8 *bg) {
    if (fg) *fg = theme_fg;
    if (bg) *bg = theme_bg;
}

void console_set_cursor(bool visible) {
    cursor_visible = visible;
    if (visible) {
        vga_hw_enable_cursor();
        console_update_cursor();
    } else {
        vga_hw_disable_cursor();
    }
}

void console_put_at(int row, int col, char c, u8 attr) {
    if (row < 0 || row >= VGA_ROWS || col < 0 || col >= VGA_COLS) return;
    vga[row * VGA_COLS + col] = (u16)((u8)c | ((u16)attr << 8));
}

int console_bootart_cols(void) { return BOOTART_COLS; }

void console_draw_bootart(int row, int col) {
    for (int r = 0; r < BOOTART_ROWS; r++) {
        for (int c = 0; c < bootart_widths[r]; c++) {
            if (row + r >= VGA_ROWS || col + c >= VGA_COLS) continue;
            bootart_cell_t cell = bootart_rows[r][c];
            console_put_at(row + r, col + c, (char)cell.ch, cell.attr);
        }
    }
    if (row + BOOTART_ROWS < VGA_ROWS) cur_row = row + BOOTART_ROWS;
    cur_col = 0;
    console_update_cursor();
}

void console_get_cursor(int *row, int *col) {
    if (row) *row = cur_row;
    if (col) *col = cur_col;
}

void console_gotoxy(int row, int col) {
    if (row < 0) row = 0;
    if (row >= VGA_ROWS) row = VGA_ROWS - 1;
    if (col < 0) col = 0;
    if (col >= VGA_COLS) col = VGA_COLS - 1;
    cur_row = row;
    cur_col = col;
    console_update_cursor();
}

void console_write_at(int row, int col, const char *s, u8 attr) {
    while (*s && col < VGA_COLS) {
        console_put_at(row, col, *s, attr);
        col++;
        s++;
    }
}

u16 *console_buffer(void) {
    return (u16 *)vga;
}

void kputc(char c) { console_putc(c); }
void kputs(const char *s) { console_puts(s); }

/* ------------------------------------------------------------------------- */
/* Formatted output                                                          */
/* ------------------------------------------------------------------------- */
static int fmt_number(char *out, int cap, u64 value, int base, bool upper,
                      bool negative, int width, bool zero, bool left, bool plus) {
    char rev[68];
    int n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char sign = negative ? '-' : (plus ? '+' : 0);

    if (value == 0) rev[n++] = '0';
    while (value) { rev[n++] = digits[value % (u32)base]; value /= (u32)base; }

    int len = n + (sign ? 1 : 0);
    int pad = width > len ? width - len : 0;
    int pos = 0;

    if (!left && !zero) { while (pad-- > 0 && pos < cap - 1) out[pos++] = ' '; }
    if (sign && pos < cap - 1) out[pos++] = sign;
    if (!left && zero) { while (pad-- > 0 && pos < cap - 1) out[pos++] = '0'; }
    while (n && pos < cap - 1) out[pos++] = rev[--n];
    if (left) { while (pad-- > 0 && pos < cap - 1) out[pos++] = ' '; }
    out[pos] = 0;
    return pos;
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t pos = 0;
    if (!size) return 0;

#define PUT(ch) do { if (pos < size - 1) buf[pos] = (ch); pos++; } while (0)

    while (*fmt) {
        if (*fmt != '%') { PUT(*fmt++); continue; }
        fmt++;
        if (*fmt == '%') { PUT('%'); fmt++; continue; }

        bool left = false, zero = false, plus = false;
        for (;;) {
            if (*fmt == '-') { left = true; fmt++; }
            else if (*fmt == '0') { zero = true; fmt++; }
            else if (*fmt == '+') { plus = true; fmt++; }
            else break;
        }
        int width = 0;
        while (isdigit((unsigned char)*fmt)) {
            width = width * 10 + (*fmt++ - '0');
            if (width > 60) width = 60;
        }
        int precision = -1;
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            while (isdigit((unsigned char)*fmt)) precision = precision * 10 + (*fmt++ - '0');
        }
        int lng = 0;
        while (*fmt == 'l') { lng++; fmt++; }
        if (*fmt == 'z') { lng = 1; fmt++; }
        char spec = *fmt++;
        char tmp[72];
        const char *str = NULL;
        int len = 0;
        bool neg = false;

        switch (spec) {
            case 'd':
            case 'i': {
                s64 v;
                if (lng >= 2) v = va_arg(ap, s64);
                else v = va_arg(ap, int);
                if (v < 0) { neg = true; v = -v; }
                len = fmt_number(tmp, sizeof(tmp), (u64)v, 10, false, neg, width, zero, left, plus);
                str = tmp;
                break;
            }
            case 'u': {
                u64 v;
                if (lng >= 2) v = va_arg(ap, u64);
                else v = va_arg(ap, u32);
                len = fmt_number(tmp, sizeof(tmp), v, 10, false, false, width, zero, left, false);
                str = tmp;
                break;
            }
            case 'x':
            case 'X': {
                u64 v;
                if (lng >= 2) v = va_arg(ap, u64);
                else v = va_arg(ap, u32);
                len = fmt_number(tmp, sizeof(tmp), v, 16, spec == 'X', false, width, zero, left, false);
                str = tmp;
                break;
            }
            case 'p': {
                u32 v = va_arg(ap, u32);
                tmp[0] = '0'; tmp[1] = 'x';
                len = 2 + fmt_number(tmp + 2, sizeof(tmp) - 2, v, 16, false, false, 8, true, false, false);
                str = tmp;
                break;
            }
            case 'c': {
                tmp[0] = (char)va_arg(ap, int);
                tmp[1] = 0;
                len = 1;
                str = tmp;
                break;
            }
            case 's': {
                str = va_arg(ap, const char *);
                if (!str) str = "(null)";
                len = (int)strlen(str);
                if (precision >= 0 && len > precision) len = precision;
                break;
            }
            default:
                tmp[0] = '%';
                tmp[1] = spec;
                tmp[2] = 0;
                str = tmp;
                len = 2;
                break;
        }

        int pad = width > len ? width - len : 0;
        if (!left) while (pad-- > 0) PUT(' ');
        for (int i = 0; i < len; i++) PUT(str[i]);
        if (left) while (pad-- > 0) PUT(' ');
    }
    if (pos >= size) pos = size - 1;
    buf[pos] = 0;
    return (int)pos;
#undef PUT
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = kvsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

void kprintf(const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    console_puts(buf);
}

void kprintf_color(u8 fg, u8 bg, const char *fmt, ...) {
    char buf[2048];
    u8 ofg, obg;
    va_list ap;
    console_get_color(&ofg, &obg);
    console_set_color(fg, bg);
    va_start(ap, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    console_puts(buf);
    console_set_color(ofg, obg);
}

void klog(const char *tag, const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    kprintf("[%s] %s\n", tag, msg);
}

void kpanic(const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    cli();
    console_set_color(VGA_WHITE, VGA_RED);
    console_puts("\n\n*** KERNEL PANIC ***\n");
    console_puts(msg);
    console_puts("\nSystem halted. Reboot required.\n");
    serial_puts("\n*** KERNEL PANIC ***\n");
    serial_puts(msg);
    serial_puts("\nSystem halted.\n");
    for (;;) hlt();
}
