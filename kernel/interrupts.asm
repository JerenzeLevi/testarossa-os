; Interrupt entry stubs. Each vector pushes a dummy error code when the CPU
; does not, then the vector number, then jumps to one shared handler that
; saves all registers and calls isr_dispatch(regs_t *) in C.

section .text

extern isr_dispatch

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0                    ; fake error code so the frame layout is uniform
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1                   ; CPU already pushed the real error code
    jmp isr_common
%endmacro

; CPU exceptions 0-31. Vectors 8, 10-14, 17, and 30 push an error code.
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

; Hardware IRQs 0-15, remapped by the PIC to vectors 32-47.
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

isr_common:
    pusha                           ; eax..edi
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10                    ; kernel data segments for the C handler
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                        ; regs_t *, the frame we just built
    call isr_dispatch
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8                      ; drop vector number and error code
    iret

; Table of stub addresses, indexed by vector number. Read by idt.c.
section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 48
    dd isr%+i
%assign i i+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
