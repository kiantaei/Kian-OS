# Kian OS Roadmap

Version **0.4.0 "Horizon" (Linux edition)** — first Linux-based release.

## Convention
- `[DONE]` shipped in the current `main`
- `[PARTIAL]` shipped but with documented limitations
- `[PLANNED]` scheduled for a future milestone
- Each milestone ships a bootable `kian-os-<version>-x86_64.iso` (built by CI).

---

## Milestone L1 — Linux base migration (0.4.0)
- [DONE]   archiso profile (`archiso/kian/`) derived from the official releng profile
- [DONE]   Real Linux kernel + systemd + GNU userland; GRUB (UEFI) + syslinux (BIOS)
- [DONE]   Real pacman with keyring initialization at boot (`pacman-init.service`)
- [DONE]   Real Bash + bash-completion + GNU coreutils
- [DONE]   Real user account `kian` (uid 1000, wheel/sudo); root login disabled
- [DONE]   Asia/Tehran default timezone
- [DONE]   Real toolchain preinstalled: gcc, g++, make, git, python3, nano
- [DONE]   Automated QEMU boot test in CI (login, commands, screenshots)
- [DONE]   ISO published to GitHub Releases by CI

## Milestone L2 — Kian desktop (0.4.0)
- [DONE]   XFCE4 desktop with Kian branding (legacy gradient wallpaper, #0078D4 accent)
- [DONE]   Working taskbar: real window buttons, systray, clock/calendar flyout
- [DONE]   Whisker start menu with search, bound to the Super key
- [DONE]   LightDM greeter (Kian wallpaper, big clock); live autologin as kian
- [DONE]   App set: Xenox, Kian Terminal, File Manager, Notepad, Calculator,
           Settings, Task Manager — proper .desktop registration, SVG icons
- [DONE]   Kian Terminal look: #0C0C0C background, Kian prompt, DejaVu Sans Mono
- [DONE]   2 virtual desktops, Win+Arrow snapping, Win+D show desktop, Win+L lock
- [PARTIAL] Desktop theming depth — Materia-dark is a close match, not a pixel
           clone of the legacy C++ UI; the legacy acrylic-blur look is approximated
           with panel transparency

## Milestone L3 — Xenox browser (0.4.0)
- [DONE]   Real Chromium engine with a Kian profile (`/usr/bin/xenox`)
- [DONE]   Branded start page, default browser registration, Kian SVG icon
- [PARTIAL] Custom Xenox chrome (own UI on top of the engine) — future work

## Milestone L4 — Installation story
- [DONE]   archinstall included for real disk installation
- [DONE]   `kian-apply`: Kian identity, wallpaper, apps and panel defaults on an Arch base
- [PLANNED] Kian-flavored archinstall preset (kian user, Asia/Tehran) and a first-boot wizard
- [PLANNED] First-boot wizard to change the default `kian/kian` password
- [PLANNED] Persistence option for the live medium

## Milestone L5 — Nice-to-haves
- [PLANNED] Paint replacement (e.g. a GTK drawing app) — legacy Paint was archived
- [PLANNED] Plymouth boot splash with the Kian logo
- [PLANNED] Custom GTK theme derived from the exact legacy palette
- [PLANNED] xenox:// internal pages (start/bookmarks/history like the legacy UI)
- [PLANNED] Reproducible builds pinned to Arch snapshot mirrors
