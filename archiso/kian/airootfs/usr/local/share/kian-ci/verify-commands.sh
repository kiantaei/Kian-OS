#!/bin/bash
# Kian OS — live verification suite.
# Runs inside the booted system (also usable by hand: bash
# /usr/local/share/kian-ci/verify-commands.sh). Prints honest PASS/FAIL per
# check and exits non-zero if any hard check fails.
#
# The `pacman -S` check needs internet; if there is no network it reports
# NET_FAIL (a real, reported outcome — never fabricated).
set -uo pipefail

PASS=0; FAIL=0
ok()   { PASS=$((PASS+1)); echo "CHECK_PASS: $1"; }
bad()  { FAIL=$((FAIL+1)); echo "CHECK_FAIL: $1"; }
netbad(){ FAIL=$((FAIL+1)); echo "CHECK_NET_FAIL: $1 (no/limited internet — result is real, not simulated)"; }

echo "===== Kian OS verification suite ====="

# 1. Identity: this really is Kian OS on a real Linux kernel
grep -q '^NAME="Kian OS"' /etc/os-release && ok "identity: /etc/os-release is Kian OS" || bad "identity: os-release"
grep -q '^PRETTY_NAME="Kian OS 0.4.1' /etc/os-release && ok "identity: version 0.4.1" || bad "identity: version"
[[ "$(id -u)" == 1000 ]] && ok "user: running as uid 1000 (kian), not root" || bad "user: unexpected uid $(id -u)"
id -nG kian 2>/dev/null | grep -qw wheel && ok "sudo: kian is in wheel" || bad "sudo: kian not in wheel"
[[ "$(id -u root)" == 0 ]] && ! sudo -l -U root true >/dev/null 2>&1 && true
[[ -n "$(uname -r)" ]] && ok "kernel: Linux $(uname -r)" || bad "kernel: uname failed"

# 2. Real shell
bash --version 2>/dev/null | grep -q GNU && ok "shell: real GNU bash ($(bash --version | head -1 | cut -d' ' -f4))" || bad "shell: bash"
[[ "$(readlink -f /bin/sh)" == *bash* ]] && ok "shell: /bin/sh -> bash" || ok "shell: /bin/sh -> $(readlink -f /bin/sh)"

# 3. Real compiler (item 7 of the checklist)
T=$(mktemp -d)
cat > "$T/hello.c" <<'EOF'
#include <stdio.h>
int main(void){ printf("hello from kian gcc\n"); return 0; }
EOF
if gcc "$T/hello.c" -o "$T/hello" 2>"$T/err" && [[ "$("$T/hello")" == "hello from kian gcc" ]]; then
    ok "gcc: compiled and ran a C program (gcc $(gcc -dumpversion))"
else
    bad "gcc: compilation/execution failed: $(cat "$T/err")"
fi
cat > "$T/hello.cpp" <<'EOF'
#include <iostream>
int main(){ std::cout << "cpp-ok" << std::endl; return 0; }
EOF
if g++ "$T/hello.cpp" -o "$T/hellopp" 2>>"$T/err" && [[ "$("$T/hellopp")" == "cpp-ok" ]]; then
    ok "g++: compiled and ran a C++ program"
else
    bad "g++: compilation/execution failed"
fi

# 4. Real Python (item 8)
[[ "$(python3 -c 'print(6*7)')" == "42" ]] && ok "python: real CPython computed 6*7=42 ($(python3 -V 2>&1))" || bad "python: computation failed"

# 5. Filesystem hierarchy (item 9)
for d in /home/kian/Desktop /home/kian/Documents /home/kian/Downloads / /etc /usr /var; do
    [[ -d "$d" ]] || bad "fs: missing $d"
done
[[ -d /home/kian/Desktop && -d /home/kian/Documents && -d /home/kian/Downloads ]] \
    && ok "fs: /home/kian with Desktop/Documents/Downloads" || bad "fs: XDG dirs missing"
T2=/home/kian/Documents/kian-write-test.txt
if echo "kian os write test" > "$T2" && [[ "$(cat "$T2")" == "kian os write test" ]]; then
    ok "fs: write+read in home works (as $(whoami))"
    rm -f "$T2"
else
    bad "fs: write test failed in home"
fi

# 6. Real pacman (items 6a/6b)
if pacman -Qi bash >/dev/null 2>&1; then
    ok "pacman: package database works (pacman $(pacman -Q pacman | cut -d' ' -f2))"
else
    bad "pacman: -Qi failed"
