; Small CPU-table loaders called from C. The C side passes the address of
; a packed descriptor pointer (limit + base) as the only argument.

section .text

global gdt_flush
global idt_flush

gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10                    ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:gdt_reload             ; far jump reloads CS with the kernel code selector
gdt_reload:
    ret

idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
