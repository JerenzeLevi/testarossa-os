#ifndef TESTAROSSA_PAGING_H
#define TESTAROSSA_PAGING_H

#include <stdint.h>

#define PAGE_SIZE       4096u
#define PAGE_PRESENT    0x001
#define PAGE_WRITE      0x002
#define PAGE_LARGE      0x080      /* 4 MiB page, used for the identity map */

/* Build a page directory that identity-maps the first 1 GiB with 4 MiB pages,
 * then turn paging on. Needs the pmm to have been initialised. Returns 0 on success. */
int paging_init(void);

/* Identity-map the 4 MiB regions that cover [phys, phys + size).
 * Used for the framebuffer, which sits above the first 1 GiB. Returns 0 on success. */
int paging_identity_large(uint32_t phys, uint32_t size);

/* Map one 4 KiB page virt -> phys. The virt address must not fall inside an
 * identity-mapped 4 MiB region. Returns 0 on success, -1 otherwise. */
int paging_map(uint32_t virt, uint32_t phys, uint32_t flags);

/* Remove a 4 KiB mapping. Does nothing if the page is not mapped. */
void paging_unmap(uint32_t virt);

/* Returns 1 if virt has a present 4 KiB mapping, otherwise 0. */
int paging_is_mapped(uint32_t virt);

/* Map a frame at a fresh virtual address, write and read it back, unmap,
 * and check the mapping is gone. Prints PASS or FAIL. Returns 0 on pass. */
int paging_selftest(void);

#endif
