#!/bin/bash
# Kian OS CI — END-TO-END DISK INSTALL TEST.
#
# Boots the ISO in QEMU with an extra 14 GiB virtual disk, logs in on the
# serial console as kian/kian and drives the REAL installer
# (`sudo kian-install`) through its actual prompts:
#   region "20" = keep the bundled mirrors, timezone "2" = UTC,
#   Enter at the 10-second erase countdown,
# then waits for "Kian OS is installed on /dev/vda". After the VM powers
# off, the installed disk image is loop-mounted ON THE RUNNER and verified
# offline against the full Kian OS identity — proving the installed system
# is KIAN OS (never plain Arch) and that /etc/fstab really has a root entry
# (the real-hardware "fstab came out empty" bug that killed every install
# is impossible to ship again once this test is green).
#
# Usage: scripts/ci-install-test.sh <path-to.iso> <disk-image> <output-dir>
#
# HONESTY POLICY: every check prints PASS/FAIL from what actually happened;
# any failure changes the exit code and is visible in the job log.
set -uo pipefail

ISO="${1:?usage: ci-install-test.sh <iso> <diskimg> <outdir>}"
DISKIMG="${2:?usage: ci-install-test.sh <iso> <diskimg> <outdir>}"
OUTDIR="${3:?usage: ci-install-test.sh <iso> <diskimg> <outdir>}"
TRANSCRIPT=/tmp/kian-install-test-transcript.txt
# The expect script reads these via $env(ISO) / $env(DISKIMG) — they MUST be
# exported or qemu is spawned with an empty -cdrom and the test dies at spawn
# (this is exactly what failed run 43484412717's install test).
export ISO DISKIMG

command -v expect   >/dev/null || { echo "FATAL: expect missing"; exit 2; }
command -v qemu-system-x86_64 >/dev/null || { echo "FATAL: qemu missing"; exit 2; }
command -v losetup  >/dev/null || { echo "FATAL: losetup missing (privileged container required)"; exit 2; }
[[ -s "${ISO}" ]]     || { echo "FATAL: ISO missing/empty: ${ISO}"; exit 2; }
[[ -s "${DISKIMG}" ]] || { echo "FATAL: disk image missing/empty: ${DISKIMG}"; exit 2; }

mkdir -p "${OUTDIR}"
rm -f "${TRANSCRIPT}" /tmp/kian-qemu-inst.sock
SUMMARY="${OUTDIR}/install-test-summary.txt"
: > "${SUMMARY}"

STATUS=0
expect <<'EOT' || STATUS=1
    set timeout 3600
    log_file -a /tmp/kian-install-test-transcript.txt
    spawn qemu-system-x86_64 \
        -m 3072 -smp 4 \
        -cdrom $env(ISO) -boot d \
        -drive file=$env(DISKIMG),format=raw,if=virtio \
        -display none -serial mon:stdio -no-reboot \
        -monitor unix:/tmp/kian-qemu-inst.sock,server,nowait

    # --- 1. serial login as kian (same flow as the boot test) ---
    expect {
        -re "kian-os login:" { puts "\n=== INSTALL_TEST: login prompt reached ===" }
        timeout { puts "\n=== INSTALL_FAIL: no login prompt ==="; exit 10 }
        eof     { puts "\n=== INSTALL_FAIL: qemu exited early ==="; exit 11 }
    }
    send "kian\r"
    expect {
        -re "Password:" {}
        timeout { puts "INSTALL_FAIL: no password prompt"; exit 12 }
    }
    send "kian\r"
    set timeout 1500
    expect {
        -re {kian@kian-os} {}
        timeout { puts "INSTALL_FAIL: no shell session"; exit 13 }
    }
    send "\r"
    expect { -re {\$ } {} timeout {} }
    set timeout 300

    # --- 2. run the real installer, answer its real prompts ---
    # region: REGIONS has 19 entries, then "keep the bundled mirrors" = 20
    #         (KEEP_IDX in kian-install). Bundled mirrors are replaced by the
    #         built-in fallback list when the ISO's list has no active server.
    # timezone: TZ_OPTIONS[1] = "UTC" (selection number 2).
    send "sudo kian-install\r"
    expect {
        "Select region" {}
        timeout { puts "INSTALL_FAIL: no region prompt"; exit 14 }
    }
    send "20\r"
    expect {
        "Select timezone" {}
        timeout { puts "INSTALL_FAIL: no timezone prompt"; exit 15 }
    }
    send "2\r"
    expect {
        "Continuing in 10 seconds" {}
        timeout { puts "INSTALL_FAIL: no erase countdown"; exit 16 }
    }
    send "\r"
    puts "\n=== INSTALL_TEST: installer running (pacstrap downloads a few GB) ==="

    # --- 3. wait for the success banner (this is the step that used to die
    #        with 'fstab came out empty' on real hardware) ---
    set timeout 3600
    expect {
        -re "Kian OS is installed on /dev/vda" {
            puts "\n=== INSTALL_OK: kian-install completed on /dev/vda ==="
        }
        timeout { puts "\n=== INSTALL_FAIL: installer did not finish in time ==="; exit 17 }
        eof     { puts "\n=== INSTALL_FAIL: qemu died during install ==="; exit 18 }
    }

    # --- 4. clean poweroff ---
    send "sudo poweroff\r"
    expect {
        eof { puts "\n=== INSTALL_TEST: VM powered off cleanly ===" }
        timeout { puts "\n=== POWEROFF_TIMEOUT (continuing to offline verify) ===" }
    }
