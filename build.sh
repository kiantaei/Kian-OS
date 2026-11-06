#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$ROOT/kernel"
ISO_DIR="$ROOT/iso"
OUT_ISO="$ROOT/kian-os-0.2.0.iso"
KTARGET=x86_64-unknown-none
CARGO_OUT="$ROOT/build/cargo"
KLIB="$CARGO_OUT/$KTARGET/release/libkian_kernel.a"
LIMINE_DIR="$ROOT/build/limine"

# Clone Limine BEFORE compiling (boot/limine_requests.c includes <limine.h>).
[ -d "$LIMINE_DIR" ] || git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1 "$LIMINE_DIR"

command -v clang >/dev/null && command -v ld.lld >/dev/null || { echo "need: sudo apt-get install -y clang lld"; exit 1; }

OBJ="$ROOT/build/obj"
rm -rf "$OBJ"
mkdir -p "$OBJ" "$ISO_DIR/boot"

COMMON="--target=x86_64-unknown-none-elf -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mcmodel=kernel -mno-red-zone -mgeneral-regs-only -O2 -Wall -Wextra -I$ROOT/include -I$LIMINE_DIR"
CFLAGS="$COMMON -std=gnu11"
CXXFLAGS="$COMMON -std=gnu++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -Wglobal-constructors"

echo "[Kian] Building boot glue (C)..."
for f in "$ROOT"/boot/*.c; do
  o="$OBJ/boot_$(basename "${f%.c}").o"
  clang $CFLAGS -c "$f" -o "$o"
done

echo "[Kian] Building console (C)..."
for f in "$ROOT"/console/*.c; do
  o="$OBJ/console_$(basename "${f%.c}").o"
  extra=""
  [ "$(basename "$f")" = "libc_min.c" ] && extra="-fno-builtin"
  clang $CFLAGS $extra -c "$f" -o "$o"
done

echo "[Kian] Building GUI (C++)..."
for f in "$ROOT"/gui/*.cpp; do
  o="$OBJ/gui_$(basename "${f%.cpp}").o"
  clang++ $CXXFLAGS -c "$f" -o "$o"
done

echo "[Kian] Building kernel (Rust, target: $KTARGET)..."
( cd "$KERNEL_DIR" && cargo +nightly build --release --target "$KTARGET" --target-dir "$CARGO_OUT" )

if [ ! -f "$KLIB" ]; then
  echo "[Kian] ERROR: kernel library not found: $KLIB"
  ls -R "$CARGO_OUT" 2>&1 | head -40 || true
  exit 1
fi
echo "[Kian] Kernel library: $KLIB"

echo "[Kian] Linking kernel ELF (C/C++ objects + Rust archive, higher-half)..."
ld.lld \
  -T "$KERNEL_DIR/linker.ld" \
  -nostdlib -static \
  -z max-page-size=0x1000 -z muldefs \
  -u _start \
  "$OBJ"/*.o "$KLIB" \
  -o "$ISO_DIR/boot/kian-kernel.elf"

echo "[Kian] ---- ELF check ----"
nm "$ISO_DIR/boot/kian-kernel.elf" | grep -E " (T|t) _start$" || { echo "[Kian] ERROR: _start missing in ELF"; exit 1; }
nm "$ISO_DIR/boot/kian-kernel.elf" | grep -E " T (kian_shell_main|gui_main|console_init|kian_boot_info|kian_alloc)$" || { echo "[Kian] ERROR: one or more ABI symbols missing (kian_shell_main/gui_main/console_init/kian_boot_info/kian_alloc)"; exit 1; }
readelf -S "$ISO_DIR/boot/kian-kernel.elf" | grep -q limine_requests || { echo "ERROR: Limine requests missing from ELF"; exit 1; }
readelf -lS "$ISO_DIR/boot/kian-kernel.elf" | head -60

echo "[Kian] Building ISO..."
bash "$ROOT/tools/make_iso.sh" "$ISO_DIR" "$OUT_ISO"

echo "[Kian] ---- ISO contents ----"
xorriso -indev "$OUT_ISO" -find / 2>/dev/null | head -40

sha256sum "$OUT_ISO" > "$OUT_ISO.sha256"
echo "[Kian] Done: $OUT_ISO"
