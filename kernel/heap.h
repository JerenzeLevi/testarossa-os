#ifndef TESTAROSSA_HEAP_H
#define TESTAROSSA_HEAP_H

#include <stdint.h>

/* A general-purpose allocator for kernel data. It serves from a fixed
 * arena in .bss, so its blocks are contiguous. Blocks are 16-byte aligned. */

void heap_init(void);

/* Returns a block of at least n bytes, or NULL if none is big enough or n is 0. */
void *kmalloc(uint32_t n);

/* Frees a block from kmalloc. NULL is ignored. Adjacent free blocks are merged. */
void kfree(void *p);

uint32_t heap_free_bytes(void);      /* total bytes in free blocks (payload only) */
uint32_t heap_total_bytes(void);     /* size of the arena */

/* Allocate, fill, check, free in a pattern, then check the arena is one block again.
 * Prints PASS or FAIL. Returns 0 on pass. */
int heap_selftest(void);

#endif
