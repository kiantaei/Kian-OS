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
airootfs_image_tool_options=('-comp' 'zstd' '-Xcompression-level' '15' '-b' '1M')
# Serial console is enabled so the ISO can be verified programmatically in CI
# (QEMU) and debugged headlessly; tty0 stays the primary console on screen.
kernel_params_x86_64="console=ttyS0,115200n8 console=tty0"
file_permissions=(
  ["/etc/sudoers.d/kian-wheel"]="0:0:440"
  ["/usr/local/bin/kian-setup"]="0:0:755"
  ["/usr/local/bin/kian-panel-position-fix"]="0:0:755"
  ["/usr/local/bin/kian-trust-desktop"]="0:0:755"
  ["/usr/local/bin/kian-install"]="0:0:755"
  ["/usr/local/bin/kian-apply"]="0:0:755"
  # Desktop launchers ship executable so xfdesktop never has to ask whether
  # they may run (the per-user GVFS trusted flag is added in-session by
  # /usr/local/bin/kian-trust-desktop).
  ["/etc/skel/Desktop/kian-files.desktop"]="0:0:755"
  ["/etc/skel/Desktop/kian-terminal.desktop"]="0:0:755"
  ["/etc/skel/Desktop/kian-xenox.desktop"]="0:0:755"
  ["/usr/bin/xenox"]="0:0:755"
  ["/usr/bin/neofetch"]="0:0:755"
)
