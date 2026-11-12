#!/bin/bash
# Kian OS CI command verification — runs INSIDE the booted live session.
# Ubuntu 24.04 (noble) base: apt, XFCE, Calamares, live-boot persistence.
# Every check prints ok/bad and counts; a nonzero exit fails the boot test.
set -u

PASS=0
FAIL=0
ok()  { echo "CHECK_OK: $*";  PASS=$((PASS + 1)); }
bad() { echo "CHECK_FAIL: $*"; FAIL=$((FAIL + 1)); }

echo "===== Kian OS live verification (Ubuntu base) ====="

# 1. Identity: Kian OS on an Ubuntu base
if grep -q '^ID=kian$' /etc/os-release && grep -q '^ID_LIKE=.*ubuntu' /etc/os-release; then
    ok "identity: $(grep PRETTY_NAME /etc/os-release | cut -d'"' -f2)"
else
    bad "identity: os-release is not Kian-on-Ubuntu"
fi
[[ "$(readlink -f /etc/localtime)" == *Asia/Tehran* ]] && ok "timezone: Asia/Tehran (live boot param)" \
    || bad "timezone: $(readlink -f /etc/localtime 2>/dev/null || echo none)"
. /etc/os-release 2>/dev/null; [ "${VERSION_CODENAME:-}" = "noble" ] && ok "codename: noble (apt/distro tools see it)" \
    || bad "codename: ${VERSION_CODENAME:-unset}"

# 2. Ubuntu base — apt present, Arch REMOVED
command -v apt-get >/dev/null && command -v dpkg >/dev/null \
    && ok "apt: apt-get/dpkg present" || bad "apt: apt-get/dpkg missing"
if command -v pacman >/dev/null 2>&1; then
    bad "arch-removal: pacman still present"
else
    ok "arch-removal: no pacman anywhere"
fi
if dpkg -l kianinstall 2>/dev/null | grep -q '^ii'; then
    bad "arch-removal: kianinstall fork still installed"
else
    ok "arch-removal: no kianinstall fork package"
fi

# 3. Live system reality
[ -d /run/live/medium ] && ok "live: medium at /run/live/medium" || bad "live: no /run/live/medium"
grep -q 'boot=live' /proc/cmdline && ok "live: boot=live on cmdline" || bad "live: boot=live missing"
[ -s /run/live/medium/live/filesystem.squashfs ] && ok "live: squashfs present on medium (installer source)" \
    || bad "live: filesystem.squashfs missing on medium"
grep -q 'persistence-label=KIANPERSIST' /proc/cmdline \
    && ok "persist: persistence param on cmdline (graceful fallback when no store)" \
    || bad "persist: persistence-label missing from cmdline"

# 4. Kian tools
KIH="$(kian-install --help 2>/dev/null | head -12)"
echo "$KIH" | grep -q "mirror region" && ok "kian-install: --help prints (2-question installer)" || bad "kian-install: --help output unexpected"
KPH="$(kian-persist --help 2>/dev/null | head -12)"
echo "$KPH" | grep -q "persistence" && ok "kian-persist: --help prints (real live persistence)" || bad "kian-persist: --help output unexpected"
KWH="$(kian-welcome --help 2>/dev/null | head -12)"
echo "$KWH" | grep -q "Calamares" && ok "kian-welcome: --help prints (welcome GUI)" || bad "kian-welcome: --help output unexpected"
[ -x /usr/local/bin/xenox ] && [ -x /usr/bin/falkon ] && ok "xenox: launcher + falkon browser present" || bad "xenox: launcher or falkon missing"
command -v neofetch >/dev/null && neofetch --version >/dev/null 2>&1 && ok "neofetch: runs (real Ubuntu package)" || bad "neofetch: missing/broken"

# 5. Calamares graphical installer (like normal installers)
command -v calamares >/dev/null && ok "calamares: binary present" || bad "calamares: binary missing"
[ -f /etc/calamares/settings.conf ] && ok "calamares: settings.conf present" || bad "calamares: settings.conf missing"
[ -f /etc/calamares/branding/kian/branding.desc ] && ok "calamares: kian branding present" || bad "calamares: kian branding missing"
python3 -c "import yaml; yaml.safe_load(open('/etc/calamares/settings.conf'))" 2>/dev/null \
    && ok "calamares: settings.conf is valid YAML" || bad "calamares: settings.conf invalid YAML"
[ -f /usr/lib/x86_64-linux-gnu/calamares/modules/automirror/main.py ] \
    && ok "calamares: automirror module vendored (regional mirror support)" \
    || bad "calamares: automirror module missing"
python3 -c "import distro" 2>/dev/null && ok "calamares: python3-distro present (automirror dep)" || bad "calamares: python3-distro missing"
[ -x /usr/bin/calamares-launch-normal ] || [ -x /usr/local/bin/calamares-launch-normal ] \
    && ok "calamares: launch wrapper present" || bad "calamares: launch wrapper missing"

# 6. Persistence mechanics sanity (live-boot reads /persistence.conf on a
#    KIANPERSIST-labeled store created by kian-persist)
ls /usr/local/bin/kian-persist >/dev/null 2>&1 && ok "persist: kian-persist on disk" || bad "persist: tool missing"

# 7. Desktop really running
if systemctl is-active lightdm >/dev/null 2>&1; then
    ok "desktop: lightdm active"
else
    bad "desktop: lightdm not active"
fi
N=$(pgrep -c -x xfce4-session 2>/dev/null || echo 0)
[ "${N:-0}" -ge 1 ] && ok "desktop: xfce4-session running ($N)" || bad "desktop: no xfce4-session"
command -v xfce4-terminal >/dev/null && ok "apps: xfce4-terminal present" || bad "apps: xfce4-terminal missing"
command -v galculator >/dev/null && ok "apps: galculator present" || bad "apps: galculator missing"

# 8. Live sudo (casper-style NOPASSWD, removed on install)
if sudo -n true 2>/dev/null; then
    ok "sudo: live user has passwordless sudo (removed by kian-cleanup-live-only on install)"
else
    bad "sudo: live user cannot sudo without a password"
fi

# 9. apt against a real mirror (network check; the CI VM uses QEMU NAT)
if timeout 200 apt-get update -o APT::Update::Error-Mode=any >/tmp/kian-apt-update.log 2>&1; then
    ok "apt: apt-get update against noble succeeded"
else
    bad "apt: apt-get update failed (see /tmp/kian-apt-update.log in the live session)"
fi

echo "===== summary: PASS=${PASS} FAIL=${FAIL} ====="
[[ ${FAIL} -eq 0 ]] && exit 0 || exit 1
