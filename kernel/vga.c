#include <stdint.h>
#include "vga.h"

/* VGA text mode buffer: 80x25 cells, each cell is (attribute << 8) | char. */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
/* Cutecore palette: VGA color indices. Bright magenta text on near-black. */
#define VGA_BG     0x0                /* black */
#define VGA_FG     0xD                /* light magenta, bubblegum pink */
#define VGA_COLOR  ((VGA_BG << 4) | VGA_FG)

static volatile uint16_t *const vga_buf = (volatile uint16_t *)0xB8000;
static int row, col;

static void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buf[i] = (uint16_t)(' ' | (VGA_COLOR << 8));
    row = col = 0;
}

static void vga_scroll(void) {
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++)
        vga_buf[i] = vga_buf[i + VGA_WIDTH];
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buf[i] = (uint16_t)(' ' | (VGA_COLOR << 8));
    row = VGA_HEIGHT - 1;
}

void vga_init(void) {
    vga_clear();
}

void vga_putc(char c) {
    if (c == '\n') {
        col = 0;
        row++;
    } else if (c == '\b') {
        if (col > 0) {
            col--;
            vga_buf[row * VGA_WIDTH + col] = (uint16_t)(' ' | (VGA_COLOR << 8));
        }
    } else {
        vga_buf[row * VGA_WIDTH + col] = (uint16_t)((unsigned char)c | (VGA_COLOR << 8));
        if (++col == VGA_WIDTH) {
            col = 0;
            row++;
        }
    }
    if (row == VGA_HEIGHT)
        vga_scroll();
}

void vga_puts(const char *s) {
    while (*s)
        vga_putc(*s++);
}
