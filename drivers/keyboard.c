/* Minimal Desktop - PS/2 keyboard, IRQ1, set-1 scancode translation */
#include "keyboard.h"
#include "io.h"
#include "isr.h"
#include "gdt.h"
#include "console.h"
#include "pit.h"
#include "string.h"

#define KBD_DATA   0x60
#define KBD_STATUS 0x64
#define KBD_CMD    0x64

static volatile u16 key_queue[256];
static volatile u16 q_head, q_tail;
static bool shift_down, ctrl_down, caps_lock, extended, alt_down, super_down;
static int e1_skip;
static bool initialized;
static u32 irq_total;

/* Layout tables are stored as compact row strings so adding a layout costs a
 * few lines. Positions map to set-1 scancodes:
 *   digits[0..11]  -> 0x02..0x0D   (1 2 3 4 5 6 7 8 9 0 - =)
 *   row1[0..11]    -> 0x10..0x1B   (q w e r t y u i o p [ ])
 *   row2[0..10]    -> 0x1E..0x28   (a s d f g h j k l ; ')
 *   row3[0..9]     -> 0x2C..0x35   (z x c v b n m , . /)
 *   extra[0..1]    -> 0x29, 0x2B   (` and \)
 * Non-ASCII letters are folded to their closest ASCII glyph because the VGA
 * text font has no Turkish/Greek/Cyrillic glyphs. */
typedef struct {
    const char *name;      /* short id, used by `keymap`                */
    const char *label;     /* human readable name for the UI           */
    const char *digits, *digits_s;
    const char *row1, *row1_s;
    const char *row2, *row2_s;
    const char *row3, *row3_s;
    const char *extra, *extra_s;
} kbd_layout_t;

#define L(id, lbl, d, ds, r1, r1s, r2, r2s, r3, r3s, ex, exs) \
    { id, lbl, d, ds, r1, r1s, r2, r2s, r3, r3s, ex, exs }

