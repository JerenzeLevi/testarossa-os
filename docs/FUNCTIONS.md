# Testarossa OS - Function Plan

This file lists every function in the project: what it does, what it takes,
and whether it is done. Planned functions have their intended signature so
teammates build to the same interface.

Status key:
- **DONE** - written and verified in QEMU (see VERIFY.md).
- **PLANNED** - designed here, not written yet.
- **TEST ONLY** - compiled only with a special build flag.

Keep this file in sync with the code. When a function changes signature,
update this file in the same commit.

Source files live flat in `kernel/`. The master plan's sub-folders are
not used, to keep the Makefile simple.


## 1. Boot and CPU setup

### `boot/multiboot.asm`
| Symbol | Status | Purpose |
|---|---|---|
| `_start` | DONE | Multiboot entry. Sets the stack, pushes `eax` (magic) and `ebx` (info pointer), calls `kernel_main`. Halts if `kernel_main` returns. |
| `stack_top` | DONE | 16 KiB kernel stack in `.bss`. |

### `linker.ld`
| Symbol | Status | Purpose |
|---|---|---|
| `_kernel_end` | DONE | First byte after `.bss`. The frame allocator reserves everything below it. |

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
| `void isr_dispatch(regs_t *r)` | DONE | Common C handler. Exceptions (0-31): print name, vector, error code, EIP in accent colour, then halt. Page faults (14) also print CR2 and the error bits. IRQs (32-47): call the device handler, then send EOI. |

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

## 2. Memory

### `kernel/pmm.c` / `kernel/pmm.h` (physical frames)
| Function | Status | Purpose |
|---|---|---|
| `int pmm_init(uint32_t mb_info_addr)` | DONE | Reads the Multiboot memory map (bit 6 of the boot info flags). Marks `type 1` regions free. Reserves low memory below 1 MiB, the kernel image up to `_kernel_end`, the boot info frame, and the map buffer. Falls back to `mem_upper` if no map. Returns `-1` if neither is present. |
| `uint32_t pmm_alloc_frame(void)` | DONE | Returns one free 4 KiB frame address, or `0` when none is free. Frame 0 is never handed out. |
| `void pmm_free_frame(uint32_t phys)` | DONE | Frees a frame. Ignores misaligned, out-of-range, low-memory, and already-free addresses. |
| `uint32_t pmm_total_frames(void)` | DONE | Frames the bootloader reported as usable. `0` means no map. |
| `uint32_t pmm_free_frames(void)` | DONE | Frames free right now. |
| `int pmm_selftest(void)` | DONE | Allocates 256 frames, checks each is aligned, above the kernel, and unique. Frees them, checks a double free changes nothing, and checks the count returns to the start. Prints PASS or FAIL. |

Bitmap: 1 bit per frame, managing up to 1 GiB (32 KiB of `.bss`).

### `kernel/paging.c` / `kernel/paging.h`
| Function | Status | Purpose |
|---|---|---|
| `int paging_init(void)` | DONE | Builds a page directory that identity-maps the first 1 GiB with 4 MiB pages (PSE), loads CR3, sets PG. The directory frame comes from the frame allocator. |
| `int paging_identity_large(uint32_t phys, uint32_t size)` | DONE | Identity-maps the 4 MiB regions covering a range. Used for the framebuffer, which sits above 1 GiB. |
| `int paging_map(uint32_t virt, uint32_t phys, uint32_t flags)` | DONE | Maps one 4 KiB page. Allocates a page table from the frame allocator if needed. Refuses addresses inside a 4 MiB identity region. |
| `void paging_unmap(uint32_t virt)` | DONE | Clears one 4 KiB mapping and flushes the TLB entry. |
| `int paging_is_mapped(uint32_t virt)` | DONE | Returns 1 if a present mapping exists. |
| `int paging_selftest(void)` | DONE | Maps a frame at `0x80000000`, writes through it, checks the physical frame, unmaps, checks it is gone. Prints PASS or FAIL. |
| `static void zero_frame(uint32_t phys)` | DONE | Zeroes one 4 KiB frame. |
| `static void load_cr3(uint32_t phys)`, `static void invlpg(uint32_t virt)` | DONE | Small CPU helpers. |

