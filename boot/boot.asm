; Minimal Desktop - Multiboot2 entry stub and descriptor loaders
; Assembled with NASM, 32-bit protected mode.

BITS 32

MB2_MAGIC       equ 0xE85250D6
MB2_ARCH_I386   equ 0

section .multiboot
align 8
multiboot_header:
    dd MB2_MAGIC
    dd MB2_ARCH_I386
    dd multiboot_header_end - multiboot_header
    dd -(MB2_MAGIC + MB2_ARCH_I386 + (multiboot_header_end - multiboot_header))

    ; Information request tag: memory map + cmdline are mandatory, but asking
    ; explicitly documents what the kernel consumes.
    align 8
    dw 1                        ; tag type: information request
    dw 0                        ; flags
    dd 20                       ; size
    dd 6                        ; request: memory map
    dd 1                        ; request: command line

    ; Module alignment tag (modules 4 KiB aligned)
    align 8
    dw 6
    dw 0
    dd 8

    ; End tag
    align 8
    dw 0
    dw 0
    dd 8
multiboot_header_end:

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top
    ; GRUB passes magic in EAX and the Multiboot2 information pointer in EBX.
    push ebx
    push eax
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang

; ---------------------------------------------------------------------------
; void gdt_flush(u32 gdt_ptr_address)
; ---------------------------------------------------------------------------
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.reload
.reload:
    ret

; ---------------------------------------------------------------------------
; void idt_load(u32 idt_ptr_address)
; ---------------------------------------------------------------------------
global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; ---------------------------------------------------------------------------
; void tss_flush(void) - not used yet, reserved for ring-3 work
; ---------------------------------------------------------------------------
global stack_pointer
stack_pointer:
    dd 0
