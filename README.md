# Kian OS

**Kian OS** is a **Linux-based desktop operating system** (Arch Linux base),
version **0.4.0 "Horizon" (Linux edition)**.
Primary target: **VMware Workstation/Player/Fusion** (also QEMU and VirtualBox).

> **Migration notice (0.4.0).** Kian OS used to be a from-scratch hobby kernel
> (Rust kernel + custom C++ desktop). Per the 0.4.0 migration it is now a real
> Linux-based OS: real Linux kernel, real GRUB, real systemd, real pacman,
> real Bash and real applications — with the original Kian OS branding and
> desktop design preserved on top. The old standalone kernel code is archived
> (not deleted) in [`archive/legacy-standalone/`](archive/legacy-standalone/README.md).

## Identity

- OS name: **Kian OS** (`/etc/os-release`: `ID=kian`, `ID_LIKE=arch`)
- Kernel: **Linux** (real `linux` package) + **systemd** + GNU userland
- Boot loaders: **GRUB** (UEFI) + syslinux/ISOLINUX (legacy BIOS)
- Desktop: **XFCE4**, Kian-branded — the legacy dark gradient wallpaper
  (`#1A2B4A → #3B0A45`), `#0078D4` accent, bottom panel/taskbar, searchable
  start menu, big-clock login greeter
- Default user: `kian` / password: `kian` (live medium default — change after
  installing). Root login is **disabled**; privilege escalation is `sudo` only
  (kian is in `wheel`).
- Default time zone: **Asia/Tehran** (UTC+03:30; Iran abolished DST in 2022)
- Boot menu: ASCII/wordmark "KIAN OS" splash, 5-second timeout

## What is real in 0.4.0 (no fakes)

| Area | Reality |
|---|---|
| **Pacman** | Real pacman with a real package database. `pacman -Syu`, `pacman -S`, `-R`, `-Ss`, `-Qi`, `-Q`, `-Sc` all work (installing needs internet). Keyring is initialized at boot (`pacman-init.service`). |
| **Shell** | Real **Bash** + GNU coreutils: real pipes, redirection, TAB completion (`bash-completion`), `~/.bashrc` with the Kian prompt. |
| **Compiler** | Real **GCC/G++** (`base-devel`): compiles and runs C/C++ out of the box. |
| **Python** | Real **CPython 3**. |
| **Tools** | Real **git, make, nano, htop, tree, man**. |
| **neofetch** | `neofetch` was **removed from the Arch repositories** (upstream unmaintained). Kian OS ships **fastfetch** and keeps the familiar `neofetch` command working as a wrapper (`/usr/bin/neofetch`). |
| **Xenox Browser** | Real browser on the **Chromium engine** with a Kian profile (own user-data dir, branded start page, default web handler). The legacy C++ Xenox UI could not render real web pages and was archived honestly — see `archive/legacy-standalone/README.md`. |
| **Desktop** | Real XFCE4: taskbar with real window buttons (click = focus/minimize, right-click menu), Whisker start menu with search, systray, clock with calendar flyout, 2 virtual desktops. This **fixes the legacy taskbar/app-launch bugs** by moving to a battle-tested panel. |
| **Icons** | Real **SVG icons** (custom Kian set in `/usr/share/icons/hicolor/scalable/apps/`) + **Papirus-Dark** icon theme. **No emoji icons anywhere.** |
| **Apps** | Proper `.desktop` registration: Xenox Browser, Kian Terminal, File Manager (Thunar), Notepad (Mousepad), Calculator (galculator), Settings, Task Manager. |
| **Users** | Real accounts: `kian` (uid 1000, wheel/sudo) created by `kian-setup.service`; root locked; desktop never runs as root. |
| **Filesystem** | Real hierarchy: `/`, `/home/kian` with `Desktop/ Documents/ Downloads/ Music/ Pictures/ Videos/`. |
| **Installer** | Real **archinstall** is included — boot the ISO, run `sudo archinstall` to install Kian OS to disk. |

## Boot menu (GRUB/syslinux)

- **Kian OS 0.4.0 (Horizon)** — default entry, 5 s timeout
- Kian OS with hardware clock in UTC
- UEFI Firmware Settings / Boot existing OS / Reboot / Power off

## Build the ISO