### `kernel/heap.c` / `kernel/heap.h`
| Function | Status | Purpose |
|---|---|---|
| `void heap_init(void)` | DONE | Sets up one free block over a 512 KiB arena in `.bss`. |
| `void *kmalloc(uint32_t n)` | DONE | First-fit allocation. Sizes round up to 16 bytes. Splits a block when the rest is big enough. Returns `NULL` for size 0 or when nothing fits. |
| `void kfree(void *p)` | DONE | Marks a block free and merges all adjacent free blocks. Ignores `NULL`, pointers outside the arena, and double frees. |
| `uint32_t heap_free_bytes(void)` | DONE | Payload bytes in free blocks. |
| `uint32_t heap_total_bytes(void)` | DONE | Arena size, 512 KiB. |
| `int heap_selftest(void)` | DONE | 64 blocks with patterns, overlap check, free every other block and refill the holes, then check the arena merges back into one block. Prints PASS or FAIL. |

Design choice: the heap uses a fixed arena in `.bss`, not individual PMM
frames. That keeps blocks contiguous and the code short. Noted in the README.

### `kernel/ramfs.c` / `kernel/ramfs.h` (built-in read-only archive)
| Function | Status | Purpose |
|---|---|---|
| `int ramfs_count(void)` | DONE | Number of built-in files (3). |
| `const char *ramfs_name(int index)` | DONE | Path of file `index`, or `NULL` if out of range. |
| `const char *ramfs_read(const char *path)` | DONE | Contents of the file with this exact path, or `NULL`. |
| `static int streq(const char *a, const char *b)` | DONE | String equality. |

Files: `/readme.txt`, `/archive/entry01.txt`, `/system/about.txt`. Original text,
compiled into the kernel. Nothing is written to disk.

## 3. Output

### `kernel/io.h`
| Function | Status | Purpose |
|---|---|---|
| `outb`, `inb`, `outw`, `inw`, `outl`, `inl` | DONE | Port I/O. |
| `io_wait(void)` | DONE | Short delay via port `0x80`, for the PIC. |

### `kernel/vga.c` / `kernel/vga.h`
Palette (constants in `vga.h`, from DESIGN.md section 3.1): `VGA_COLOR_DIM` (8),
`VGA_COLOR_BODY` (7), `VGA_COLOR_FOCUS` (11), `VGA_COLOR_ACCENT` (13). Background is always black.

| Function | Status | Purpose |
|---|---|---|
| `void vga_init(void)` | DONE | Clears the screen. |
| `void vga_clear(void)` | DONE | Fills the 80x25 buffer with spaces, moves the cursor home. |
| `void vga_set_color(uint8_t fg)` | DONE | Sets the foreground colour for later characters. Low 4 bits only. |
| `void vga_putc(char c)` | DONE | One character. Handles `\n`, `\b` (erase previous cell), wrap, and scroll. Updates the hardware cursor. Known bug: `\b` at column 0 does nothing (to-test D1). |
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

## 4. Graphics

### `kernel/framebuffer.c` / `kernel/framebuffer.h`
Mode: 800x600, 32 bits per pixel, pixels stored as `0x00RRGGBB`.
Requires QEMU `-vga std`. The Multiboot video request is not used, because
QEMU's loader does not support it (its message: "multiboot knows VBE. we don't").

| Function | Status | Purpose |
|---|---|---|
| `int fb_init(void)` | DONE | Finds the VGA card on PCI (vendor `0x1234`, device `0x1111`). Reads BAR0 for the framebuffer address. Checks the Bochs VBE ID, sets 800x600x32, reads back width, height, bpp, enable, and virtual width. Maps the framebuffer with `paging_identity_large`. Returns 0 only when every check passes. |
| `int fb_ready(void)` | DONE | 1 after a successful `fb_init`. |
| `uint32_t fb_width(void)`, `uint32_t fb_height(void)` | DONE | Mode size, 800 and 600. |
| `uint32_t *fb_backbuffer(void)` | DONE | The off-screen buffer, 800 x 600 `uint32_t`. Drawing goes here. |
| `void fb_present(void)` | DONE | Copies the back buffer to the framebuffer, row by row, using the device pitch. |
| `static uint32_t pci_read32(uint8_t dev, uint8_t off)` | DONE | PCI configuration read, bus 0. |
| `static void vbe_write(uint16_t index, uint16_t value)`, `vbe_read` | DONE | Bochs VBE index and data ports (`0x1CE`, `0x1CF`). |
| `static int fail(const char *why)` | DONE | Logs one reason and returns -1. |