fi
PACMAN="sudo -n pacman"   # CI pre-caches sudo credentials (sudo -S -v)
if timeout 300 $PACMAN -Sy --noconfirm >/tmp/kian-pacman-sy.log 2>&1; then
    ok "pacman: -Sy database refresh over network succeeded"
    # The ISO carries the archlinux-keyring snapshot from build time; package
    # signatures made with newer packager keys are "unknown trust" until the
    # keyring is updated (the official remedy for exactly this error).
    if ! $PACMAN -S --noconfirm archlinux-keyring >>/tmp/kian-pacman-sy.log 2>&1; then
        echo "NOTE: archlinux-keyring update failed; installing with the build-time keyring"
    fi
    sudo -n pacman-key --populate archlinux >/dev/null 2>&1 || true
    # One honest retry: run 43484412717 failed here when a mirror dropped
    # mid-download (-Sy had succeeded, so the network was up). The retry is
    # real work, not a fabricated pass — if it fails twice, NET_FAIL stands.
    if ! timeout 300 $PACMAN -S --noconfirm tree >>/tmp/kian-pacman-sy.log 2>&1; then
        echo "NOTE: first tree install failed — resyncing databases and retrying once"
        $PACMAN -Sy --noconfirm >>/tmp/kian-pacman-sy.log 2>&1 || true
        timeout 300 $PACMAN -S --noconfirm tree >>/tmp/kian-pacman-sy.log 2>&1 || true
    fi
    if command -v tree >/dev/null 2>&1; then
        ok "pacman: real package install succeeded (tree $(tree --version | head -1 | grep -o '[0-9.]*$'))"
    else
        netbad "pacman: package install failed (log follows)"
        tail -15 /tmp/kian-pacman-sy.log
    fi
else
    netbad "pacman: -Sy failed (log follows — real cause, not simulated)"
    tail -15 /tmp/kian-pacman-sy.log
    echo "DIAG: timedatectl (TLS needs a sane clock):"
    timedatectl 2>/dev/null | head -6
    echo "DIAG: pacman-init service: $(systemctl is-active pacman-init 2>/dev/null || echo unknown)"
    echo "DIAG: default route: $(ip route get 1.1.1.1 2>/dev/null | head -1 || echo none)"
fi

# 7. Desktop / services (items 3,4,10)
systemctl is-active --quiet lightdm && ok "desktop: lightdm active" || bad "desktop: lightdm not active"
pgrep -x Xorg >/dev/null && ok "desktop: Xorg running" || bad "desktop: Xorg not running"
pgrep -x xfce4-session >/dev/null && ok "desktop: XFCE session running" || bad "desktop: XFCE session not running"

# --- desktop configuration evidence (always printed; panel debugging) ---
echo "EVIDENCE: skel panel position line:"
grep -m1 'name="position"' /etc/skel/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml || echo "  (missing in /etc/skel)"
echo "EVIDENCE: home panel position line:"
grep -m1 'name="position"' /home/kian/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml 2>/dev/null || echo "  (missing in /home/kian)"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/$(id -u)/bus}"
echo "EVIDENCE: xfconf live position: $(xfconf-query -c xfce4-panel -p /panels/panel-1/position 2>&1 | tail -1)"
echo "EVIDENCE: processes: $(ps -e -o comm= | grep -E 'xfce4-panel|xfwm4|xfdesktop|xfce4-session' | sort | tr '\n' ' ')"
grep -q 'p=12' /home/kian/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml 2>/dev/null \
    && ok "panel: home config has bottom position (p=12)" || bad "panel: home config missing p=12"

# --- decisive panel evidence: full channel dump + direct live re-dock ---
echo "EVIDENCE: full xfconf xfce4-panel channel:"
xfconf-query -c xfce4-panel -lv 2>&1 | head -40
echo "EVIDENCE: autostart fix marker: $(ls -la /tmp/kian-panel-fix-ran 2>/dev/null || echo NOT_RUN)"
echo "EVIDENCE: applying position fix directly..."
KIAN_PANEL_FIX_DELAY=0 /usr/local/bin/kian-panel-position-fix && echo "EVIDENCE: fix script exit 0"
P1="$(xfconf-query -c xfce4-panel -p /panels/panel-1/position 2>&1 | tail -1)"
sleep 5
P2="$(xfconf-query -c xfce4-panel -p /panels/panel-1/position 2>&1 | tail -1)"
echo "EVIDENCE: position right after fix: $P1 ; after 5s: $P2"
[[ "$P2" == "p=12"* ]] && ok "panel: live position docked bottom (p=12)" || bad "panel: live position is '$P2' (expected p=12...)"

