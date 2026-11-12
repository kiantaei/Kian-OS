#!/bin/bash
# Functional test for kian-install's two questions and disk-detection helpers,
# run OUTSIDE the ISO (sandbox): extracts the arrays + functions from the real
# script and drives them with piped input. The EOF-abort case runs in a
# subshell LAST (die() exits, and must abort rather than loop forever).
set -uo pipefail
SRC=/home/z/my-project/work/Kian-OS/archiso/kian/airootfs/usr/local/bin/kian-install
T=$(mktemp -d /tmp/kian-inst-test.XXXXXX)

sed -n '/^REGIONS=(/,/^)/p' "$SRC" > "$T/frag.sh"
sed -n '/^TZ_OPTIONS=(/,/^)/p' "$SRC" >> "$T/frag.sh"
sed -n '/^ask_region()/,/^}/p' "$SRC" >> "$T/frag.sh"
sed -n '/^ask_timezone()/,/^}/p' "$SRC" >> "$T/frag.sh"
sed -n '/^boot_medium_disk()/,/^}/p' "$SRC" >> "$T/frag.sh"
sed -n '/^list_candidate_disks()/,/^}/p' "$SRC" >> "$T/frag.sh"
sed -n '/^disk_is_fresh()/,/^}/p' "$SRC" >> "$T/frag.sh"
cat >> "$T/frag.sh" <<'EOF'
say() { printf '[kian] %s\n' "$*"; }
die() { printf 'DIE-ABORT: %s\n' "$*"; exit 1; }
warn() { printf 'WARN: %s\n' "$*"; }

# Q1: select Iran (option 1)
ask_region <<< "1" >/dev/null 2>&1
echo "REGION=$REGION_CODE"
[ "$REGION_CODE" = "IR" ] && echo "PASS region-iran" || echo "FAIL region-iran"

# Q1: out-of-range must re-prompt, then auto (option 21)
ask_region <<< "999
21" >/dev/null 2>&1
[ "$REGION_CODE" = "auto" ] && echo "PASS region-invalid-then-auto" || echo "FAIL region-invalid-then-auto ($REGION_CODE)"

# Q2: Tehran = option 1
ask_timezone <<< "1" >/dev/null 2>&1
echo "TIMEZONE=$TIMEZONE"
[ "$TIMEZONE" = "Asia/Tehran" ] && echo "PASS timezone-tehran" || echo "FAIL timezone-tehran"

# Q2: free text (option 16 = other)
ask_timezone <<< "16
Asia/Qatar" >/dev/null 2>&1
[ "$TIMEZONE" = "Asia/Qatar" ] && echo "PASS timezone-freetext" || echo "FAIL timezone-freetext ($TIMEZONE)"

# disk helpers (no bootmnt in the sandbox — boot medium is empty)
BOOTDISK="$(boot_medium_disk)"
echo "BOOTDISK='${BOOTDISK}'"
CAND="$(list_candidate_disks)"
echo "CAND=$CAND"
while IFS='|' read -r p rest; do
    [ -n "$p" ] || continue
    if disk_is_fresh "$p"; then echo "FRESH=$p yes"; else echo "FRESH=$p no"; fi
done <<< "$CAND"

# EOF on stdin must ABORT (die), never loop forever — subshell so die() only
# exits the subshell
( ask_region <<< "" >/dev/null 2>&1 )
[ $? -eq 1 ] && echo "PASS region-eof-aborts" || echo "FAIL region-eof-aborts (exit $?)"
EOF
bash "$T/frag.sh" 2>&1 | grep -E '^(REGION=|TIMEZONE=|CAND=|BOOTDISK=|FRESH=|PASS|FAIL|DIE-ABORT)'
rm -rf "$T"
