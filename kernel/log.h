#ifndef TESTAROSSA_LOG_H
#define TESTAROSSA_LOG_H

#include <stdint.h>

/* Print to both VGA and serial so diagnostics survive either way. */
void log_putc(char c);
void log_puts(const char *s);
void log_hex(uint32_t v);

#endif
