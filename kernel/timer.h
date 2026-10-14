#ifndef TESTAROSSA_TIMER_H
#define TESTAROSSA_TIMER_H

#include <stdint.h>

#define TIMER_HZ 100            /* ticks per second */

/* Program PIT channel 0 to fire IRQ0 at TIMER_HZ. Call before enabling interrupts. */
void timer_init(void);

/* Called from the IRQ0 handler. Only increments a counter. */
void timer_irq_handler(void);

/* Ticks since timer_init. Reads a 32-bit counter: at 100 Hz that wraps after ~497 days. */
uint32_t timer_ticks(void);

#endif
