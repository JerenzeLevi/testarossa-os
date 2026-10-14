#ifndef TESTAROSSA_PMM_H
#define TESTAROSSA_PMM_H

#include <stdint.h>

#define PMM_FRAME_SIZE 4096u

/* Read the Multiboot memory map and build a bitmap of 4 KiB frames.
 * Low memory, the kernel image, and the boot info are reserved.
 * Returns 0 on success, -1 if the bootloader gave no usable memory data. */
int pmm_init(uint32_t mb_info_addr);

/* Return one free frame's physical address, or 0 if none is free.
 * Frame 0 is never returned, so 0 always means failure. */
uint32_t pmm_alloc_frame(void);

/* Mark a frame free again. Ignores misaligned, reserved, out-of-range,
 * and already-free addresses. */
void pmm_free_frame(uint32_t phys);

uint32_t pmm_total_frames(void);   /* frames the bootloader reported as usable */
uint32_t pmm_free_frames(void);    /* frames free right now */

/* Allocate and free a batch of frames and check the results. Prints the
 * outcome to serial and VGA. Returns 0 on pass, -1 on fail. */
int pmm_selftest(void);

#endif
