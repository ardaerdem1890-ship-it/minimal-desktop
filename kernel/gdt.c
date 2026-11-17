/* Minimal Desktop - GDT and 8259 PIC */
#include "gdt.h"
#include "io.h"
#include "string.h"
#include "console.h"

static gdt_entry_t gdt[5];
static gdt_ptr_t   gp;

extern void gdt_flush(u32 ptr);

static void gdt_set_gate(int n, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[n].base_low    = (u16)(base & 0xFFFF);
    gdt[n].base_middle = (u8)((base >> 16) & 0xFF);
    gdt[n].base_high   = (u8)((base >> 24) & 0xFF);
    gdt[n].limit_low   = (u16)(limit & 0xFFFF);
    gdt[n].granularity = (u8)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[n].access      = access;
}

void gdt_init(void) {
    gp.limit = (u16)(sizeof(gdt) - 1);
    gp.base  = (u32)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                  /* null */
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);      /* ring 0 code, 4K gran, 32-bit */
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);      /* ring 0 data */
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);      /* ring 3 code (reserved) */
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);      /* ring 3 data (reserved) */

    gdt_flush((u32)&gp);
}

bool gdt_selftest(char *err, int errlen) {
    gdt_ptr_t p;
    u16 cs, ds, ss;

    __asm__ volatile("sgdt %0" : "=m"(p));
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));

    if (p.base != (u32)&gdt) {
        ksnprintf(err, errlen, "GDTR base %p != %p", (void *)p.base, (void *)&gdt);
        return false;
    }
    if (p.limit != sizeof(gdt) - 1) {
        ksnprintf(err, errlen, "GDTR limit %u != %u", p.limit, (u32)(sizeof(gdt) - 1));
        return false;
    }
    if (cs != 0x08 || ds != 0x10 || ss != 0x10) {
        ksnprintf(err, errlen, "segment selectors cs=%x ds=%x ss=%x", cs, ds, ss);
        return false;
    }
    if ((gdt[1].access & 0xFE) != 0x9A || (gdt[2].access & 0xFE) != 0x92) {
        ksnprintf(err, errlen, "GDT descriptor access bits corrupt (1=%x 2=%x)",
                  gdt[1].access, gdt[2].access);
        return false;
    }
    return true;
}

void gdt_dump(void) {
    kprintf("GDT  base=%p limit=0x%X entries=%u\n", (void *)(u32)&gdt, gp.limit, 5u);
    kprintf("  0 null   1 0x08 code32 r0   2 0x10 data32 r0   3 0x18 code32 r3   4 0x20 data32 r3\n");
}

void pic_remap(void) {
    u8 mask_master = inb(0x21);
    u8 mask_slave  = inb(0xA1);

    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    /* Start with everything masked; drivers unmask their own lines. */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    (void)mask_master;
    (void)mask_slave;
}

void pic_eoi(u8 irq) {
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void pic_mask_irq(u8 irq, bool masked) {
    u16 port = (irq < 8) ? 0x21 : 0xA1;
    u8 bit = (u8)(irq & 7);
    u8 value = inb(port);
    if (masked) value |= (u8)(1u << bit);
    else value &= (u8)~(1u << bit);
    outb(port, value);
}

u16 pic_get_mask(void) {
    return (u16)((u16)inb(0x21) | ((u16)inb(0xA1) << 8));
}
