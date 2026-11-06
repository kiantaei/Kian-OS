# Kian OS

**Kian OS** is an original, from-scratch operating system with its own kernel named **Kian**.
Version: **0.3.0 "Horizon" (three-language restructure)**
Architecture: **x86_64** (UEFI + legacy BIOS via Limine)
Primary target: **VMware Workstation/Player/Fusion** (also QEMU and VirtualBox)

## Identity
- OS name: Kian OS
- Kernel name: Kian Kernel (original, written from scratch — not Linux, not Windows/NT, not a fork)
- Languages: **Rust** (kernel), **C** (boot glue + console/shell), **C++** (GUI)
- Boot banner: ASCII-art "KIAN OS" + kernel version
- Default time zone: Asia/Tehran (UTC+03:30, fixed, no DST — Iran abolished DST in 2022)
- Default user: `kian` / password: `kian` (stored as SHA-256(salt+password), never plaintext)

## The three languages (strict split)
| Layer | Language | Job |
|---|---|---|
| **KERNEL** | Rust (`kernel/`, staticlib, x86_64-unknown-none) | boot flow, serial, heap/memory, PS/2 keyboard+mouse polling, RTC + Asia/Tehran time, password check, power control; net/scheduler/etc. kept but OFF by constants |
| **BOOT GLUE** | C (`boot/`) | official Limine requests (`limine.h` from the v8.x-binary checkout) + boot info + C++ ctor runner |
| **CONSOLE** | C (`console/`) | framebuffer text console, 8x8 font, minimal printf + libc, login prompt, `ksh` shell |
| **GUI** | C++ (`gui/`) | desktop, window manager, taskbar, start menu, cursor, apps — started ONLY by typing `gui` in ksh |

`include/kian_abi.h` is the single contract between the three languages.

## Repository structure
```
kian-os/
├── include/kian_abi.h   # shared C header (the ABI contract)
├── boot/limine_requests.c  # official Limine requests + kian_boot_info()/kian_run_ctors()
├── console/             # C: console.c, font8x8.c, printf.c, libc_min.c, shell.c
├── gui/                 # C++: gui.cpp, gfx.cpp, wm.cpp, desktop.cpp, runtime.cpp (+ .h)
├── kernel/              # Rust kernel (no_std, x86_64-unknown-none, staticlib)
│   ├── linker.ld        # kernel linker script (ENTRY(_start) first, base 0xffffffff80000000)
│   └── src/             # lib.rs (_start + ABI exports), input.rs, time.rs, memory/heap.rs,
│                        # security/{hash.rs,auth.rs}, arch/, net/, ... (kept-but-off modules)
├── config/limine.conf   # Limine bootloader config (timeout 5, verbose, serial)
├── tools/make_iso.sh    # hybrid BIOS+UEFI ISO creation
├── build.sh             # full build: C -> C++ -> Rust -> link -> ISO -> ELF checks
└── .github/workflows/build-iso.yml  # CI: build + ISO + QEMU boot test + release
```

## Build & run
```
./build.sh                 # needs: clang, lld, xorriso, mtools (see build.sh header)
qemu-system-x86_64 -cdrom kian-os-0.2.0.iso -m 512 -debugcon file:debugcon.log
```
In VMware: boot the ISO, watch the serial console (COM1, 115200 8N1) or the screen.

## Status (0.3.0 restructure)
- **M1 boot -> blue screen + banner from the C console: done**
- **M2 keyboard typing in ksh: done** (PS/2 set-1 polling, shift/caps/ctrl/alt)
- **M3 login kian/kian + `date` (Asia/Tehran): done**
- **M4 `gui` starts the C++ desktop; ESC / "Exit to console" returns to ksh: done**
- Partly working:
  - `passwd` in ksh is a stub (does not change the stored hash yet)
  - GUI "Files" app shows a placeholder (no filesystem mounted yet)
  - `kian_uptime_ms` uses the TSC with a ~2 GHz fallback calibration (PIT calibration pending ENABLE_PIT)
  - GUI mouse uses PS/2 relative packets; VMware absolute (vmmouse) is a kernel driver kept but off
  - GDT/IDT/APIC/PIT/HPET/NET/SCHED are implemented but OFF by stage constants