Memory: the back buffer is 1.9 MB of `.bss`.

### `kernel/draw.c` / `kernel/draw.h`
All calls clip to the screen.

| Function | Status | Purpose |
|---|---|---|
| `void draw_clear(uint32_t color)` | DONE | Fills the back buffer. |
| `void draw_pixel(int x, int y, uint32_t color)` | DONE | One pixel. |
| `void draw_rect(int x, int y, int w, int h, uint32_t color)` | DONE | Filled rectangle. |
| `void draw_frame(int x, int y, int w, int h, int thickness, uint32_t color)` | DONE | Outline rectangle with a thickness. |
| `void draw_line(int x0, int y0, int x1, int y1, uint32_t color)` | DONE | Bresenham line, all directions. |
| `void draw_char(int x, int y, char c, uint32_t color)` | DONE | One 8x16 glyph, transparent background. |
| `void draw_text(int x, int y, const char *s, uint32_t color)` | DONE | A string on one line. Does not wrap. |
| `int draw_text_width(const char *s)` | DONE | Width in pixels of a string. |

### `kernel/font.h` / `kernel/font8x16.c`
| Item | Status | Purpose |
|---|---|---|
| `font8x16_data[256 * 16]` | DONE | 8x16 bitmap font, generated by `tools/make_font.py` from Debian's Lat15-VGA16.psf. Glyph index = character code. Glyph 65 is `A` (checked). |

## 5. Input and the desktop

### `kernel/keyboard.c` / `kernel/keyboard.h`
| Function | Status | Purpose |
|---|---|---|
| `void keyboard_irq_handler(void)` | DONE | IRQ1 handler. Reads port `0x60`. Tracks Shift. Handles the `0xE0` prefix for arrow keys. Translates scancode set 1 to ASCII. Pushes into the 64-byte queue. Ignores key releases. |
| `int keyboard_getc(void)` | DONE | Pops one key, or returns `-1` if the queue is empty. |
| `static void queue_push(char c)` | DONE | Ring buffer write. Drops the key if full. |

Key codes: ASCII for letters, `\n` Enter, `\t` Tab, `\b` Backspace, `27` Esc.
Arrows: `KEY_UP` 0x80, `KEY_DOWN` 0x81, `KEY_LEFT` 0x82, `KEY_RIGHT` 0x83.

### `kernel/desktop.c` / `kernel/desktop.h`
| Function | Status | Purpose |
|---|---|---|
| `void desktop_run(void)` | DONE | Main loop, never returns. Reads keys. Redraws on each key and once per second (the uptime changes). Sleeps with `sti; hlt` between frames. |
| `static void render(void)` | DONE | Draws background, top bar, rail, hub, cards, dock, and the open window, then `fb_present`. |
| `static void handle_key(int c)` | DONE | Arrows and Tab move focus. Enter opens the focused tile, or a file in the archive list. A opens About. Esc goes back, or closes the panel. |
| `static void move_focus(int dx, int dy)` | DONE | Moves focus in the 2x2 hub grid, stopping at the edges. |
| `static void open_tile(enum tile t)` | DONE | Opens the panel for a hub tile. |
| `static void draw_background(void)` | DONE | Midnight fill and 90 fixed stars. |
| `static void draw_topbar(void)` | DONE | Name, version, and live `up HH:MM:SS`. |
| `static void draw_rail(void)` | DONE | Four squares, the focused one lit. |
| `static void draw_hub(void)` | DONE | Four tiles with pixel icons, shadows, and cyan focus outline. |
| `static void draw_tile(...)` | DONE | One tile. |
| `static void draw_cards(void)` | DONE | UPTIME, MEMORY (free frames), HEAP (free bytes). Real values. |
| `static void draw_dock(void)` | DONE | Key hints. |
| `static void draw_window(void)` | DONE | One panel at a time: shadow, title bar, close mark, border, body. |
| `static void draw_window_body(int bx, int by)` | DONE | About, System Monitor, Archive (list and file), Journal, Messages. |
| `static void draw_lines(...)` | DONE | Multi-line text inside a panel. Does not wrap long lines (to-test D2). |
| `static void draw_kv(...)` | DONE | "label value unit" on one line. |
| `static int put_dec(char *out, uint32_t v)`, `put_2d` | DONE | Number formatting without printf. |

