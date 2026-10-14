#ifndef TESTAROSSA_DESKTOP_H
#define TESTAROSSA_DESKTOP_H

/* The Testarossa desktop. Draws to the framebuffer and reads keys from the
 * keyboard queue. Never returns. Call only after fb_init() has succeeded. */
void desktop_run(void);

#endif
