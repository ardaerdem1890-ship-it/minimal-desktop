/* Minimal Desktop - IDT, exception handling and IRQ dispatch */
#include "isr.h"
#include "gdt.h"
#include "console.h"
#include "io.h"
#include "string.h"
#include "vga_hw.h"
#include "gfx.h"
#include "keyboard.h"
#include "pit.h"

extern void idt_load(u32 ptr);

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void isr_default(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;
static irq_handler_t irq_handlers[16];
static u32 irq_counts[16];
static u32 exception_count;
static bool idt_ready;

static const char *exception_names[32] = {
    "Divide-by-zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound range", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment", "Invalid TSS", "Segment not present",
    "Stack fault", "General protection fault", "Page fault", "Reserved",
    "x87 FP error", "Alignment check", "Machine check", "SIMD FP error",
    "Virtualization", "Control protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor", "VMM communication", "Security", "Reserved"
};

void idt_set_gate(u8 n, u32 handler, u16 selector, u8 flags) {
    idt[n].offset_low  = (u16)(handler & 0xFFFF);
    idt[n].selector    = selector;
    idt[n].zero        = 0;
    idt[n].type_attr   = flags;
    idt[n].offset_high = (u16)((handler >> 16) & 0xFFFF);
}

void idt_init(void) {
    static void (*const exception_stubs[32])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    static void (*const irq_stubs[16])(void) = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
    };

    memset(idt, 0, sizeof(idt));
    for (int i = 0; i < 256; i++)
        idt_set_gate((u8)i, (u32)isr_default, 0x08, 0x8E);
    for (int i = 0; i < 32; i++)
        idt_set_gate((u8)i, (u32)exception_stubs[i], 0x08, 0x8E);
    for (int i = 0; i < 16; i++)
        idt_set_gate((u8)(32 + i), (u32)irq_stubs[i], 0x08, 0x8E);

    idt_ptr.limit = (u16)(sizeof(idt) - 1);
    idt_ptr.base  = (u32)&idt;
    idt_load((u32)&idt_ptr);
    idt_ready = true;
}

void isr_init(void) {
    memset(irq_handlers, 0, sizeof(irq_handlers));
    memset(irq_counts, 0, sizeof(irq_counts));
    exception_count = 0;
}

void isr_install_handler(u8 irq, irq_handler_t fn) {
    if (irq < 16) irq_handlers[irq] = fn;
}

void isr_uninstall_handler(u8 irq) {
    if (irq < 16) irq_handlers[irq] = 0;
}

const char *exception_name(u32 n) {
    return (n < 32) ? exception_names[n] : "Unknown";
}

u32 isr_exception_count(void) { return exception_count; }
u32 isr_irq_count(u8 irq) { return irq < 16 ? irq_counts[irq] : 0; }

/* ------------------------------------------------------------------------- */
/* crash screen                                                              */
/* ------------------------------------------------------------------------- */
static void crash_delay(void) {
    for (int i = 0; i < 2000; i++) {
        __asm__ volatile("outb %%al, $0x80" : : "a"(0));
    }
}

