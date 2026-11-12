# Kian OS

**Kian OS** is a **Linux-based desktop operating system** on an **Ubuntu 24.04
LTS (noble) base**, version **1.0 "Horizon"**.
Primary target: **VMware Workstation/Player/Fusion** (also QEMU and VirtualBox).

> **Rebase notice (1.0).** Kian OS 0.4.0 was Arch-based (pacman, archiso,
> archinstall). Per the 1.0 rebase it is **Ubuntu-based end to end**: real
> `apt`/`dpkg` on a noble base, live-build, live-boot + live-config, the
> **Calamares** graphical installer — no pacman, no archinstall, no Arch
> leftovers. The whole 0.4.0 Arch profile is archived (not deleted) in
> [`archive/archiso-arch-base/`](archive/archiso-arch-base/airootfs/), and the
> pre-Linux from-scratch kernel in
> [`archive/legacy-standalone/`](archive/legacy-standalone/README.md).

## Identity

- OS name: **Kian OS** (`/etc/os-release`: `ID=kian`, `ID_LIKE=debian ubuntu`,
  `UBUNTU_CODENAME=noble`)
- Base: **Ubuntu 24.04 LTS (noble)** — real `apt`, `dpkg`, Ubuntu kernel
  (6.8 generic), systemd, GNU userland
- Boot: live-build iso-hybrid — **GRUB (UEFI) + ISOLINUX/syslinux (BIOS)**
- Desktop: **XFCE4**, Kian-branded — the dark gradient wallpaper
  (`#1A2B4A → #3B0A45`), `#0078D4` accent, bottom panel, searchable start
  menu (Whisker), big-clock login greeter
- Live user: `kian` / password: `live` (live-config default, like Ubuntu's own
  live images; passwordless live sudo, removed from the installed system).
  Installed system: `kian` / `kian` — change after first login.
- Default time zone: **Asia/Tehran** (UTC+03:30; Iran abolished DST in 2022)
- The **Welcome window opens automatically** on the live desktop with the
  install, persistence and terminal buttons (live session only — an installed
  system never shows it).

## What is real in 1.0 (no fakes)

| Area | Reality |
|---|---|
| **apt / dpkg** | Real Ubuntu archive: `sudo apt update`, `apt install`, `full-upgrade` all work over the regional mirror you choose. |
| **Shell** | Real **Bash** + GNU coreutils, `bash-completion`, the Kian prompt. |
| **Compiler** | Real **GCC** (build-essential via live-build chroot deps; `gcc`, `python3` verified in CI). |
| **Python** | Real **CPython 3** (noble = 3.12). |
| **neofetch** | Real `neofetch` (still in noble universe — no wrapper needed). |
| **Xenox Browser** | Real browser on the **Falkon engine** (`/usr/bin/xenox` opens Falkon with the Kian start page). Chromium/Firefox are snap shims on noble and cannot live in a live-build image — falkon is a real deb. |
| **Desktop** | Real XFCE4: taskbar with real window buttons, Whisker start menu, systray, clock flyout, 2 virtual desktops; compositing **off** by default (VMware-safe). |
| **Icons** | Real **SVG icons** (custom Kian set) + **Papirus-Dark**; Materia dark GTK theme. **No emoji icons anywhere.** |
| **Apps** | Kian Terminal (xfce4-terminal), Files (Thunar), Notepad (Mousepad), Calculator (galculator), Settings, Task Manager — all `.desktop`-registered. |
| **Installer** | **Calamares** (graphical, like normal installers: language/region, keyboard, partitioning, users) **plus** `kian-install` (terminal, asks exactly **mirror region + timezone**). Both work **fully offline** — the install copies the complete live root (rsync/squashfs unpack), then writes the regional mirror (`<cc>.archive.ubuntu.com`) for future updates. `automirror` (vendored from calamares-settings-ubuntu, GPL-3+) adds geoip mirror selection to the graphical installer. |
| **Persistence** | `kian-persist` creates a `KIANPERSIST` store; the `persistence persistence-label=KIANPERSIST` boot parameter (verified in live-boot 20230131) then really saves the whole live session to disk. |
| **Welcome GUI** | `kian-welcome` (GTK3/PyGObject) — the menu that opens after the live desktop starts: Install / persistence / terminal. |

## Build the ISO

