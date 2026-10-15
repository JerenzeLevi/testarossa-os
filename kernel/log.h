#ifndef TESTAROSSA_LOG_H
#define TESTAROSSA_LOG_H

#include <stdint.h>

/* Print to serial and VGA, and to an optional extra sink (the desktop's
 * terminal window). Special characters:
 *   '\n' newline   '\b' backspace   '\f' clear the screen */
typedef void (*log_sink_fn)(char c);

void log_set_sink(log_sink_fn sink);   /* NULL removes the sink */

void log_putc(char c);
void log_puts(const char *s);
void log_hex(uint32_t v);
void log_dec(uint32_t v);

#endif
