# Testarossa OS - Function Plan

This file lists every function in the project: what it does, what it takes,
and whether it is done. Planned functions have their intended signature so
teammates build to the same interface.

Status key:
- **DONE** - written and verified in QEMU (see VERIFY.md).
- **PLANNED** - designed here, not written yet.
- **STRETCH** - only if earlier work is stable.

Keep this file in sync with the code. When a function changes signature,
update this file in the same commit.


## 1. Boot and CPU setup

### `boot/multiboot.asm`
| Symbol | Status | Purpose |
|---|---|---|
| `_start` | DONE | Multiboot entry. Sets the stack, pushes `eax` (magic) and `ebx` (info pointer), calls `kernel_main`. Halts if `kernel_main` returns. |
| `stack_top` | DONE | 16 KiB kernel stack in `.bss`. |

### `kernel/cpu.asm`
| Symbol | Status | Purpose |
|---|---|---|
| `gdt_flush(uint32_t ptr)` | DONE | Loads the GDT with `lgdt`, reloads data segments with `0x10`, far-jumps to reload CS with `0x08`. |
| `idt_flush(uint32_t ptr)` | DONE | Loads the IDT with `lidt`. |

### `kernel/gdt.c` / `kernel/gdt.h`
| Function | Status | Purpose |
|---|---|---|
| `void gdt_init(void)` | DONE | Builds 3 descriptors (null, flat ring-0 code, flat ring-0 data) and loads them. |
| `static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)` | DONE | Packs one 8-byte descriptor. |

Constants: `GDT_KERNEL_CODE = 0x08`, `GDT_KERNEL_DATA = 0x10`.

### `kernel/idt.c` / `kernel/idt.h`
| Function | Status | Purpose |
|---|---|---|
| `void idt_init(void)` | DONE | Installs gates for vectors 0-47 from `isr_stub_table` and loads the IDT. |
| `static void idt_set_gate(uint8_t vector, uint32_t handler)` | DONE | Fills one interrupt gate (ring 0, 32-bit interrupt gate). |
| `void isr_dispatch(regs_t *r)` | DONE | Common C handler. Exceptions (0-31): print name, vector, error code, EIP, then halt. IRQs (32-47): call the device handler, then send EOI. |

`regs_t` (in `idt.h`) is the register frame built by `isr_common`. Field order matches the push order.

### `kernel/interrupts.asm`
| Symbol | Status | Purpose |
|---|---|---|
| `isr0` ... `isr31` | DONE | Exception stubs. Vectors 8, 10-14, 17, 30 push no dummy error code. |
| `isr32` ... `isr47` | DONE | Hardware IRQ stubs (IRQ0-15 after PIC remap). |
| `isr_common` | DONE | Saves registers, loads kernel data segments, calls `isr_dispatch`, restores, `iret`. |
| `isr_stub_table` | DONE | Table of the 48 stub addresses, read by `idt_init`. |

### `kernel/pic.c` / `kernel/pic.h`
| Function | Status | Purpose |
|---|---|---|
| `void pic_init(void)` | DONE | Remaps master PIC to vectors 32-39 and slave to 40-47. Unmasks IRQ0 (timer) and IRQ1 (keyboard). Masks all others. |
| `void pic_send_eoi(uint8_t irq)` | DONE | Sends end-of-interrupt. Also to the slave PIC for IRQ 8-15. |

### `kernel/timer.c` / `kernel/timer.h`
| Function | Status | Purpose |
|---|---|---|
| `void timer_init(void)` | DONE | Programs PIT channel 0 for `TIMER_HZ` (100 Hz). |
| `void timer_irq_handler(void)` | DONE | IRQ0 handler. Increments the tick counter only. |
| `uint32_t timer_ticks(void)` | DONE | Returns ticks since `timer_init`. |

Constant: `TIMER_HZ = 100`.

### `kernel/keyboard.c` / `kernel/keyboard.h`
| Function | Status | Purpose |
|---|---|---|
| `void keyboard_irq_handler(void)` | DONE | IRQ1 handler. Reads port `0x60`, tracks Shift, translates scancode set 1 to ASCII, pushes into the queue. Ignores key releases. |
| `int keyboard_getc(void)` | DONE | Pops one character, or returns `-1` if the queue is empty. |
| `static void queue_push(char c)` | DONE | Ring buffer write. Drops the key if the queue is full (64 bytes). |

## 2. Output

