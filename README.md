# Vesper OS

A from-scratch 32-bit x86 teaching operating system written in C and NASM.
Runs in QEMU.

## Build and run

Inside WSL2 (Ubuntu):

```sh
sudo apt install -y nasm qemu-system-x86 make gcc
make            # builds build/vesper.elf
make run        # boots it in QEMU, serial output goes to the terminal
```

## Roadmap

| Week | Milestone |
|------|-----------|
| 1 | Multiboot entry, VGA and serial output, keyboard via IRQ1 |
| 2 | GDT/IDT, paging, physical frame allocator, kernel heap |
| 3 | Processes, round-robin scheduler, context switch, syscalls via `int 0x80` |
| 4 | Ramdisk filesystem, user-mode shell, demo and write-up |

## Layout

```
boot/      NASM entry code (multiboot header, _start)
kernel/    C kernel: VGA, serial, and later drivers and subsystems
linker.ld  Places the kernel at 1 MiB
build/     Generated output (ignored)
```
