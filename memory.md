# Kian OS — Project Memory (for AI agents)

## Identity
- OS: Kian OS, version 0.2.0 "Horizon"
- Kernel: Kian Kernel (original, from scratch — NOT Linux/Windows/NT fork)
- Arch: x86_64, UEFI + legacy BIOS via Limine
- Primary VM target: VMware Workstation/Player/Fusion (also QEMU, VirtualBox)
- Languages: Rust kernel (`#![no_std]`, built-in `x86_64-unknown-none` target), C drivers/bootstrap, C++ userland/GUI, asm only for boot/context/IRQ
- Default login: `kian` / `kian` (password hashed, never plaintext); root account exists
- Time zone: Asia/Tehran (UTC+03:30, fixed, NO DST — Iran abolished DST 2022)
- Boot banner: ASCII art "KIAN OS" + version v0.2.0 "Horizon"

## Repository layout (working tree at C:\Users\Kian\Documents\GitHub\Kian-OS)
```
kernel/          Rust kernel (#![no_std]); kernel/target.json DELETED, uses x86_64-unknown-none
                 Cargo.toml, build.rs, .cargo/config.toml, linker.ld,
                 src/{arch,memory,scheduler,process,syscall,vfs,fs,drivers,net,security,console,panic,lib.rs}
boot/            boot.asm (BIOS stub)
config/          limine.cfg (Limine BIOS+UEFI)
drivers/c/       C low-level drivers (VGA, serial COM1, RTC) + drivers/include/kian_drivers.h
userland/        C++ userspace: init/, login/, shell/ksh.cpp, utils/, editor/, launcher/
                 bsh/                 BlueShell (Rust, PowerShell-style)
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
make iso               # -> kian-os-0.2.0.iso + kian-os-0.2.0.iso.sha256
qemu-system-x86_64 -m 2G -smp 2 -cdrom kian-os-0.2.0.iso -boot d -serial stdio
```

## Key design facts (do not contradict)
- Kernel entry: `kernel_main` at higher-half `0xffffffff80000000` (linker.ld)
- Rust built-in target `x86_64-unknown-none` (no custom target.json needed)
- Limine config `config/limine.cfg` loads `boot/kian-kernel.elf` + `iso/initramfs.tar.gz`, cmdline includes `tz=Asia/Tehran`
- Syscall numbers: WRITE=1, EXIT=2, GETPID=3, TIME=4, YIELD=5, READ=6, OPEN=7, CLOSE=8, STAT=9, EXEC=10 (`kernel/src/syscall.rs`)
- Security: `kernel/src/security/auth.rs` (users root/kian), `hash.rs` (SHA-512/Argon2 stubs → real impl needed)
- ksh built-ins: ls cd cat cp mv rm mkdir ps kill mount ifconfig ip ping date uname clear echo edit passwd whoami su useradd exit history; supports `\|`, `>`, `>>`, `<`
- BlueShell `bsh`: Get-Help, Get-Command, Get-ChildItem, Set-Location, Get-Content, Set-Content, New-Item, Remove-Item, Copy-Item, Move-Item, Get-Process, Stop-Process, Get-Service, Get-Date (Asia/Tehran), Test-Connection, Invoke-WebRequest, Get-NetIPAddress, Get-Cache, Clear-Cache, Write-Output, Read-Host; pipeline: Where-Object, Select-Object, Sort-Object, ForEach-Object, Measure-Object, Format-Table, Format-List, ConvertTo-Json
- `date` applies Asia/Tehran offset (+03:30, no DST)

## Planned / not yet implemented (ROADMAP.md)
- GUI desktop, full TCP/IP stack (ARP/IP/ICMP/UDP/TCP/DHCP/DNS), AHCI/NVMe/LSI-SCSI, e1000/e1000e/vmxnet3, VMware SVGA II, vmmouse, tab-completion, real libc/ELF loader, graphical login, TLS, Kian Browser

## Rules for agents
- Never claim ISO works unless actually built+booted; show real logs.
- Commit as `kiantaei` only; never use/embed the GitHub token.
- Do not fake results; mark uncertain features as `[PLANNED]`.
- Milestones: (1) boot+banner, (2) memory+scheduler+keyboard+shell, (3) login+kian/kian+Asia/Tehran clock (minimum ISO), (4) FS+networking, (5) GUI.
