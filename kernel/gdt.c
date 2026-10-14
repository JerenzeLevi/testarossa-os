#include <stdint.h>
#include "gdt.h"

/* Flat 4 GiB code and data segments. Paging is off, so segments only set
 * privilege and default operand size; base is 0 and limit covers all memory. */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr gdtp;

/* Defined in kernel/cpu.asm: lgdt, reload segment registers, reload CS. */
extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].access      = access;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].base_high   = (base >> 24) & 0xFF;
}

void gdt_init(void) {
    gdt_set(0, 0, 0, 0, 0);             /* null descriptor, required */
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xCF); /* kernel code: present, ring 0, exec/read, 4 KiB pages, 32-bit */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xCF); /* kernel data: present, ring 0, read/write */

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint32_t)&gdt;
    gdt_flush((uint32_t)&gdtp);
}
