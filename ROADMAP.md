# Kian OS Roadmap

Version 0.1.0 "Genesis"

## Convention
- `[DONE]` implemented in current source
- `[PLANNED]` scheduled for a future milestone
- Each milestone ships a bootable `kian-os.iso`.

---

## Milestone 1 — Boot + banner (target < 5s boot)
- [DONE]   Limine BIOS+UEFI boot config
- [DONE]   Rust `#![no_std]` kernel, custom `x86_64-kian` target
- [DONE]   Higher-half kernel, GDT, IDT, paging
- [DONE]   VGA text console + serial COM1 debug
- [DONE]   ASCII-art "KIAN OS" boot banner with version
- [DONE]   Kernel panic screen with debug info
- [PLANNED] Limine binary + ISO assembly (needs build toolchain on GitHub)

## Milestone 2 — Memory + scheduler + keyboard + shell
- [DONE]   Physical + virtual memory manager, heap allocator
- [DONE]   Preemptive multitasking, round-robin scheduler, timer tick (PIT)
- [DONE]   PS/2 keyboard driver (interrupt-driven)
- [DONE]   Syscall interface (write, exit, getpid, time, yield)
- [DONE]   Ring 3 userspace isolation (entry stub)
- [DONE]   ksh shell: built-ins (ls, cd, cat, cp, mv, rm, mkdir, ps, kill, mount, ifconfig/ip, ping, date, uname, clear, echo, edit, passwd, whoami, su, useradd, history)
- [DONE]   Pipes (`|`), redirection (`>`, `>>`, `<`), history
- [PLANNED] Tab-completion, full external command execution

## Milestone 3 — Login (kian/kian) + Asia/Tehran clock (MINIMUM VIABLE ISO)
- [DONE]   Init system (`/sbin init`) + login manager
- [DONE]   Default user `kian` / password `kian` (hashed, never plaintext)
- [DONE]   Root/admin account
- [DONE]   Commands: passwd, whoami, su, useradd
- [DONE]   Hardware RTC read + Asia/Tehran fixed +03:30 (no DST, abolished 2022)
- [DONE]   `date` tool shows local Asia/Tehran time
- [DONE]   Login screen on boot (text; graphical if GUI loads)
- [PLANNED] Prompt to change default password after first login
- [PLANNED] Graphical login screen fallback

## Milestone 4 — Filesystem + networking
- [DONE]   VFS layer, ramdisk/initramfs root
- [DONE]   KianFS native filesystem driver
- [DONE]   FAT32 driver (stub)
- [DONE]   ELF loader (stub)
- [PLANNED] AHCI/SATA, NVMe, LSI Logic/BusLogic SCSI storage drivers
- [PLANNED] TCP/IP stack: ARP, IP, ICMP, UDP, TCP, DHCP, DNS
- [PLANNED] Intel e1000/e1000e, vmxnet3 network drivers
- [PLANNED] `ifconfig`/`ip`, `ping` working over real NIC

## Milestone 5 — GUI desktop (Windows-like usability)
- [PLANNED] VESA/GOP framebuffer + VMware SVGA II driver
- [PLANNED] PS/2 mouse + VMware absolute-pointer (vmmouse)
- [PLANNED] Window manager, taskbar, start-menu launcher
- [PLANNED] File manager, terminal, settings app
- [PLANNED] Desktop clock showing Asia/Tehran time
- [PLANNED] Graphical settings: change time zone
- [PLANNED] Clean, modern, Windows-like UX

## Hardware support (VMware / QEMU / VirtualBox)
- [DONE]   APIC/IOAPIC, ACPI (stub), HPET, PIT, RTC
- [PLANNED] Console: VESA/GOP framebuffer, VMware SVGA II
- [PLANNED] Input: PS/2 keyboard+mouse, vmmouse absolute pointer
- [PLANNED] Storage: AHCI/SATA, NVMe, LSI Logic/BusLogic SCSI
- [PLANNED] Network: Intel e1000/e1000e, vmxnet3

## Security
- [DONE]   User/group permissions, root vs kian
- [DONE]   Password hashing: SHA-512 crypt / Argon2id (stub → real)
- [PLANNED] Password never stored plaintext
- [PLANNED] Ring 3 userspace isolation enforced

## Deliverables
- [PLANNED] `kian-os.iso` (hybrid BIOS+UEFI) + `kian-os.iso.sha256`
- [PLANNED] `kian-os.vmx` + VMware setup instructions
- [PLANNED] GitHub Actions workflow: build ISO + publish to Releases
- [PLANNED] QEMU headless boot log (serial output) published