static NORETURN void crash_screen(registers_t *r) {
    cli();

    /* A fault in mode 13h leaves the text console invisible and the DAC full
     * of graphics colours, which looked like random garble. Restore both. */
    gfx_recover_text();
    console_clear();
    for (int row = 0; row < 25; row++)
        for (int col = 0; col < 80; col++)
            console_put_at(row, col, ' ', console_make_attr(VGA_LIGHT_GREY, VGA_BLUE));

    /* header */
    for (int c = 0; c < 80; c++)
        console_put_at(0, c, ' ', console_make_attr(VGA_WHITE, VGA_RED));
    console_write_at(0, 2, "  S Y S T E M   F A U L T  ",
                     console_make_attr(VGA_WHITE, VGA_RED));
    console_write_at(0, 60, "Minimal Desktop", console_make_attr(VGA_WHITE, VGA_RED));

    char line[81];
    ksnprintf(line, sizeof(line), "  Exception %u: %s", r->int_no,
              exception_name(r->int_no));
    console_write_at(2, 2, line, console_make_attr(VGA_YELLOW, VGA_BLUE));

    ksnprintf(line, sizeof(line), "  Error code : 0x%08X", r->err_code);
    console_write_at(3, 2, line, console_make_attr(VGA_LIGHT_GREY, VGA_BLUE));
    ksnprintf(line, sizeof(line), "  EIP        : 0x%08X   CS 0x%04X  EFLAGS 0x%08X",
              r->eip, r->cs, r->eflags);
    console_write_at(4, 2, line, console_make_attr(VGA_WHITE, VGA_BLUE));
    ksnprintf(line, sizeof(line), "  EAX 0x%08X  EBX 0x%08X  ECX 0x%08X  EDX 0x%08X",
              r->eax, r->ebx, r->ecx, r->edx);
    console_write_at(6, 2, line, console_make_attr(VGA_LIGHT_CYAN, VGA_BLUE));
    ksnprintf(line, sizeof(line), "  ESI 0x%08X  EDI 0x%08X  EBP 0x%08X  ESP 0x%08X",
              r->esi, r->edi, r->ebp, r->useresp);
    console_write_at(7, 2, line, console_make_attr(VGA_LIGHT_CYAN, VGA_BLUE));
    ksnprintf(line, sizeof(line), "  DS 0x%04X  ES 0x%04X  FS 0x%04X  GS 0x%04X",
              r->ds, r->es, r->fs, r->gs);
    console_write_at(8, 2, line, console_make_attr(VGA_LIGHT_CYAN, VGA_BLUE));

    if (r->int_no == 14) {
        ksnprintf(line, sizeof(line), "  Page fault address (CR2): 0x%08X", read_cr2());
        console_write_at(10, 2, line, console_make_attr(VGA_LIGHT_RED, VGA_BLUE));
    }
    ksnprintf(line, sizeof(line), "  Fault #%u since boot", exception_count);
    console_write_at(11, 2, line, console_make_attr(VGA_DARK_GREY, VGA_BLUE));

    console_write_at(14, 2, "  The system will restart in 5 seconds.",
                     console_make_attr(VGA_WHITE, VGA_BLUE));
    console_write_at(15, 2, "  Press any key to stay on this screen (for a photo).",
                     console_make_attr(VGA_LIGHT_GREY, VGA_BLUE));

    serial_puts("[crash] exception ");
    serial_puts(exception_name(r->int_no));
    serial_puts("\n");

    /* Count down with the PIT so the delay is a real five seconds. Interrupts
     * have to be on for ticks to advance; the keyboard IRQ feeds the same
     * buffer we poll here. */
    bool stay = false;
    sti();
    u32 started = (u32)pit_uptime_ms();
    while ((u32)(pit_uptime_ms() - started) < 5000) {
        if (kbd_has_key()) { kbd_flush(); stay = true; break; }
        hlt();
    }
    cli();

    if (stay) {
        console_write_at(17, 2, "  Halted. Press Ctrl+Alt+Del or power-cycle to restart.",
                         console_make_attr(VGA_YELLOW, VGA_BLUE));
        for (;;) hlt();
    }

    console_write_at(17, 2, "  Rebooting...", console_make_attr(VGA_WHITE, VGA_BLUE));
    /* Three reset paths, in order of how well they work across machines:
     *   1. i8042 pulse reset line (classic PS/2 controller)
     *   2. chipset reset control register 0xCF9
     *   3. load an empty IDT and raise an interrupt -> triple fault
     */
    for (int i = 0; i < 16; i++) {
        while (inb(0x64) & 0x02) { }
        outb(0x64, 0xFE);
        crash_delay();
    }
    outb(0xCF9, 0x02);
    crash_delay();
    outb(0xCF9, 0x06);
    crash_delay();
    for (;;) {
        __asm__ volatile("lidt (%0)" : : "r"((u16 *)0));
    }
}

void isr_handler_c(registers_t *r) {
    if (r->int_no < 32) {
        exception_count++;
        if (exception_count > 4) {
            /* Fault loop: stop trying to print, just reset the machine. */
            for (int i = 0; i < 32; i++) {
                while (inb(0x64) & 0x02) { }
                outb(0x64, 0xFE);
                crash_delay();
            }
            __asm__ volatile("lidt (%0)" : : "r"((u16 *)0));
            for (;;) hlt();
        }
        crash_screen(r);
    }

    if (r->int_no >= 32 && r->int_no < 48) {
        u8 irq = (u8)(r->int_no - 32);
        irq_counts[irq]++;
        if (irq_handlers[irq]) irq_handlers[irq](r);
        pic_eoi(irq);
    }
}

bool idt_selftest(char *err, int errlen) {
    idt_ptr_t p;
    __asm__ volatile("sidt %0" : "=m"(p));
    if (!idt_ready) {
        ksnprintf(err, errlen, "IDT not initialised");
        return false;
    }
    if (p.base != (u32)&idt) {
        ksnprintf(err, errlen, "IDTR base %p != %p", (void *)p.base, (void *)&idt);
        return false;
    }
    if (p.limit != sizeof(idt) - 1) {
        ksnprintf(err, errlen, "IDTR limit %u != %u", p.limit, (u32)(sizeof(idt) - 1));
        return false;
    }
    if (idt[0x20].offset_low == 0 && idt[0x20].offset_high == 0) {
        ksnprintf(err, errlen, "IRQ0 gate missing");
        return false;
    }
    if (idt[0x21].type_attr != 0x8E) {
        ksnprintf(err, errlen, "keyboard gate type %x", idt[0x21].type_attr);
        return false;
    }
    u16 mask = pic_get_mask();
    if ((mask & 0x1007) != 0) {
        ksnprintf(err, errlen, "PIC mask %x: IRQ0/1/2/12 not enabled", mask);
        return false;
    }
    return true;
}

void idt_dump(void) {
    kprintf("IDT  base=%p limit=0x%X gates=256\n", (void *)(u32)&idt, idt_ptr.limit);
    kprintf("  IRQ0=PIT(32) IRQ1=keyboard(33) IRQ2=cascade(34) IRQ12=mouse(44)\n");
}
