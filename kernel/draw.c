#include <stdint.h>
#include "draw.h"
#include "font.h"
#include "framebuffer.h"

static int inside(int x, int y) {
    return x >= 0 && y >= 0 && x < (int)FB_WIDTH && y < (int)FB_HEIGHT;
}

void draw_clear(uint32_t color) {
    uint32_t *buf = fb_backbuffer();
    for (uint32_t i = 0; i < FB_WIDTH * FB_HEIGHT; i++)
        buf[i] = color;
}

void draw_pixel(int x, int y, uint32_t color) {
    if (!inside(x, y))
        return;
    fb_backbuffer()[(uint32_t)y * FB_WIDTH + (uint32_t)x] = color;
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0)
        return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > (int)FB_WIDTH) ? (int)FB_WIDTH : x + w;
    int y1 = (y + h > (int)FB_HEIGHT) ? (int)FB_HEIGHT : y + h;
    uint32_t *buf = fb_backbuffer();
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            buf[(uint32_t)yy * FB_WIDTH + (uint32_t)xx] = color;
}

void draw_frame(int x, int y, int w, int h, int thickness, uint32_t color) {
    if (w <= 0 || h <= 0 || thickness <= 0)
        return;
    draw_rect(x, y, w, thickness, color);                       /* top */
    draw_rect(x, y + h - thickness, w, thickness, color);       /* bottom */
    draw_rect(x, y, thickness, h, color);                       /* left */
    draw_rect(x + w - thickness, y, thickness, h, color);       /* right */
}

/* Bresenham's line algorithm, all octants. */
void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;      /* negative */
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_char(int x, int y, char c, uint32_t color) {
    const uint8_t *glyph = &font8x16_data[(uint8_t)c * FONT_HEIGHT];
    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col))
                draw_pixel(x + col, y + row, color);
        }
    }
}

void draw_text(int x, int y, const char *s, uint32_t color) {
    for (; *s; s++, x += FONT_WIDTH)
        draw_char(x, y, *s, color);
}

int draw_text_width(const char *s) {
    int n = 0;
    while (s[n])
        n++;
    return n * FONT_WIDTH;
}
