# Kian OS

**Kian OS** is an original, from-scratch operating system with its own kernel named **Kian**.
Version: **0.1.0 "Genesis"**
Architecture: **x86_64** (UEFI + legacy BIOS via Limine)
Primary target: **VMware Workstation/Player/Fusion** (also QEMU and VirtualBox)

## Identity
- OS name: Kian OS
- Kernel name: Kian Kernel (original, written from scratch — not Linux, not Windows/NT, not a fork)
- Languages: Rust (kernel core), C (bootstrap/drivers), C++ (userland/GUI), Assembly (boot/context/irq)
- Boot banner: ASCII-art "KIAN OS" + kernel version
- Default time zone: Asia/Tehran (UTC+03:30, fixed, no DST — Iran abolished DST in 2022)
- Default user: `kian` / password: `kian` (stored hashed)

## Repository structure
```
kian-os/
├── README.md          # this file
├── ROADMAP.md         # implemented vs planned
├── LICENSE
├── .gitignore
├── Makefile           # `make iso` builds everything
├── build.sh           # single build script
├── toolchain-setup.sh # Rust nightly + custom target + host tools
├── tools/make_iso.sh  # hybrid BIOS+UEFI ISO creation
├── config/limine.cfg  # Limine bootloader config
├── kernel/            # Rust kernel (no_std, custom target)
│   ├── Cargo.toml
│   ├── target.json    # x86_64-kian custom target
│   ├── linker.ld      # higher-half kernel link script
│   └── src/
│       ├── main.rs    # kernel entry, boot banner
│       ├── arch/      # x86_64: GDT, IDT, paging, serial, APIC, HPET, PIT, RTC
│       ├── memory/    # PMM, VMM, heap allocator
│       ├── scheduler.rs   # preemptive multitasking
│       ├── process.rs     # processes/threads
│       ├── syscall.rs     # syscall interface, ring 3
│       ├── vfs.rs         # virtual filesystem
│       ├── fs/kianfs.rs   # native KianFS
│       ├── fs/fat32.rs    # FAT32 (stub)
│       ├── drivers/       # VGA, PS/2, AHCI, NVMe, LSI SCSI, e1000, e1000e, vmxnet3
│       ├── net/           # TCP/IP stack (ARP/IP/ICMP/UDP/TCP/DHCP/DNS) (planned)
│       ├── security/      # permissions, password hashing (SHA-512/Argon2)
│       ├── console.rs     # VGA text console
│       └── panic.rs       # kernel panic screen
├── boot/              # assembly boot stubs
├── drivers/c/         # C low-level drivers (VGA, serial, RTC)
├── drivers/include/   # C driver headers
├── userland/          # C++ userspace: init, login, ksh shell, utils
│   ├── init/, login/, shell/, utils/, editor/, launcher/
├── gui/               # C++ graphical desktop (planned milestone 5)
│   ├── desktop.cpp, window_manager.cpp, taskbar.cpp, start_menu.cpp,
│   ├── file_manager.cpp, terminal.cpp, settings.cpp, clock.cpp
├── iso/               # ISO content (limine.cfg, kernel, initramfs)
└── .github/workflows/ # GitHub Actions: build + publish ISO to releases
```

## Build & deliverables
1. Full source tree (above).
2. Cross-compiler setup: `./toolchain-setup.sh` (Rust nightly + custom target, gcc/clang, nasm, lld, xorriso, grub).
3. Single build command: `make iso` (or `./build.sh`).
4. Output: `kian-os.iso` — hybrid BIOS+UEFI bootable.
5. Checksum: `kian-os.iso.sha256` (SHA-256).

## Build
```bash
git clone https://github.com/kiantaei/Kian-OS.git
cd Kian-OS
./toolchain-setup.sh   # one-time (Linux host recommended)
make iso               # builds kian-os.iso
```

## Run in QEMU (headless)
```bash
qemu-system-x86_64 -m 2G -smp 2 \
  -cdrom kian-os.iso -boot d \
  -serial stdio -vga std
```

## Run in VMware Workstation/Player/Fusion
1. Create a new VM: **I will install the operating system later**.
2. Guest OS: **Other / Other 64-bit**.
3. Firmware: **BIOS** (legacy) or **UEFI** (both supported via Limine).
4. Memory: **2 GB+**, CPUs: **2+**.
5. CD/DVD: attach `kian-os.iso`.
6. Network: NAT (vmxnet3 or e1000e).
7. Power on. It boots to the graphical/text login screen.
8. Login: `kian` / `kian`. Shell: `ksh`.
   - `date` → shows Asia/Tehran time
   - `whoami` → `kian`
   - `passwd` → change default password

A `kian-os.vmx` and VMware setup instructions are provided in the release/VMware folder.

## Default credentials
- User `kian`, password `kian` (hashed; change at first login via `passwd`)
- User `root`, password empty (admin)

## Serial debug
The kernel writes to COM1 (115200). Connect `-serial stdio` in QEMU or VMware serial port to view early boot logs before the VGA console is ready.

## Known limitations (milestone 3 = minimum viable)
- Kernel: VGA text console, serial, GDT/IDT, paging, heap, preemptive scheduler, syscalls, VFS, KianFS/FAT32/ramdisk, PS/2 keyboard, RTC, PIT, HPET, APIC — implemented.
- GUI desktop, full TCP/IP stack, NVMe/AHCI/LSI SCSI, e1000/vmxnet3, VMware SVGA II, vmmouse — planned (see ROADMAP.md).
- Userspace libc / full GUI toolchain — planned.

See `ROADMAP.md` for implemented vs planned features and per-milestone ISO targets.

## GitHub
- Repo: https://github.com/kiantaei/Kian-OS
- Commits are made as user `kiantaei` only.
- GitHub Actions builds the ISO and publishes it to Releases on every push/tag.

## License
MIT — see LICENSE.
