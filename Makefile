CC      := gcc
AS      := nasm
LD      := ld
QEMU    := qemu-system-i386

CFLAGS  := -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -fno-stack-protector -fno-pic -fno-builtin -nostdlib -I kernel
ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

BUILD   := build
KERNEL  := $(BUILD)/testarossa.elf

ASM_SRC := $(wildcard boot/*.asm) $(wildcard kernel/*.asm)
C_SRC   := $(wildcard kernel/*.c)
OBJS    := $(addprefix $(BUILD)/,$(notdir $(ASM_SRC:.asm=.o) $(C_SRC:.c=.o)))

.PHONY: all run clean

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
	$(QEMU) -kernel $(KERNEL) -serial stdio

clean:
	rm -rf $(BUILD)
