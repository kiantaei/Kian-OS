#!/bin/bash
# Functional test for kian-install v3 (Ubuntu) questions + help outputs.
set -uo pipefail
SRC=/home/z/my-project/work/Kian-OS/live/config/includes.chroot/usr/local/bin/kian-install
T=$(mktemp -d /tmp/kian-inst3-test.XXXXXX)

# help outputs must work without root/GUI
echo "--- help outputs ---"
bash "$SRC" --help | head -3
H1="$(bash "$SRC" --help | grep -c 'mirror region')"
echo "help-mentions-region=$H1"
bash /home/z/my-project/work/Kian-OS/live/config/includes.chroot/usr/local/bin/kian-persist --help | head -3
bash /home/z/my-project/work/Kian-OS/live/config/includes.chroot/usr/local/bin/kian-welcome --help | head -4

# extract the question machinery verbatim and drive it with piped input
sed -n '/^MIRRORS=(/,/^)/p' "$SRC" > "$T/frag.sh"
sed -n '/^ask_region()/,/^}/p' "$SRC" >> "$T/frag.sh"
sed -n '/^write_target_sources()/,/^}/p' "$SRC" >> "$T/frag.sh"
sed -n '/^TZ_OPTIONS=(/,/^)/p' "$SRC" >> "$T/frag.sh"
sed -n '/^ask_timezone()/,/^}/p' "$SRC" >> "$T/frag.sh"
sed -n '/^boot_medium_disk()/,/^}/p' "$SRC" >> "$T/frag.sh"
sed -n '/^disk_is_fresh()/,/^}/p' "$SRC" >> "$T/frag.sh"
sed -n '/^pick_target_disk()/,/^}/p' "$SRC" >> "$T/frag.sh"
cat >> "$T/frag.sh" <<'EOF'
say() { printf '[kian] %s\n' "$*"; }
warn() { printf '[kian-warn] %s\n' "$*"; }
die() { printf 'DIE-ABORT: %s\n' "$*"; exit 1; }

# Q1: Iran (option 1)
ask_region <<< "1" >/dev/null 2>&1
echo "MIRROR=$MIRROR_HOST"
[ "$MIRROR_HOST" = "ir.archive.ubuntu.com" ] && echo "PASS region-iran" || echo "FAIL region-iran ($MIRROR_HOST)"

# Q1: out-of-range then geoip-auto (option 8)
ask_region <<< "999
8" >/dev/null 2>&1
echo "PASS region-invalid-then-auto (mirror=$MIRROR_HOST, geoip may fail offline -> fallback)"

# Q2: Tehran (option 1)
ask_timezone <<< "1" >/dev/null 2>&1
echo "TIMEZONE=$TIMEZONE"
[ "$TIMEZONE" = "Asia/Tehran" ] && echo "PASS timezone-tehran" || echo "FAIL timezone-tehran"

# Q2: free text (option 14 = other)
ask_timezone <<< "14
Asia/Qatar" >/dev/null 2>&1
[ "$TIMEZONE" = "Asia/Qatar" ] && echo "PASS timezone-freetext" || echo "FAIL timezone-freetext"

# EOF on stdin must abort, not loop
( ask_region <<< "" >/dev/null 2>&1 )
[ $? -eq 1 ] && echo "PASS region-eof-aborts" || echo "FAIL region-eof-aborts"
EOF
bash "$T/frag.sh" 2>&1 | grep -E '^(MIRROR=|TIMEZONE=|PASS|FAIL|DIE-ABORT)'
rm -rf "$T"
