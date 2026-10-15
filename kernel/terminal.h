#ifndef TESTAROSSA_TERMINAL_H
#define TESTAROSSA_TERMINAL_H

#include <stdint.h>

/* A text grid inside a desktop window. The shell writes to it through the
 * log sink, so its output looks like the text screen, in the same colours. */

#define TERM_COLS 56
#define TERM_ROWS 18

void terminal_clear(void);

/* Log sink: store one character. Handles '\n', '\b', '\f', and wrapping. */
void terminal_sink(char c);

/* Draw the grid with its top-left corner at (x, y), and a cursor. */
void terminal_draw(int x, int y);

#endif
