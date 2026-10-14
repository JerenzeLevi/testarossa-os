#include <stdint.h>
#include "shell.h"
#include "keyboard.h"
#include "log.h"
#include "serial.h"
#include "timer.h"
#include "vga.h"
#include "version.h"

#define LINE_MAX 127

static char line[LINE_MAX + 1];
static int line_len;

/* Returns 1 if s starts with the word w followed by a space or the end. */
static int starts_word(const char *s, const char *w) {
    while (*w) {
        if (*s != *w)
            return 0;
        s++;
        w++;
    }
    return *s == '\0' || *s == ' ';
}

static void prompt(void) {
    log_puts("testarossa> ");
}

static void cmd_help(void) {
    log_puts("Commands:\n");
    log_puts("  help      list commands\n");
    log_puts("  about     what Testarossa OS is and how it was built\n");
    log_puts("  version   print the version\n");
    log_puts("  clear     clear the screen\n");
    log_puts("  echo ...  print the text that follows\n");
    log_puts("Not built yet: mem, ls, cat\n");
}

static void cmd_about(void) {
    log_puts(TESTAROSSA_NAME " " TESTAROSSA_VERSION "\n");
    log_puts("  A from-scratch 32-bit x86 educational operating system.\n");
    log_puts("  CPU mode: 32-bit protected mode (i386)\n");
    log_puts("  Kernel: monolithic, freestanding C and NASM\n");
    log_puts("  Output: VGA text 80x25 and serial COM1\n");
    log_puts("  Input: PS/2 keyboard via IRQ1\n");
}

static void cmd_version(void) {
    log_puts(TESTAROSSA_NAME " " TESTAROSSA_VERSION "\n");
}

static void cmd_uptime(void) {
    uint32_t t = timer_ticks();
    log_puts("up ");
    log_dec(t / TIMER_HZ);
    log_puts(" s (");
    log_dec(t);
    log_puts(" ticks at ");
    log_dec(TIMER_HZ);
    log_puts(" Hz)\n");
}

static void cmd_clear(void) {
    vga_clear();
    serial_puts("\x1b[2J\x1b[H");   /* ANSI clear + home for a serial terminal */
}

static void cmd_echo(const char *arg) {
    log_puts(arg);
    log_putc('\n');
}

static void cmd_not_built(const char *name, const char *needs) {
    log_puts(name);
    log_puts(": not implemented yet (needs ");
    log_puts(needs);
    log_puts(")\n");
}

static void execute(const char *cmd) {
    /* Skip leading spaces. */
    while (*cmd == ' ')
        cmd++;
    if (*cmd == '\0')
        return;

    if (starts_word(cmd, "help")) {
        cmd_help();
    } else if (starts_word(cmd, "about")) {
        cmd_about();
    } else if (starts_word(cmd, "version")) {
        cmd_version();
    } else if (starts_word(cmd, "clear")) {
        cmd_clear();
    } else if (starts_word(cmd, "echo")) {
        const char *arg = cmd + 4;
        while (*arg == ' ')
            arg++;
        cmd_echo(arg);
    } else if (starts_word(cmd, "uptime")) {
        cmd_uptime();
    } else if (starts_word(cmd, "mem")) {
        cmd_not_built("mem", "the physical memory allocator");
    } else if (starts_word(cmd, "ls")) {
        cmd_not_built("ls", "RAMFS");
    } else if (starts_word(cmd, "cat")) {
        cmd_not_built("cat", "RAMFS");
    } else {
        log_puts("unknown command: ");
        log_puts(cmd);
        log_puts("  (type help)\n");
    }
}

void shell_run(void) {
    log_puts("Shell ready. Type help.\n");
    prompt();

    for (;;) {
        /* Same pattern as before: check the queue with interrupts off, then
         * "sti; hlt" so a key pressed in between cannot be missed. */
        __asm__ volatile("cli");
        int c = keyboard_getc();
        if (c < 0) {
            __asm__ volatile("sti; hlt");
            continue;
        }
        __asm__ volatile("sti");

        if (c == '\n') {
            log_putc('\n');
            line[line_len] = '\0';
            execute(line);
            line_len = 0;
            prompt();
        } else if (c == '\b') {
            if (line_len > 0) {
                line_len--;
                log_putc('\b');
            }
        } else if (c >= 32 && c < 127) {
            if (line_len < LINE_MAX) {
                line[line_len++] = (char)c;
                log_putc((char)c);
            }
        }
        /* Other keys (Tab, control codes) are ignored for now. */
    }
}
