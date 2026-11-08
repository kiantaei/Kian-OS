#!/usr/bin/env bash
# Kian OS - Toolchain setup
# Installs Rust nightly + x86_64-unknown-none target + host linkers.
set -euo pipefail

echo "[Kian] Installing Rust nightly..."
rustup toolchain install nightly --profile minimal 2>/dev/null || rustup toolchain install nightly
rustup default nightly
rustup component add rust-src llvm-tools-preview --toolchain nightly

echo "[Kian] Adding target x86_64-unknown-none..."
rustup target add x86_64-unknown-none --toolchain nightly

echo "[Kian] Installing host linkers (lld, ld)..."
if command -v apt-get >/dev/null 2>&1; then
  sudo apt-get update -y
  sudo apt-get install -y binutils lld xorriso mtools nasm qemu-system-x86 grub-pc-bin grub-efi-amd64
elif command -v apk >/dev/null 2>&1; then
  sudo apk add binutils lld xorriso mtools nasm qemu-system-x86_64 grub efibootmgr
fi

echo "[Kian] Toolchain ready."
echo "  Build with: make iso"
