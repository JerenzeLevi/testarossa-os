#ifndef TESTAROSSA_PALETTE_H
#define TESTAROSSA_PALETTE_H

#include <stdint.h>

/* Graphics palette from docs/DESIGN.md section 3. Colours are 0x00RRGGBB. */
#define C_MIDNIGHT  0x090A16u
#define C_PANEL     0x12142Bu   /* a little lighter than midnight, for cards and windows */
#define C_VIOLET    0x30205Eu   /* deep violet: shadows, title bars */
#define C_AMETHYST  0x9B7BFFu   /* borders and glow */
#define C_CYAN      0x36E4F5u   /* focus and selection */
#define C_MAGENTA   0xFF66C4u   /* accents and close buttons */
#define C_LAVENDER  0xE8DFF5u   /* main text */
#define C_DIM       0x7D7AA0u   /* secondary text */
#define C_STAR_A    0x3B3470u   /* faint background stars */
#define C_STAR_B    0x5A4A9Cu

/* Map a text-mode colour index (vga.h) to the graphics palette. */
uint32_t palette_from_vga(uint8_t vga_color);

#endif
