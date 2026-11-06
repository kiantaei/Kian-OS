#!/usr/bin/env bash
set -euo pipefail
ISO_DIR="$1"
OUT_ISO="$2"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIMINE_DIR="$ROOT/build/limine"

command -v xorriso >/dev/null || { echo "[Kian] ERROR: xorriso missing (sudo apt-get install -y xorriso mtools)"; exit 1; }

if [ ! -d "$LIMINE_DIR" ]; then
  git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1 "$LIMINE_DIR"
fi
make -C "$LIMINE_DIR"

mkdir -p "$ISO_DIR/boot/limine" "$ISO_DIR/EFI/BOOT"
cp "$ROOT/config/limine.conf" "$ISO_DIR/boot/limine/limine.conf"
cp "$LIMINE_DIR/limine-bios.sys" "$LIMINE_DIR/limine-bios-cd.bin" "$LIMINE_DIR/limine-uefi-cd.bin" "$ISO_DIR/boot/limine/"
cp "$LIMINE_DIR/BOOTX64.EFI" "$LIMINE_DIR/BOOTIA32.EFI" "$ISO_DIR/EFI/BOOT/"

xorriso -as mkisofs -R -r -J \
  -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
  -hfsplus -apm-block-size 2048 \
  --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label \
  "$ISO_DIR" -o "$OUT_ISO"

"$LIMINE_DIR/limine" bios-install "$OUT_ISO"
echo "[Kian] ISO created: $OUT_ISO"