### `kernel/io.h`
| Function | Status | Purpose |
|---|---|---|
| `outb(uint16_t port, uint8_t val)` | DONE | Port write. |
| `inb(uint16_t port)` | DONE | Port read. |
| `io_wait(void)` | DONE | Short delay via port `0x80`, for the PIC. |

### `kernel/vga.c` / `kernel/vga.h`
| Function | Status | Purpose |
|---|---|---|
| `void vga_init(void)` | DONE | Clears the screen. |
| `void vga_clear(void)` | DONE | Fills the 80x25 buffer with spaces, moves the cursor home. |
| `void vga_putc(char c)` | DONE | One character. Handles `\n`, `\b` (erase previous cell), wrap, and scroll. Updates the hardware cursor. |
| `void vga_puts(const char *s)` | DONE | Writes a string. |
| `static void vga_scroll(void)` | DONE | Moves rows up by one, clears the last row. |
| `static void vga_sync_cursor(void)` | DONE | Writes the cursor position to VGA ports `0x3D4`/`0x3D5`. |

### `kernel/serial.c` / `kernel/serial.h`
| Function | Status | Purpose |
|---|---|---|
| `void serial_init(void)` | DONE | COM1 at 38400 baud, 8N1, FIFO on. |
| `void serial_putc(char c)` | DONE | Waits for the transmit buffer, then sends one byte. |
| `void serial_puts(const char *s)` | DONE | Sends a string. Converts `\n` to `\r\n`. |

### `kernel/log.c` / `kernel/log.h`
| Function | Status | Purpose |
|---|---|---|
| `void log_putc(char c)` | DONE | Writes to serial and VGA. `\b` erases on both. |
| `void log_puts(const char *s)` | DONE | String version of `log_putc`. |
| `void log_dec(uint32_t v)` | DONE | Prints an unsigned decimal number. |
| `void log_hex(uint32_t v)` | DONE | Prints `0x` plus 8 hex digits. |

## 3. Shell

### `kernel/shell.c` / `kernel/shell.h`
| Function | Status | Purpose |
|---|---|---|
| `void shell_run(void)` | DONE | Main loop. Reads keys, edits the line, runs commands on Enter. Never returns. |
| `static void execute(const char *cmd)` | DONE | Splits the command word from its arguments and dispatches. |
| `static void cmd_help(void)` | DONE | Lists commands. |
| `static void cmd_about(void)` | DONE | Name, version, architecture, I/O facts. |
| `static void cmd_version(void)` | DONE | Name and version. |
| `static void cmd_clear(void)` | DONE | Clears VGA and sends an ANSI clear to serial. |
| `static void cmd_echo(const char *arg)` | DONE | Prints the text after `echo`. |
| `static void cmd_uptime(void)` | DONE | Prints seconds and ticks from `timer_ticks()`. |
| `static void cmd_not_built(const char *name, const char *needs)` | DONE | Honest message for commands whose feature is missing. |
| `static int starts_word(const char *s, const char *w)` | DONE | True if `s` begins with the word `w` followed by a space or end. |

Commands: `help`, `about`, `version`, `clear`, `echo`, `uptime`.
Not built yet: `mem` (needs PMM), `ls` and `cat` (need RAMFS).

## 4. Kernel entry

### `kernel/kernel.c`
| Function | Status | Purpose |
|---|---|---|
| `void kernel_main(uint32_t mb_magic, uint32_t mb_info)` | DONE | Initializes VGA and serial, checks the Multiboot magic, then GDT, IDT, PIC, timer, enables interrupts, and starts the shell. |
| `static void trigger_divide_error(void)` | DONE (test only) | Compiled only with `-DTESTAROSSA_FAULT_TEST` (`make fault-test`). |

## 5. Planned (not written yet)

These are the interfaces to build to. They are not in the code yet.

### Memory - `kernel/memory/pmm.c` / `pmm.h` (Week 2, Day 9-10)
| Function | Purpose |
|---|---|
| `void pmm_init(uint32_t mb_info)` | Read the Multiboot memory map. First check bit 6 (0x40) of the boot info `flags` field is set, which means the map is present. Reserve kernel and boot info ranges, then build a frame bitmap. |
| `uint32_t pmm_alloc_frame(void)` | Return one free 4 KiB frame address, or `0` if none is free. |
| `void pmm_free_frame(uint32_t phys)` | Mark a frame free. Ignores addresses outside managed memory. |
| `uint32_t pmm_free_count(void)` | Number of free frames. Used by System Monitor. |

Acceptance: allocation tests never return a reserved frame.

