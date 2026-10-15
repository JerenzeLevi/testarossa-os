#include <stdint.h>
#include "io.h"
#include "keyboard.h"

/* PS/2 keyboard data port. The 8042 controller delivers scancode set 1 by default. */
#define KBD_DATA   0x60
#define KBD_STATUS 0x64
#define KBD_AUX    0x20         /* status bit: the waiting byte is from the mouse */

#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_RELEASE  0x80        /* bit 7 set on key release */
#define SC_TABLE_LEN 0x3A       /* make codes 0x00-0x39 are printable or control keys */

/* Scancode set 1 make codes -> ASCII. 0 means no character (modifiers, F-keys, etc.). */
static const char scancode_plain[SC_TABLE_LEN] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  0,    ' '
};

static const char scancode_shift[SC_TABLE_LEN] = {
    0,    27,   '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  0,    ' '
};

#define QUEUE_SIZE 64

static volatile char queue[QUEUE_SIZE];
static volatile unsigned int q_head;   /* next write position */
static volatile unsigned int q_tail;   /* next read position */
static int shift_down;
static int e0_pending;

#define SC_EXTENDED   0xE0
#define SC_EXT_UP     0x48
#define SC_EXT_DOWN   0x50
#define SC_EXT_LEFT   0x4B
#define SC_EXT_RIGHT  0x4D

static void queue_push(char c) {
    unsigned int next = (q_head + 1) % QUEUE_SIZE;
    if (next == q_tail)
        return;                         /* full: drop the key rather than overwrite */
    queue[q_head] = c;
    q_head = next;
}

void keyboard_irq_handler(void) {
    if (inb(KBD_STATUS) & KBD_AUX)
        return;                         /* a mouse byte; IRQ12 reads it */
    uint8_t sc = inb(KBD_DATA);

    /* Arrow keys and a few others send 0xE0 first, then the code. */
    if (sc == SC_EXTENDED) {
        e0_pending = 1;
        return;
    }
    if (e0_pending) {
        e0_pending = 0;
        if (sc & SC_RELEASE)
            return;
        switch (sc) {
        case SC_EXT_UP:    queue_push(KEY_UP);    break;
        case SC_EXT_DOWN:  queue_push(KEY_DOWN);  break;
        case SC_EXT_LEFT:  queue_push(KEY_LEFT);  break;
        case SC_EXT_RIGHT: queue_push(KEY_RIGHT); break;
        default: break;
        }
        return;
    }

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift_down = 1;
        return;
    }
    if (sc == (SC_LSHIFT | SC_RELEASE) || sc == (SC_RSHIFT | SC_RELEASE)) {
        shift_down = 0;
        return;
    }
    if (sc & SC_RELEASE)
        return;                         /* ignore other key releases */
    if (sc >= SC_TABLE_LEN)
        return;

    char c = shift_down ? scancode_shift[sc] : scancode_plain[sc];
    if (c)
        queue_push(c);
}

int keyboard_getc(void) {
    if (q_head == q_tail)
        return -1;
    char c = queue[q_tail];
    q_tail = (q_tail + 1) % QUEUE_SIZE;
    return (unsigned char)c;
}
