#include <stdint.h>
#include "palette.h"
#include "vga.h"

/* Text-mode colour indices from vga.h, mapped to the graphics palette so the
 * terminal window keeps the same colour roles as the text screen. */
uint32_t palette_from_vga(uint8_t vga_color) {
    switch (vga_color) {
    case VGA_COLOR_DIM:    return C_DIM;
    case VGA_COLOR_FOCUS:  return C_CYAN;
    case VGA_COLOR_ACCENT: return C_MAGENTA;
    default:               return C_LAVENDER;
    }
}
