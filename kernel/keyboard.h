#ifndef TESTAROSSA_KEYBOARD_H
#define TESTAROSSA_KEYBOARD_H

/* Special keys returned by keyboard_getc(). They are above 127, so they never
 * clash with ASCII. Enter, Tab, Escape, and Backspace use their ASCII codes
 * ('\n', '\t', 27, '\b'). */
#define KEY_UP     0x80
#define KEY_DOWN   0x81
#define KEY_LEFT   0x82
#define KEY_RIGHT  0x83

/* Called from the IRQ1 handler. Reads one scancode and queues any character. */
void keyboard_irq_handler(void);

/* Pop one character from the queue. Returns -1 when the queue is empty. */
int keyboard_getc(void);

#endif
