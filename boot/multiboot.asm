; Multiboot entry point. QEMU's -kernel option loads any multiboot ELF
; directly, so we don't need our own bootloader for the first milestone.

MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x00000003          ; align modules on pages, provide memory map
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384                      ; 16 KiB kernel stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top              ; set up the stack before any C code runs
    push ebx                        ; multiboot info pointer, arg for kernel_main
    push eax                        ; multiboot magic, arg for kernel_main
    call kernel_main

.halt:
    cli
    hlt
    jmp .halt

section .note.GNU-stack noalloc noexec nowrite progbits
