#ifndef TESTAROSSA_VGA_H
#define TESTAROSSA_VGA_H

#include <stdint.h>

/* Dark cyber-gothic text palette: the 16-colour VGA stand-in for the
 * colours in docs/DESIGN.md section 3.1. Background is always black. */
#define VGA_COLOR_DIM     8    /* dark grey: hints, quiet text */
#define VGA_COLOR_BODY    7    /* light grey: normal text (stands in for pale lavender) */
#define VGA_COLOR_FOCUS   11   /* light cyan: prompt and focus */
#define VGA_COLOR_ACCENT  13   /* light magenta: banner and errors */

void vga_init(void);
void vga_clear(void);
void vga_set_color(uint8_t fg);
uint8_t vga_get_color(void);
void vga_putc(char c);
void vga_puts(const char *s);

#endif
