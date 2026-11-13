# Kian OS — Project Memory (for AI agents)

## Identity (updated for the 0.4.0 Linux migration)
- OS: **Kian OS**, version 0.4.0 "Horizon" — **Linux-based** (Arch Linux base,
  built with archiso). NOT a from-scratch kernel anymore.
- Kernel: real **Linux** package + **systemd** + GNU userland.
- Boot: **GRUB** (UEFI) + syslinux (BIOS); bootmodes `('bios.syslinux' 'uefi.grub')`.
- Default login: `kian` / `kian` (live medium default). Root login disabled;
  `kian` is in `wheel` (sudo with password). Never run the desktop as root.
- Time zone: Asia/Tehran (UTC+03:30, fixed — Iran abolished DST 2022).
- Desktop: XFCE4, Kian-branded. Palette from the legacy theme: dark navy
  `#1A2B4A` → purple `#3B0A45` gradient wallpaper, accent `#0078D4`,
  Kian Terminal background `#0C0C0C`. Fonts: Liberation Sans (UI),
  DejaVu Sans Mono (terminal). Icons: custom Kian SVGs + Papirus-Dark.
- Apps: Xenox Browser (Chromium engine + Kian profile, `/usr/bin/xenox`),
  Kian Terminal (xfce4-terminal), Thunar, Mousepad, galculator,
  xfce4-settings, xfce4-taskmanager. No emoji icons anywhere.

## Repository layout (post-migration)
```
archiso/kian/            the OS itself (archiso profile)
  profiledef.sh          ISO identity, bootmodes, kernel params, file perms
  packages.x86_64        package list (names verified against archlinux.org)
  pacman.conf            build-time pacman config (from releng)
  grub/, syslinux/       boot menus + Kian splash/background
  airootfs/etc/          hostname/locale/motd/sudoers/lightdm/systemd units/skel
  airootfs/usr/          xenox, neofetch wrapper, kian-setup, verify-commands.sh,
                         wallpaper, SVG icons, xenox start page, os-release
scripts/ci-boot-test.sh  real QEMU boot test (serial login + checks + screenshots)
build.sh, Makefile       local build entry points (mkarchiso wrappers)
docs/verification.md     11-point checklist + evidence mapping
archive/legacy-standalone/   pre-0.4.0 from-scratch kernel/GUI (frozen, not built)
.github/workflows/build-iso.yml   archiso build + QEMU boot test + releases
```

## Key mechanisms (don't reinvent)
- Live user is created at boot by `kian-setup.service`
  (`/usr/local/bin/kian-setup`): useradd kian (uid 1000) + wheel + chpasswd +
  `usermod -L root` + home seeding from `/etc/skel`. The chpasswd is gated on
  a first-boot marker (`~/.kian-live-initialized`) so a PERSISTENT live
  session (see below) does not reset a changed password on every boot.
- pacman works live thanks to releng's `pacman-init.service` +
  `etc-pacman.d-gnupg.mount` (enabled in `multi-user.target.wants`).
- Services are enabled via `.wants` symlinks inside
  `airootfs/etc/systemd/system/` (mkarchiso copies them verbatim).
- LightDM autologin as kian on the live medium; greeter on logout.
- CI boots the ISO with `-serial mon:stdio` and logs in over serial
  (getty enabled on ttyS0; kernel params `console=ttyS0,115200n8 console=tty0`).
- `neofetch` command = fastfetch wrapper (neofetch was removed from Arch repos).
- Installer: `kian-install` is Kian OS's OWN installer (no archinstall) —
  asks exactly 2 questions (mirror region via the official mirrorlist API +
  timezone), auto-detects the target disk (empty disks first, 10 s erase
  countdown, `--disk /dev/sdX` override), GPT+ESP on UEFI / MBR on BIOS,
  pacstraps the full desktop set, chroot-configures locale/timezone/user/GRUB
  (`GRUB_DISTRIBUTOR="Kian OS"`), then `kian-apply -y /mnt`. Needs
  `arch-install-scripts` + `parted` (in packages.x86_64). `kianinstall` (the
  archinstall fork, vendored at /opt/kianinstall) stays as the advanced option.
- Persistence: archiso cow mechanics verified from mkinitcpio-archiso 73
  (`/usr/share/doc/mkinitcpio-archiso/README.bootparams`): with squashfs the
  overlay is OVERLAYFS; `cow_label=KIANPERSIST` mounts that partition as the
  overlay so upperdir/workdir persist; a MISSING cow device drops to an
  emergency shell, so the persistent boot entry must NEVER be the default.
  `kian-persist` creates the KIANPERSIST ext4 partition; persistent entries
  exist in syslinux + grub ("persistent session"); default entry passes
  `cow_spacesize=2G` (archiso default 256M is too small to create files).

## Honesty rules (project policy)
- Never fabricate build or test results; CI logs are the source of truth.
- Package names in `packages.x86_64` must be verified against
  https://archlinux.org/packages/ when touched.
- No fake icons (emoji), no fake commands, no simulated outputs.
- Legacy code is archived, not deleted; the two architectures must not be
  mixed in the build.