Not built in the desktop: mouse, dragging, several windows at once, a Terminal
panel that uses the shell, a Messages feature, icons beyond four simple shapes.

## 6. Shell (text mode)

### `kernel/shell.c` / `kernel/shell.h`
Used when the framebuffer is not available, and in `make run-text`.

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
| `static void cmd_mem(void)` | DONE | Usable, free, and in-use KiB from the frame allocator, plus heap free. |
| `static void cmd_ls(void)` | DONE | Lists every archive path and a count. |
| `static void cmd_cat(const char *arg)` | DONE | Prints one archive file. Usage message if no path. "no such file" if not found. |
| `static void cmd_unknown(const char *cmd)` | DONE | Error line in the accent colour. |
| `static void error_begin(void)` / `error_end(void)` | DONE | Switch to and from the accent colour. |
| `static void skip_spaces(char **p)` | DONE | Advances past spaces. |
| `static int starts_word(const char *s, const char *w)` | DONE | True if `s` begins with the word `w` followed by a space or end. |

Commands: `help`, `about`, `version`, `clear`, `echo`, `uptime`, `mem`, `ls`, `cat`.

## 7. Kernel entry

### `kernel/kernel.c`
| Function | Status | Purpose |
|---|---|---|
| `void kernel_main(uint32_t mb_magic, uint32_t mb_info)` | DONE | Order: VGA and serial, banner, Multiboot check, GDT, IDT, PIC, timer, PMM and self-test, paging and self-test, heap and self-test, framebuffer. If the framebuffer works, the desktop runs. Otherwise the text shell runs. |
| `static void trigger_divide_error(void)` | TEST ONLY | `-DTESTAROSSA_FAULT_TEST` (`make fault-test`). Runs right after the timer starts. |
| Page-fault test read | TEST ONLY | `-DTESTAROSSA_PAGE_FAULT_TEST` (`make pf-test`). Reads `0x80000000` after paging is on. |
| Text-only build | - | `-DTESTAROSSA_NO_GRAPHICS` (`make run-text`) skips the framebuffer. |

## 8. Planned (not written yet)

These are the interfaces still to build. Nothing below is in the code yet.

### Window manager (more than one panel at once)
| Function | Purpose |
|---|---|
| `int window_open(const char *title, int kind)` | Open a panel, return an id or -1 when full. |
| `void window_close(int id)` | Close a panel. |
| `void window_focus(int id)` | Make a panel the active one and raise it. |

### Terminal panel
| Function | Purpose |
|---|---|
| `void term_put(char c)` | Send shell output to a text area inside a window. Needs the log output to be pluggable first. |

### Mouse
| Function | Purpose |
|---|---|
| `void mouse_irq_handler(void)` | Read the PS/2 mouse packets (IRQ12). Optional, last. |

### Scheduler and user mode
Not planned for this exam (see the master plan, P3).

## 9. Rules for writing a function

1. Put the name in this file before writing it, with its purpose and inputs.
2. Use the `TESTAROSSA_` prefix for header guards and the name `Testarossa`
   in visible text.
3. Keep interrupt handlers short: set a flag or count, then return.
4. Check every pointer and every range before using it.
5. Never write to a framebuffer or memory address that was not validated.
6. Say "not implemented yet" for any feature that is not built. Never fake results.
