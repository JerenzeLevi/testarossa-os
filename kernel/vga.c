#include <stdint.h>
#include "io.h"
#include "vga.h"

/* VGA text mode buffer: 80x25 cells, each cell is (attribute << 8) | char. */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_BG     0x0                /* black background */

static volatile uint16_t *const vga_buf = (volatile uint16_t *)0xB8000;
static int row, col;
static uint8_t fg_color = VGA_COLOR_BODY;

/* One screen cell: character in the low byte, colour attribute in the high byte. */
static uint16_t make_cell(char c) {
    return (uint16_t)((unsigned char)c | (((VGA_BG << 4) | fg_color) << 8));
}

void vga_set_color(uint8_t fg) {
    fg_color = fg & 0x0F;
}

uint8_t vga_get_color(void) {
    return fg_color;
}

/* Move the blinking hardware cursor to the current row and column.
 * Without this the cursor stays where the BIOS left it. */
static void vga_sync_cursor(void) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buf[i] = make_cell(' ');
    row = col = 0;
    vga_sync_cursor();
}

static void vga_scroll(void) {
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++)
        vga_buf[i] = vga_buf[i + VGA_WIDTH];
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buf[i] = make_cell(' ');
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
            vga_buf[row * VGA_WIDTH + col] = make_cell(' ');
        }
    } else {
        vga_buf[row * VGA_WIDTH + col] = make_cell(c);
        if (++col == VGA_WIDTH) {
            col = 0;
            row++;
        }
    }
    if (row == VGA_HEIGHT)
        vga_scroll();
    vga_sync_cursor();
}

void vga_puts(const char *s) {
    while (*s)
        vga_putc(*s++);
}