static const kbd_layout_t layouts[] = {
    L("us", "English (US)",
      "1234567890-=", "!@#$%^&*()_+",
      "qwertyuiop[]", "QWERTYUIOP{}",
      "asdfghjkl;'",  "ASDFGHJKL:\"",
      "zxcvbnm,./",   "ZXCVBNM<>?",
      "`\\",          "~|"),
    L("uk", "English (UK)",
      "1234567890-=", "!\"#$%^&*()_+",
      "qwertyuiop[]", "QWERTYUIOP{}",
      "asdfghjkl;'",  "ASDFGHJKL:@",
      "zxcvbnm,./",   "ZXCVBNM<>?",
      "`#",           "~|"),
    L("de", "German (QWERTZ)",
      "1234567890s'", "!\"#$%&/()=?`",
      "qwertzuiopu+", "QWERTZUIOPU*",
      "asdfghjkloa",  "ASDFGHJKLOA",
      "yxcvbnm,.-",   "YXCVBNM;:_",
      "^#",           "~'"),
    L("fr", "French (AZERTY)",
      "&e\"'(-e_ca)=", "1234567890+=",
      "azertyuiop^$",  "AZERTYUIOP^$",
      "qsdfghjklm'",   "QSDFGHJKLM%",
      "wxcvbn,;:!",    "WXCVBN?./+",
      "<>",            "><"),
    L("be", "Belgian (AZERTY)",
      "&e\"'(seca)=+", "1234567890+=",
      "azertyuiop^$", "AZERTYUIOP^$",
      "qsdfghjklm'",  "QSDFGHJKLM%",
      "wxcvbn,;:=",   "WXCVBN?./+",
      "<>",           "><"),
    L("es", "Spanish",
      "1234567890'!", "!\"#$%&/()=?`",
      "qwertyuiop`+", "QWERTYUIOP^*",
      "asdfghjkln'",  "ASDFGHJKLN\"",
      "zxcvbnm,.-",   "ZXCVBNM;:_",
      "<>",           "><"),
    L("it", "Italian",
      "1234567890'!", "!\"#$%&/()=?`",
      "qwertyuiope+", "QWERTYUIOPE*",
      "asdfghjkloa",  "ASDFGHJKLOA",
      "zxcvbnm,.-",   "ZXCVBNM;:_",
      "\\<",          "|>"),
    L("pt", "Portuguese",
      "1234567890'<", "!\"#$%&/()=?`",
      "qwertyuiop'+", "QWERTYUIOP`*",
      "asdfghjklc~",  "ASDFGHJKLC^",
      "zxcvbnm,.-",   "ZXCVBNM;:_",
      "\\|",          "|!"),
    L("nl", "Dutch",
      "1234567890-=", "!@#$%^&*()_+",
      "qwertyuiop[]", "QWERTYUIOP{}",
      "asdfghjkl;'",  "ASDFGHJKL:\"",
      "zxcvbnm,./",   "ZXCVBNM<>?",
      "`\\",          "~|"),
    L("se", "Swedish",
      "1234567890+'", "!\"#$%&/()=?`",
      "qwertyuiopa'", "QWERTYUIOPA^",
      "asdfghjkloa",  "ASDFGHJKLOA",
      "zxcvbnm,.-",   "ZXCVBNM;:_",
      "<>",           "><"),
    L("no", "Norwegian",
      "1234567890+'", "!\"#$%&/()=?`",
      "qwertyuiopa'", "QWERTYUIOPA^",
      "asdfghjkloa",  "ASDFGHJKLOA",
      "zxcvbnm,.-",   "ZXCVBNM;:_",
      "<>",           "><"),
    L("dk", "Danish",
      "1234567890+'", "!\"#$%&/()=?`",
      "qwertyuiopa'", "QWERTYUIOPA^",
      "asdfghjkloa",  "ASDFGHJKLOA",
      "zxcvbnm,.-",   "ZXCVBNM;:_",
      "<>",           "><"),
    L("fi", "Finnish",
      "1234567890+'", "!\"#$%&/()=?`",
      "qwertyuiopa'", "QWERTYUIOPA^",
      "asdfghjkloa",  "ASDFGHJKLOA",
      "zxcvbnm,.-",   "ZXCVBNM;:_",
      "<>",           "><"),
    L("pl", "Polish",
      "1234567890-=", "!@#$%^&*()_+",
      "qwertyuiop[]", "QWERTYUIOP{}",
      "asdfghjkl;'",  "ASDFGHJKL:\"",
      "zxcvbnm,./",   "ZXCVBNM<>?",
      "`~",           "~~"),
    L("cz", "Czech (QWERTZ)",
      "1234567890-=", "!@#$%^&*()_+",
      "qwertzuiopu)", "QWERTZUIOPU(",
      "asdfghjklus",  "ASDFGHJKLUS",
      "yxcvbnm,.-",   "YXCVBNM;:_",
      "\\|",          "|!"),
    L("hu", "Hungarian (QWERTZ)",
      "1234567890o'", "!\"#$%&/()=?`",
      "qwertzuiopou", "QWERTZUIOPOU",
      "asdfghjkloa",  "ASDFGHJKLEA",
      "yxcvbnm,.-",   "YXCVBNM;:_",
      "0~",           "|!"),
    L("ro", "Romanian",
      "1234567890-=", "!@#$%^&*()_+",
      "qwertyuiopai", "QWERTYUIOPAI",
      "asdfghjklst",  "ASDFGHJKLST",
      "zxcvbnm,.-",   "ZXCVBNM;:_",
      "\\|",          "|!"),
    L("tr-q", "Turkish (Q)",
      "1234567890*-", "!'^+%&/()=?`",
      "qwertyuiopgu", "QWERTYUIOPGU",
      "asdfghjklsi",  "ASDFGHJKLSI",
      "zxcvbnmoc.",   "ZXCVBNMOC:",
      "<>",           "><"),
    L("tr-f", "Turkish (F)",
      "1234567890*-", "!'^+%&/()=?`",
      "fggiodrnhpqw", "FGGIODRNHPQW",
      "uieautkmlys",  "UIEAUTKMLYS",
      "cxvbnm,.-/",   "CXVBNM;:_?",
      "<>",           "><"),
    L("az", "Azerbaijani",
      "1234567890-=", "!@#$%^&*()_+",
      "quertyuiopog", "QUERTYUIOPOG",
      "asdfghjklis",  "ASDFGHJKLIS",
      "zxcvbnm,.-",   "ZXCVBNM;:_",
      "<>",           "><"),
    L("br", "Portuguese (Brazil)",
      "1234567890-=", "!@#$%^&*()_+",
      "qwertyuiop[]", "QWERTYUIOP{}",
      "asdfghjklc~",  "ASDFGHJKLC^",
      "zxcvbnm,.\\",  "ZXCVBNM<>?",
      "'\\",          "\"|"),
};

#undef L

static const int layout_count = (int)(sizeof(layouts) / sizeof(layouts[0]));

static int current_layout = 0; /* us */
static volatile u32 layout_generation;

static void queue_push(u16 key) {
    u16 next = (u16)((q_head + 1) & 0xFF);
    if (next == q_tail) return;   /* full */
    key_queue[q_head] = key;
    q_head = next;
}

static int queue_pop(void) {
    if (q_head == q_tail) return -1;
    u16 k = key_queue[q_tail];
    q_tail = (u16)((q_tail + 1) & 0xFF);
    return (int)k;
}

static void kbd_wait_write(void) {
    int guard = 100000;
    while ((inb(KBD_STATUS) & 0x02) && guard-- > 0) { }
}

