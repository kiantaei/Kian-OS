#!/bin/bash
# Build the Kian OS ISO — Ubuntu 24.04 (noble) base, live-build.
#
# Run as root:  sudo ./build.sh          (builds live/kian-os-1.0.0-amd64.iso)
#               ./build.sh --check       (sanity checks only, no build)
#
# Prerequisites (Ubuntu noble host or the CI runner):
#   apt-get install -y live-build debootstrap squashfs-tools xorriso \
#       grub-pc-bin grub-efi-amd64-bin syslinux-common isolinux mtools
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE="${ROOT}/live"
VERSION="1.0.0"
ISO_NAME="kian-os-${VERSION}-amd64.iso"

say() { printf '\033[1;34m[kian-build]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[kian-build] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

# ---- sanity checks ----------------------------------------------------------
[[ -d "${PROFILE}/config" ]] || die "live-build profile not found: ${PROFILE}"

say "checking profile scripts ..."
bash -n "${PROFILE}/auto/config" || die "auto/config has syntax errors"
bash -n "${ROOT}/scripts/ci-boot-test.sh" || die "ci-boot-test.sh has syntax errors"
for hook in "${PROFILE}/config/hooks/"*.hook.chroot; do
    bash -n "$hook" || die "hook has syntax errors: $hook"
done
for tool in kian-install kian-persist kian-welcome xenox kian-cleanup-live-only; do
    f="${PROFILE}/config/includes.chroot/usr/local/bin/${tool}"
    [[ -f "$f" ]] || die "missing tool: $f"
    case "$tool" in
        kian-welcome) python3 -c "import ast; ast.parse(open('$f').read())" || die "kian-welcome does not parse" ;;
        *) bash -n "$f" || die "$tool has syntax errors" ;;
    esac
done
grep -q "ID=kian" "${PROFILE}/config/includes.chroot/etc/os-release" || die "os-release is not Kian"

if [[ "${1:-}" == "--check" ]]; then
    say "sanity checks passed (no ISO built)"
    exit 0
fi

command -v lb >/dev/null 2>&1 || die "live-build not found. Install: apt-get install -y live-build debootstrap squashfs-tools xorriso grub-pc-bin grub-efi-amd64-bin syslinux-common isolinux mtools"
[[ $(id -u) -eq 0 ]] || die "live-build needs root: run with sudo"

# ---- build ------------------------------------------------------------------
cd "${PROFILE}"
lb clean --purge >/dev/null 2>&1 || true
./auto/config
lb build

[[ -f binary.iso ]] || die "lb build finished without binary.iso"
mv -f binary.iso "${ISO_NAME}"
sha256sum "${ISO_NAME}" > "${ISO_NAME}.sha256"

say "done. artifacts:"
ls -lh "${ISO_NAME}" "${ISO_NAME}.sha256"
