#include <stdint.h>
#include "vga.h"
#include "serial.h"
#include "log.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "framebuffer.h"
#include "desktop.h"
#include "shell.h"
#include "version.h"

#ifdef TESTAROSSA_FAULT_TEST
/* Build-time check only: `make fault-test`. Divides by zero to trigger #DE,
 * which should print an EXCEPTION line and halt. */
static void trigger_divide_error(void) {
    volatile uint32_t zero = 0;
    volatile uint32_t x = 1;
    x = x / zero;
}
#endif

void kernel_main(uint32_t mb_magic, uint32_t mb_info) {
    vga_init();
    serial_init();

    vga_set_color(VGA_COLOR_ACCENT);
    vga_puts("  ~*~ " TESTAROSSA_NAME " ~*~  ");
    vga_set_color(VGA_COLOR_DIM);
    vga_puts("v" TESTAROSSA_VERSION "\n\n");
    vga_set_color(VGA_COLOR_BODY);
    serial_puts(TESTAROSSA_NAME " v" TESTAROSSA_VERSION " booting...\n");

    if (mb_magic != 0x2BADB002) {
        vga_puts("error: not loaded by a multiboot bootloader\n");
        return;
    }

    gdt_init();
    serial_puts("GDT loaded\n");

    idt_init();
    serial_puts("IDT loaded\n");

    pic_init();
    serial_puts("PIC remapped to vectors 32-47; IRQ0 (timer) and IRQ1 (keyboard) unmasked\n");

    timer_init();
    serial_puts("PIT programmed to 100 Hz\n");

#ifdef TESTAROSSA_FAULT_TEST
    /* Build-time check only: `make fault-test`. Divides by zero, which should
     * print an EXCEPTION line and halt. Runs before anything else starts. */
    __asm__ volatile("sti");
    trigger_divide_error();
#endif

    if (pmm_init(mb_info) == 0) {
        log_puts("PMM: memory map read, ");
        log_dec(pmm_free_frames() * 4);
        log_puts(" KiB free\n");
    } else {
        log_puts("PMM: no memory map from bootloader; allocator disabled\n");
    }
    pmm_selftest();

    if (paging_init() == 0)
        log_puts("Paging on: first 1 GiB identity-mapped with 4 MiB pages\n");
    else
        log_puts("Paging: could not start, running without it\n");
    paging_selftest();

#ifdef TESTAROSSA_PAGE_FAULT_TEST
    /* Build-time check only: `make pf-test`. Reads an unmapped address,
     * which should print a page fault and halt. */
    volatile uint32_t *unmapped = (volatile uint32_t *)0x80000000u;
    (void)*unmapped;
#endif

    heap_init();
    heap_selftest();

#ifndef TESTAROSSA_NO_GRAPHICS
    if (fb_init() == 0) {
        /* Graphics are up: the desktop takes over the screen. Serial keeps the log. */
        desktop_run();
    }
    log_puts("graphics unavailable, starting the text shell\n");
#endif

    __asm__ volatile("sti");
    shell_run();
}