static void kbd_wait_read(void) {
    int guard = 100000;
    while (!(inb(KBD_STATUS) & 0x01) && guard-- > 0) { }
}

/* Map a set-1 scancode through a layout row table. */
static char layout_char(const kbd_layout_t *l, u8 code, bool shift) {
    const char *row = NULL;
    int idx = -1;

    if (code == 0x39) return ' ';
    if (code >= 0x02 && code <= 0x0D) {
        row = shift ? l->digits_s : l->digits;
        idx = code - 0x02;
    } else if (code >= 0x10 && code <= 0x1B) {
        row = shift ? l->row1_s : l->row1;
        idx = code - 0x10;
    } else if (code >= 0x1E && code <= 0x28) {
        row = shift ? l->row2_s : l->row2;
        idx = code - 0x1E;
    } else if (code >= 0x2C && code <= 0x35) {
        row = shift ? l->row3_s : l->row3;
        idx = code - 0x2C;
    } else if (code == 0x29 || code == 0x2B) {
        row = shift ? l->extra_s : l->extra;
        idx = (code == 0x29) ? 0 : 1;
    }
    if (!row || idx < 0) return 0;
    return row[idx];
}

static int kbd_translate(u8 code) {
    int key = -1;

    if (e1_skip > 0) { e1_skip--; return -1; }   /* Pause key 2-byte tail */
    if (code == 0xE1) { e1_skip = 2; return -1; }

    if (extended) {
        extended = false;
        if (code & 0x80) {
            u8 rel = (u8)(code & 0x7F);
            if (rel == 0x1D) ctrl_down = false;
            if (rel == 0x38) alt_down = false;
            if (rel == 0x5B || rel == 0x5C) super_down = false;
            return -1;
        }
        switch (code) {
            case 0x48: key = KEY_UP; break;
            case 0x50: key = KEY_DOWN; break;
            case 0x4B: key = KEY_LEFT; break;
            case 0x4D: key = KEY_RIGHT; break;
            case 0x47: key = KEY_HOME; break;
            case 0x4F: key = KEY_END; break;
            case 0x53: key = KEY_DELETE; break;
            case 0x49: key = KEY_PGUP; break;
            case 0x51: key = KEY_PGDN; break;
            case 0x1C: key = KEY_ENTER; break;
            case 0x1D: ctrl_down = true; return -1;
            case 0x38: alt_down = true; return -1;
            case 0x37: key = KEY_PRTSC; break;
            case 0x5B: case 0x5C: super_down = true; return KEY_SUPER;
            default: return -1;
        }
        return key;
    }

    if (code == 0xE0) { extended = true; return -1; }
    if (code & 0x80) {
        u8 rel = (u8)(code & 0x7F);
        if (rel == 0x2A || rel == 0x36) shift_down = false;
        if (rel == 0x1D) ctrl_down = false;
        if (rel == 0x38) alt_down = false;
        return -1;
    }

    switch (code) {
        case 0x01: return KEY_ESC;
        case 0x0E: return KEY_BACKSPACE;
        case 0x0F: return KEY_TAB;
        case 0x1C: return KEY_ENTER;
        case 0x2A: case 0x36:
            /* Alt+Shift cycles the keyboard layout (Windows style). Only the
             * initial press counts, not typematic repeats. */
            if (!shift_down && alt_down) kbd_cycle_layout(1);
            shift_down = true;
            return -1;
        case 0x1D: ctrl_down = true; return -1;
        case 0x38: alt_down = true; return -1;
        case 0x3A: caps_lock = !caps_lock; return -1;
        case 0x3B: return KEY_F1;
        case 0x3C: return KEY_F2;
        default: break;
    }

    const kbd_layout_t *lay = &layouts[current_layout];
    char c = layout_char(lay, code, shift_down);
    if (!c) return -1;
    if (caps_lock && c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (caps_lock && c >= 'A' && c <= 'Z' && !shift_down) c = (char)(c + 32);
    if (ctrl_down) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            c = (char)((toupper((unsigned char)c) - 'A') + 1);
        }
    }
    return (int)(unsigned char)c;
}

static void keyboard_irq(registers_t *r) {
    UNUSED(r);
    irq_total++;
    while (inb(KBD_STATUS) & 0x01) {
        u8 code = inb(KBD_DATA);
        int key = kbd_translate(code);
        if (key >= 0) queue_push((u16)key);
    }
}