EOT

cp "${TRANSCRIPT}" "${OUTDIR}/kian-install-transcript.txt" 2>/dev/null || true
if [[ ${STATUS} -ne 0 ]]; then
    echo "RESULT: INSTALL TEST FAILED (expect exit ${STATUS}) — installer transcript in artifacts" | tee -a "${SUMMARY}"
    exit ${STATUS}
fi

# --- 5. offline verification of the INSTALLED disk image --------------------
LOOP="$(sudo losetup -fP --show "${DISKIMG}")"
if [[ -z "${LOOP}" ]]; then
    echo "RESULT: INSTALL TEST FAILED (could not loop-mount the installed image)" | tee -a "${SUMMARY}"
    exit 19
fi
ROOTP="${LOOP}p1"
[[ -b "${LOOP}p2" ]] && ROOTP="${LOOP}p2"   # UEFI layout: p2 = root
MNT=/mnt/kian-diskcheck
sudo mkdir -p "${MNT}"
if ! sudo mount -o ro "${ROOTP}" "${MNT}" 2>/dev/null; then
    echo "RESULT: INSTALL TEST FAILED (could not mount installed root ${ROOTP})" | tee -a "${SUMMARY}"
    sudo losetup -d "${LOOP}" 2>/dev/null || true
    exit 20
fi

PASS=0; FAIL=0
chk() {   # chk <description> <command...>
    local desc="$1"; shift
    if "$@" >/dev/null 2>&1; then
        PASS=$((PASS+1)); echo "CHECK_INSTALL_PASS: $desc" | tee -a "${SUMMARY}"
    else
        FAIL=$((FAIL+1)); echo "CHECK_INSTALL_FAIL: $desc" | tee -a "${SUMMARY}"
    fi
}
R="${MNT}"
chk "identity: /usr/lib/os-release is Kian OS"                grep -q '^NAME="Kian OS"' "$R/usr/lib/os-release"
chk "identity: ID=kian in os-release"                         grep -q '^ID=kian' "$R/usr/lib/os-release"
chk "fstab: root entry present (THE hardware bug)"            awk '$2 == "/" {found=1} END {exit !found}' "$R/etc/fstab"
chk "fstab: entries use UUID= (genfstab -U)"                  grep -q '^UUID=' "$R/etc/fstab"
chk "xenox browser installed and executable"                  test -x "$R/usr/bin/xenox"
chk "Kian wallpaper installed"                                test -s "$R/usr/share/backgrounds/kian/kian-wallpaper.jpg"
chk "Kian app launcher present (kian-files)"                  test -f "$R/usr/share/applications/kian-files.desktop"
chk "at least 5 Kian launchers (not plain Arch)"              bash -c 'ls '"$R"'/usr/share/applications/kian-*.desktop 2>/dev/null | wc -l | grep -qE "^([5-9]|[0-9]{2})$"'
chk "GRUB menu is branded Kian OS (GRUB_DISTRIBUTOR)"         grep -q '^GRUB_DISTRIBUTOR="Kian OS"' "$R/etc/default/grub"
chk "GRUB config generated and mentions Kian OS"              grep -q "Kian OS" "$R/boot/grub/grub.cfg"
chk "Linux kernel installed (vmlinuz-linux)"                  test -s "$R/boot/vmlinuz-linux"
chk "hostname is kian-os"                                     grep -qx kian-os "$R/etc/hostname"
chk "user kian exists (uid 1000)"                             bash -c 'grep -q "^kian:x:1000:" "'"$R"'/etc/passwd"'
chk "graphical login enabled (display-manager -> lightdm)"    bash -c 'readlink -f "'"$R"'/etc/systemd/system/display-manager.service" | grep -q lightdm'
chk "hook source /usr/share/kian/os-release installed"        test -f "$R/usr/share/kian/os-release"
chk "os-release hook has the missing-source guard"            grep -qF "[ -f /usr/share/kian/os-release ]" "$R/etc/pacman.d/hooks/kian-os-release.hook"
chk "sudo wheel rule installed"                               test -f "$R/etc/sudoers.d/kian-wheel"
chk "session helper kian-trust-desktop installed"             test -x "$R/usr/local/bin/kian-trust-desktop"
chk "motd says Kian OS (identity everywhere)"                 grep -q "Kian OS" "$R/etc/motd"

sudo umount "${MNT}" 2>/dev/null || true
sudo rmdir "${MNT}" 2>/dev/null || true
sudo losetup -d "${LOOP}" 2>/dev/null || true

if [[ ${FAIL} -eq 0 ]]; then
    echo "RESULT: INSTALL TEST PASSED (${PASS}/${PASS} checks — the installed disk is full Kian OS)" | tee -a "${SUMMARY}"
else
    echo "RESULT: INSTALL TEST FAILED (${PASS} passed, ${FAIL} failed — see CHECK_INSTALL_FAIL above)" | tee -a "${SUMMARY}"
    exit 21
fi
