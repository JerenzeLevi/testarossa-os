#ifndef TESTAROSSA_KEYBOARD_H
#define TESTAROSSA_KEYBOARD_H

/* Called from the IRQ1 handler. Reads one scancode and queues any character. */
void keyboard_irq_handler(void);

/* Pop one character from the queue. Returns -1 when the queue is empty. */
int keyboard_getc(void);

#endif
