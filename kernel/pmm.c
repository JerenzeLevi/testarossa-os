#include <stdint.h>
#include "pmm.h"
#include "log.h"

/* Symbol from linker.ld: first byte after the kernel image. */
extern char _kernel_end[];

/* Multiboot 0.6 boot info flags (first 32-bit word of the info struct). */
#define MB_INFO_MEM        0x001   /* mem_lower and mem_upper are valid */
#define MB_INFO_MMAP       0x040   /* mmap_length and mmap_addr are valid */
#define MB_MMAP_AVAILABLE  1       /* entry type for usable RAM */

#define LOW_MEMORY_END     0x00100000u   /* 1 MiB: BIOS, VGA, and legacy areas */
#define MAX_PHYS_BYTES     (1u << 30)    /* manage up to 1 GiB of physical memory */
#define MAX_FRAMES         (MAX_PHYS_BYTES / PMM_FRAME_SIZE)

/* One bit per frame. 1 = used or reserved, 0 = free. Starts all 1s. */
static uint8_t bitmap[MAX_FRAMES / 8];
static uint32_t total_frames;
static uint32_t free_frames;
static int ready;

static int bit_get(uint32_t f) {
    return (bitmap[f >> 3] >> (f & 7)) & 1;
}

static void bit_set(uint32_t f) {
    bitmap[f >> 3] |= (uint8_t)(1u << (f & 7));
}

static void bit_clear(uint32_t f) {
    bitmap[f >> 3] &= (uint8_t)~(1u << (f & 7));
}

static uint32_t kernel_end_addr(void) {
    return (uint32_t)(uintptr_t)_kernel_end;
}

