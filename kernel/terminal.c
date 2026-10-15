#include <stdint.h>
#include "terminal.h"
#include "draw.h"
#include "font.h"
#include "palette.h"
#include "vga.h"

#define FONT_CELL_W FONT_WIDTH
#define FONT_CELL_H FONT_HEIGHT

static char cells[TERM_ROWS][TERM_COLS];
static uint8_t colors[TERM_ROWS][TERM_COLS];
static int row, col;

static void blank_row(int r) {
    for (int c = 0; c < TERM_COLS; c++) {
        cells[r][c] = ' ';
        colors[r][c] = VGA_COLOR_BODY;
    }
}

void terminal_clear(void) {
    for (int r = 0; r < TERM_ROWS; r++)
        blank_row(r);
    row = col = 0;
}

static void newline(void) {
    col = 0;
    row++;
    if (row == TERM_ROWS) {
        /* Scroll up one line; the bottom row becomes blank. */
        for (int r = 1; r < TERM_ROWS; r++) {
            for (int c = 0; c < TERM_COLS; c++) {
                cells[r - 1][c] = cells[r][c];
                colors[r - 1][c] = colors[r][c];
            }
        }
        blank_row(TERM_ROWS - 1);
        row = TERM_ROWS - 1;
    }
}

void terminal_sink(char c) {
    if (c == '\f') {
        terminal_clear();
    } else if (c == '\n') {
        newline();
    } else if (c == '\b') {
        if (col > 0) {
            col--;
            cells[row][col] = ' ';
            colors[row][col] = VGA_COLOR_BODY;
        }
    } else {
        if (col == TERM_COLS)
            newline();                  /* wrap long lines at the window edge */
        cells[row][col] = c;
        colors[row][col] = vga_get_color();
        col++;
    }
}

void terminal_draw(int x, int y) {
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            if (cells[r][c] != ' ')
                draw_char(x + c * FONT_CELL_W, y + r * FONT_CELL_H, cells[r][c], palette_from_vga(colors[r][c]));
        }
    }
    /* Cursor: an underline at the insertion point. */
    if (col < TERM_COLS)
        draw_rect(x + col * FONT_CELL_W, y + row * FONT_CELL_H + FONT_CELL_H - 2, FONT_CELL_W, 2, C_LAVENDER);
}
