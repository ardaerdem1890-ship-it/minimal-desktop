#pragma once
#include "types.h"

typedef struct PACKED {
    u16 limit_low;
    u16 base_low;
    u8  base_middle;
    u8  access;
    u8  granularity;
    u8  base_high;
} gdt_entry_t;

typedef struct PACKED {
    u16 limit;
    u32 base;
} gdt_ptr_t;

typedef struct PACKED {
    u16 offset_low;
    u16 selector;
    u8  zero;
    u8  type_attr;
    u16 offset_high;
} idt_entry_t;

typedef struct PACKED {
    u16 limit;
    u32 base;
} idt_ptr_t;

void gdt_init(void);
void idt_init(void);
void pic_remap(void);
void pic_eoi(u8 irq);
void pic_mask_irq(u8 irq, bool masked);
u16  pic_get_mask(void);

bool gdt_selftest(char *err, int errlen);
bool idt_selftest(char *err, int errlen);
void gdt_dump(void);
void idt_dump(void);

/* IDT gate installation helpers shared with the IRQ layer */
void idt_set_gate(u8 n, u32 handler, u16 selector, u8 flags);
