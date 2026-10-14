# Verification Log

This file is the record of what has been tested, how, and what happened.
It is kept in the repository root and should not be moved or deleted.
Add a new entry for every milestone. Never remove an old entry. If an old
result turns out to be wrong, add a correction underneath it.

How to run each check is in `to-test.txt` (classmate version) and in
README.md (build and run).


## Summary

| # | Milestone | Commit | Build | Boot in QEMU | Keyboard / shell | Result |
|---|-----------|--------|-------|--------------|------------------|--------|
| 1 | Baseline boot skeleton (Vesper name) | `fabd52e` | pass | pass (serial) | n/a | PASS |
| 2 | Rename to Testarossa OS | `da05cbe` | pass | pass (serial) | n/a | PASS |
| 3 | GDT, IDT, PIC, IRQ1 keyboard | `3b930ce` | pass | pass (serial) | pass (monitor keys) | PASS |
| 4 | Shell (help, about, version, clear, echo) | this commit | pass | pass (serial + screen) | pass | PASS |
| 5 | Timer (100 Hz) and uptime command | this commit | pass | pass (serial) | pass | PASS |
| 6 | Exception path (fault-test build) | this commit | pass | pass (serial) | n/a | PASS |
| 7 | Dark cyber-gothic text palette | next commit | pass | pass (screen) | n/a | PASS |
| 8 | Physical memory allocator, PMM self-test, `mem` | next commit | pass | pass (serial) | pass | PASS |
| 9 | RAM filesystem, `ls`, `cat` | next commit | pass | pass (serial + screen) | pass | PASS |
| 10 | Backspace across a wrapped line | next commit | n/a | pass (screen) | **BUG** | KNOWN BUG, left for tester challenge D2 |

---

## Entry 1 - Baseline (fabd52e)

- Environment: WSL2 Ubuntu. Installed `nasm` and `qemu-system-x86` as root.
- `make` from clean: pass. Linker warnings: missing `.note.GNU-stack`, RWX LOAD segment.
- `qemu-system-i386 -kernel build/vesper.elf -serial stdio -display none`:
  serial printed `Vesper OS v0.1.0 booting...` and kernel ran.

## Entry 2 - Rename (da05cbe)

- `make clean && make`: pass. Output `build/testarossa.elf`.
- QEMU headless: serial printed `Testarossa OS v0.1.0 booting...`.
- `grep -ri vesper` over code and docs (excluding the master plan): no matches.

## Entry 3 - GDT, IDT, PIC, keyboard (3b930ce)

- First build failed: `keyboard_getc` implicit declaration (missing include). Fixed.
- `make clean && make`: pass, no GNU-stack warnings.
- QEMU headless with monitor on TCP 55444. Sent keys: `h`, `i`, `ret`,
  `shift-a`, `shift-b`, `backspace`, `c`.
- Serial result: `hi`, then `AB`, backspace, `c` -> `AB<BS> c`. Matches the input.
- Not tested in this entry: the exception path.

## Entry 4 - Shell (this commit)

- Build: pass.
- QEMU headless. Driver: `scratchpad/drive_shell.py` typed `help`, `about`,
  `echo hello there`, `uptime`, `bogus`, `version`, `clear`, `version`.
- Serial log matched the expected output for each command.
- Screen captures (`docs/screenshots/`) checked by eye:
  - `shell-help.png`: help list and about text render correctly.
  - `shell-mixed.png`: echo, unknown command, version all correct.
  - `shell-clear.png`: `clear` removed the old text; prompt at top.
- Bug found and fixed: the hardware VGA cursor stayed at an old location
  after output. Fix: `vga_sync_cursor()` updates ports `0x3D4`/`0x3D5`
  after every write. Re-checked by screenshot: cursor sits after the prompt.

## Entry 5 - Timer and uptime (this commit)

- PIT programmed to 100 Hz (divisor 11931). IRQ0 unmasked.
- `uptime` after about 7 seconds of run time: `up 7 s (739 ticks at 100 Hz)`.
  739 ticks in about 7.4 s matches 100 Hz.
- Known gap: the uptime test relies on a manual timing check. A repeatable
  check is in `to-test.txt`, Test 10.

## Entry 6 - Exception path (this commit)

- `make clean && make fault-test` built the kernel with `-DTESTAROSSA_FAULT_TEST`.
- Serial result after startup lines:
  `!! EXCEPTION divide error vector=0x00000000 err=0x00000000 eip=0x00100450`
  then `system halted`. The CPU stopped as designed.
- Restored the normal build with `make clean && make`.

---

## Entry 7 - Dark cyber-gothic text palette (next commit)

- Changed from light magenta on black (the old Vesper look) to the palette in
  DESIGN.md section 3.1: body light grey, prompt and focus light cyan,
  banner and errors light magenta, version dark grey. Background black.
- Build: pass. Checked by screenshots `docs/screenshots/shell-errors-help.png`:
  prompt cyan, error lines magenta, body grey.
- Serial output is unchanged (colour does not reach serial).
- Exception output also uses the accent colour (checked with `make fault-test`).

## Entry 8 - Physical memory allocator and `mem` (next commit)

- `pmm_init` reads the Multiboot map (bit 6 of the boot info flags). QEMU
  default run: `PMM: memory map read, 129856 KiB free`.
- Self-test at boot: 256 frames allocated, checked for alignment, uniqueness,
  and being above the kernel; freed; a double free was checked to change nothing;
  the free count was checked to return to the start. Result: PASS on every boot tested.
- `mem` in the shell: `usable 130556 KiB`, `free 129856 KiB`, `in use 700 KiB`.
  The "in use" value is the 640 KiB of low memory plus the kernel image.
- Known limitation: "usable" is the bootloader's figure and includes the low
  memory that the allocator reserves, so it is not the same as the free-RAM count.
- Not yet tested: a machine without a memory map (the fallback path). It can be
  checked only by building a Multiboot test that clears the flag.

## Entry 9 - RAM filesystem, `ls`, `cat` (next commit)

- Three built-in files: `/readme.txt`, `/archive/entry01.txt`, `/system/about.txt`.
- `ls` prints the three paths and `3 files`.
- `cat` prints each file (checked by screen capture `shell-cat.png`).
- Errors: `cat /nope.txt` gives "no such file". `cat` alone gives the usage line.
  Both are in the accent colour.
- Trailing spaces after a path are removed before lookup.

## Entry 10 - Backspace across a wrapped line (known bug)

- Reproduced in QEMU with `scratchpad/drive_d2.py`-style input: 100 letters
  (wrap past column 80), then 50 Backspaces, then `z`.
- Result: the screen still shows the first 68 letters and `z` is placed at the
  start of row 2. Expected: the line shows 50 letters then `z`.
- Cause (from reading `vga_putc`): Backspace at column 0 does nothing, so it
  does not move back to the end of the previous row. The shell's line length
  still goes down, so the screen and the line buffer disagree.
- Status: left unfixed on purpose. It is debug challenge D2 in `to-test.txt`.
  Fix it in a separate commit and record it here.
- Screenshot: `docs/screenshots/bug-backspace-wrap.png`.

---

## Open items (not yet verified)

- Real PS/2 keyboard hardware (only tested through the QEMU monitor).
- Exception paths other than divide error (page fault, general protection).
- Long-run stability (hours of uptime). Tick counter wraps after about 497 days.
- Memory map fallback path (no bootloader map).
- Backspace across a wrapped line: known bug, see Entry 10.
- Paging, heap, framebuffer, desktop: not built yet.
