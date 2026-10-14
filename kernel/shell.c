#include <stdint.h>
#include "shell.h"
#include "heap.h"
#include "keyboard.h"
#include "log.h"
#include "pmm.h"
#include "ramfs.h"
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
    vga_set_color(VGA_COLOR_FOCUS);
    log_puts("testarossa>");
    vga_set_color(VGA_COLOR_BODY);
    log_puts(" ");
}

/* Error lines use the accent colour so they stand out from normal output. */
static void error_begin(void) {
    vga_set_color(VGA_COLOR_ACCENT);
}

static void error_end(void) {
    vga_set_color(VGA_COLOR_BODY);
}

static void cmd_help(void) {
    log_puts("Commands:\n");
    log_puts("  help        list commands\n");
    log_puts("  about       what Testarossa OS is and how it was built\n");
    log_puts("  version     print the version\n");
    log_puts("  clear       clear the screen\n");
    log_puts("  echo ...    print the text that follows\n");
    log_puts("  uptime      time since boot, from the 100 Hz timer\n");
    log_puts("  mem         physical memory from the frame allocator\n");
    log_puts("  ls          list the built-in archive files\n");
    log_puts("  cat <path>  print one archive file, e.g. cat /readme.txt\n");
}

static void cmd_about(void) {
    log_puts(TESTAROSSA_NAME " " TESTAROSSA_VERSION "\n");
    log_puts("  A from-scratch 32-bit x86 educational operating system.\n");
    log_puts("  CPU mode: 32-bit protected mode (i386)\n");
    log_puts("  Kernel: monolithic, freestanding C and NASM\n");
    log_puts("  Output: VGA text 80x25 and serial COM1\n");
    log_puts("  Input: PS/2 keyboard via IRQ1\n");
    log_puts("  Memory: physical frame allocator, 4 KiB frames\n");
    log_puts("  Files: read-only RAM archive, built in\n");
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

static void cmd_mem(void) {
    uint32_t total = pmm_total_frames();
    if (total == 0) {
        error_begin();
        log_puts("mem: the bootloader gave no memory map\n");
        error_end();
        return;
    }
    uint32_t free_n = pmm_free_frames();
    uint32_t used = total - free_n;

    log_puts("memory, 4 KiB frames:\n");
    log_puts("  usable  ");
    log_dec(total * 4);
    log_puts(" KiB\n");
    log_puts("  free    ");
    log_dec(free_n * 4);
    log_puts(" KiB\n");
    log_puts("  in use  ");
    log_dec(used * 4);
    log_puts(" KiB (kernel, low memory, boot info, allocations)\n");
    log_puts("  heap    ");
    log_dec(heap_free_bytes() / 1024);
    log_puts(" KiB free of ");
    log_dec(heap_total_bytes() / 1024);
    log_puts(" KiB\n");
}

static void cmd_ls(void) {
    int n = ramfs_count();
    for (int i = 0; i < n; i++) {
        log_puts("  ");
        log_puts(ramfs_name(i));
        log_putc('\n');
    }
    log_dec((uint32_t)n);
    log_puts(" files\n");
}

static void cmd_cat(const char *arg) {
    if (*arg == '\0') {
        error_begin();
        log_puts("cat: usage: cat <path>\n");
        error_end();
        return;
    }
    const char *data = ramfs_read(arg);
    if (!data) {
        error_begin();
        log_puts("cat: no such file: ");
        log_puts(arg);
        log_putc('\n');
        error_end();
        return;
    }
    log_puts(data);
}

static void cmd_clear(void) {
    vga_clear();
    serial_puts("\x1b[2J\x1b[H");   /* ANSI clear + home for a serial terminal */
}

static void cmd_echo(const char *arg) {
    log_puts(arg);
    log_putc('\n');
}

static void cmd_unknown(const char *cmd) {
    error_begin();
    log_puts("unknown command: ");
    log_puts(cmd);
    log_puts("  (type help)\n");
    error_end();
}

/* Advance *p past any spaces. */
static void skip_spaces(char **p) {
    while (**p == ' ')
        (*p)++;
}

static void execute(char *cmd) {
    skip_spaces(&cmd);
    if (*cmd == '\0')
        return;

    /* Trim trailing spaces so "cat /readme.txt  " still finds the file. */
    int len = 0;
    while (cmd[len])
        len++;
    while (len > 0 && cmd[len - 1] == ' ')
        cmd[--len] = '\0';

    if (starts_word(cmd, "help")) {
        cmd_help();
    } else if (starts_word(cmd, "about")) {
        cmd_about();
    } else if (starts_word(cmd, "version")) {
        cmd_version();
    } else if (starts_word(cmd, "clear")) {
        cmd_clear();
    } else if (starts_word(cmd, "echo")) {
        char *arg = cmd + 4;
        skip_spaces(&arg);
        cmd_echo(arg);
    } else if (starts_word(cmd, "uptime")) {
        cmd_uptime();
    } else if (starts_word(cmd, "mem")) {
        cmd_mem();
    } else if (starts_word(cmd, "ls")) {
        cmd_ls();
    } else if (starts_word(cmd, "cat")) {
        char *arg = cmd + 3;
        skip_spaces(&arg);
        cmd_cat(arg);
    } else {
        cmd_unknown(cmd);
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
