# Kian OS Makefile — Ubuntu 24.04 base edition (live-build).
# Single command: make iso

.PHONY: all iso check clean run-qemu help

ROOT := $(CURDIR)
OUT_ISO := $(ROOT)/live/kian-os-1.0.0-amd64.iso

all: iso

iso:
	bash ./build.sh

check:
	bash ./build.sh --check

clean:
	rm -rf $(ROOT)/build

run-qemu: $(OUT_ISO)
	qemu-system-x86_64 -m 2048 -smp 2 -enable-kvm \
		-cdrom $(OUT_ISO) -boot d

help:
	@echo "Kian OS build targets (Linux-based edition):"
	@echo "  make iso       - Build live/kian-os-1.0.0-amd64.iso via live-build (needs root, Ubuntu 24.04 host)"
	@echo "  make check     - Run profile sanity checks only"
	@echo "  make run-qemu  - Boot the built ISO in QEMU"
	@echo "  make clean     - Remove build artifacts"
