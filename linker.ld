ENTRY(_start)

SECTIONS
{
    /* Load the kernel at 1 MiB, the conventional home for a higher-half-free kernel. */
    . = 1M;

    .multiboot : { KEEP(*(.multiboot)) }
    .text      : { *(.text*) }
    .rodata    : { *(.rodata*) }
    .data      : { *(.data*) }
    .bss       : {
        *(COMMON)
        *(.bss*)
    }
}
