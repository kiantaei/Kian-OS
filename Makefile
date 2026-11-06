# Kian OS Makefile
# Single command: make iso

.PHONY: all iso clean run-qemu toolchain

ROOT := $(CURDIR)
ISO_DIR := $(ROOT)/iso
OUT_ISO := $(ROOT)/kian-os-0.2.0.iso

all: iso

iso:
	bash ./build.sh

toolchain:
	bash ./toolchain-setup.sh

clean:
	rm -rf $(ROOT)/target $(ROOT)/kian-os-0.2.0.iso $(ROOT)/kian-os-0.2.0.iso.sha256
	rm -rf $(ROOT)/iso/boot

run-qemu: iso
	qemu-system-x86_64 \
		-m 2G -smp 2 \
		-drive file=$(OUT_ISO),format=raw,media=cdrom \
		-serial stdio

help:
	@echo "Kian OS build targets:"
	@echo "  make iso       - Build kian-os-0.2.0.iso (BIOS+UEFI hybrid)"
	@echo "  make toolchain - Install Rust nightly + cross target"
	@echo "  make run-qemu  - Boot ISO in QEMU"
	@echo "  make clean     - Clean build artifacts"
