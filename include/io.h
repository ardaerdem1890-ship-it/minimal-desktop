#pragma once
#include "types.h"

static inline void outb(u16 port, u8 val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(u16 port, u16 val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(u16 port, u32 val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline u16 inw(u16 port) {
    u16 r;
    __asm__ volatile("inw %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline u32 inl(u16 port) {
    u32 r;
    __asm__ volatile("inl %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }
static inline void nop(void) { __asm__ volatile("nop"); }

static inline u32 irq_save(void) {
    u32 flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}
static inline void irq_restore(u32 flags) {
    if (flags & 0x200u) __asm__ volatile("sti" ::: "memory");
}

static inline u32 read_cr0(void) {
    u32 v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v;
}
static inline u32 read_cr2(void) {
    u32 v; __asm__ volatile("mov %%cr2, %0" : "=r"(v)); return v;
}
static inline u32 read_cr3(void) {
    u32 v; __asm__ volatile("mov %%cr3, %0" : "=r"(v)); return v;
}
static inline u32 read_cr4(void) {
    u32 v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v;
}
static inline void write_cr0(u32 v) { __asm__ volatile("mov %0, %%cr0" : : "r"(v)); }
static inline void write_cr3(u32 v) { __asm__ volatile("mov %0, %%cr3" : : "r"(v)); }
static inline void write_cr4(u32 v) { __asm__ volatile("mov %0, %%cr4" : : "r"(v)); }

static inline u64 rdtsc(void) {
    u32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static inline void cpu_relax(void) { __asm__ volatile("pause"); }
