CC      := gcc
AS      := nasm
LD      := ld
QEMU    := qemu-system-i386

CFLAGS  := -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -fno-stack-protector -fno-pic -fno-builtin -nostdlib -I kernel $(EXTRA_CFLAGS)
ASFLAGS := -f elf32
EXTRA_CFLAGS :=                    # e.g. make EXTRA_CFLAGS=-DTESTAROSSA_FAULT_TEST
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

BUILD   := build
KERNEL  := $(BUILD)/testarossa.elf

ASM_SRC := $(wildcard boot/*.asm) $(wildcard kernel/*.asm)
C_SRC   := $(wildcard kernel/*.c)
OBJS    := $(addprefix $(BUILD)/,$(notdir $(ASM_SRC:.asm=.o) $(C_SRC:.c=.o)))

.PHONY: all run clean fault-test pf-test

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: kernel/%.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: boot/%.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

run: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -serial stdio -vga std -no-reboot

# Text shell only: builds without the graphics path. Run `make clean && make` afterwards.
run-text: clean
	$(MAKE) EXTRA_CFLAGS=-DTESTAROSSA_NO_GRAPHICS run

clean:
	rm -rf $(BUILD)

# Builds a kernel that divides by zero on purpose, to check the exception path.
# Afterwards run `make clean && make` to get the normal kernel back.
fault-test: clean
	$(MAKE) EXTRA_CFLAGS=-DTESTAROSSA_FAULT_TEST $(KERNEL)

# Builds a kernel that reads an unmapped address on purpose, to check the page-fault report.
pf-test: clean
	$(MAKE) EXTRA_CFLAGS=-DTESTAROSSA_PAGE_FAULT_TEST $(KERNEL)
