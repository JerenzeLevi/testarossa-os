#include <stdint.h>
#include "paging.h"
#include "pmm.h"
#include "log.h"

#define CR4_PSE   0x00000010u
#define CR0_PG    0x80000000u
#define LARGE_SIZE   (4u * 1024u * 1024u)
#define IDENTITY_TOP (1u << 30)            /* first 1 GiB is identity mapped */
#define TABLE_ENTRIES 1024

/* Page directory and tables. While paging is still off, physical == virtual,
 * so the pmm frame addresses can be used directly as pointers. */
static uint32_t *page_dir;
static int enabled;

static void zero_frame(uint32_t phys) {
    volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)phys;
    for (uint32_t i = 0; i < PAGE_SIZE / 4; i++)
        p[i] = 0;
}

static void load_cr3(uint32_t phys) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(phys) : "memory");
}

static void invlpg(uint32_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

int paging_init(void) {
    uint32_t dir_phys = pmm_alloc_frame();
    if (dir_phys == 0)
        return -1;
    zero_frame(dir_phys);
    page_dir = (uint32_t *)(uintptr_t)dir_phys;

    for (uint32_t i = 0; i < IDENTITY_TOP / LARGE_SIZE; i++)
        page_dir[i] = (i * LARGE_SIZE) | PAGE_PRESENT | PAGE_WRITE | PAGE_LARGE;

    /* Turn on 4 MiB pages, point CR3 at the directory, then set PG. */
    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= CR4_PSE;
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

    load_cr3(dir_phys);

    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= CR0_PG;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");

    enabled = 1;
    return 0;
}

int paging_identity_large(uint32_t phys, uint32_t size) {
    if (!enabled || size == 0)
        return -1;
    uint32_t first = phys / LARGE_SIZE;
    uint32_t last = (phys + size - 1) / LARGE_SIZE;
    for (uint32_t i = first; i <= last; i++) {
        if (i >= TABLE_ENTRIES)
            return -1;
        page_dir[i] = (i * LARGE_SIZE) | PAGE_PRESENT | PAGE_WRITE | PAGE_LARGE;
        invlpg(i * LARGE_SIZE);
    }
    return 0;
}

int paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    if (!enabled)
        return -1;
    uint32_t pdi = virt >> 22;
    uint32_t pti = (virt >> 12) & 0x3FF;

    if (page_dir[pdi] & PAGE_LARGE)
        return -1;                      /* already covered by a 4 MiB page */

    if (!(page_dir[pdi] & PAGE_PRESENT)) {
        uint32_t table = pmm_alloc_frame();
        if (table == 0)
            return -1;
        zero_frame(table);
        page_dir[pdi] = table | PAGE_PRESENT | PAGE_WRITE;
    }

    uint32_t *table = (uint32_t *)(uintptr_t)(page_dir[pdi] & 0xFFFFF000u);
    table[pti] = (phys & 0xFFFFF000u) | (flags & 0xFFFu) | PAGE_PRESENT;
    invlpg(virt);
    return 0;
}

void paging_unmap(uint32_t virt) {
    if (!enabled)
        return;
    uint32_t pdi = virt >> 22;
    uint32_t pti = (virt >> 12) & 0x3FF;
    if (!(page_dir[pdi] & PAGE_PRESENT) || (page_dir[pdi] & PAGE_LARGE))
        return;
    uint32_t *table = (uint32_t *)(uintptr_t)(page_dir[pdi] & 0xFFFFF000u);
    table[pti] = 0;
    invlpg(virt);
}

int paging_is_mapped(uint32_t virt) {
    if (!enabled)
        return 0;
    uint32_t pdi = virt >> 22;
    uint32_t pti = (virt >> 12) & 0x3FF;
    if (!(page_dir[pdi] & PAGE_PRESENT))
        return 0;
    if (page_dir[pdi] & PAGE_LARGE)
        return 1;
    uint32_t *table = (uint32_t *)(uintptr_t)(page_dir[pdi] & 0xFFFFF000u);
    return (table[pti] & PAGE_PRESENT) ? 1 : 0;
}

#define TEST_VIRT 0x80000000u           /* outside the identity map, so it needs a table */

int paging_selftest(void) {
    uint32_t frame = pmm_alloc_frame();
    if (frame == 0) {
        log_puts("paging self-test: FAIL, no frame for the test\n");
        return -1;
    }
    int ok = 1;

    if (paging_is_mapped(TEST_VIRT)) {
        log_puts("paging self-test: FAIL, test address was already mapped\n");
        ok = 0;
    }
    if (ok && paging_map(TEST_VIRT, frame, PAGE_WRITE) != 0) {
        log_puts("paging self-test: FAIL, map returned an error\n");
        ok = 0;
    }
    if (ok) {
        volatile uint32_t *v = (volatile uint32_t *)(uintptr_t)TEST_VIRT;
        volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)frame;
        *v = 0xC0FFEE42u;                 /* write through the new mapping */
        if (*p != 0xC0FFEE42u) {          /* read the same frame by its physical address */
            log_puts("paging self-test: FAIL, write did not reach the frame\n");
            ok = 0;
        }
    }
    if (ok) {
        paging_unmap(TEST_VIRT);
        if (paging_is_mapped(TEST_VIRT)) {
            log_puts("paging self-test: FAIL, unmap left the page mapped\n");
            ok = 0;
        }
    }
    pmm_free_frame(frame);

    if (ok)
        log_puts("paging self-test: PASS (identity map on, 4 KiB map, write, unmap)\n");
    return ok ? 0 : -1;
}
