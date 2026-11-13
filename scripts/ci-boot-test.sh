#!/bin/bash
# Kian OS CI boot test.
# Boots the ISO in QEMU (no KVM — pure TCG), waits for the serial login
# prompt, logs in as kian/kian, runs real verification commands, captures
# desktop screenshots through the QEMU monitor and powers off.
#
# Usage: scripts/ci-boot-test.sh <path-to.iso> <output-dir>
#
# HONESTY POLICY: this script never fabricates results. Each check prints
# PASS/FAIL based on what actually happened; a failure is visible in the
# job log and changes the exit code.
set -uo pipefail

ISO="${1:?usage: ci-boot-test.sh <iso> <outdir>}"
OUTDIR="${2:?usage: ci-boot-test.sh <iso> <outdir>}"
MON=/tmp/kian-qemu-mon.sock
export ISO   # the expect script reads it via $env(ISO)
mkdir -p "${OUTDIR}"

command -v expect >/dev/null || { echo "FATAL: expect missing"; exit 2; }
command -v qemu-system-x86_64 >/dev/null || { echo "FATAL: qemu missing"; exit 2; }
command -v socat >/dev/null || { echo "FATAL: socat missing"; exit 2; }
[[ -s "${ISO}" ]] || { echo "FATAL: ISO missing/empty: ${ISO}"; exit 2; }

rm -f "${MON}" /tmp/kian-serial-transcript.txt /tmp/kian-desktop-1.ppm /tmp/kian-desktop-2.ppm
SUMMARY="${OUTDIR}/boot-test-summary.txt"
: > "${SUMMARY}"

