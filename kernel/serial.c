#include <stdint.h>
#include "serial.h"

/* COM1 at I/O port 0x3F8. Output goes to QEMU's -serial stdio. */
#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* disable interrupts */
    outb(COM1 + 3, 0x80);   /* enable DLAB, set baud divisor */
    outb(COM1 + 0, 0x03);   /* divisor low: 38400 baud */
    outb(COM1 + 1, 0x00);   /* divisor high */
    outb(COM1 + 3, 0x03);   /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7);   /* enable FIFO */
    outb(COM1 + 4, 0x0B);   /* RTS/DSR set */
}

void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20))  /* wait for transmit buffer empty */
        ;
    outb(COM1, (uint8_t)c);
}

void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s++);
    }
}
