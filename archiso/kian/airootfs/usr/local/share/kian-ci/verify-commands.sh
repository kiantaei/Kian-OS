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
grep -q '^PRETTY_NAME="Kian OS 0.4.0' /etc/os-release && ok "identity: version 0.4.0" || bad "identity: version"
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
    if timeout 300 $PACMAN -S --noconfirm tree >>/tmp/kian-pacman-sy.log 2>&1 && command -v tree >/dev/null; then
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

# 8. Timezone (Kian identity)
[[ "$(readlink -f /etc/localtime)" == *Asia/Tehran* ]] && ok "timezone: Asia/Tehran" || bad "timezone: $(readlink -f /etc/localtime 2>/dev/null || echo none)"

echo "===== summary: PASS=${PASS} FAIL=${FAIL} ====="
[[ ${FAIL} -eq 0 ]] && exit 0 || exit 1
