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

Source files live flat in `kernel/` for now. The master plan's sub-folders
(`memory/`, `fs/`, `gui/`, `apps/`) are created only when a planned module
is written.


## 1. Boot and CPU setup

### `boot/multiboot.asm`
| Symbol | Status | Purpose |
|---|---|---|
| `_start` | DONE | Multiboot entry. Sets the stack, pushes `eax` (magic) and `ebx` (info pointer), calls `kernel_main`. Halts if `kernel_main` returns. |
| `stack_top` | DONE | 16 KiB kernel stack in `.bss`. |

### `linker.ld`
| Symbol | Status | Purpose |
|---|---|---|
| `_kernel_end` | DONE | First byte after `.bss`. The allocator reserves everything below it. |

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
| `void isr_dispatch(regs_t *r)` | DONE | Common C handler. Exceptions (0-31): print name, vector, error code, EIP in accent colour, then halt. IRQs (32-47): call the device handler, then send EOI. |

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

## 2. Memory and built-in files

### `kernel/pmm.c` / `kernel/pmm.h` (physical memory manager)
| Function | Status | Purpose |
|---|---|---|
| `int pmm_init(uint32_t mb_info_addr)` | DONE | Reads the Multiboot memory map (bit 6 of the boot info flags). Marks `type 1` regions free. Reserves: low memory below 1 MiB, the kernel image up to `_kernel_end`, the boot info frame, and the map buffer. Falls back to `mem_upper` if no map. Returns `-1` if neither is present. |
| `uint32_t pmm_alloc_frame(void)` | DONE | Returns one free 4 KiB frame address, or `0` when none is free. Frame 0 is never handed out. |
| `void pmm_free_frame(uint32_t phys)` | DONE | Frees a frame. Ignores misaligned, out-of-range, low-memory, and already-free addresses. |
| `uint32_t pmm_total_frames(void)` | DONE | Frames the bootloader reported as usable. `0` means no map. |
| `uint32_t pmm_free_frames(void)` | DONE | Frames free right now. |
| `int pmm_selftest(void)` | DONE | Allocates 256 frames, checks each is aligned, above the kernel, and unique. Frees them, checks a double free changes nothing, and checks the count returns to the start. Prints PASS or FAIL. |
| `static void make_available(uint64_t start, uint64_t end)` | DONE | Clears bits for whole frames inside a region. Counts each frame once. |
| `static void reserve(uint64_t start, uint64_t end)` | DONE | Sets bits for frames overlapping a region. Only free frames change. |
| `static uint64_t read_u64(const uint8_t *p)` | DONE | Reads a 64-bit field as two 32-bit reads, since map entries may be unaligned. |

Bitmap: 1 bit per frame, managing up to 1 GiB (32 KiB of `.bss`).

### `kernel/ramfs.c` / `kernel/ramfs.h` (built-in read-only archive)
| Function | Status | Purpose |
|---|---|---|
| `int ramfs_count(void)` | DONE | Number of built-in files (3). |
| `const char *ramfs_name(int index)` | DONE | Path of file `index`, or `NULL` if out of range. |
| `const char *ramfs_read(const char *path)` | DONE | Contents of the file with this exact path, or `NULL`. |
| `static int streq(const char *a, const char *b)` | DONE | String equality. |

Files: `/readme.txt`, `/archive/entry01.txt`, `/system/about.txt`. Contents
are original text and are compiled into the kernel. Nothing is written to disk.

## 3. Output

### `kernel/io.h`
| Function | Status | Purpose |
|---|---|---|
| `outb(uint16_t port, uint8_t val)` | DONE | Port write. |
| `inb(uint16_t port)` | DONE | Port read. |
| `io_wait(void)` | DONE | Short delay via port `0x80`, for the PIC. |

### `kernel/vga.c` / `kernel/vga.h`
Palette (constants in `vga.h`, from DESIGN.md section 3.1): `VGA_COLOR_DIM` (8),
`VGA_COLOR_BODY` (7), `VGA_COLOR_FOCUS` (11), `VGA_COLOR_ACCENT` (13). Background is always black.

| Function | Status | Purpose |
|---|---|---|
| `void vga_init(void)` | DONE | Clears the screen. |
| `void vga_clear(void)` | DONE | Fills the 80x25 buffer with spaces, moves the cursor home. |
| `void vga_set_color(uint8_t fg)` | DONE | Sets the foreground colour for later characters. Low 4 bits only. |
| `void vga_putc(char c)` | DONE | One character. Handles `\n`, `\b` (erase previous cell), wrap, and scroll. Updates the hardware cursor. |
| `void vga_puts(const char *s)` | DONE | Writes a string. |
| `static uint16_t make_cell(char c)` | DONE | Builds a screen cell from a character and the current colour. |
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

## 4. Shell

