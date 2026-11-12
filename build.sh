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

say "checking profile scripts ..."
bash -n "${PROFILE}/profiledef.sh" || die "profiledef.sh has syntax errors"
bash -n "${ROOT}/scripts/ci-boot-test.sh" 2>/dev/null || true

if [[ "${1:-}" == "--check" ]]; then
    say "syntax checks passed (no ISO built)"
    exit 0
fi

command -v mkarchiso >/dev/null 2>&1 || die "mkarchiso not found. Install: pacman -S archiso grub syslinux (Windows: see build.ps1)"
[[ $(id -u) -eq 0 ]] || die "mkarchiso needs root: run with sudo"

# ---- vendor the kianinstall fork -------------------------------------------
# The ISO ships Kian's own fork of archinstall
# (https://github.com/kiantaei/kianinstall) pinned to a commit, vendored into
# the image at /opt/kianinstall so the installer works offline. The copy into
# the profile is build-time only: it is removed again when this script exits,
# so the git tree stays clean.
KIANINSTALL_REPO="https://github.com/kiantaei/kianinstall.git"
KIANINSTALL_REF="17965c0d614d157290bb4324acff1c807d1b5e73"   # tag: kianinstall-v4.5
VENDOR_SRC="${WORK}/kianinstall-vendor"
VENDOR_DST="${PROFILE}/airootfs/opt/kianinstall"

prepare_vendor_src() {
    rm -rf "${VENDOR_SRC}"
    mkdir -p "${VENDOR_SRC}"
    # Fetch exactly the pinned commit (minimal download, independent of
    # branches); fall back to a full clone+checkout.
    if ! (git -C "${VENDOR_SRC}" init -q \
          && git -C "${VENDOR_SRC}" remote add origin "${KIANINSTALL_REPO}" \
          && git -C "${VENDOR_SRC}" fetch -q --depth 1 origin "${KIANINSTALL_REF}" \
          && git -C "${VENDOR_SRC}" checkout -q FETCH_HEAD); then
        say "shallow fetch of ${KIANINSTALL_REF} failed — trying a full clone"
        rm -rf "${VENDOR_SRC}"
        git clone -q "${KIANINSTALL_REPO}" "${VENDOR_SRC}" \
            && git -C "${VENDOR_SRC}" checkout -q "${KIANINSTALL_REF}" \
            || { say "WARNING: could not clone the kianinstall fork (offline?) — the ISO will fall back to pacman's archinstall"; return 1; }
    fi
    return 0
}

VENDOR_OK=1
if [[ -d "${VENDOR_SRC}" ]]; then
    # reuse a previous build's checkout; just move to the pinned commit
    git -C "${VENDOR_SRC}" fetch -q origin "${KIANINSTALL_REF}" 2>/dev/null || true
    git -C "${VENDOR_SRC}" checkout -q "${KIANINSTALL_REF}" 2>/dev/null || VENDOR_OK=0
else
    prepare_vendor_src || VENDOR_OK=0
fi

rm -rf "${VENDOR_DST}"
if [[ ${VENDOR_OK} -eq 1 && -d "${VENDOR_SRC}/archinstall" ]]; then
    mkdir -p "${VENDOR_DST}"
    cp -a "${VENDOR_SRC}/archinstall" "${VENDOR_SRC}/pyproject.toml" "${VENDOR_SRC}/README.md" "${VENDOR_SRC}/LICENSE" "${VENDOR_DST}/"
    find "${VENDOR_DST}" -name '__pycache__' -type d -prune -exec rm -rf {} + 2>/dev/null || true
    say "vendored kianinstall at ${KIANINSTALL_REF} into airootfs/opt/kianinstall"
fi
# the vendor copy lives only inside mkarchiso's run; keep the git tree clean
cleanup_vendor() { rm -rf "${VENDOR_DST}"; }
trap cleanup_vendor EXIT

say "building Kian OS ISO (this downloads Arch packages; takes a while) ..."
mkdir -p "${WORK}" "${OUT}"
mkarchiso -v -w "${WORK}" -o "${OUT}" "${PROFILE}"

say "done. artifacts:"
ls -lh "${OUT}"

ISO="$(ls "${OUT}"/*.iso 2>/dev/null | head -n1)"
if [[ -n "${ISO}" ]]; then
    ( cd "${OUT}" && sha256sum "$(basename "${ISO}")" > "$(basename "${ISO}").sha256" )
    say "sha256: $(cat "${OUT}/"*.iso.sha256)"
fi