/* Mark frames that lie fully inside [start, end) as free. Counts each frame once. */
static void make_available(uint64_t start, uint64_t end) {
    if (end > MAX_PHYS_BYTES)
        end = MAX_PHYS_BYTES;
    uint64_t first = (start + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    uint64_t last = end / PMM_FRAME_SIZE;
    for (uint64_t f = first; f < last; f++) {
        if (bit_get((uint32_t)f)) {
            bit_clear((uint32_t)f);
            total_frames++;
            free_frames++;
        }
    }
}

/* Mark frames that overlap [start, end) as used. Only frames that are currently free change. */
static void reserve(uint64_t start, uint64_t end) {
    if (end <= start || start >= MAX_PHYS_BYTES)
        return;
    if (end > MAX_PHYS_BYTES)
        end = MAX_PHYS_BYTES;
    uint64_t first = start / PMM_FRAME_SIZE;
    uint64_t last = (end + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    for (uint64_t f = first; f < last; f++) {
        if (!bit_get((uint32_t)f)) {
            bit_set((uint32_t)f);
            free_frames--;
        }
    }
}

/* Read one 64-bit little-endian value as two 32-bit reads. The map entries
 * are not guaranteed to be 8-byte aligned. */
static uint64_t read_u64(const uint8_t *p) {
    uint32_t lo = *(const uint32_t *)p;
    uint32_t hi = *(const uint32_t *)(p + 4);
    return ((uint64_t)hi << 32) | lo;
}

int pmm_init(uint32_t mb_info_addr) {
    const uint32_t *info = (const uint32_t *)(uintptr_t)mb_info_addr;
    uint32_t flags = info[0];

    for (uint32_t i = 0; i < sizeof(bitmap); i++)
        bitmap[i] = 0xFF;
    total_frames = 0;
    free_frames = 0;

    if (flags & MB_INFO_MMAP) {
        uint32_t map_len = info[11];                   /* offset 44 */
        uint32_t map_addr = info[12];                  /* offset 48 */
        const uint8_t *p = (const uint8_t *)(uintptr_t)map_addr;
        const uint8_t *end = p + map_len;

        while (p < end) {
            /* Each entry: u32 size (not counting itself), u64 base, u64 length, u32 type. */
            uint32_t size = *(const uint32_t *)p;
            uint64_t base = read_u64(p + 4);
            uint64_t length = read_u64(p + 12);
            uint32_t type = *(const uint32_t *)(p + 20);
            if (type == MB_MMAP_AVAILABLE)
                make_available(base, base + length);
            p += size + 4;
        }

        /* The map buffer itself lives in RAM the bootloader gave us. Keep it. */
        reserve(map_addr, (uint64_t)map_addr + map_len);
    } else if (flags & MB_INFO_MEM) {
        /* Fallback: only mem_upper (KiB above 1 MiB) is known. */
        uint32_t mem_upper_kib = info[2];
        make_available(LOW_MEMORY_END, LOW_MEMORY_END + (uint64_t)mem_upper_kib * 1024);
    } else {
        ready = 0;
        return -1;
    }

    /* Always reserved, whatever the map says. */
    reserve(0, LOW_MEMORY_END);                        /* low memory */
    reserve(LOW_MEMORY_END, kernel_end_addr());        /* kernel image, stack, bss */
    reserve(mb_info_addr, (uint64_t)mb_info_addr + PMM_FRAME_SIZE); /* boot info */

    ready = 1;
    return 0;
}

uint32_t pmm_alloc_frame(void) {
    if (!ready || free_frames == 0)
        return 0;
    for (uint32_t f = 0; f < MAX_FRAMES; f++) {
        if (!bit_get(f)) {
            bit_set(f);
            free_frames--;
            return f * PMM_FRAME_SIZE;
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t phys) {
    if (!ready || (phys % PMM_FRAME_SIZE) != 0)
        return;
    if (phys < kernel_end_addr())
        return;                         /* never free low memory or the kernel */
    uint32_t f = phys / PMM_FRAME_SIZE;
    if (f >= MAX_FRAMES || !bit_get(f))
        return;                         /* out of range, or already free */
    bit_clear(f);
    free_frames++;
}

uint32_t pmm_total_frames(void) {
    return total_frames;
}

uint32_t pmm_free_frames(void) {
    return free_frames;
}

#define SELFTEST_COUNT 256

int pmm_selftest(void) {
    static uint32_t got[SELFTEST_COUNT];
    uint32_t before = free_frames;
    uint32_t n = 0;
    int ok = 1;

    while (n < SELFTEST_COUNT) {
        uint32_t f = pmm_alloc_frame();
        if (f == 0)
            break;
        got[n++] = f;
    }
    if (n != SELFTEST_COUNT) {
        log_puts("PMM self-test: FAIL, could only allocate ");
        log_dec(n);
        log_puts(" frames\n");
        ok = 0;
    }

    uint32_t kernel_end = kernel_end_addr();
    for (uint32_t i = 0; i < n && ok; i++) {
        uint32_t f = got[i];
        if (f % PMM_FRAME_SIZE != 0 || f < kernel_end) {
            log_puts("PMM self-test: FAIL, bad frame ");
            log_hex(f);
            log_putc('\n');
            ok = 0;
            break;
        }
        for (uint32_t j = 0; j < i; j++) {
            if (got[j] == f) {
                log_puts("PMM self-test: FAIL, frame handed out twice: ");
                log_hex(f);
                log_putc('\n');
                ok = 0;
                break;
            }
        }
    }

    for (uint32_t i = 0; i < n; i++)
        pmm_free_frame(got[i]);

    /* Freeing a frame that is already free must change nothing. */
    uint32_t after_free = free_frames;
    if (n > 0)
        pmm_free_frame(got[0]);
    if (free_frames != after_free) {
        log_puts("PMM self-test: FAIL, double free changed the count\n");
        ok = 0;
    }

    if (free_frames != before) {
        log_puts("PMM self-test: FAIL, free count did not return to start\n");
        ok = 0;
    }

    if (ok)
        log_puts("PMM self-test: PASS (256 frames allocated, unique, above the kernel, freed, double free ignored)\n");
    return ok ? 0 : -1;
}
