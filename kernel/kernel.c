#include <stdint.h>
#include "vga.h"
#include "serial.h"
#include "log.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"

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

    gdt_init();
    serial_puts("GDT loaded\n");

    idt_init();
    serial_puts("IDT loaded\n");

    pic_init();
    serial_puts("PIC remapped to vectors 32-47; IRQ1 (keyboard) unmasked\n");

    __asm__ volatile("sti");
    vga_puts("Keyboard ready. Type something.\n");

    for (;;) {
        /* Check the queue with interrupts off, then sleep with "sti; hlt".
         * sti delays interrupts by one instruction, so a key pressed between
         * the check and hlt cannot be missed. */
        __asm__ volatile("cli");
        int c = keyboard_getc();
        if (c < 0) {
            __asm__ volatile("sti; hlt");
            continue;
        }
        __asm__ volatile("sti");
        log_putc((char)c);
    }
}
