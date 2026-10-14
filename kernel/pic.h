#ifndef TESTAROSSA_PIC_H
#define TESTAROSSA_PIC_H

#include <stdint.h>

/* Remap the legacy 8259 PICs to vectors 32-47 and unmask only IRQ1 (keyboard). */
void pic_init(void);

/* Acknowledge an IRQ (0-15). Slave PIC needs an EOI too for IRQs 8-15. */
void pic_send_eoi(uint8_t irq);

#endif
