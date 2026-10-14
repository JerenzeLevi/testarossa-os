#ifndef TESTAROSSA_VGA_H
#define TESTAROSSA_VGA_H

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char *s);

#endif
