#!/usr/bin/env bash
# Kian OS - Build script
# Usage: ./build.sh [iso]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$ROOT/kernel"
ISO_DIR="$ROOT/iso"
OUT_ISO="$ROOT/kian-os-0.2.0.iso"

mkdir -p "$ISO_DIR/boot"

echo "[Kian] Building kernel (target: x86_64-unknown-none)..."
cargo +nightly build --release --manifest-path "$KERNEL_DIR/Cargo.toml"

KERNEL_A="$ROOT/kernel/target/x86_64-unknown-none/release/libkian_kernel.a"

echo "[Kian] Linking kernel ELF (higher-half)..."
ld.lld \
  -T "$ROOT/kernel/linker.ld" \
  -o "$ISO_DIR/boot/kian-kernel.elf" \
  "$KERNEL_A" \
  -Ttext=0xffffffff80000000

echo "[Kian] Kernel ELF: $ISO_DIR/boot/kian-kernel.elf"
file "$ISO_DIR/boot/kian-kernel.elf" 2>/dev/null || true

echo "[Kian] Copying Limine config..."
cp "$ROOT/config/limine.cfg" "$ISO_DIR/limine.cfg"

echo "[Kian] Building ISO..."
"$ROOT/tools/make_iso.sh" "$ISO_DIR" "$OUT_ISO"

echo "[Kian] Done: $OUT_ISO"
sha256sum "$OUT_ISO" > "$OUT_ISO.sha256"
echo "[Kian] Checksum: $OUT_ISO.sha256"
