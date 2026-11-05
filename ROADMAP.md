# Kian OS Roadmap

Version 0.2.0 "Horizon"

## Convention
- `[DONE]` implemented in current source
- `[PARTIAL]` code written, needs build+test on Linux
- `[PLANNED]` scheduled for a future milestone
- Each milestone ships a bootable `kian-os-0.2.0.iso`.

---

## Milestone 1 — Boot + banner (target < 5s boot)
- [DONE]   Limine BIOS+UEFI boot config
- [DONE]   Rust `#![no_std]` kernel, built-in `x86_64-unknown-none` target
- [DONE]   Higher-half kernel at 0xffffffff80000000, GDT, IDT, paging
- [DONE]   VGA text console + serial COM1 debug
- [DONE]   ASCII-art "KIAN OS" boot banner with version v0.2.0 "Horizon"
- [DONE]   Kernel panic screen with debug info
- [PARTIAL] Limine binary + ISO assembly (`make iso`)

## Milestone 2 — Memory + scheduler + keyboard + shell
- [DONE]   Physical + virtual memory manager, heap allocator
- [DONE]   Preemptive multitasking, round-robin scheduler, timer tick (PIT)
- [DONE]   PS/2 keyboard driver (interrupt-driven)
- [DONE]   PS/2 mouse driver (streaming mode)
- [DONE]   Syscall interface (write, exit, getpid, time, yield, read, open, close, stat, exec)
- [DONE]   Ring 3 userspace isolation (entry stub)
- [DONE]   ksh shell: built-ins (ls, cd, cat, cp, mv, rm, mkdir, ps, kill, mount, ifconfig/ip, ping, date, uname, clear, echo, edit, passwd, whoami, su, useradd, history)
- [DONE]   Pipes (`|`), redirection (`>`, `>>`, `<`), history
- [PARTIAL] BlueShell `bsh` (PowerShell-style, Rust): cmdlets, object pipeline, language, REPL
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

## Milestone 4 — Networking (VMware NAT out of the box)
- [PARTIAL] Ethernet driver framework: e1000, e1000e, vmxnet3, rtl8139/rtl8169, virtio-net
- [PARTIAL] ARP, IPv4, ICMP, UDP, TCP (basic handshake), DHCP client, DNS resolver with cache
- [PARTIAL] IPv6 stub
- [PARTIAL] Tools: `ip`, `ifconfig`, `ping`, `traceroute`, `nslookup`, `fetch`, `netstat`
- [PLANNED] TLS 1.2/1.3 (BearSSL/mbedTLS), root CA store, HTTP/1.1 + HTTPS client
- [PLANNED] NTP client setting Asia/Tehran clock
- [PLANNED] Network settings app in GUI with tray status icon
- [PLANNED] Auto-connect via DHCP on boot

## Milestone 5 — GUI desktop (Windows-like usability)
- [PARTIAL] Compositing window manager, double-buffered rendering, SIMD blits
- [PARTIAL] VMware SVGA II driver with dynamic resolution change, GOP/VESA fallback
- [PARTIAL] PS/2 mouse + VMware absolute-pointer (vmmouse), scroll wheel, drag, double-click
- [PARTIAL] Keyboard: US English + Persian (Farsi), Alt+Shift layout switch, key repeat, LEDs
- [PARTIAL] Clipboard shared across apps
- [PLANNED] Anti-aliased TrueType fonts (FreeType/stb_truetype) with Persian/Arabic RTL shaping
- [PLANNED] Rounded corners, soft shadows, transparency/blur, smooth animations, dark/light themes
- [PLANNED] Wallpaper, icon set, taskbar, start-menu launcher with search
- [PLANNED] System tray (network, volume, language, clock Asia/Tehran), notifications, lock screen
- [PLANNED] Alt+Tab, snap-to-edge, virtual desktops
- [PLANNED] Apps: File Manager, Settings, Text Editor, Calculator, Task Manager, Image Viewer, Terminal, Kian Browser, BlueShell GUI

## Milestone 6 — Kian Browser + TLS + Cache
- [PLANNED] Browser engine (port NetSurf or write HTML5/CSS renderer)
- [PLANNED] QuickJS/Duktape JavaScript engine with basic DOM
- [PLANNED] HTTPS with certificate validation, bundled root CA store
- [PLANNED] Tabs, bookmarks, history, downloads, cache in ~/.cache/kian-browser
- [PLANNED] PNG/JPEG/GIF/WebP image support

## .cache system (system-wide)
- [PARTIAL] Kernel page cache + block cache (write-back, LRU eviction)
- [PARTIAL] Dentry/inode cache, DNS cache
- [PARTIAL] Per-user /home/<user>/.cache with size cap, expiry, auto-cleanup
- [PARTIAL] Tools: `cache stat`, `cache clear`, `cache list`
- [PLANNED] BlueShell cmdlets: Get-Cache, Clear-Cache
- [PLANNED] Settings: Storage/Cache page

## Hardware support (VMware / QEMU / VirtualBox)
- [DONE]   APIC/IOAPIC, ACPI (stub), HPET, PIT, RTC
- [PARTIAL] Console: VESA/GOP framebuffer, VMware SVGA II
- [PARTIAL] Input: PS/2 keyboard+mouse, vmmouse absolute pointer
- [PLANNED] Storage: AHCI/SATA, NVMe, LSI Logic/BusLogic SCSI
- [PARTIAL] Network: Intel e1000/e1000e, vmxnet3, rtl8139/rtl8169, virtio-net

## Security
- [DONE]   User/group permissions, root vs kian
- [DONE]   Password hashing: SHA-512 crypt / Argon2id (stub → real)
- [PLANNED] Password never stored plaintext
- [PLANNED] Ring 3 userspace isolation enforced

## Quality & speed
- [PLANNED] Boot to login in under 5 seconds in a VM
- [PLANNED] GUI at 60 FPS in VMware at 1080p
- [PLANNED] No kernel panics during a 30-minute test

## Deliverables
- [DONE]   `kian-os-0.2.0.iso` (hybrid BIOS+UEFI) + `kian-os-0.2.0.iso.sha256`
- [PLANNED] `kian-os.vmx` + VMware setup instructions
- [PLANNED] GitHub Actions workflow: build ISO + publish to Releases
- [PLANNED] QEMU headless boot log (serial output) published
