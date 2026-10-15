#include <stdint.h>
#include "log.h"
#include "serial.h"
#include "vga.h"

static log_sink_fn extra_sink;

void log_set_sink(log_sink_fn sink) {
    extra_sink = sink;
}

void log_putc(char c) {
    if (c == '\f') {
        serial_puts("\x1b[2J\x1b[H");     /* ANSI clear + home for a serial terminal */
        vga_clear();
    } else {
        if (c == '\n')
            serial_puts("\n");
        else if (c == '\b')
            serial_puts("\b \b");
        else
            serial_putc(c);
        vga_putc(c);
    }
    if (extra_sink)
        extra_sink(c);
}

void log_puts(const char *s) {
    while (*s)
        log_putc(*s++);
}

void log_dec(uint32_t v) {
    char buf[11];                       /* up to 10 digits for a 32-bit value, plus NUL */
    int i = 0;
    do {
        buf[i++] = (char)('0' + v % 10);
        v /= 10;
    } while (v != 0);
    while (i > 0)
        log_putc(buf[--i]);
}

void log_hex(uint32_t v) {
    static const char digits[] = "0123456789ABCDEF";
    log_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        log_putc(digits[(v >> shift) & 0xF]);
}
