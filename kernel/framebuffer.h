#ifndef TESTAROSSA_FRAMEBUFFER_H
#define TESTAROSSA_FRAMEBUFFER_H

#include <stdint.h>

/* Linear framebuffer through the Bochs VBE display interface.
 * QEMU's standard VGA (-vga std) provides it. Other machines are not supported yet. */

#define FB_WIDTH   800
#define FB_HEIGHT  600
#define FB_BPP     32

/* Find the VGA device on PCI, set the mode, read back what the device accepted,
 * and map the framebuffer. Needs paging. Returns 0 only if every check passes;
 * on any failure the caller should fall back to text mode. */
int fb_init(void);

int fb_ready(void);
uint32_t fb_width(void);
uint32_t fb_height(void);

/* Off-screen pixels, FB_WIDTH x FB_HEIGHT, 0x00RRGGBB per pixel. Draw here,
 * then call fb_present() to copy the frame to the screen in one pass. */
uint32_t *fb_backbuffer(void);
void fb_present(void);

#endif