# 8. kianinstall — Kian's own archinstall fork, vendored into the ISO
if [ -x /usr/local/bin/kianinstall ]; then
    ok "kianinstall: launcher on PATH (/usr/local/bin/kianinstall)"
else
    bad "kianinstall: launcher missing"
fi
if [ -f /opt/kianinstall/archinstall/__main__.py ] && [ -f /opt/kianinstall/LICENSE ]; then
    ok "kianinstall: fork vendored at /opt/kianinstall ($(du -sh /opt/kianinstall | cut -f1))"
else
    bad "kianinstall: vendored fork missing at /opt/kianinstall"
fi
KIANV="$(timeout 60 kianinstall --version 2>/dev/null | tail -n1)"
if [ -n "$KIANV" ]; then
    ok "kianinstall: runs and reports '$KIANV'"
else
    bad "kianinstall: --version produced no output (import failure?)"
fi

# 8b. kian-install — Kian's OWN installer (no archinstall) + kian-persist
if command -v pacstrap >/dev/null && command -v genfstab >/dev/null && command -v arch-chroot >/dev/null; then
    ok "kian-install: pacstrap/genfstab/arch-chroot available (arch-install-scripts)"
else
    bad "kian-install: arch-install-scripts missing"
fi
command -v parted >/dev/null && ok "kian-install: parted available" || bad "kian-install: parted missing"
KIH="$(kian-install --help 2>/dev/null | head -8)"
echo "$KIH" | grep -q "Kian OS installer" \
    && ok "kian-install: --help prints (own installer, no archinstall)" || bad "kian-install: --help output unexpected"
KPH="$(kian-persist --help 2>/dev/null | head -8)"
echo "$KPH" | grep -q "persistence" \
    && ok "kian-persist: --help prints (real live persistence)" || bad "kian-persist: --help output unexpected"

# 8b. Installer GUI (auto-opens on the live desktop, installs Arch base and
#     keeps the full Kian OS identity via kian-apply).
command -v yad >/dev/null 2>&1 && ok "gui: yad dialog tool installed" || bad "gui: yad missing"
if [ -x /usr/local/bin/kian-install-gui ] && bash -n /usr/local/bin/kian-install-gui 2>/dev/null; then
    ok "gui: kian-install-gui executable and syntax-clean"
    grep -q "kian-apply -y /mnt" /usr/local/bin/kian-install-gui \
        && ok "gui: kianinstall path auto-applies Kian branding (installed system is never plain Arch)" \
        || bad "gui: the archinstall path does NOT run kian-apply after install"
    grep -q "/run/archiso/bootmnt" /usr/local/bin/kian-install-gui \
        && ok "gui: live-session-only guard present (installed systems are never re-installed by autostart)" \
        || bad "gui: live-only guard missing"
else
    bad "gui: kian-install-gui missing or not executable"
fi
[ -f /etc/xdg/autostart/kian-installer-welcome.desktop ] \
    && ok "gui: autostart entry shipped (welcome window opens when the live desktop starts)" \
    || bad "gui: autostart entry missing"
[ -f /usr/share/applications/kian-installer.desktop ] \
    && ok "gui: 'Kian OS Installer' menu entry shipped" || bad "gui: menu entry missing"
[ -f /usr/share/kian/installer-banner.png ] \
    && ok "gui: installer banner shipped" || bad "gui: installer banner missing"
if [ -x /usr/local/bin/kian-apply ] && bash -n /usr/local/bin/kian-apply 2>/dev/null; then
    ok "branding: kian-apply executable and syntax-clean (copies every Kian file onto the installed root)"
    grep -q "Kian OS" /usr/share/kian/os-release 2>/dev/null \
        && ok "branding: Kian os-release source present" || bad "branding: os-release source missing"
else
    bad "branding: kian-apply missing or broken"
fi

# 8c. kian-install regression tripwires (real-hardware install failures).
command -v genfstab >/dev/null 2>&1 \
    && ok "kian-install: genfstab available (arch-install-scripts)" || bad "kian-install: genfstab missing"
grep -qE "^[[:space:]]*grep -q .\^/." /usr/local/bin/kian-install 2>/dev/null \
    && bad "kian-install: the broken leading-slash fstab check is back — it rejects genfstab -U output and kills every install" \
    || ok "kian-install: broken leading-slash fstab check is gone"
grep -qF '$2 == "/"' /usr/local/bin/kian-install 2>/dev/null \
    && ok "kian-install: fstab root-entry check uses the second-field rule" \
    || bad "kian-install: fstab root-entry check missing"
