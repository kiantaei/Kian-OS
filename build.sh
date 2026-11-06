#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$ROOT/kernel"
ISO_DIR="$ROOT/iso"
OUT_ISO="$ROOT/kian-os-0.2.0.iso"
KTARGET=x86_64-unknown-none
CARGO_OUT="$ROOT/build/cargo"
KLIB="$CARGO_OUT/$KTARGET/release/libkian_kernel.a"

rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot"

echo "[Kian] Building kernel (target: $KTARGET)..."
( cd "$KERNEL_DIR" && cargo +nightly build --release --target "$KTARGET" --target-dir "$CARGO_OUT" )

if [ ! -f "$KLIB" ]; then
  echo "[Kian] ERROR: kernel library not found: $KLIB"
  ls -R "$CARGO_OUT" 2>&1 | head -40 || true
  exit 1
fi
echo "[Kian] Kernel library: $KLIB"

command -v ld.lld >/dev/null || { echo "[Kian] ERROR: ld.lld not found (sudo apt-get install -y lld)"; exit 1; }

echo "[Kian] Linking kernel ELF (higher-half)..."
ld.lld \
  -T "$KERNEL_DIR/linker.ld" \
  -nostdlib -static \
  -z max-page-size=0x1000 \
  -u _start \
  --whole-archive "$KLIB" --no-whole-archive \
  -o "$ISO_DIR/boot/kian-kernel.elf"

echo "[Kian] Kernel ELF:"
file "$ISO_DIR/boot/kian-kernel.elf" || true
readelf -h "$ISO_DIR/boot/kian-kernel.elf" | grep -E "Entry point|Type" || true

echo "[Kian] ---- ELF check ----"
readelf -lS "$ISO_DIR/boot/kian-kernel.elf" | head -60
nm "$ISO_DIR/boot/kian-kernel.elf" | grep -E " _start$" || { echo "[Kian] ERROR: _start missing in ELF"; exit 1; }
readelf -S "$ISO_DIR/boot/kian-kernel.elf" | grep -q "limine" || { echo "[Kian] ERROR: no .limine_requests section in the ELF (the linker removed the Limine requests)"; exit 1; }

echo "[Kian] Building ISO..."
bash "$ROOT/tools/make_iso.sh" "$ISO_DIR" "$OUT_ISO"

echo "[Kian] ---- ISO contents ----"
xorriso -indev "$OUT_ISO" -find / 2>/dev/null | head -40

sha256sum "$OUT_ISO" > "$OUT_ISO.sha256"
echo "[Kian] Done: $OUT_ISO"
