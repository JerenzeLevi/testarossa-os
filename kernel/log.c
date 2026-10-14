#include <stdint.h>
#include "log.h"
#include "serial.h"
#include "vga.h"

void log_putc(char c) {
    if (c == '\n')
        serial_puts("\n");
    else if (c == '\b')
        serial_puts("\b \b");
    else
        serial_putc(c);
    vga_putc(c);
}

void log_puts(const char *s) {
    while (*s)
        log_putc(*s++);
}

void log_hex(uint32_t v) {
    static const char digits[] = "0123456789ABCDEF";
    log_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        log_putc(digits[(v >> shift) & 0xF]);
}
