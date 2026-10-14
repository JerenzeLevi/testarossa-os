#ifndef TESTAROSSA_DRAW_H
#define TESTAROSSA_DRAW_H

#include <stdint.h>

/* Drawing onto the framebuffer back buffer. Every call clips to the screen,
 * so out-of-range coordinates are safe. Colours are 0x00RRGGBB. */

void draw_clear(uint32_t color);
void draw_pixel(int x, int y, uint32_t color);
void draw_rect(int x, int y, int w, int h, uint32_t color);           /* filled */
void draw_frame(int x, int y, int w, int h, int thickness, uint32_t color);
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);

/* Text uses the 8x16 font. Background is transparent. */
void draw_char(int x, int y, char c, uint32_t color);
void draw_text(int x, int y, const char *s, uint32_t color);
int draw_text_width(const char *s);

#endif
