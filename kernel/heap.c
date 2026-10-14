#include <stdint.h>
#include "heap.h"
#include "log.h"

#define HEAP_SIZE   (512u * 1024u)
#define HEADER_SIZE 16u                 /* keeps payloads 16-byte aligned */
#define ALIGN       16u

struct block {
    uint32_t size;                      /* payload bytes after the header */
    uint32_t free;                      /* 1 if free */
    struct block *next;                 /* next block in address order */
};

static uint8_t arena[HEAP_SIZE] __attribute__((aligned(16)));
static struct block *head;

static uint32_t round_up(uint32_t n) {
    return (n + ALIGN - 1) & ~(ALIGN - 1);
}

static int in_arena(const void *p) {
    const uint8_t *b = (const uint8_t *)p;
    return b >= arena && b < arena + HEAP_SIZE;
}

void heap_init(void) {
    head = (struct block *)arena;
    head->size = HEAP_SIZE - HEADER_SIZE;
    head->free = 1;
    head->next = 0;
}

void *kmalloc(uint32_t n) {
    if (n == 0 || head == 0)
        return 0;
    if (n > HEAP_SIZE)
        return 0;
    n = round_up(n);

    for (struct block *b = head; b; b = b->next) {
        if (!b->free || b->size < n)
            continue;

        /* Split only if the rest is big enough to be a block of its own. */
        if (b->size >= n + HEADER_SIZE + ALIGN) {
            struct block *rest = (struct block *)((uint8_t *)b + HEADER_SIZE + n);
            rest->size = b->size - n - HEADER_SIZE;
            rest->free = 1;
            rest->next = b->next;
            b->next = rest;
            b->size = n;
        }
        b->free = 0;
        return (uint8_t *)b + HEADER_SIZE;
    }
    return 0;
}

void kfree(void *p) {
    if (p == 0)
        return;
    uint8_t *raw = (uint8_t *)p - HEADER_SIZE;
    if (!in_arena(raw))
        return;                         /* not ours: ignore rather than corrupt */
    struct block *b = (struct block *)raw;
    if (b->free)
        return;                         /* double free: ignore */
    b->free = 1;

    /* Merge every run of adjacent free blocks. The list is short, so one pass is enough. */
    for (struct block *c = head; c; c = c->next) {
        while (c->free && c->next && c->next->free) {
            c->size += HEADER_SIZE + c->next->size;
            c->next = c->next->next;
        }
    }
}

uint32_t heap_free_bytes(void) {
    uint32_t total = 0;
    for (struct block *b = head; b; b = b->next) {
        if (b->free)
            total += b->size;
    }
    return total;
}

uint32_t heap_total_bytes(void) {
    return HEAP_SIZE;
}

#define SELFTEST_BLOCKS 64

int heap_selftest(void) {
    static void *blocks[SELFTEST_BLOCKS];
    static uint32_t sizes[SELFTEST_BLOCKS];
    int ok = 1;

    for (uint32_t i = 0; i < SELFTEST_BLOCKS; i++) {
        sizes[i] = (i * 37u) % 1000u + 1u;
        blocks[i] = kmalloc(sizes[i]);
        if (!blocks[i]) {
            ok = 0;
            break;
        }
        uint8_t *p = blocks[i];
        for (uint32_t j = 0; j < sizes[i]; j++)
            p[j] = (uint8_t)(i + 1);
        if (((uintptr_t)p & (ALIGN - 1)) != 0) {
            ok = 0;
            break;
        }
    }

    /* Blocks must not overlap: each one still holds its own pattern. */
    for (uint32_t i = 0; ok && i < SELFTEST_BLOCKS; i++) {
        uint8_t *p = blocks[i];
        for (uint32_t j = 0; j < sizes[i]; j++) {
            if (p[j] != (uint8_t)(i + 1)) {
                ok = 0;
                break;
            }
        }
    }

    /* Free every other block, then allocate into the holes and check again. */
    for (uint32_t i = 0; i < SELFTEST_BLOCKS; i += 2) {
        kfree(blocks[i]);
        blocks[i] = 0;
    }
    for (uint32_t i = 0; ok && i < SELFTEST_BLOCKS; i += 2) {
        blocks[i] = kmalloc(16);
        if (!blocks[i]) {
            ok = 0;
            break;
        }
        uint8_t *p = blocks[i];
        for (uint32_t j = 0; j < 16; j++)
            p[j] = 0xAA;
        sizes[i] = 16;
    }
    for (uint32_t i = 1; ok && i < SELFTEST_BLOCKS; i += 2) {
        uint8_t *p = blocks[i];
        for (uint32_t j = 0; j < sizes[i]; j++) {
            if (p[j] != (uint8_t)(i + 1)) {
                ok = 0;
                break;
            }
        }
    }

    for (uint32_t i = 0; i < SELFTEST_BLOCKS; i++)
        kfree(blocks[i]);

    /* Everything freed: the arena must be one free block again. */
    if (ok) {
        if (head->next != 0 || !head->free || head->size != HEAP_SIZE - HEADER_SIZE) {
            ok = 0;
        }
    }

    if (ok) {
        log_puts("heap self-test: PASS (64 blocks, no overlap, holes reused, arena merged back)\n");
        return 0;
    }
    log_puts("heap self-test: FAIL\n");
    return -1;
}
