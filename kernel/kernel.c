#include <stdint.h>
#include "vga.h"
#include "serial.h"

#define TESTAROSSA_VERSION "0.1.0"

void kernel_main(uint32_t mb_magic, uint32_t mb_info) {
    (void)mb_info;

    vga_init();
    serial_init();

    vga_puts("  ~*~ T E S T A R O S S A   O S ~*~  v" TESTAROSSA_VERSION "\n");
    vga_puts("  xoxo  it's not a bug, it's a feature  xoxo\n\n");
    serial_puts("Testarossa OS v" TESTAROSSA_VERSION " booting...\n");

    if (mb_magic != 0x2BADB002) {
        vga_puts("error: not loaded by a multiboot bootloader\n");
        return;
    }

    vga_puts("Kernel is alive. Nothing else to do yet.\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
