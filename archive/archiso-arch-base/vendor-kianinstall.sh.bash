#!/bin/bash
# Vendor the pinned kianinstall fork into an archiso profile's airootfs.
#
# The ISO ships Kian's own fork of archinstall
# (https://github.com/kiantaei/kianinstall — unmodified upstream installer
# logic, rebranded, GPL-3.0-only with upstream attribution) pinned to one
# commit, copied into <profile>/airootfs/opt/kianinstall so the installed
# command `kianinstall` works offline.
#
# Usage:
#   scripts/vendor-kianinstall.sh <profile-dir> <work-dir>   # vendor
#   scripts/vendor-kianinstall.sh --clean <profile-dir>      # remove the copy
#
# Exit codes: 0 on success AND on honest offline degradation (with a warning);
# nonzero only on bad usage or an unusable profile. Callers decide whether a
# missing vendor tree is fatal — the ISO falls back to pacman's archinstall.

set -uo pipefail

KIANINSTALL_REPO="https://github.com/kiantaei/kianinstall.git"
KIANINSTALL_REF="17965c0d614d157290bb4324acff1c807d1b5e73"   # tag: kianinstall-v4.5

say() { printf '\033[1;34m[kian-vendor]\033[0m %s\n' "$*"; }

ACTION=""
PROFILE=""
WORK=""
if [[ "${1:-}" == "--clean" ]]; then
    ACTION="--clean"
    PROFILE="${2:-}"
else
    PROFILE="${1:-}"
    WORK="${2:-}"
fi

if [[ "$ACTION" == "--clean" ]]; then
    [[ -n "$PROFILE" ]] || { echo "usage: $0 --clean <profile-dir>" >&2; exit 1; }
    rm -rf "${PROFILE}/airootfs/opt/kianinstall"
    exit 0
fi

[[ -n "$PROFILE" && -n "$WORK" ]] || { echo "usage: $0 <profile-dir> <work-dir>" >&2; exit 1; }
[[ -d "$PROFILE" ]] || { echo "profile not found: $PROFILE" >&2; exit 1; }

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
    say "vendored kianinstall at ${KIANINSTALL_REF} into ${VENDOR_DST}"
fi
