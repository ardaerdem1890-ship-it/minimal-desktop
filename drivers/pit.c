/* Minimal Desktop - Programmable Interval Timer (PIT), 100 Hz system tick */
#include "pit.h"
#include "io.h"
#include "isr.h"
#include "gdt.h"
#include "task.h"
#include "console.h"

#define PIT_INPUT_HZ 1193182u

static volatile u32 ticks;
static volatile u64 uptime_ms;
static volatile u32 seconds;
static u32 frequency = PIT_FREQUENCY;

static void pit_irq(registers_t *r) {
    UNUSED(r);
    ticks++;
    uptime_ms += 1000u / PIT_FREQUENCY;
    if ((ticks % PIT_FREQUENCY) == 0) seconds++;
    task_tick();
    /* Network receive is polled from the stack; no work in IRQ context. */
}

void pit_init(void) {
    u32 divisor = PIT_INPUT_HZ / PIT_FREQUENCY;
    ticks = 0;
    uptime_ms = 0;
    seconds = 0;

    outb(0x43, 0x36);                       /* channel 0, lo/hi, mode 3, binary */
    outb(0x40, (u8)(divisor & 0xFF));
    outb(0x40, (u8)((divisor >> 8) & 0xFF));

    isr_install_handler(0, pit_irq);
    pic_mask_irq(0, false);
    pic_mask_irq(2, false);
    sti();
}

u32 pit_ticks(void) { return ticks; }
u64 pit_uptime_ms(void) { return uptime_ms; }
u32 pit_seconds(void) { return seconds; }
u32 pit_get_frequency(void) { return frequency; }

void pit_sleep_ticks(u32 count) {
    u32 target = ticks + count;
    while ((s32)(ticks - target) < 0) {
        __asm__ volatile("sti; hlt");
    }
}

void pit_sleep_ms(u32 ms) {
    u32 count = (ms * PIT_FREQUENCY + 999) / 1000;
    if (count == 0) count = 1;
    pit_sleep_ticks(count);
}

bool pit_selftest(char *err, int errlen) {
    u32 t0 = ticks;
    u64 u0 = uptime_ms;
    pit_sleep_ms(60);
    u32 delta = ticks - t0;
    if (delta < 2) {
        ksnprintf(err, errlen, "tick did not advance (delta=%u)", delta);
        return false;
    }
    if (uptime_ms < u0 + 20) {
        ksnprintf(err, errlen, "uptime did not advance (%u ms)", (u32)(uptime_ms - u0));
        return false;
    }
    if (frequency != PIT_FREQUENCY) {
        ksnprintf(err, errlen, "frequency %u != %u", frequency, PIT_FREQUENCY);
        return false;
    }
    return true;
}
