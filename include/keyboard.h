#pragma once
#include "types.h"

#define KEY_ESC        0x1B
#define KEY_BACKSPACE  0x08
#define KEY_TAB        0x09
#define KEY_ENTER      0x0A
#define KEY_UP         0x101
#define KEY_DOWN       0x102
#define KEY_LEFT       0x103
#define KEY_RIGHT      0x104
#define KEY_HOME       0x105
#define KEY_END        0x106
#define KEY_DELETE     0x107
#define KEY_PGUP       0x108
#define KEY_PGDN       0x109
#define KEY_F1         0x110
#define KEY_F2         0x111
#define KEY_PRTSC      0x120
#define KEY_SUPER      0x121

void keyboard_init(void);
int  kbd_getchar(void);          /* non-blocking, -1 if empty */
bool kbd_has_key(void);
void kbd_flush(void);
bool keyboard_selftest(char *err, int errlen);
u32  kbd_irq_count(void);
int  kbd_wait_key(u32 timeout_ms); /* -1 on timeout */
bool kbd_set_layout(const char *name);
void kbd_cycle_layout(int dir);
int  kbd_layout_index(void);
int  kbd_layout_count(void);
const char *kbd_layout_name_at(int i);
const char *kbd_layout_label_at(int i);
const char *kbd_layout_label(void);
u32  kbd_layout_generation(void);
const char *kbd_layout_name(void);
bool kbd_alt_down(void);
bool kbd_ctrl_down(void);
bool kbd_shift_down(void);
bool kbd_super_down(void);
