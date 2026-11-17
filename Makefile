# Minimal Desktop - build system
#
# Requirements (Ubuntu/WSL):
#   i686-linux-gnu-gcc, nasm, grub-mkrescue, xorriso, mtools, python3, qemu-system-i386
#   sudo tools/install_toolchain.sh
#
# Targets:
#   make            build build/minimal_desktop.iso
#   make iso        same, plus publish to $(PUBLISH_DIR) and website/downloads
#   make run        boot the ISO in QEMU with stdio serial and an RTL8139
#   make autotest   headless selftest, exits 0 on PASS and 1 on FAIL
#   make clean      remove build output

CC      := i686-linux-gnu-gcc
AS      := nasm
PYTHON  := python3

BUILD   := build
OBJDIR  := $(BUILD)/obj
ISODIR  := $(BUILD)/isodir
KERNEL  := $(BUILD)/kernel.elf
STORE   := $(BUILD)/store.img
ISO     := $(BUILD)/minimal_desktop.iso
PUBLISH_DIR ?= /mnt/d

CFLAGS  := -m32 -std=gnu11 -ffreestanding -fno-pic -fno-pie \
           -fno-stack-protector -fno-builtin -fno-asynchronous-unwind-tables \
           -fno-unwind-tables -Wall -Wextra -Wno-unused-parameter -O2 -Iinclude
ASFLAGS := -f elf32
LDFLAGS := -m32 -ffreestanding -nostdlib -no-pie -Wl,-T,linker.ld \
           -Wl,--build-id=none -Wl,-z,noexecstack

CSRC := $(wildcard kernel/*.c drivers/*.c fs/*.c net/*.c gui/*.c)
ASRC := boot/boot.asm boot/isr.asm
OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(CSRC)) $(patsubst %.asm,$(OBJDIR)/%.o,$(ASRC))

QEMU_NET := -netdev user,id=net0 -device rtl8139,netdev=net0
QEMU_RUN := qemu-system-i386 -cdrom $(ISO) -m 256M -serial stdio $(QEMU_NET)
AUTOTEST_ISO := $(BUILD)/minimal_desktop_autotest.iso
QEMU_TEST := qemu-system-i386 -cdrom $(AUTOTEST_ISO) -m 256M -display none -serial stdio \
             $(QEMU_NET) -device isa-debug-exit,iobase=0x501,iosize=0x04

.PHONY: all iso kernel store run autotest clean publish dirs check

all: $(ISO)

dirs:
	@mkdir -p $(OBJDIR)/kernel $(OBJDIR)/drivers $(OBJDIR)/fs \
	          $(OBJDIR)/net $(OBJDIR)/gui $(OBJDIR)/boot \
	          $(ISODIR)/boot/grub website/downloads

$(OBJDIR)/%.o: %.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.asm | dirs
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lgcc
	@i686-linux-gnu-size $@ || true

$(STORE): tools/gen_store.py | dirs
	$(PYTHON) tools/gen_store.py --out $(STORE) --web website/store

store: $(STORE)

$(ISO): $(KERNEL) $(STORE) boot/grub.cfg boot/install_source.cfg
	@mkdir -p $(ISODIR)/boot/grub
	rm -f $(ISODIR)/boot/install_source.img
	cp $(KERNEL) $(ISODIR)/boot/kernel.elf
	cp $(STORE) $(ISODIR)/boot/store.img
	cp boot/grub.cfg $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/base.iso $(ISODIR) >/dev/null
	# Add a FAT16 data partition so /disk survives a reboot after install.
	$(PYTHON) tools/append_data_part.py $(BUILD)/base.iso >/dev/null
	# The base ISO is isohybrid: written raw to a disk it boots through GRUB.
	# Embed it as a module so the `install` command can stream it to ATA.
	cp $(BUILD)/base.iso $(ISODIR)/boot/install_source.img
	cp boot/install_source.cfg $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISODIR) >/dev/null
	@echo "ISO built: $(ISO)"
	@ls -lh $(ISO)

iso: $(ISO)
	@mkdir -p website/downloads
	cp $(ISO) website/downloads/minimal_desktop.iso
	@if [ -d "$(PUBLISH_DIR)" ]; then cp $(ISO) "$(PUBLISH_DIR)/minimal_desktop.iso" && \
	    echo "Published: $(PUBLISH_DIR)/minimal_desktop.iso"; fi
	@sha256sum $(ISO)

run: $(ISO)
	$(QEMU_RUN)

$(AUTOTEST_ISO): $(KERNEL) $(STORE) boot/grub.cfg
	@mkdir -p $(BUILD)/autotest/boot/grub
	cp $(KERNEL) $(BUILD)/autotest/boot/kernel.elf
	cp $(STORE) $(BUILD)/autotest/boot/store.img
	sed 's/^set default=0/set default=1/' boot/grub.cfg > $(BUILD)/autotest/boot/grub/grub.cfg
	grub-mkrescue -o $(AUTOTEST_ISO) $(BUILD)/autotest >/dev/null
	@echo "autotest ISO: $(AUTOTEST_ISO)"

autotest: $(AUTOTEST_ISO)
	@set +e; $(QEMU_TEST); code=$$?; \
	 if [ $$code -eq 33 ]; then echo "AUTOTEST: PASS"; exit 0; \
	 elif [ $$code -eq 35 ]; then echo "AUTOTEST: FAIL"; exit 1; \
	 else echo "AUTOTEST: QEMU exit $$code"; exit 1; fi

check:
	./tools/compile_check.sh

clean:
	rm -rf $(BUILD)
