# Kian OS — Project Memory (for AI agents)

## Identity (updated for the 1.0 Ubuntu rebase)
- OS: **Kian OS**, version **1.0.0 "Horizon"** — **Ubuntu 24.04 LTS (noble)
  base**, built with **live-build** (NOT archiso anymore; NOT a from-scratch
  kernel).
- Kernel: Ubuntu noble `linux-image-generic` (6.8) + systemd + GNU userland.
- Package manager: **apt/dpkg only. pacman must NOT exist anywhere** — the
  chroot hook and CI both assert `command -v pacman` fails.
- Boot: live-build iso-hybrid; **ISOLINUX (BIOS) + GRUB (UEFI)**; live-boot +
  live-config handle the live system (user creation, autologin, tz/locale).
- Live login: `kian` / `live` (live-config user-setup default password "live";
  `username=kian` boot param). Installed system: `kian` / `kian`.
  Live sudo is passwordless via `/etc/sudoers.d/kian-live` (casper-style) —
  removed on install by `kian-cleanup-live-only`.
- Time zone: Asia/Tehran via `timezone=Asia/Tehran` boot param (live-config).
- Desktop: XFCE4, Kian-branded; dark gradient wallpaper, `#0078D4` accent;
  xfwm4 compositing OFF by default (VMware black-window fix).
- Browser: **xenox = falkon wrapper** (chromium/firefox are snap shims on
  noble — never ship them). neofetch is the real noble package (no wrapper).

## Repository layout (post-1.0 rebase)
```
live/                    the OS (live-build profile)
  auto/config            lb config: --mode ubuntu --distribution noble, boot params
  config/package-lists/  kian.list.chroot (all names verified against noble index)
  config/includes.chroot/  Kian overlay: os-release(ID=kian, ID_LIKE=debian ubuntu),
                         motd(apt), skel+icons+wallpaper, xenox, kian-welcome,
                         kian-install, kian-persist, calamares settings/branding,
                         vendored automirror module, kian-cleanup-live-only
  config/hooks/          1010-kian-desktop.hook.chroot (lightdm, grub-pc preseed,
                         sanity: no pacman, falkon/calamares present, YAML valid)
scripts/ci-boot-test.sh  real QEMU boot test (serial login kian/live + checks)
build.sh, Makefile       local build entry points (lb build wrappers)
docs/verification.md     checklist + evidence mapping
kian-os.vmx              VMware config (ISO name kian-os-1.0.0-amd64.iso)
archive/archiso-arch-base/    the 0.4.0 Arch profile (frozen, NOT built)
archive/legacy-standalone/    pre-0.4.0 from-scratch kernel (frozen)
```

## Key mechanisms (verified from the real packages — don't reinvent)
- **live-boot 20230131** (noble universe) persistence: boot params
  `persistence persistence-label=KIANPERSIST overlay-size=4G`. live-boot
  scans for a filesystem/file labelled KIANPERSIST, reads
  `/persistence.conf` from it; the line `/ union` = full-root overlayfs
  persistence (upperdir+workdir on the store). **If no store exists it
  falls back to a RAM overlay and boots normally** (9990-overlay.sh
  `cowdevice="tmpfs"`) — so persistence params are safe on EVERY entry
  (unlike archiso, which dropped into an emergency shell).
- **live-config 11.0.4** boot params: `username=`, `user-fullname=`,
  `locales=`, `timezone=`, `keyboard-layouts=`. Creates the live user via
  debconf user-setup (password hash for "live" baked in), adds to sudo
  group, configures lightdm autologin (0100-lightdm).
- **Calamares 3.3.5** with `settings.conf` sequence: partition, mount,
  unpackfs (source `/run/live/medium/live/filesystem.squashfs` — the
  live-boot medium path), machineid, fstab, locale/localecfg, users,
  displaymanager(lightdm), networkcfg, hwclock, initramfscfg+initramfs,
  grubcfg, bootloader (efiBootloaderId KIAN_OS), automirror, umount.
  `automirror` is vendored from calamares-settings-ubuntu-common (GPL-3+,
  header kept); it geoip-detects the country and writes
  `<cc>.archive.ubuntu.com` into the target's
  `/etc/apt/sources.list.d/ubuntu.sources` (needs python3-distro).