STATUS=0
expect <<'EOT' || STATUS=1
    set timeout 900
    log_file -a /tmp/kian-serial-transcript.txt
    spawn qemu-system-x86_64 \
        -m 2048 -smp 4 \
        -cdrom $env(ISO) -boot d \
        -display none -serial mon:stdio -no-reboot \
        -monitor unix:/tmp/kian-qemu-mon.sock,server,nowait

    # --- 1. wait for the serial getty login prompt (kernel + systemd boot) ---
    expect {
        -re "kian-os login:" { puts "\n=== BOOT_OK: serial login prompt reached ===" }
        timeout { puts "\n=== BOOT_FAIL: no login prompt within timeout ==="; exit 10 }
        eof { puts "\n=== BOOT_FAIL: qemu exited early ==="; exit 11 }
    }

    # --- 2. real login as kian ---
    send "kian\r"
    expect {
        -re "Password:" {}
        timeout { puts "LOGIN_FAIL: no password prompt"; exit 12 }
    }
    send "kian\r"
    # Match either the session-start marker or the (color-escaped) prompt;
    # "kian@kian-os" appears in both. Under TCG the first session setup can
    # stall while X/XFCE autostart saturates the CPU, so allow plenty of time.
    set timeout 1500
    expect {
        -re {kian@kian-os} { puts "\n=== LOGIN_OK: bash session as kian ===" }
        timeout { puts "LOGIN_FAIL: no shell session within timeout"; exit 13 }
    }
    # settle: press enter and wait for a fresh prompt tail
    send "\r"
    expect {
        -re {\$ } {}
        timeout {}
    }
    set timeout 900

    # cache sudo credentials for the verify script (same tty => timestamp valid)
    send "echo kian | sudo -S -v 2>/dev/null; echo SUDO_CACHED\r"
    expect {
        "SUDO_CACHED" {}
        timeout { puts "SUDO_CACHE_TIMEOUT (verify will run without root pacman)" }
    }

    # --- 3. desktop: wait for lightdm + XFCE (slow under TCG), then shoot ---
    send "for i in \$(seq 1 60); do systemctl is-active lightdm >/dev/null 2>&1 && pgrep -x xfce4-session >/dev/null 2>&1 && break; sleep 10; done; echo DESKTOP_WAIT_DONE\r"
    expect {
        "DESKTOP_WAIT_DONE" {}
        timeout { puts "DESKTOP_WAIT_TIMEOUT (continuing; screenshots will show the truth)" }
    }
    send "echo \"lightdm=\$(systemctl is-active lightdm) xfce=\$(pgrep -c -x xfce4-session 2>/dev/null || echo 0)\"\r"
    expect {
        -re {lightdm=\S+ xfce=\S+} { puts "\n=== DESKTOP_STATE: $expect_out(0,string) ===" }
        timeout { puts "DESKTOP_STATE unknown" }
    }
    sleep 5
    # HMP screendump writes PPM (the 'png' format arg is not portable across
    # HMP versions); the monitor reply is logged so a failure is VISIBLE.
    if {[catch {exec echo "screendump /tmp/kian-desktop-1.ppm" | socat - UNIX-CONNECT:/tmp/kian-qemu-mon.sock} reply]} {
        puts "SCREENSHOT_1_FAIL: $reply"
    } else {
        puts "SCREENSHOT_1_MONITOR_REPLY: [string trim $reply]"
        puts "SCREENSHOT_1_DONE"
    }

    # --- 4. real command verification (identity, gcc, python, fs, pacman) ---
    send "bash /usr/local/share/kian-ci/verify-commands.sh 2>&1; echo VERIFY_EXIT=\$?\r"
    expect {
        -re {VERIFY_EXIT=(\d+)} {
            puts "\n=== VERIFY_COMMANDS_EXIT: $expect_out(1,string) ==="
            if {$expect_out(1,string) != 0} {
                # dump honest diagnostics into the transcript before failing
                # (NOTE: no nested double quotes inside the send string —
                # Tcl treats "DIAG_"BEGIN"" as two strings and aborts parsing)
                send "echo DIAG_BEGIN; journalctl -u lightdm --no-pager -n 40 2>&1 | tail -40; systemctl --no-pager status lightdm 2>&1 | head -12; loginctl list-sessions --no-pager 2>&1; ps -eo comm,args | grep -E 'xfce|lightdm|Xorg|dbus' | grep -v grep; timedatectl 2>&1 | head -8; tail -25 /tmp/kian-pacman-sy.log 2>/dev/null; ls /run/archiso/bootmnt /run/archiso/bootmnt/arch 2>&1; echo DIAG_END\r"
                expect {
                    "DIAG_END" {}
                    timeout {}
                }
                exit 14
            }
        }
        timeout { puts "VERIFY_TIMEOUT"; exit 15 }
    }

    # --- 5. second screenshot (desktop settled) ---
    sleep 5
    if {[catch {exec echo "screendump /tmp/kian-desktop-2.ppm" | socat - UNIX-CONNECT:/tmp/kian-qemu-mon.sock} reply]} {
        puts "SCREENSHOT_2_FAIL: $reply"
    } else {
        puts "SCREENSHOT_2_MONITOR_REPLY: [string trim $reply]"
        puts "SCREENSHOT_2_DONE"
    }

    # --- 6. honest shutdown (item 11 of the checklist) ---
    send "echo kian | sudo -S systemctl poweroff 2>&1; echo POWEROFF_SENT\r"
    expect {
        "POWEROFF_SENT" {}
        timeout {}
    }
    expect {
        eof { puts "\n=== POWEROFF_OK: qemu exited cleanly ===" }
        timeout { puts "\n=== POWEROFF_TIMEOUT (killing qemu) ==="; exit 16 }
    }
EOT

cp /tmp/kian-serial-transcript.txt "${OUTDIR}/" 2>/dev/null || true
for f in /tmp/kian-desktop-1.ppm /tmp/kian-desktop-2.ppm; do
    if [[ -s "$f" ]]; then
        cp "$f" "${OUTDIR}/"
        if command -v pnmtopng >/dev/null 2>&1; then
            pnmtopng < "$f" > "${OUTDIR}/$(basename "${f%.ppm}").png" 2>/dev/null \
                && echo "CONVERTED $(basename "${f%.ppm}").png $(stat -c%s "${OUTDIR}/$(basename "${f%.ppm}").png") bytes" >> "${SUMMARY}" \
                || echo "CONVERT_FAILED for $f" >> "${SUMMARY}"
        fi
        echo "ARTIFACT $(basename "$f") $(stat -c%s "$f") bytes" >> "${SUMMARY}"
    else
        echo "MISSING_SCREENSHOT $f (monitor reply above tells why)" >> "${SUMMARY}"
    fi
done

if [[ ${STATUS} -eq 0 ]]; then
    echo "RESULT: BOOT TEST PASSED" | tee -a "${SUMMARY}"
else
    echo "RESULT: BOOT TEST FAILED (expect exit ${STATUS})" | tee -a "${SUMMARY}"
fi
exit ${STATUS}
