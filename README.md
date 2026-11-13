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
| **Installer** | A **welcome GUI opens automatically on the live desktop** (`kian-install-gui`, yad): one click launches the installer in a terminal window. **kian-install** (Kian's own installer, no archinstall) asks only **mirror region** + **timezone**, auto-detects the target disk, pacstraps the full desktop, sets up GRUB + the `kian` user, then applies the Kian branding. For menu control, **kianinstall** — Kian's own archinstall fork ([github.com/kiantaei/kianinstall](https://github.com/kiantaei/kianinstall), vendored at `/opt/kianinstall`) — and the GUI **auto-runs `kian-apply`** after it, so an installed system is always full Kian OS, never plain Arch. |

## Boot menu (GRUB/syslinux)

- **Kian OS 0.4.0 (Horizon)** — default entry, 1 s timeout (arrow keys keep the menu open)
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
4. **Installing Kian OS** — when the live desktop starts, the
   **installer welcome window opens automatically** (`kian-install-gui`,
   also in the menu as *Kian OS Installer*) with three buttons:
   * **Install Kian OS** — opens the kianinstall fork of archinstall
     ([github.com/kiantaei/kianinstall](https://github.com/kiantaei/kianinstall),
     pinned commit, vendored at `/opt/kianinstall`) in a terminal window for
     menu-driven control (partitioning, encryption, etc.). When it finishes,
     `kian-apply` runs **automatically** on the mounted target — the installed
     system is **Kian OS** (wallpaper, xenox, Kian apps, panel defaults,
     os-release), never plain Arch.
   * **Simple installer** — `sudo kian-install`, Kian OS's own installer: it
     asks exactly **two questions** (which **mirror region** to download
     packages from — it regenerates `/etc/pacman.d/mirrorlist` for that
     region from the official status API; and the **timezone**). Everything
     else is automatic: the target disk is detected (empty unpartitioned
     disks first, then the largest disk that is not the boot medium; there is
     a loud 10-second erase countdown before anything happens — Ctrl+C
     aborts), the disk is partitioned (GPT + ESP on UEFI, MBR on BIOS), the
     full Kian OS desktop is installed with `pacstrap`, GRUB is set up, the
     `kian` user is created (password `kian`), and `kian-apply` brands the
     new system. Reboot with the ISO detached.
   * **Try Kian OS** — close the window and keep using the live session.
   The welcome window exists only on the live ISO; an installed Kian OS
   never re-opens it.

### Real persistence in the live session ("not like live")

A plain live session throws everything away at shutdown — the overlay lives
in RAM. Kian OS can make the live session **really save**:

1. Attach a disk (a new VMware/QEMU virtual disk is perfect) and run
   `sudo kian-persist` — it auto-picks an empty disk (or pass one:
   `sudo kian-persist /dev/sdX`), warns loudly for 8 seconds, then creates an
   ext4 partition labelled `KIANPERSIST`.
2. Reboot and choose **"Kian OS 0.4.0 — persistent session"** in the boot
   menu (present for both BIOS/syslinux and UEFI/GRUB; the plain entry stays
   non-persistent so the ISO always boots everywhere).
3. In that session every file you create in `/home/kian`, every `/etc`
   change and every `pacman` install is written to the KIANPERSIST partition
   (archiso's cow device) and comes back on every reboot — not RAM, not
   tmpfs: disk. The default session also gets a 2 GiB overlay instead of
   archiso's 256 MiB, so creating files works even without persistence.

### Installed system

The `kian-install` installer puts a real Arch Linux base on disk from the
mirrors of the region you chose; the Kian overlay lives only on the live ISO.
It therefore runs `kian-apply` on the new root right after `pacstrap`, which copies:

- `/usr/lib/os-release` (Kian identity) plus the hook that keeps it through
  future `filesystem` upgrades,
- `/usr/share/kian` and `/usr/share/backgrounds/kian` (start page, wallpaper),
- `xenox`, the `neofetch` wrapper, Kian `.desktop` entries and the panel/trust
  helpers, with the XFCE autostart entries,
- `/etc/skel` defaults, seeded into existing users without overwriting their
  files, plus the Kian wallpaper in each user's XFCE desktop channel,
- `GRUB_DISTRIBUTOR="Kian OS"` and a regenerated GRUB menu.

You can also run it at any time from the live ISO:

```bash
lsblk -f
sudo kian-apply              # detects the install and asks
sudo kian-apply /dev/sda2    # or name the root partition
sudo kian-apply /mnt         # or an already-mounted root
sudo kian-apply --no-gui     # branding only, never install packages
```

It understands both archinstall layouts: plain `ext4` and the default
**"btrfs with subvolumes"** layout — the root lives in the `@` subvolume there,
and `kian-apply` mounts that automatically (a plain mount of the btrfs top
level shows no `/etc`, which older versions rejected as "not an Arch install").

If the target has no desktop (archinstall *minimal* profile), `kian-apply`
offers to install the full **Kian OS desktop** — Xfce, LightDM, Chromium and
the Kian app set, the same package list the live ISO ships — into the target
with pacman, and enables `lightdm` + `NetworkManager`, so the installed system
really is Kian OS with a GUI, not plain Arch. Pass `--no-gui` to skip that.

The result is Kian OS **on an Arch base**: Arch packages and updates still come
from the official repositories, while identity, branding and Kian's own files
come from the ISO.

### Installer troubleshooting

The Textual-based archinstall (4.x) can crash with
`ScreenStackError: Can't pop screen; there must be at least one screen on the
stack`. This is an archinstall menu bug: a key press is delivered to a menu
screen that has already been dismissed — typically a double Enter, or a held
Enter while the Install entry's preview is still running its configuration
sync — so the second `dismiss()` finds only the default screen. It happens
before anything is written to disk.

What to do:

- run `sudo kian-install`, which refreshes archinstall to the current
  repository version before starting it (Arch's remedy for a misbehaving
  installer on a live ISO),
- press Enter **once** per menu and let the preview pane settle before the next
  key, and
- if it still crashes, re-run `sudo kian-install`; the live session and the
  target disk are untouched.

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