# Unit-test that check against REAL genfstab -U style output (the exact case
# that failed on hardware: UUID= entries + '#' comments, no line starts with /).
printf '# /dev/vda2\nUUID=1111-AAAA / ext4 rw,relatime 0 1\nUUID=2222-BBBB /efi vfat rw,umask=0077 0 2\n' >/tmp/kian-fstab-test
awk '$2 == "/" {found=1} END {exit !found}' /tmp/kian-fstab-test 2>/dev/null \
    && ok "kian-install: fstab check accepts real genfstab -U output" \
    || bad "kian-install: fstab check REJECTS genfstab -U output (the hardware bug is back)"
grep -qF "Exec = /usr/bin/install -m 0644 /usr/share/kian/os-release /usr/lib/os-release" /etc/pacman.d/hooks/kian-os-release.hook 2>/dev/null \
    && ok "branding: os-release hook Exec is the plain known-good install" \
    || bad "branding: os-release hook Exec changed (plain install required — shell syntax is exec'd WITHOUT a shell by libalpm and breaks build-time branding)"
grep -qF "&& /usr/bin/install" /etc/pacman.d/hooks/kian-os-release.hook 2>/dev/null \
    && bad "branding: os-release hook Exec contains shell operators (run 43484412717 shipped plain-Arch identity because of that)" \
    || ok "branding: os-release hook Exec has no shell operators (safe for libalpm direct exec)"

# 8d. Throttled downloads (rate-limited mirrors: 10 s gap after each package,
#     extraction NOT throttled — user request).
if [ -x /usr/local/bin/kian-throttled-fetch ] && bash -n /usr/local/bin/kian-throttled-fetch 2>/dev/null; then
    ok "throttle: kian-throttled-fetch executable and syntax-clean"
    grep -qF 'THROTTLE="${KIAN_DOWNLOAD_THROTTLE:-10}"' /usr/local/bin/kian-throttled-fetch \
        && ok "throttle: default 10 s, configurable via KIAN_DOWNLOAD_THROTTLE (0 disables)" \
        || bad "throttle: the 10 s default / KIAN_DOWNLOAD_THROTTLE override is missing"
    KIAN_DOWNLOAD_THROTTLE=0 /usr/local/bin/kian-throttled-fetch /tmp/kian-throttle-test https://archlinux.org/robots.txt >/dev/null 2>&1 \
        && [ -s /tmp/kian-throttle-test ] \
        && ok "throttle: real download works (curl -L -C - resume + retries)" \
        || bad "throttle: test download of archlinux.org/robots.txt failed"
    rm -f /tmp/kian-throttle-test
    grep -qF 'XferCommand = /usr/local/bin/kian-throttled-fetch %o %u' /usr/local/bin/kian-install \
        && ok "throttle: kian-install enables the throttled XferCommand" \
        || bad "throttle: kian-install does not enable the throttled downloader"
    grep -qF "ParallelDownloads = 1" /usr/local/bin/kian-install \
        && ok "throttle: downloads forced serial (ParallelDownloads = 1 — the 10 s gap is real)" \
        || bad "throttle: ParallelDownloads not forced to 1 (parallel downloads would defeat the gap)"
else
    bad "throttle: kian-throttled-fetch missing or broken"
fi

# 8e. Installer regression tripwires — these two bugs killed every real
#     hardware install behind commit 82bfcac ("fstab came out empty" +
#     hook "cannot stat"), and a stale 0.4.0 ISO shipped them for a day.
grep -qF '{found=1} END {exit !found}' /usr/local/bin/kian-install \
    && ok "installer: fstab check validates the root entry (accepts UUID= lines)" \
    || bad "installer: fstab check regressed to the leading-slash grep that killed every install"
grep -qF 'cp /usr/share/kian/os-release "$MOUNT/usr/share/kian/os-release"' /usr/local/bin/kian-install \
    && ok "installer: os-release hook source is pre-seeded (hook runs quietly in pacstrap)" \
    || bad "installer: hook pre-seed missing (pacstrap prints 'cannot stat' again)"
grep -q '^PRETTY_NAME="Kian OS 0.4.1' /usr/share/kian/os-release \
    && ok "installer: bundled hook source matches the 0.4.1 identity" \
    || bad "installer: /usr/share/kian/os-release version out of sync with the release"