The build uses [live-build](https://salsa.debian.org/live-team/live-build) with
the profile in `live/`, on a noble host or the CI runner (same release — no
version skew):

```bash
# on Ubuntu 24.04 (or any noble container/runner)
sudo apt-get install -y live-build debootstrap squashfs-tools xorriso \
    grub-pc-bin grub-efi-amd64-bin syslinux-common isolinux mtools
sudo ./build.sh             # or: make iso
# result: live/kian-os-1.0.0-amd64.iso (+ .sha256)
./build.sh --check          # sanity checks only (no root needed)
```

**Continuous integration:** every push to `main` triggers
[`.github/workflows/build-iso.yml`](.github/workflows/build-iso.yml), which
installs live-build on the ubuntu-latest runner, builds the ISO, runs a **real
QEMU boot test** (serial login as `kian`/`live`, apt update over the network,
Calamares/persistence/tool checks, desktop screenshots) and publishes the ISO
to GitHub Releases (rolling `latest` + per-tag releases). Build/test logs are
the real outputs — nothing is simulated.

### Automated QEMU boot test

`scripts/ci-boot-test.sh` boots the ISO with `-display none -serial mon:stdio`,
then:

1. waits for the `kian-os login:` prompt on the serial console,
2. logs in as `kian` / `live` (real PAM login),
3. waits for LightDM + the XFCE session and captures **desktop screenshots**
   through the QEMU monitor,
4. runs `/usr/local/share/kian-ci/verify-commands.sh` inside the booted system
   (Kian-on-Ubuntu identity, apt update, **no pacman**, Calamares + automirror,
   persistence boot parameter, kian tools, LightDM/XFCE status, Asia/Tehran
   timezone),
5. powers the VM off via `sudo systemctl poweroff`.

Each check prints `CHECK_OK` / `CHECK_FAIL` (failures are honest, never
simulated). Screenshots and the serial transcript are uploaded as CI artifacts.

### Verify in VMware (manual checklist)

See [`docs/verification.md`](docs/verification.md). Quick manual pass:

1. Attach `kian-os-1.0.0-amd64.iso` to a VM (BIOS or UEFI both work) and boot
   it — GRUB (UEFI) or the syslinux menu (BIOS) appears.
2. The live session auto-logs-in as `kian` into the Kian-branded XFCE desktop
   and the **Welcome window opens**.
3. Click **Install Kian OS (graphical installer)** — Calamares runs (language,
   region, keyboard, partitioning, user creation, like a normal installer).
   Or open the terminal button and run `sudo kian-install`: it asks exactly
   **two questions** — the **mirror region** for future apt updates
   (ir/de/tr/fr/uk/jp `.archive.ubuntu.com`, verified reachable, plus
   geoip auto-detect) and the **timezone** — then picks the target disk
   automatically (empty unpartitioned disks first, then the largest disk that
   is not the boot medium; loud 10-second erase countdown; `--disk /dev/sdX`
   to force), partitions (GPT + ESP on UEFI, MBR on BIOS), copies the whole
   live root, writes fstab, regenerates machine-id + initramfs, installs GRUB,
   and cleans the live-only bits.
4. After installing: reboot with the ISO detached; login `kian` / `kian`.

### Real persistence in the live session ("not like live")

A plain live session throws everything away at shutdown — the overlay lives
in RAM. Kian OS can make the live session **really save** (live-boot's
persistence, verified against live-boot 20230131 sources):

1. From the Welcome window click **Real persistence**, or run
   `sudo kian-persist` — it auto-picks an empty disk (or pass one:
   `sudo kian-persist /dev/sdX`), warns loudly for 8 seconds, then creates an
   ext4 partition labelled `KIANPERSIST` with `/persistence.conf` containing
   `/ union`.
2. Reboot — nothing else to choose. The `persistence persistence-label=
   KIANPERSIST` parameter is already on **every** boot entry; when the store
   exists, live-boot mounts it as the overlay (full-root: `/home/kian`, `/etc`,
   `apt` installs) and everything really survives reboots. When no store
   exists, live-boot falls back to a RAM overlay and boots normally — that is
   why persistence can safely be the default.
3. Without persistence the session still gets a 4 GiB RAM overlay
   (`overlay-size=4G`) instead of the 50% default, so creating files works.

### Installed system

Both installers put the **same Kian OS root** on disk (it is copied from the
live medium — the Kian branding, xenox, icons, wallpaper and panel defaults
are already inside it), then:

- the machine-id and initramfs are regenerated for the real hardware,
- GRUB is installed with `GRUB_DISTRIBUTOR="Kian OS"`,
- `/etc/apt/sources.list.d/kian-ubuntu.sources` points at the regional mirror
  you chose (`https://<cc>.archive.ubuntu.com/ubuntu` for updates — the live
  copy install itself needs no network),
- the live-only bits are removed (`/etc/sudoers.d/kian-live` passwordless
  sudo + the welcome autostart) via `kian-cleanup-live-only`.

## Repository layout

```
Kian-OS/
├── live/                         # the operating system (live-build profile)
│   ├── auto/config               # lb config: noble, XFCE, calamares, boot params
│   └── config/
│       ├── package-lists/        # kian.list.chroot (every name index-verified)
│       ├── includes.chroot/      # Kian overlay: os-release, motd, skel, icons,
│       │                         # wallpaper, xenox (falkon), kian-welcome GUI,
│       │                         # kian-install, kian-persist, Calamares config
│       │                         # + branding, vendored automirror module
│       └── hooks/                # chroot hook (lightdm, grub-pc preseed, checks)
├── scripts/ci-boot-test.sh       # real QEMU boot test (CI)
├── build.sh / Makefile           # local build entry points (live-build)
├── kian-os.vmx                   # VMware VM config (points at the 1.0 ISO)
├── docs/verification.md          # checklist + evidence mapping
├── ROADMAP.md                    # roadmap
└── archive/
    ├── archiso-arch-base/        # the 0.4.0 Arch profile (frozen, not built)
    └── legacy-standalone/        # the pre-0.4.0 from-scratch kernel (frozen)
```

## Honest limitations (1.0)

- The live session password is `live` (Ubuntu live convention); the installed
  system is `kian`/`kian`. Both should be changed on real hardware.
- Chromium/Firefox are snap shims on noble and are NOT shipped; xenox uses
  Falkon. Snaps are not used anywhere in Kian OS.
- Paint is still not migrated.
- ISO size is large (~2-3 GB) because a real browser, the Calamares installer
  stack and full firmware are bundled.
