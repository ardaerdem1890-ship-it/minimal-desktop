/* Minimal Desktop - PS/2 mouse (IRQ12), three-byte packet decoder */
#include "mouse.h"
#include "io.h"
#include "isr.h"
#include "gdt.h"
#include "console.h"
#include "string.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

static mouse_state_t state;
static u8 packet[3];
static int packet_index;
static u32 packet_count;
static bool initialized;
static int bound_w = 320, bound_h = 200;

static void ps2_wait_write(void) {
    int guard = 100000;
    while ((inb(PS2_STATUS) & 0x02) && guard-- > 0) { }
}

static void ps2_wait_read(void) {
    int guard = 100000;
    while (!(inb(PS2_STATUS) & 0x01) && guard-- > 0) { }
}

static bool mouse_write(u8 value) {
    ps2_wait_write();
    outb(PS2_CMD, 0xD4);
    ps2_wait_write();
    outb(PS2_DATA, value);
    ps2_wait_read();
    u8 ack = inb(PS2_DATA);
    return ack == 0xFA;
}

static void mouse_irq(registers_t *r) {
    UNUSED(r);
    while ((inb(PS2_STATUS) & 0x21) == 0x21) {
        u8 data = inb(PS2_DATA);
        if (packet_index == 0 && !(data & 0x08)) continue;   /* resync */
        packet[packet_index++] = data;
        if (packet_index < 3) continue;
        packet_index = 0;

        if (packet[0] & 0xC0) continue;     /* overflow bits */
        int dx = packet[1];
        int dy = packet[2];
        if (packet[0] & 0x10) dx |= ~0xFF;
        if (packet[0] & 0x20) dy |= ~0xFF;

        state.x += dx;
        state.y -= dy;
        if (state.x < 0) state.x = 0;
        if (state.y < 0) state.y = 0;
        if (state.x >= bound_w) state.x = bound_w - 1;
        if (state.y >= bound_h) state.y = bound_h - 1;
        state.buttons = (u8)(packet[0] & 0x07);
        state.irq_seen = true;
        packet_count++;
    }
}

void mouse_init(void) {
    initialized = false;
    state.present = false;
    state.irq_seen = false;
    state.buttons = 0;
    state.x = bound_w / 2;
    state.y = bound_h / 2;
    packet_index = 0;
    packet_count = 0;

    ps2_wait_write();
    outb(PS2_CMD, 0xA8);                    /* enable auxiliary port */

    ps2_wait_write();
    outb(PS2_CMD, 0x20);                    /* controller config */
    ps2_wait_read();
    u8 config = inb(PS2_DATA);
    config |= 0x02;                         /* IRQ12 */
    config &= (u8)~0x20;                    /* mouse clock enabled */
    ps2_wait_write();
    outb(PS2_CMD, 0x60);
    ps2_wait_write();
    outb(PS2_DATA, config);

    /* Mouse defaults, then enable data reporting. */
    bool ack1 = mouse_write(0xF6);
    bool ack2 = mouse_write(0xF4);

    isr_install_handler(12, mouse_irq);
    pic_mask_irq(12, false);
    pic_mask_irq(2, false);
    state.present = ack1 && ack2;
    initialized = true;
}

void mouse_get_state(mouse_state_t *st) {
    u32 flags = irq_save();
    *st = state;
    irq_restore(flags);
}

void mouse_set_bounds(int w, int h) {
    bound_w = w > 0 ? w : 320;
    bound_h = h > 0 ? h : 200;
    if (state.x >= bound_w) state.x = bound_w - 1;
    if (state.y >= bound_h) state.y = bound_h - 1;
}

u32 mouse_packets(void) { return packet_count; }

bool mouse_selftest(char *err, int errlen) {
    if (!initialized) {
        ksnprintf(err, errlen, "PS/2 auxiliary port not initialised");
        return false;
    }
    if (!state.present) {
        ksnprintf(err, errlen, "mouse did not acknowledge F6/F4");
        return false;
    }
    if ((pic_get_mask() & 0x1000) != 0) {
        ksnprintf(err, errlen, "IRQ12 still masked");
        return false;
    }
    if (state.x < 0 || state.x >= bound_w || state.y < 0 || state.y >= bound_h) {
        ksnprintf(err, errlen, "cursor outside bounds (%d,%d)", state.x, state.y);
        return false;
    }
    return true;
}
