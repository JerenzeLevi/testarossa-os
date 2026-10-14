#include <stdint.h>
#include "vga.h"
#include "serial.h"

#define VESPER_VERSION "0.1.0"

void kernel_main(uint32_t mb_magic, uint32_t mb_info) {
    (void)mb_info;

    vga_init();
    serial_init();

    vga_puts("  ~*~ V E S P E R ~*~  bubblegum kernel v" VESPER_VERSION "\n");
    vga_puts("  xoxo  it's not a bug, it's a feature  xoxo\n\n");
    serial_puts("Vesper OS v" VESPER_VERSION " booting...\n");

    if (mb_magic != 0x2BADB002) {
        vga_puts("error: not loaded by a multiboot bootloader\n");
        return;
    }

    vga_puts("Kernel is alive. Nothing else to do yet.\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
