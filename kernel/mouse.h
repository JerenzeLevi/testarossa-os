#ifndef TESTAROSSA_MOUSE_H
#define TESTAROSSA_MOUSE_H

#include <stdint.h>

#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

/* Turn on the PS/2 auxiliary port (the mouse) and start data reporting.
 * Needs the PIC to unmask IRQ12. Returns 0 on success, -1 if no mouse answered. */
int mouse_init(void);

/* IRQ12 handler: collects 3-byte packets and updates the pointer position. */
void mouse_irq_handler(void);

/* Current pointer position, clamped to the screen, and button state. */
void mouse_state(int *x, int *y, uint8_t *buttons);

#endif
