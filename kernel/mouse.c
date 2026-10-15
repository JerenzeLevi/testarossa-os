#include <stdint.h>
#include "mouse.h"
#include "framebuffer.h"
#include "io.h"

/* 8042 keyboard controller: data port 0x60, command/status port 0x64. */
#define KBC_DATA    0x60
#define KBC_CMD     0x64
#define KBC_STATUS  0x64
#define KBC_OBF     0x01          /* output buffer full: data waiting to be read */
#define KBC_IBF     0x02          /* input buffer full: controller not ready for a write */
#define KBC_AUX     0x20          /* the waiting byte came from the mouse, not the keyboard */

#define KBC_CMD_ENABLE_AUX   0xA8
#define KBC_CMD_READ_CONFIG  0x20
#define KBC_CMD_WRITE_CONFIG 0x60
#define KBC_CMD_WRITE_AUX    0xD4  /* the next data byte goes to the mouse */

#define CFG_AUX_IRQ     0x02
#define CFG_AUX_CLOCK_OFF 0x20

#define MOUSE_ACK       0xFA
#define MOUSE_SET_DEFAULTS 0xF6
#define MOUSE_ENABLE    0xF4

static volatile int pos_x = FB_WIDTH / 2;
static volatile int pos_y = FB_HEIGHT / 2;
static volatile uint8_t buttons;
static uint8_t packet[3];
static int packet_len;
static int ready;

static int wait_input(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (!(inb(KBC_STATUS) & KBC_IBF))
            return 0;
    }
    return -1;
}

static int wait_output(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (inb(KBC_STATUS) & KBC_OBF)
            return 0;
    }
    return -1;
}

/* Send one byte to the mouse and wait for its acknowledgement. */
static int mouse_write(uint8_t byte) {
    if (wait_input() != 0)
        return -1;
    outb(KBC_CMD, KBC_CMD_WRITE_AUX);
    if (wait_input() != 0)
        return -1;
    outb(KBC_DATA, byte);
    if (wait_output() != 0)
        return -1;
    return inb(KBC_DATA) == MOUSE_ACK ? 0 : -1;
}

int mouse_init(void) {
    /* Drop any byte already waiting (a key pressed during boot), so it is not
     * read below as the mouse's acknowledgement. */
    for (int i = 0; i < 16 && (inb(KBC_STATUS) & KBC_OBF); i++)
        (void)inb(KBC_DATA);

    if (wait_input() != 0)
        return -1;
    outb(KBC_CMD, KBC_CMD_ENABLE_AUX);

    /* Turn on the mouse IRQ (bit 1) and the mouse clock (clear bit 5 of the config byte). */
    if (wait_input() != 0)
        return -1;
    outb(KBC_CMD, KBC_CMD_READ_CONFIG);
    if (wait_output() != 0)
        return -1;
    uint8_t cfg = inb(KBC_DATA);
    cfg |= CFG_AUX_IRQ;
    cfg &= (uint8_t)~CFG_AUX_CLOCK_OFF;
    if (wait_input() != 0)
        return -1;
    outb(KBC_CMD, KBC_CMD_WRITE_CONFIG);
    if (wait_input() != 0)
        return -1;
    outb(KBC_DATA, cfg);

    if (mouse_write(MOUSE_SET_DEFAULTS) != 0)
        return -1;
    if (mouse_write(MOUSE_ENABLE) != 0)
        return -1;

    ready = 1;
    return 0;
}

void mouse_irq_handler(void) {
    if ((inb(KBC_STATUS) & (KBC_OBF | KBC_AUX)) != (KBC_OBF | KBC_AUX))
        return;                         /* nothing from the mouse is waiting */
    uint8_t b = inb(KBC_DATA);

    /* Byte 0 always has bit 3 set. Use that to find the start of a packet. */
    if (packet_len == 0 && !(b & 0x08))
        return;
    packet[packet_len++] = b;
    if (packet_len < 3)
        return;
    packet_len = 0;

    uint8_t flags = packet[0];
    if (flags & 0xC0)                   /* X or Y overflow: discard the packet */
        return;

    int dx = packet[1];
    int dy = packet[2];
    if (flags & 0x10)
        dx -= 256;
    if (flags & 0x20)
        dy -= 256;

    int nx = pos_x + dx;
    int ny = pos_y - dy;                /* the mouse reports Y up as positive */
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx >= (int)FB_WIDTH) nx = (int)FB_WIDTH - 1;
    if (ny >= (int)FB_HEIGHT) ny = (int)FB_HEIGHT - 1;
    pos_x = nx;
    pos_y = ny;
    buttons = flags & 0x07;
}

void mouse_state(int *x, int *y, uint8_t *b) {
    if (!ready) {
        *x = -1;
        *y = -1;
        *b = 0;
        return;
    }
    *x = pos_x;
    *y = pos_y;
    *b = buttons;
}
