#!/usr/bin/env bash
# Kian OS - Build a hybrid BIOS+UEFI ISO from an ISO dir
# Usage: make_iso.sh <iso_dir> <output.iso>
set -euo pipefail

ISO_DIR="${1:?usage: make_iso.sh <iso_dir> <output.iso>}"
OUT_ISO="${2:?usage: make_iso.sh <iso_dir> <output.iso>}"

EFI_DIR="$ISO_DIR/EFI/BOOT"
mkdir -p "$EFI_DIR"

# If limine is available, use it; otherwise build a simple El Torito ISO.
if command -v limine-deploy >/dev/null 2>&1 || [ -f "$ROOT/vendor/limine/limine-deploy" ]; then
  echo "[Kian] Using Limine to deploy bootloader..."
  limine-deploy bios "$ISO_DIR" 2>/dev/null || true
  limine-deploy uefi "$ISO_DIR" 2>/dev/null || true
else
  echo "[Kian] Limine not found; building plain El Torito ISO (BIOS boot via isolinux not included)."
fi

# Build ISO with xorriso (hybrid BIOS+UEFI)
if command -v xorriso >/dev/null 2>&1; then
  echo "[Kian] Creating hybrid ISO with xorriso..."
  xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -volid "KIAN_OS" \
    -volset "KIAN OS" \
    -publisher "Kian OS Project" \
    -preparer "Kian OS 0.1.0 Genesis" \
    -sysid "LINUX" \
    -boot-load-size 4 \
    -boot-info-table \
    -eltorito-boot boot/limine-bios.sys \
    -eltorito-catalog boot/boot.cat \
    -no-emul-boot \
    -eltorito-alt-boot \
    -e EFI/BOOT/BOOTX64.EFI \
    -no-emul-boot \
    -isohybrid-gpt-basdat \
    -output "$OUT_ISO" \
    "$ISO_DIR"
  echo "[Kian] ISO written: $OUT_ISO"
else
  echo "[Kian] xorriso not found; skipping ISO creation."
  exit 1
fi
