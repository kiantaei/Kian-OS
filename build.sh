#!/bin/bash
# Kian OS 0.4.0 "Horizon" — Linux-based ISO build script.
#
# Builds a bootable ISO (BIOS + UEFI) with archiso from the profile in
# archiso/kian. Requirements:
#   * an Arch Linux host (or container), or any host with mkarchiso
#   * packages: archiso grub syslinux mtools dosfstools  (CI installs them)
#
# Usage:
#   sudo ./build.sh                 # build ISO into ./out
#   ./build.sh --check              # only run profile sanity checks
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE="${ROOT}/archiso/kian"
WORK="${ROOT}/build/work"
OUT="${ROOT}/build/out"

say() { printf '\033[1;34m[kian-build]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[kian-build] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

# ---- sanity checks ----------------------------------------------------------
[[ -d "${PROFILE}" ]] || die "profile not found: ${PROFILE}"
command -v mkarchiso >/dev/null 2>&1 || die "mkarchiso not found. Install: pacman -S archiso grub syslinux"

say "checking profile scripts ..."
bash -n "${PROFILE}/profiledef.sh" || die "profiledef.sh has syntax errors"
bash -n "${ROOT}/scripts/ci-boot-test.sh" 2>/dev/null || true

if [[ "${1:-}" == "--check" ]]; then
    say "syntax checks passed (no ISO built)"
    exit 0
fi

[[ $(id -u) -eq 0 ]] || die "mkarchiso needs root: run with sudo"

say "building Kian OS ISO (this downloads Arch packages; takes a while) ..."
mkarchiso -v -w "${WORK}" -o "${OUT}" "${PROFILE}"

say "done. artifacts:"
ls -lh "${OUT}"

ISO="$(ls "${OUT}"/*.iso 2>/dev/null | head -n1)"
if [[ -n "${ISO}" ]]; then
    ( cd "${OUT}" && sha256sum "$(basename "${ISO}")" > "$(basename "${ISO}").sha256" )
    say "sha256: $(cat "${OUT}/"*.iso.sha256)"
fi