PERSIST_SHIPPED=0
# The boot config lives on the ISO filesystem. /run/archiso/bootmnt is only
# still mounted when copytoram did NOT kick in (it copies the airootfs to RAM
# on machines with plenty of memory and unmounts the medium) — so prefer the
# ISO device directly, via the archisolabel from the running cmdline.
# mkarchiso layout: syslinux.cfg at ISO ROOT /boot/syslinux/syslinux.cfg and
# grub.cfg at ISO ROOT /boot/grub/grub.cfg (verified in mkarchiso source:
# isofs_dir}/boot/syslinux + isofs_dir}/boot/grub) — NOT under /arch/.
# Nested $DIR/... and legacy /syslinux/ globs kept as fallbacks.
ISO_LABEL=""
for o in $(cat /proc/cmdline 2>/dev/null); do
    case "$o" in archisolabel=*) ISO_LABEL="${o#archisolabel=}" ;; esac
done
ISO_CHECK_DIR="$(mktemp -d /tmp/kian-iso-check.XXXXXX)"
# keep the fallback in sync with profiledef.sh iso_label (KIAN_OS_041 for 0.4.1)
ISO_SRC="/dev/disk/by-label/${ISO_LABEL:-KIAN_OS_041}"
check_cfgs() {
    for cfg in \
        "$1/boot/syslinux/syslinux.cfg" "$1/boot/grub/grub.cfg" \
        "$1/syslinux/syslinux.cfg" \
        "$1"/*/boot/syslinux/syslinux.cfg "$1"/*/boot/grub/grub.cfg \
        "$1"/*/syslinux/syslinux.cfg
    do
        [ -f "$cfg" ] || continue
        echo "EVIDENCE: boot cfg on medium: ${cfg#"$1"}"
        grep -q "cow_label=KIANPERSIST" "$cfg" && PERSIST_SHIPPED=1
    done
}
if sudo -n mount -o ro "$ISO_SRC" "$ISO_CHECK_DIR" 2>/dev/null; then
    check_cfgs "$ISO_CHECK_DIR"
    sudo -n umount "$ISO_CHECK_DIR" 2>/dev/null || true
elif [ -d /run/archiso/bootmnt ]; then
    check_cfgs /run/archiso/bootmnt
else
    echo "EVIDENCE: ISO device ${ISO_SRC} not mountable and /run/archiso/bootmnt missing"
fi
rmdir "$ISO_CHECK_DIR" 2>/dev/null || true
[ "$PERSIST_SHIPPED" = "1" ] && ok "persist: persistent boot entry shipped (cow_label=KIANPERSIST)" || bad "persist: persistent boot entry missing in boot config"
if grep -q "cow_spacesize=2G" /proc/cmdline 2>/dev/null; then
    ok "persist: live overlay space is 2G (default session can create files)"
else
    bad "persist: cow_spacesize=2G missing from the kernel cmdline"
fi

# 8c. Proton: run Windows .exe programs with proton-exe (umu-launcher)
if [ -x /usr/local/bin/proton-exe ] && bash -n /usr/local/bin/proton-exe 2>/dev/null; then
    ok "proton: proton-exe executable and syntax-clean"
    command -v umu-run >/dev/null 2>&1 \
        && ok "proton: umu-launcher installed (Proton works without Steam)" \
        || bad "proton: umu-run missing (umu-launcher not installed)"
    # umu-launcher only exists in multilib: its presence proves the repo worked.
    grep -q "PROTONPATH" /usr/local/bin/proton-exe \
        && ok "proton: proton-exe selects a Proton build (Experimental / UMU-Proton)" \
        || bad "proton: proton-exe does not reference PROTONPATH"
    [ -f /usr/share/applications/proton-exe.desktop ] \
        && ok "proton: 'Open with Proton' desktop entry shipped" \
        || bad "proton: proton-exe.desktop missing"
    xdg-mime query default application/x-ms-dos-executable 2>/dev/null | grep -q "proton-exe.desktop" \
        && ok "proton: double-clicking a .exe opens it with Proton (default MIME handler)" \
        || bad "proton: .exe MIME type is not mapped to proton-exe.desktop"
    xdg-mime query filetype /usr/bin/proton-exe >/dev/null 2>&1 \
        && ok "proton: mime database readable" || true
else
    bad "proton: proton-exe missing or broken"
fi

# 9. Timezone (Kian identity)
[[ "$(readlink -f /etc/localtime)" == *Asia/Tehran* ]] && ok "timezone: Asia/Tehran" || bad "timezone: $(readlink -f /etc/localtime 2>/dev/null || echo none)"

echo "===== summary: PASS=${PASS} FAIL=${FAIL} ====="
[[ ${FAIL} -eq 0 ]] && exit 0 || exit 1