void keyboard_init(void) {
    q_head = q_tail = 0;
    shift_down = ctrl_down = caps_lock = extended = alt_down = super_down = false;
    e1_skip = 0;
    irq_total = 0;

    kbd_wait_write();
    outb(KBD_CMD, 0xAD);                 /* disable keyboard */
    kbd_wait_write();
    outb(KBD_CMD, 0xA7);                 /* disable auxiliary (mouse) */

    while (inb(KBD_STATUS) & 0x01) (void)inb(KBD_DATA);

    kbd_wait_write();
    outb(KBD_CMD, 0x20);                 /* read controller config */
    kbd_wait_read();
    u8 config = inb(KBD_DATA);
    config |= 0x01;                      /* keyboard IRQ1 */
    config |= 0x02;                      /* mouse IRQ12 */
    config |= 0x40;                      /* set-1 translation */
    config &= (u8)~0x10;                 /* keyboard clock enabled */
    kbd_wait_write();
    outb(KBD_CMD, 0x60);
    kbd_wait_write();
    outb(KBD_DATA, config);

    kbd_wait_write();
    outb(KBD_CMD, 0xAE);                 /* enable keyboard */
    isr_install_handler(1, keyboard_irq);
    pic_mask_irq(1, false);
    pic_mask_irq(2, false);
    initialized = true;
}

int kbd_getchar(void) {
    u32 flags = irq_save();
    int k = queue_pop();
    irq_restore(flags);
    return k;
}

bool kbd_has_key(void) {
    return q_head != q_tail;
}

void kbd_flush(void) {
    u32 flags = irq_save();
    q_tail = q_head;
    /* Also drain the 8042 output buffer: an Enter from a previous prompt can
     * sit here and be delivered to the next reader after the queue is cleared. */
    for (int i = 0; i < 32; i++) {
        if (!(inb(KBD_STATUS) & 0x01)) break;
        (void)inb(KBD_DATA);
    }
    irq_restore(flags);
}

u32 kbd_irq_count(void) { return irq_total; }

int kbd_wait_key(u32 timeout_ms) {
    u32 start = pit_ticks();
    for (;;) {
        int c = kbd_getchar();
        if (c >= 0) return c;
        if ((pit_ticks() - start) * 10 >= timeout_ms) return -1;
        __asm__ volatile("sti; hlt");
    }
}

bool keyboard_selftest(char *err, int errlen) {
    if (!initialized) {
        ksnprintf(err, errlen, "keyboard controller not initialised");
        return false;
    }
    u8 status = inb(KBD_STATUS);
    if (status == 0xFF || status == 0x00) {
        ksnprintf(err, errlen, "keyboard controller absent (status=%x)", status);
        return false;
    }
    for (int i = 0; i < layout_count; i++) {
        const kbd_layout_t *l = &layouts[i];
        if (strlen(l->digits) != 12 || strlen(l->digits_s) != 12 ||
            strlen(l->row1) != 12 || strlen(l->row1_s) != 12 ||
            strlen(l->row2) != 11 || strlen(l->row2_s) != 11 ||
            strlen(l->row3) != 10 || strlen(l->row3_s) != 10 ||
            strlen(l->extra) != 2 || strlen(l->extra_s) != 2) {
            ksnprintf(err, errlen, "layout '%s' has a malformed row table", l->name);
            return false;
        }
    }
    if (layout_char(&layouts[0], 0x1E, false) != 'a' ||
        layout_char(&layouts[0], 0x39, false) != ' ') {
        ksnprintf(err, errlen, "scancode translation table corrupt");
        return false;
    }
    if ((pic_get_mask() & 0x0002) != 0) {
        ksnprintf(err, errlen, "IRQ1 still masked");
        return false;
    }
    return true;
}

bool kbd_set_layout(const char *name) {
    if (!name) return false;
    for (int i = 0; i < layout_count; i++) {
        if (strcasecmp(layouts[i].name, name) == 0) {
            if (i != current_layout) {
                current_layout = i;
                layout_generation++;
            }
            return true;
        }
    }
    return false;
}

const char *kbd_layout_name(void) { return layouts[current_layout].name; }
const char *kbd_layout_label(void) { return layouts[current_layout].label; }
int kbd_layout_index(void) { return current_layout; }
int kbd_layout_count(void) { return layout_count; }
const char *kbd_layout_name_at(int i) {
    if (i < 0 || i >= layout_count) return NULL;
    return layouts[i].name;
}
const char *kbd_layout_label_at(int i) {
    if (i < 0 || i >= layout_count) return NULL;
    return layouts[i].label;
}
u32 kbd_layout_generation(void) { return layout_generation; }

void kbd_cycle_layout(int dir) {
    if (layout_count <= 0) return;
    current_layout = (current_layout + (dir < 0 ? layout_count - 1 : 1)) % layout_count;
    layout_generation++;
}
bool kbd_alt_down(void) { return alt_down; }
bool kbd_ctrl_down(void) { return ctrl_down; }
bool kbd_shift_down(void) { return shift_down; }
bool kbd_super_down(void) { return super_down; }
