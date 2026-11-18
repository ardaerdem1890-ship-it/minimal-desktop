#pragma once
#include "types.h"

#define OS_NAME        "Minimal Desktop"
#define OS_SHORTNAME   "minimal_desktop"
#define OS_VERSION     "1.7.1"
#define OS_CODENAME    "LoginFix"
#define OS_BUILD       __DATE__ " " __TIME__
#define OS_ARCH        "x86 (i386 protected mode)"

/* VGA text colours */
enum {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
};

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
int  serial_rx_ready(void);
char serial_read(void);
void serial_enable_mirror(bool on);

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_puts(const char *s);
void console_write(const char *s, size_t n);
void console_set_color(u8 fg, u8 bg);
void console_reset_color(void);
void console_get_color(u8 *fg, u8 *bg);
void console_set_theme(u8 fg, u8 bg);
void console_get_theme(u8 *fg, u8 *bg);
void console_set_cursor(bool visible);
void console_scroll(void);
void console_put_at(int row, int col, char c, u8 attr);
void console_write_at(int row, int col, const char *s, u8 attr);
u8   console_make_attr(u8 fg, u8 bg);
u16 *console_buffer(void);
void console_draw_bootart(int row, int col);
int  console_bootart_cols(void);
void console_gotoxy(int row, int col);
void console_get_cursor(int *row, int *col);

/* Output capture used by the GUI terminal. */
void console_capture_begin(char *buf, int max);
int  console_capture_end(void);

void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kprintf_color(u8 fg, u8 bg, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void kputs(const char *s);
void kputc(char c);
int  ksnprintf(char *buf, size_t size, const char *fmt, ...);
int  kvsnprintf(char *buf, size_t size, const char *fmt, __builtin_va_list ap);
void kpanic(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void klog(const char *tag, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
