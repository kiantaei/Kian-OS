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

## Complete installation guide

From "I have the ISO" to "Kian OS runs from disk". Building or downloading the
image is step 0 — see [Build the ISO](#build-the-iso).

### 0. What you need

- `kian-os-0.4.0-x86_64.iso` (built locally or taken from a release) and its
  `.sha256` file.
- **2 GB RAM minimum, 4 GB recommended** (Chromium + the installer).
- **20 GB** of free disk space for the installation.
- **Internet inside the live session** — the packages come from the Arch
  mirrors; without a connection the installer cannot pacstrap.
- A disk (virtual or physical) you are willing to **erase completely**.

### 1. Boot the ISO

**VMware Workstation/Player/Fusion (primary target)**

1. *Create a New Virtual Machine* → *Installer disc image file (iso)* → pick the ISO.
2. Guest OS: *Linux → Other Linux 5.x and later kernel 64-bit*.
3. 2–4 GB RAM, 2 CPUs, 20 GB+ disk, network **NAT**.
4. Start the VM. If the display stays black on a host without 3D support, turn
   *Accelerate 3D graphics* off in the VM settings.

**QEMU**

```bash
qemu-system-x86_64 -enable-kvm -m 4096 -smp 2 \
  -drive file=kian-os.qcow2,if=virtio,format=qcow2 \
  -cdrom kian-os-0.4.0-x86_64.iso -boot d
```

**Real PC**

Write the ISO to a USB stick (`sudo dd if=kian-os-0.4.0-x86_64.iso of=/dev/sdX
bs=4M status=progress oflag=sync` — check `/dev/sdX` with `lsblk`), boot from it,
and disable Secure Boot if the firmware refuses the unsigned bootloader.

Pick **Kian OS 0.4.0 (Horizon)** in the GRUB (UEFI) or syslinux (BIOS) menu.

### 2. The live desktop

The live session auto-logs-in as **`kian` / `kian`** into the Kian XFCE desktop.
Check the network before installing:

```bash
ping -c1 archlinux.org
ip -brief address
```

Everything in the live session lives in RAM: files, settings and packages are
gone after a reboot unless you install to disk (or use `sudo kian-persist` for a
persistent live session).

### 3. Install — pick one path

**A. Simple installer (recommended).** The welcome window opens by itself; click
*Simple installer*, or open a terminal and run:

```bash
sudo kian-install
```

It asks exactly **two** questions — mirror region and timezone — then detects the
target disk, shows a **10-second erase countdown** (Ctrl+C aborts), creates
GPT+ESP (UEFI) or MBR (BIOS) partitions, pacstraps the full Kian desktop,
installs GRUB, creates the `kian` user (password `kian`) and applies the Kian
branding. Override the disk with `sudo kian-install --disk /dev/sdX`. The log is
`/tmp/kian-install.log`.

**B. Full control (archinstall fork).** Welcome window → *Install Kian OS*, or:

```bash
sudo kianinstall
```

Configure **Disk configuration** first, then Profile and Authentication. When it
finishes choose **Exit** (not Reboot) at the post-install screen — that is when
`kian-apply` brands the new system. If you rebooted by mistake, boot the ISO and
run `sudo kian-apply`.

**C. Your own Arch install.** Already have Arch on disk? Boot the ISO and run
`sudo kian-apply /dev/sdXN` (see [Installed system](#installed-system)).

### 4. Finish and reboot

1. Shut down the machine.
2. Detach the ISO (VMware: *VM → Settings → CD/DVD → disconnect*; real PC:
   remove the USB stick).
3. Power on.

### 5. First boot and login

GRUB shows the **Kian OS** entry; LightDM shows the Kian greeter. Log in with
`kian` / `kian` (simple installer) or the account you created in the
archinstall menu.

### 6. First things to do on the installed system

```bash
sudo pacman -Syu          # update to the current Arch packages
passwd                    # change the default kian/kian password
sudo systemctl status NetworkManager
proton-exe "test.exe"     # Windows programs via Proton (see below)
```

Your files now live on the disk and survive reboots. If you installed a *minimal*
profile, there is no desktop: boot the ISO and run `sudo kian-apply /dev/sdXN`,
which offers to install the full Kian desktop into that system.

### 7. If something goes wrong

| Symptom | What to do |
|---|---|
| Black screen in a VM right after the boot menu | Disable 3D acceleration, or add `nomodeset` to the kernel command line in the boot menu |
| `ScreenStackError: Can't pop screen` | archinstall TUI bug — see [Installer troubleshooting](#installer-troubleshooting); the text installer `sudo kian-install` is unaffected |
| Installer window closes instantly | Start it from the desktop menu (**Kian OS Installer**) instead of a root shell |
| No internet in the live session | Check the VM network is NAT/bridged; `nmcli device status`, `ping -c1 archlinux.org` |
| Installation stops at `pacstrap` | No network, or a mirror region that is unreachable — re-run `sudo kian-install` and pick another region |
| Installed system lands on a text login | Log in and run `sudo systemctl enable --now lightdm`, then `startxfce4` |
| Wrong system booted after install | Reboot and pick *Kian OS* in GRUB; check the boot order in the firmware/VM settings |

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

### Run Windows .exe programs (Proton)

Kian OS ships **`proton-exe`**, which runs Windows programs through Proton
without the Steam client:

```bash
proton-exe "test.exe"                     # run a program
proton-exe --proton ~/GE-Proton10-4 app.exe   # specific Proton build
proton-exe --prefix ~/.wine game.exe      # specific Wine prefix
```

It uses, in order: `$PROTON_PATH`/`--proton`, Valve's **"Proton - Experimental"**
from an installed Steam, then **UMU-Proton** — the Proton-Experimental-based
build that `umu-launcher` downloads on first use. `umu-launcher` ships in the
image (multilib), so no Steam account is needed; the first run downloads
Proton plus the Steam Linux Runtime (once, into `~/.local/share/Steam/`).

Double-clicking a `.exe` works the same way: `proton-exe.desktop` is the
registered default handler for the Windows executable MIME types
(`/etc/xdg/mimeapps.list`), so Thunar, xfdesktop and any XDG file manager open
it with Proton instead of asking for an application.

Prefixes are kept per Proton build under `~/.local/share/kian-proton/`, so
switching builds never corrupts an existing prefix. If `umu-run` is missing
but a Proton directory exists, the command falls back to Proton's own
runner; with neither available it prints exactly what to install.

### First boot of the installed system

1. Power off the VM/machine, detach the ISO (VMware: *VM → Settings → CD/DVD →
   Use physical drive / disconnect*), then power on.
2. GRUB shows the **Kian OS** entry (the brand name comes from
   `GRUB_DISTRIBUTOR`), then LightDM's greeter with the Kian wallpaper.
3. Log in as `kian` with password `kian` (the simple installer), or with the
   user/password you created in the kianinstall (archinstall) menu.
4. If the machine stops at a text login instead of the greeter, log in and run
   `sudo systemctl enable --now lightdm`, or start the desktop directly with
   `startxfce4` (a *minimal* install has no desktop — install Xfce first).

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

**The welcome window closes instantly when started from a root shell.**
`kian-install-gui` needs the live desktop's `DISPLAY` and its X cookie, which a
root shell does not have, so `yad` exits at once. The script now re-runs itself
as the desktop user (with that session's real `DISPLAY`/`XAUTHORITY`) and, if
the window still cannot open, prints yad's actual error instead of claiming the
window was closed. On an older ISO the workaround is to start it from the
desktop menu (**Kian OS Installer**) as `kian`, or to use the text installer
`sudo kian-install`.

## Credits & components

Kian OS is glue, branding and a lot of configuration on top of other people's
work. Everything below is used unmodified (or configured, not forked) unless
stated otherwise; all trademarks belong to their owners, and this project is
not affiliated with Arch Linux, Valve, XFCE or any other upstream.

**Operating system and packaging**

- **Arch Linux** — the base distribution: kernel, systemd, GNU userland, pacman,
  mirrors. <https://archlinux.org>
- **archiso** (`mkinitcpio-archiso`) — builds the live ISO, its squashfs image and
  the persistence (`cow_label`) support.
- **Linux kernel**, **systemd**, **GNU coreutils / Bash**, **pacman**, **GRUB**
  and **syslinux/ISOLINUX** — boot, init, shell, package management.
- **archinstall** — the upstream installer; Kian OS also ships **kianinstall**,
  Kian's fork of it, vendored at `/opt/kianinstall`
  (<https://github.com/kiantaei/kianinstall>), plus Kian's own two-question
  `kian-install`.
- **parted**, **dosfstools**, **e2fsprogs**, **arch-install-scripts** — the
  partitioning / `pacstrap` / `arch-chroot` tooling the installer drives.

**Desktop**

- **Xfce 4** — `xfwm4`, `xfce4-panel`, `xfdesktop`, `xfce4-session`,
  `xfce4-settings`, `xfconf`, `Thunar`, `Mousepad`, `xfce4-terminal`,
  `xfce4-taskmanager`, `galculator` (Calculator), **Whisker Menu** plugin.
  <https://xfce.org>
- **LightDM** + **lightdm-gtk-greeter** — the login screen.
- **GTK**, **Materia** GTK theme, **Papirus** icon theme, **Liberation / DejaVu /
  Noto** fonts.
- **yad** — the installer welcome / status dialogs.

**Applications and tools**

- **Chromium** — the engine behind **Xenox**, Kian's browser profile (own
  user-data dir and start page); <https://www.chromium.org>
- **fastfetch** — provides the `neofetch` command (neofetch is no longer in the
  Arch repositories).
- **git**, **GCC / base-devel**, **Python**, **nano**, **htop**, **tree**,
  **man-db**, **bash-completion**.
- **NetworkManager** + applet, **polkit-gnome**, **GVFS / tumbler /
  thunar-volman**, **xdg-utils / xdg-user-dirs**.
- **open-vm-tools**, **qemu-guest-agent** — guest integration for VMware/QEMU.

**Proton (Windows programs)**

- **Valve Steam** and **Proton**, including **Proton Experimental** — Valve's
  Wine-based compatibility layer for Windows games and applications.
  <https://github.com/ValveSoftware/Proton>
- **umu-launcher** (Open Wine Components) — runs Proton outside the Steam
  client, inside the Steam Linux Runtime; this is what `proton-exe` and the
  double-click `.exe` handler use. <https://github.com/Open-Wine-Components/umu-launcher>
- **Wine**, **DXVK**, **VKD3D-Proton**, **Steam Linux Runtime** — used indirectly
  through Proton (bundled in the Proton builds, not modified by Kian OS).
- **UMU-Proton** — the Proton-Experimental-based build `umu-launcher` downloads
  on first use.

**Build, test and release**

- **Docker / WSL** for building the ISO on Windows (`build.ps1`), **QEMU** for
  the automated boot test, **VMware Workstation** as the primary target VM.
- **GitHub Actions** + `softprops/action-gh-release` for CI builds and releases.

**Design**

- The Kian OS wordmark, wallpaper gradient (`#1A2B4A → #3B0A45`), `#0078D4`
  accent and the Kian SVG icon set are original to this project (see
  `archiso/kian/airootfs/usr/share/icons/`).

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
