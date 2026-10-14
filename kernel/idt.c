#include <stdint.h>
#include "idt.h"
#include "gdt.h"
#include "log.h"
#include "pic.h"
#include "keyboard.h"
#include "timer.h"
#include "vga.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES      256
#define IRQ_BASE         32       /* vector number of IRQ0 after PIC remap */
#define IRQ_COUNT        16
#define GATE_INTERRUPT   0x8E     /* present, ring 0, 32-bit interrupt gate (clears IF) */

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

/* Stub addresses from kernel/interrupts.asm, one per vector 0-47. */
extern uint32_t isr_stub_table[];
/* Defined in kernel/cpu.asm. */
extern void idt_flush(uint32_t idt_ptr_addr);

static const char *exception_names[32] = {
    "divide error", "debug", "NMI", "breakpoint", "overflow", "bound range",
    "invalid opcode", "device not available", "double fault", "coprocessor overrun",
    "invalid TSS", "segment not present", "stack fault", "general protection",
    "page fault", "reserved", "x87 floating point", "alignment check",
    "machine check", "SIMD floating point", "virtualization", "control protection",
    "reserved", "reserved", "reserved", "reserved", "reserved", "reserved",
    "hypervisor injection", "VMM communication", "security", "reserved"
};

static void idt_set_gate(uint8_t vector, uint32_t handler) {
    idt[vector].offset_low  = handler & 0xFFFF;
    idt[vector].selector    = GDT_KERNEL_CODE;
    idt[vector].zero        = 0;
    idt[vector].type_attr   = GATE_INTERRUPT;
    idt[vector].offset_high = (handler >> 16) & 0xFFFF;
}

void idt_init(void) {
    for (int v = 0; v < IRQ_BASE + IRQ_COUNT; v++)
        idt_set_gate((uint8_t)v, isr_stub_table[v]);

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;
    idt_flush((uint32_t)&idtp);
}

/* Called from isr_common with interrupts disabled. Keep IRQ work short. */
void isr_dispatch(regs_t *r) {
    if (r->int_no < 32) {
        vga_set_color(VGA_COLOR_ACCENT);
        log_puts("\n!! EXCEPTION ");
        log_puts(exception_names[r->int_no]);
        log_puts(" vector=");
        log_hex(r->int_no);
        log_puts(" err=");
        log_hex(r->err_code);
        log_puts(" eip=");
        log_hex(r->eip);
        if (r->int_no == 14) {
            /* Page fault: CR2 holds the faulting address. The error code says why. */
            uint32_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            log_puts("\n   address=");
            log_hex(cr2);
            log_puts(r->err_code & 1 ? " protection violation" : " page not present");
            log_puts(r->err_code & 2 ? ", write" : ", read");
            log_puts(r->err_code & 4 ? ", user mode" : ", kernel mode");
        }
        log_puts("\nsystem halted\n");
        for (;;)
            __asm__ volatile("cli; hlt");
    }

    if (r->int_no >= IRQ_BASE && r->int_no < IRQ_BASE + IRQ_COUNT) {
        uint8_t irq = (uint8_t)(r->int_no - IRQ_BASE);
        if (irq == 0)
            timer_irq_handler();
        else if (irq == 1)
            keyboard_irq_handler();
        pic_send_eoi(irq);
    }
}
