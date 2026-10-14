#ifndef TESTAROSSA_IDT_H
#define TESTAROSSA_IDT_H

#include <stdint.h>

/* Register frame built by isr_common in kernel/interrupts.asm.
 * Field order must match the push order exactly. */
typedef struct regs {
    uint32_t gs, fs, es, ds;                    /* pushed last, so lowest address */
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;   /* from pusha */
    uint32_t int_no, err_code;                  /* pushed by our stub */
    uint32_t eip, cs, eflags;                   /* pushed by the CPU */
} regs_t;

void idt_init(void);

#endif
