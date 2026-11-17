/* Minimal Desktop - kernel link script (32-bit, loaded at 1 MiB) */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(8) :
    {
        KEEP(*(.multiboot))
    }

    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(16) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
        . = ALIGN(16);
    }

    kernel_end = .;
    kernel_start = 1M;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.gcc_except_table*)
    }
}