- **Installer launch**: Ubuntu flavors run calamares via a wrapper
  `sudo -E calamares` (`calamares-launch-normal`, shipped in usr/bin) —
  works because the live sudo is NOPASSWD. pkexec is NOT used.
- **kian-install** (CLI): 2 questions (mirror region from a VERIFIED list —
  ir/tr/de/fr/uk/jp.archive.ubuntu.com probed HTTP 200 on noble Release on
  2026-11-12 + geoip auto + default archive.ubuntu.com; timezone list +
  free IANA text) → auto disk (fresh-first, ≥10 GiB, not the boot medium,
  10 s erase countdown, `--disk` override) → GPT+ESP or MBR →
  `rsync -aAHXx` the live root (one-fs flag skips mounts; excludes only
  machine-ids) → deb822 `kian-ubuntu.sources` with the chosen mirror →
  chroot: tz/locale/hostname/machine-id/chpasswd kian:kian/update-initramfs/
  grub-install(KIAN_OS)/update-grub/enable lightdm+NetworkManager →
  kian-cleanup-live-only. Offline-capable like Ubiquity.
- **kian-welcome** (GTK3 PyGObject, autostart desktop file): buttons =
  graphical install (calamares-launch-normal), terminal install
  (kian-install), persistence (kian-persist), terminal. LIVE-ONLY GUARD:
  exits silently unless `/run/live/medium` exists — installed systems never
  show it even before cleanup removes the autostart.
- EOF-on-stdin in interactive reads must abort (`read || die`), never loop —
  found by the functional test in `scripts/test_kian_install_ask.sh`.

## Honesty rules (project policy)
- Never fabricate build or test results; CI logs are the source of truth.
- Package names in `live/config/package-lists/kian.list.chroot` verified
  against the noble archive indexes (main/universe/restricted/multiverse)
  downloaded from archive.ubuntu.com — re-verify when touching.
- No fake icons (emoji), no fake commands, no simulated outputs.
- Archived code must never be mixed into the build: only `live/` is built.

## CI debugging trail (Ubuntu rebase, 2026-11-12) — real failures and fixes
1. exit 126: auto/config not executable → chmod +x + `sh ./auto/config` in build.sh.
2. `--updates`/`--backports` are NOT live-build options in 3.0~a57 → removed.
3. live-build ubuntu49's --firmware-* pass feeds mangled linux-firmware file
   lists to apt ("COMPUTER", "Corp.-VALLEYVIEW"...) → --firmware-binary/chroot
   false + linux-firmware shipped explicitly in the package list.
4. grub-efi-amd64 CONFLICTS with grub-pc → ship grub-efi-amd64 + both -bin
   packages; BIOS installs use `grub-install --target=i386-pc` (modules from
   grub-pc-bin).
5. ubuntu-mode lb tries live-config-upstart (dead in noble) → --initsystem systemd.
6. ubuntu-mode lb default syslinux theme pulls dead syslinux-themes-ubuntu-oneiric
   + gfxboot-theme-ubuntu → --syslinux-theme live-build.
7. live-build's theme templates lack isolinux.bin + noble's 6.04 .c32 modules →
   config/bootloaders/isolinux/ override dir ships isolinux.bin, isohdpfx.bin,
   ldlinux/libcom32/libutil/vesamenu .c32 (same 6.04 ABI) + lb cfg templates.
8. ubuntu-mode lb unconditionally extracts gfxboot-theme-ubuntu/bootlogo.tar.gz
   (package deleted from noble) → chroot hook writes a minimal valid stub tar
   (empty cpio bootlogo) so the extraction succeeds harmlessly.
Next failure, if any, would appear in lb_binary_iso (xorriso) — logs in Actions.