### `kernel/shell.c` / `kernel/shell.h`
| Function | Status | Purpose |
|---|---|---|
| `void shell_run(void)` | DONE | Main loop. Reads keys, edits the line, runs commands on Enter. Never returns. |
| `static void prompt(void)` | DONE | Prints `testarossa> ` with the prompt word in the focus colour. |
| `static void execute(char *cmd)` | DONE | Trims spaces, then dispatches on the command word. |
| `static void cmd_help(void)` | DONE | Lists commands. |
| `static void cmd_about(void)` | DONE | Name, version, architecture, I/O and memory facts. |
| `static void cmd_version(void)` | DONE | Name and version. |
| `static void cmd_clear(void)` | DONE | Clears VGA and sends an ANSI clear to serial. |
| `static void cmd_echo(const char *arg)` | DONE | Prints the text after `echo`. |
| `static void cmd_uptime(void)` | DONE | Prints seconds and ticks from `timer_ticks()`. |
| `static void cmd_mem(void)` | DONE | Prints usable, free, and in-use KiB from the PMM. Says so if there is no map. |
| `static void cmd_ls(void)` | DONE | Lists every RAMFS path and a count. |
| `static void cmd_cat(const char *arg)` | DONE | Prints one RAMFS file. Usage message if no path. "no such file" if not found. |
| `static void cmd_unknown(const char *cmd)` | DONE | Error line in the accent colour. |
| `static void error_begin(void)` / `error_end(void)` | DONE | Switch to and from the accent colour. |
| `static void skip_spaces(char **p)` | DONE | Advances past spaces. |
| `static int starts_word(const char *s, const char *w)` | DONE | True if `s` begins with the word `w` followed by a space or end. |

Commands: `help`, `about`, `version`, `clear`, `echo`, `uptime`, `mem`, `ls`, `cat`.

## 5. Kernel entry

### `kernel/kernel.c`
| Function | Status | Purpose |
|---|---|---|
| `void kernel_main(uint32_t mb_magic, uint32_t mb_info)` | DONE | Initializes VGA and serial, prints the accent banner, checks the Multiboot magic, then GDT, IDT, PIC, timer, PMM and its self-test, enables interrupts, and starts the shell. |
| `static void trigger_divide_error(void)` | DONE (test only) | Compiled only with `-DTESTAROSSA_FAULT_TEST` (`make fault-test`). |

## 6. Planned (not written yet)

These are the interfaces to build to. They are not in the code yet.

### Paging - `kernel/paging.c` / `paging.h` (conditional)
| Function | Purpose |
|---|---|
| `void paging_init(void)` | Build the page directory and identity-map kernel memory. |
| `void paging_map(uint32_t virt, uint32_t phys, uint32_t flags)` | Map one page. |
| `void page_fault_handler(regs_t *r)` | Decode CR2 and the error code, print them, halt. |

Enable paging only after the page-fault handler prints correctly.

### Heap - `kernel/heap.c` / `heap.h` (conditional, after paging)
| Function | Purpose |
|---|---|
| `void heap_init(void)` | Start a simple allocator on top of PMM frames. |
| `void *kmalloc(uint32_t size)` | Allocate `size` bytes. Returns `NULL` when out of memory. |
| `void kfree(void *ptr)` | Free a block from `kmalloc`. |

### Framebuffer - `kernel/framebuffer.c` / `framebuffer.h` (Week 3, Day 15-16)
| Function | Purpose |
|---|---|
| `int fb_init(uint32_t mb_info)` | Read the framebuffer tag. Validate address, width, height, pitch, bpp. Return 0 only if the mode is usable. |
| `void fb_put_pixel(int x, int y, uint32_t color)` | Clipped write. Respects pitch and pixel format. |
| `int fb_width(void)`, `int fb_height(void)` | Validated mode size. |

Boot cost: the Multiboot header flags must request a video mode (flag bit 0x4
plus the video fields). That changes the boot protocol, so it needs a
known-good checkpoint first (see the master plan, section 7).

### Drawing - `kernel/draw.c` / `draw.h` (after framebuffer)
| Function | Purpose |
|---|---|
| `void draw_fill_rect(int x, int y, int w, int h, uint32_t c)` | Filled rectangle, clipped. |
| `void draw_outline_rect(int x, int y, int w, int h, uint32_t c)` | 1-pixel border. |
| `void draw_line(int x0, int y0, int x1, int y1, uint32_t c)` | Bresenham line. |

### Font - `kernel/font.c` / `font.h` (after framebuffer)
| Function | Purpose |
|---|---|
| `void font_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg)` | Draw one 8x16 glyph. |
| `void font_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg)` | Draw a string. |

### Desktop - `kernel/desktop.c` / `desktop.h` (Week 3-4)
| Function | Purpose |
|---|---|
| `void desktop_init(void)` | Set the initial focus (first hub tile). |
| `void desktop_draw(void)` | Redraw background, top bar, rail, hub, cards, dock. |
| `void desktop_handle_key(int key)` | Tab, arrows, Enter, Esc, number keys. Updates focus and open windows. |

### Windows - `kernel/window.c` / `window.h` (Week 4)
| Function | Purpose |
|---|---|
| `int window_open(const char *title, int kind)` | Open a panel. Returns an id, or -1 when the limit (4) is reached. |
| `void window_close(int id)` | Close a panel. |
| `void window_draw(int id, int focused)` | Draw chrome and body. |

### Apps - `kernel/apps.c` (Week 4)
| Function | Purpose |
|---|---|
| `void app_about_draw(int x, int y, int w, int h)` | Name, version, build details. |
| `void app_sysmon_draw(int x, int y, int w, int h)` | Real uptime from `timer_ticks()`, and free frames from `pmm_free_frames()`. |
| `void app_files_draw(int x, int y, int w, int h)` | Lists RAMFS files. |

## 7. Rules for writing a function

1. Put the name in this file before writing it, with its purpose and inputs.
2. Use the `TESTAROSSA_` prefix for header guards and the name `Testarossa`
   in visible text.
3. Keep interrupt handlers short: set a flag or count, then return.
4. Check every pointer and every range before using it.
5. Never write to a framebuffer or memory address that was not validated.
6. Say "not implemented yet" for any feature that is not built. Never fake results.
