#include <stdint.h>
#include "io.h"
#include "timer.h"

/* 8253/8254 Programmable Interval Timer. Its input clock is 1.193182 MHz. */
#define PIT_CH0_DATA  0x40
#define PIT_CMD       0x43
#define PIT_CLOCK_HZ  1193182u

static volatile uint32_t ticks;

void timer_init(void) {
    uint32_t divisor = PIT_CLOCK_HZ / TIMER_HZ;
    /* Command: channel 0, low/high byte access, mode 3 (square wave), binary. */
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0_DATA, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_irq_handler(void) {
    ticks++;
}

uint32_t timer_ticks(void) {
    return ticks;
}
