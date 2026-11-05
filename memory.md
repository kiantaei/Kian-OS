# Kian OS — Project Memory (for AI agents)

## Identity
- OS: Kian OS, version 0.1.0 "Genesis"
- Kernel: Kian Kernel (original, from scratch — NOT Linux/Windows/NT fork)
- Arch: x86_64, UEFI + legacy BIOS via Limine
- Primary VM target: VMware Workstation/Player/Fusion (also QEMU, VirtualBox)
- Languages: Rust kernel (`#![no_std]`, custom target `x86_64-kian`), C drivers/bootstrap, C++ userland/GUI, asm only for boot/context/IRQ
- Default login: `kian` / `kian` (password hashed, never plaintext); root account exists
- Time zone: Asia/Tehran (UTC+03:30, fixed, NO DST — Iran abolished DST 2022)
- Boot banner: ASCII art "KIAN OS" + version

## Repository layout (working tree at C:\Users\Kian\Desktop\Kian-os)
```
kernel/          Rust kernel (#![no_std]); kernel/target.json, linker.ld, Cargo.toml, src/{arch,memory,scheduler,process,syscall,vfs,fs,drivers,net,security,console,panic,main.rs}
boot/            boot.asm (BIOS stub)
config/          limine.cfg (Limine BIOS+UEFI)
drivers/c/       C low-level drivers (VGA, serial COM1, RTC) + drivers/include/kian_drivers.h
userland/        C++ userspace: init/, login/, shell/ksh.cpp, utils/, editor/, launcher/
gui/             C++ desktop scaffolding (desktop, window_manager, taskbar, start_menu, file_manager, terminal, settings, clock)
iso/             limine.cfg, grub.cfg, initramfs.list (ISO content)
tools/           make_iso.sh
.github/workflows/build-iso.yml   CI: build + QEMU serial test + publish ISO+SHA256 to Releases as kiantaei
Makefile, build.sh, toolchain-setup.sh, README.md, ROADMAP.md, LICENSE, .gitignore
```

## Git state
- Local repo initialized, one commit: `31943d4` "genesis: initial Kian OS source tree (0.1.0 Genesis)", 80 files, ~2309 lines
- Git user set to `kiantaei` / `kiantaei@users.noreply.github.com`
- **NOT pushed to GitHub.** No token used/stored here. Remote not added.
- No ISO built. No QEMU boot tested (this host lacks rustc/cargo/gcc/qemu/make).

## Build commands (run on Linux host)
```
./toolchain-setup.sh   # one-time: rust nightly + binutils/lld/xorriso/nasm/qemu/grub
make iso               # -> kian-os.iso + kian-os.iso.sha256
qemu-system-x86_64 -m 2G -smp 2 -cdrom kian-os.iso -boot d -serial stdio
```

## Key design facts (do not contradict)
- Kernel entry: `kernel_main` at higher-half `0xffffffff80000000` (linker.ld)
- Custom Rust target defined in `kernel/target.json` (path-based, not installed via rustup target add)
- Limine config `config/limine.cfg` loads `boot/kian-kernel.elf` + `iso/initramfs.tar.gz`, cmdline includes `tz=Asia/Tehran`
- Syscall numbers: WRITE=1, EXIT=2, GETPID=3, TIME=4, YIELD=5 (`kernel/src/syscall.rs`)
- Security: `kernel/src/security/auth.rs` (users root/kian), `hash.rs` (SHA-512/Argon2 stubs → real impl needed)
- ksh built-ins: ls cd cat cp mv rm mkdir ps kill mount ifconfig ip ping date uname clear echo edit passwd whoami su useradd exit history; supports `\|`, `>`, `>>`, `<`
- `date` applies Asia/Tehran offset (+03:30, no DST)

## Planned / not yet implemented (ROADMAP.md)
- GUI desktop, full TCP/IP stack (ARP/IP/ICMP/UDP/TCP/DHCP/DNS), AHCI/NVMe/LSI-SCSI, e1000/e1000e/vmxnet3, VMware SVGA II, vmmouse, tab-completion, real libc/ELF loader, graphical login

## Rules for agents
- Never claim ISO works unless actually built+booted; show real logs.
- Commit as `kiantaei` only; never use/embed the GitHub token.
- Do not fake results; mark uncertain features as `[PLANNED]`.
- Milestones: (1) boot+banner, (2) memory+scheduler+keyboard+shell, (3) login+kian/kian+Asia/Tehran clock (minimum ISO), (4) FS+networking, (5) GUI.
