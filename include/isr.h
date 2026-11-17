#pragma once
#include "types.h"

typedef struct PACKED {
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags;
    u32 useresp, ss;
} registers_t;

typedef void (*irq_handler_t)(registers_t *);

void isr_init(void);
void isr_install_handler(u8 irq, irq_handler_t fn);
void isr_uninstall_handler(u8 irq);
void isr_handler_c(registers_t *r);
const char *exception_name(u32 n);
u32 isr_exception_count(void);
u32 isr_irq_count(u8 irq);
