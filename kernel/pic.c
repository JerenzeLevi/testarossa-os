#include <stdint.h>
#include "io.h"
#include "pic.h"

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20

#define PIC1_VECTOR_BASE 0x20   /* IRQ0-7  -> vectors 32-39 */
#define PIC2_VECTOR_BASE 0x28   /* IRQ8-15 -> vectors 40-47 */

void pic_init(void) {
    /* ICW1: start initialization, expect ICW4. */
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();
    /* ICW2: vector offsets. */
    outb(PIC1_DATA, PIC1_VECTOR_BASE); io_wait();
    outb(PIC2_DATA, PIC2_VECTOR_BASE); io_wait();
    /* ICW3: master has slave on IRQ2; slave's cascade identity is 2. */
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    /* ICW4: 8086 mode. */
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();
    /* OCW1: unmask IRQ0 (timer) and IRQ1 (keyboard); mask the rest. */
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}
