#!/usr/bin/env bash
# shellcheck disable=SC2034

# Kian OS 0.4.0 "Horizon" — Linux-based desktop operating system.
# Built with archiso (https://gitlab.archlinux.org/archlinux/archiso).
# The profile is derived from the official archiso "releng" profile.

iso_name="kian-os"
iso_label="KIAN_OS_040"
iso_publisher="Kian OS <https://github.com/kiantaei/Kian-OS>"
iso_application="Kian OS Live/Desktop Medium (Linux-based)"
iso_version="0.4.0"
install_dir="kian"
bootmodes=('bios.syslinux' 'uefi.grub')
pacman_conf="pacman.conf"
airootfs_image_type="squashfs"
# zstd instead of xz: the live system decompresses the squashfs image on
# every boot, and zstd is several times faster to decompress than
# xz -9e while staying a similar size. This is the single biggest boot-time
# win on the live medium.
# Level 19 instead of 15: GitHub Releases rejects release assets above the
# hard 2 GiB limit (2147483648 bytes) and the ISO reached 2.414 GB after the
# Proton/multilib additions. zstd supports ONLY -Xcompression-level here
# (-Xdict-size is an xz option — run 43496884928 failed on it); the size
# saving comes mainly from dropping linux-firmware from the live ISO (see
# packages.x86_64), level 19 adds a few percent of margin.
airootfs_image_tool_options=('-comp' 'zstd' '-Xcompression-level' '19' '-b' '1M')
# Serial console is enabled so the ISO can be verified programmatically in CI
# (QEMU) and debugged headlessly; tty0 stays the primary console on screen.
# cow_spacesize=2G: the default (non-persistent) session writes to a RAM
# overlay that archiso caps at 256M by default — too small to actually create
# files. 2G grows on demand and is safe with 4 GB VMs. Real persistence is a
# separate boot entry (cow_label=KIANPERSIST, see syslinux/grub + kian-persist).
kernel_params_x86_64="console=ttyS0,115200n8 console=tty0 cow_spacesize=2G"
file_permissions=(
  ["/etc/sudoers.d/kian-wheel"]="0:0:440"
  ["/usr/local/bin/kian-setup"]="0:0:755"
  ["/usr/local/bin/kian-panel-position-fix"]="0:0:755"
  ["/usr/local/bin/kian-trust-desktop"]="0:0:755"
  ["/usr/local/bin/kian-install"]="0:0:755"
  ["/usr/local/bin/kian-persist"]="0:0:755"
  ["/usr/local/bin/kianinstall"]="0:0:755"
  ["/usr/local/bin/kian-apply"]="0:0:755"
  ["/usr/local/bin/kian-throttled-fetch"]="0:0:755"
  ["/usr/local/bin/proton-exe"]="0:0:755"
  # mkarchiso copies airootfs with --no-preserve=mode — every executable MUST
  # be listed here or it ships as 644 (found by the boot test: the installer
  # GUI script was present but not executable).
  ["/usr/local/bin/kian-install-gui"]="0:0:755"
  # Desktop launchers ship executable so xfdesktop never has to ask whether
  # they may run (the per-user GVFS trusted flag is added in-session by
  # /usr/local/bin/kian-trust-desktop).
  ["/etc/skel/Desktop/kian-files.desktop"]="0:0:755"
  ["/etc/skel/Desktop/kian-terminal.desktop"]="0:0:755"
  ["/etc/skel/Desktop/kian-xenox.desktop"]="0:0:755"
  ["/usr/bin/xenox"]="0:0:755"
  ["/usr/bin/neofetch"]="0:0:755"
)