The build uses [archiso](https://gitlab.archlinux.org/archlinux/archiso) with a
profile derived from the official `releng` profile:

```bash
# on an Arch Linux host (or in the archlinux:base-devel container)
sudo pacman -S archiso grub syslinux mtools dosfstools
./build.sh                  # or: make iso
# result: build/out/kian-os-0.4.0-x86_64.iso (+ .sha256)
./build.sh --check          # syntax sanity checks only
```

**On Windows** mkarchiso cannot run natively; `build.ps1` builds the same ISO
inside Docker Desktop (Linux containers) or an Arch Linux WSL distro and leaves
it in `build\out\`:

```powershell
.\build.ps1
```

**Continuous integration:** every push to `main` triggers
[`.github/workflows/build-iso.yml`](.github/workflows/build-iso.yml), which
builds the ISO inside an `archlinux:base-devel` container, runs a **real
QEMU boot test** (serial login as `kian`, gcc/g++/python/pacman/filesystem/
desktop-service checks, desktop screenshots) and publishes the ISO to GitHub
Releases (rolling `latest` release + per-tag releases). Build/test logs are
the real outputs — nothing is simulated.

### Automated QEMU boot test

`scripts/ci-boot-test.sh` boots the ISO with `-display none -serial mon:stdio`,
then:

1. waits for the `kian-os login:` prompt on the serial console,
2. logs in as `kian` / `kian` (real PAM login),
3. waits for LightDM + the XFCE session and captures **desktop screenshots**
   through the QEMU monitor,
4. runs `/usr/local/share/kian-ci/verify-commands.sh` inside the booted system
   (identity, sudo group, gcc, g++, python, filesystem writes, `pacman -Sy` +
   real package install over the network, lightdm/Xorg/xfce4-session status,
   Asia/Tehran timezone),
5. powers the VM off via `sudo systemctl poweroff`.

Each check prints `CHECK_PASS` / `CHECK_FAIL` / `CHECK_NET_FAIL` (network
failures are reported honestly, never simulated). Screenshots and the full
serial transcript are uploaded as CI artifacts.

### Verify in VMware (manual checklist)

The 11-point checklist from the migration requirements maps to CI + manual
steps; see [`docs/verification.md`](docs/verification.md). Quick manual pass:

1. Attach `kian-os-0.4.0-x86_64.iso` to a VM (BIOS or UEFI firmware both work)
   and boot it — GRUB (UEFI) or the syslinux menu (BIOS) appears.
2. The live session auto-logs-in as `kian` into the Kian-branded XFCE desktop
   (log out to see the LightDM greeter).
3. Use the taskbar/start menu, open **Kian Terminal** and try `pacman -Syu`,
   `gcc`, `python3`, `neofetch`, `sudo pacman -S tree`.
4. `sudo archinstall` installs Kian OS to a disk (choose the default
   "xorg-server + lightdm + xfce4" style profile packages or let archinstall
   pick its defaults).

## Repository layout

```
Kian-OS/
├── archiso/kian/                 # the operating system (archiso profile)
│   ├── profiledef.sh             # ISO identity, bootmodes, kernel params
│   ├── packages.x86_64           # package list (verified against Arch repos)
│   ├── pacman.conf               # build-time pacman config (from releng)
│   ├── grub/, syslinux/          # boot menus (Kian-branded)
│   └── airootfs/
│       ├── etc/                  # hostname, locale, sudoers, lightdm,
│       │                         # systemd units (kian-setup, pacman-init),
│       │                         # skel/ (XFCE theme, bashrc, .desktop)
│       └── usr/                  # xenox (Chromium profile), neofetch wrapper,
│                                 # kian-setup, verify-commands.sh, wallpaper,
│                                 # SVG icons, start page, os-release
├── scripts/ci-boot-test.sh       # real QEMU boot test (CI)
├── build.sh / Makefile           # local build entry points
├── kian-os.vmx                   # VMware VM config (points at the 0.4.0 ISO)
├── docs/verification.md          # 11-point checklist + evidence mapping
├── ROADMAP.md                    # Linux-era roadmap
└── archive/legacy-standalone/    # the pre-0.4.0 from-scratch kernel (frozen)
```

## Honest limitations (0.4.0)

- The **live session is volatile** (standard live-media behavior): files in
  `/home/kian` are lost on reboot unless you install Kian OS to disk.
- **Paint was not migrated** — there is no replacement in 0.4.0 yet.
- The legacy C++ desktop (code-drawn icons, mini-python, RAM filesystem) is
  archived; only its **design and branding** live on.
- Xenox is a Chromium profile, not a custom engine — the custom UI remains a
  future idea (see ROADMAP).
- ISO size is large (~2 GB) because a real browser, full toolchain and
  firmware are bundled.
- `neofetch` is fastfetch under the hood (neofetch no longer exists in Arch
  repos) — the command and the familiar look are preserved.
