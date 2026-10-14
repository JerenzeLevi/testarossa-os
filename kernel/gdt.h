#ifndef TESTAROSSA_GDT_H
#define TESTAROSSA_GDT_H

#define GDT_KERNEL_CODE 0x08    /* GDT entry 1 */
#define GDT_KERNEL_DATA 0x10    /* GDT entry 2 */

void gdt_init(void);

#endif
