#include <stdint.h>
#include "framebuffer.h"
#include "io.h"
#include "log.h"
#include "paging.h"

/* PCI configuration access ports. */
#define PCI_ADDR  0xCF8
#define PCI_DATA  0xCFC

/* QEMU's standard VGA card: vendor 0x1234, device 0x1111. */
#define BOCHS_VENDOR 0x1234
#define BOCHS_DEVICE 0x1111

/* Bochs VBE display interface: an index port and a data port. */
#define VBE_INDEX 0x1CE
#define VBE_DATA  0x1CF

#define VBE_ID          0
#define VBE_XRES        1
#define VBE_YRES        2
#define VBE_BPP         3
#define VBE_ENABLE      4
#define VBE_VIRT_WIDTH  6
#define VBE_VIRT_HEIGHT 7
#define VBE_X_OFFSET    8
#define VBE_Y_OFFSET    9

#define VBE_ID_MIN      0xB0C0
#define VBE_ID_MAX      0xB0C5
#define VBE_ENABLED     0x01
#define VBE_LFB         0x40        /* use the linear framebuffer */

static uint32_t backbuf[FB_WIDTH * FB_HEIGHT] __attribute__((aligned(16)));
static uint32_t lfb_phys;
static uint32_t pitch_bytes;
static int ready;

static uint32_t pci_read32(uint8_t dev, uint8_t off) {
    outl(PCI_ADDR, 0x80000000u | ((uint32_t)dev << 11) | (off & 0xFCu));
    return inl(PCI_DATA);
}

static void vbe_write(uint16_t index, uint16_t value) {
    outw(VBE_INDEX, index);
    outw(VBE_DATA, value);
}

static uint16_t vbe_read(uint16_t index) {
    outw(VBE_INDEX, index);
    return inw(VBE_DATA);
}

static int fail(const char *why) {
    log_puts("framebuffer: ");
    log_puts(why);
    log_putc('\n');
    return -1;
}

int fb_init(void) {
    /* 1. Find the VGA card on PCI bus 0. */
    int dev = -1;
    for (int d = 0; d < 32; d++) {
        if (pci_read32((uint8_t)d, 0) == ((BOCHS_DEVICE << 16) | BOCHS_VENDOR)) {
            dev = d;
            break;
        }
    }
    if (dev < 0)
        return fail("no VGA card on PCI (QEMU needs -vga std)");

    /* 2. Its BAR0 must be a memory window, and it gives the framebuffer address. */
    uint32_t bar0 = pci_read32((uint8_t)dev, 0x10);
    if (bar0 & 1)
        return fail("VGA BAR0 is an I/O window, not memory");
    uint32_t lfb = bar0 & 0xFFFFFFF0u;
    if (lfb == 0)
        return fail("VGA BAR0 is zero");

    /* 3. The VBE interface must answer with a known ID. */
    uint16_t id = vbe_read(VBE_ID);
    if (id < VBE_ID_MIN || id > VBE_ID_MAX)
        return fail("no Bochs VBE interface answered");

    /* 4. Ask for the mode, then check the device accepted it. */
    vbe_write(VBE_ENABLE, 0);
    vbe_write(VBE_XRES, FB_WIDTH);
    vbe_write(VBE_YRES, FB_HEIGHT);
    vbe_write(VBE_BPP, FB_BPP);
    vbe_write(VBE_VIRT_WIDTH, FB_WIDTH);
    vbe_write(VBE_VIRT_HEIGHT, FB_HEIGHT);
    vbe_write(VBE_X_OFFSET, 0);
    vbe_write(VBE_Y_OFFSET, 0);
    vbe_write(VBE_ENABLE, VBE_ENABLED | VBE_LFB);

    if (vbe_read(VBE_XRES) != FB_WIDTH || vbe_read(VBE_YRES) != FB_HEIGHT)
        return fail("device did not accept the mode size");
    if (vbe_read(VBE_BPP) != FB_BPP)
        return fail("device did not accept 32 bits per pixel");
    if (!(vbe_read(VBE_ENABLE) & VBE_ENABLED))
        return fail("device did not enable the mode");

    uint32_t pitch = (uint32_t)vbe_read(VBE_VIRT_WIDTH) * (FB_BPP / 8);
    if (pitch < FB_WIDTH * (FB_BPP / 8))
        return fail("row pitch is smaller than one row of pixels");

    /* 5. Map the framebuffer memory so the kernel can write to it. */
    if (paging_identity_large(lfb, pitch * FB_HEIGHT) != 0)
        return fail("could not map the framebuffer memory");

    lfb_phys = lfb;
    pitch_bytes = pitch;
    ready = 1;

    log_puts("framebuffer: PCI slot ");
    log_dec((uint32_t)dev);
    log_puts(", memory at ");
    log_hex(lfb);
    log_puts(", ");
    log_dec(FB_WIDTH);
    log_puts("x");
    log_dec(FB_HEIGHT);
    log_puts("x");
    log_dec(FB_BPP);
    log_puts(", pitch ");
    log_dec(pitch);
    log_putc('\n');
    return 0;
}

int fb_ready(void) {
    return ready;
}

uint32_t fb_width(void) {
    return FB_WIDTH;
}

uint32_t fb_height(void) {
    return FB_HEIGHT;
}

uint32_t *fb_backbuffer(void) {
    return backbuf;
}

void fb_present(void) {
    if (!ready)
        return;
    volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)lfb_phys;
    uint32_t stride = pitch_bytes / 4;
    for (uint32_t y = 0; y < FB_HEIGHT; y++) {
        const uint32_t *src = &backbuf[y * FB_WIDTH];
        volatile uint32_t *row = &dst[y * stride];
        for (uint32_t x = 0; x < FB_WIDTH; x++)
            row[x] = src[x];
    }
}