### Paging - `kernel/memory/paging.c` / `paging.h` (conditional, Week 2)
| Function | Purpose |
|---|---|
| `void paging_init(void)` | Build the page directory and identity-map kernel memory. |
| `void paging_map(uint32_t virt, uint32_t phys, uint32_t flags)` | Map one page. |
| `void page_fault_handler(regs_t *r)` | Decode CR2 and the error code, print them, halt. |

Enable paging only after the page-fault handler prints correctly.

### Heap - `kernel/memory/heap.c` / `heap.h` (conditional)
| Function | Purpose |
|---|---|
| `void heap_init(void)` | Start a simple allocator on top of PMM frames. |
| `void *kmalloc(uint32_t size)` | Allocate `size` bytes. Returns `NULL` when out of memory. |
| `void kfree(void *ptr)` | Free a block from `kmalloc`. |

### RAM filesystem - `kernel/fs/ramfs.c` / `ramfs.h` (Week 4, Day 24)
| Function | Purpose |
|---|---|
| `void ramfs_init(void)` | Load the built-in read-only files: `/readme.txt`, `/archive/entry01.txt`, `/system/about.txt`. |
| `int ramfs_count(void)` | Number of files. |
| `const char *ramfs_name(int i)` | Name of file `i`. |
| `int ramfs_read(const char *path, const char **out, uint32_t *len)` | Find a file. Returns 0 on success. |

Shell then gets `ls` and `cat <path>` built on these.

### Framebuffer - `kernel/gui/framebuffer.c` / `framebuffer.h` (Week 3, Day 15-16)
| Function | Purpose |
|---|---|
| `int fb_init(uint32_t mb_info)` | Read the framebuffer tag. Validate address, width, height, pitch, bpp. Return 0 only if the mode is usable. |
| `void fb_put_pixel(int x, int y, uint32_t color)` | Clipped write. Respects pitch and pixel format. |
| `int fb_width(void)`, `int fb_height(void)` | Validated mode size. |

Boot cost: the Multiboot header flags must request a video mode (flag bit 0x4
plus the video fields). That changes the boot protocol, so it needs a
known-good checkpoint first (see the master plan, section 7).

### Drawing - `kernel/gui/draw.c` / `draw.h` (Week 3, Day 17-18)
| Function | Purpose |
|---|---|
| `void draw_fill_rect(int x, int y, int w, int h, uint32_t c)` | Filled rectangle, clipped. |
| `void draw_outline_rect(int x, int y, int w, int h, uint32_t c)` | 1-pixel border. |
| `void draw_line(int x0, int y0, int x1, int y1, uint32_t c)` | Bresenham line. |

### Font - `kernel/gui/font.c` / `font.h` (Week 3, Day 17-18)
| Function | Purpose |
|---|---|
| `void font_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg)` | Draw one 8x16 glyph. |
| `void font_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg)` | Draw a string. |

### Desktop - `kernel/gui/desktop.c` / `desktop.h` (Week 3, Day 19-21)
| Function | Purpose |
|---|---|
| `void desktop_init(void)` | Set the initial focus (first hub tile). |
| `void desktop_draw(void)` | Redraw background, top bar, rail, hub, cards, dock. |
| `void desktop_handle_key(int key)` | Tab, arrows, Enter, Esc, number keys. Updates focus and open windows. |

### Windows - `kernel/gui/window.c` / `window.h` (Week 3-4)
| Function | Purpose |
|---|---|
| `int window_open(const char *title, int kind)` | Open a panel. Returns an id, or -1 when the limit (4) is reached. |
| `void window_close(int id)` | Close a panel. |
| `void window_draw(int id, int focused)` | Draw chrome and body. |

### Apps - `kernel/apps/` (Week 4, Day 22-23)
| File | Function | Purpose |
|---|---|---|
| `about.c` | `void app_about_draw(int x, int y, int w, int h)` | Name, version, build details. |
| `system_info.c` | `void app_sysmon_draw(int x, int y, int w, int h)` | Real uptime from `timer_ticks()`. Free frames only if PMM exists. |
| `files.c` | `void app_files_draw(int x, int y, int w, int h)` | Lists RAMFS files. Only if RAMFS exists. |

## 6. Rules for writing a function

1. Put the name in this file before writing it, with its purpose and inputs.
2. Use the `TESTAROSSA_` prefix for header guards and the name `Testarossa`
   in visible text.
3. Keep interrupt handlers short: set a flag or count, then return.
4. Check every pointer and every range before using it.
5. Never write to a framebuffer or memory address that was not validated.
6. Say "not implemented yet" for any feature that is not built. Never fake results.
