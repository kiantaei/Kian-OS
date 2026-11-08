#![no_std]
#![no_main]
#![feature(alloc_error_handler)]
#![feature(allocator_api)]
#![feature(abi_x86_interrupt)]
#![feature(ptr_as_uninit)]
#![allow(dead_code)]
#![allow(unused_variables)]
#![allow(unused_imports)]

extern crate alloc;
extern crate spin;
extern crate x86_64;

pub mod arch;
pub mod memory;
pub mod scheduler;
pub mod process;
pub mod syscall;
pub mod fs;
pub mod vfs;
pub mod drivers;
pub mod net;
pub mod security;
pub mod panic;
pub mod font;
pub mod input;
pub mod time;

use core::sync::atomic::{AtomicBool, Ordering};
use crate::arch::x86_64::serial;
use x86_64::instructions::port::Port;

// Stage gating - switch to true one at a time after each stage prints OK.
// While these are false we do NOT touch Limine's GDT or page tables.
const ENABLE_GDT: bool = false;
const ENABLE_IDT: bool = false;
const ENABLE_APIC: bool = false;
const ENABLE_PIT: bool = false;
const ENABLE_HPET: bool = false;
const ENABLE_NET: bool = false;
const ENABLE_SCHED: bool = false;

/* ==== ABI mirrors of include/kian_abi.h (layouts must match EXACTLY) ==== */

#[repr(C)]
pub struct Fb {
    pub addr: u64,        // virtual address, pitch in BYTES
    pub width: u32,
    pub height: u32,
    pub pitch: u32,
    pub bpp: u32,
}

#[repr(C)]
pub struct MemRegion {
    pub base: u64,
    pub length: u64,
    pub usable: u32,
}

#[repr(C)]
pub struct BootInfo {
    pub valid: u32,
    pub fb: Fb,
    pub hhdm_offset: u64,
    pub rsdp: u64,
    pub mem_count: u32,
    pub mem: [MemRegion; 64],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KianEvent {
    pub kind: i32,
    pub a: i32,
    pub b: i32,
    pub c: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KianTime {
    pub year: i32,
    pub month: i32,
    pub day: i32,
    pub hour: i32,
    pub minute: i32,
    pub second: i32,
    pub weekday: i32,
}

/* ==== C functions provided by boot/, console/ (see kian_abi.h) ==== */

unsafe extern "C" {
    fn kian_boot_info() -> *const BootInfo;
    fn console_init(fb: *const Fb);
    fn console_write(s: *const u8, n: usize);
    fn kian_shell_main() -> !;
    fn kian_run_ctors();
}

/* ==== kprint!/kprintln! - serial ALWAYS, C console when it is ready.
 * No Mutex, no .lock(), no allocation (fixed 512-byte stack buffer). ==== */

static CONSOLE_READY: AtomicBool = AtomicBool::new(false);

struct BufOut<'a> {
    buf: &'a mut [u8],
    n: usize,
}

impl core::fmt::Write for BufOut<'_> {
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        for &b in s.as_bytes() {
            if self.n < self.buf.len() {
                self.buf[self.n] = b;
            }
            self.n += 1;
        }
        Ok(())
    }
}

pub fn kprint_emit(args: core::fmt::Arguments) {
    use core::fmt::Write;
    let mut buf = [0u8; 512];
    let mut out = BufOut { buf: &mut buf, n: 0 };
    let _ = out.write_fmt(args);
    let n = out.n.min(buf.len());
    let s = &buf[..n];
    for b in s {
        serial::write_byte(*b);
    }
    if CONSOLE_READY.load(Ordering::Relaxed) {
        unsafe { console_write(s.as_ptr(), s.len()); }
    }
}

#[macro_export]
macro_rules! kprint {
    ($($arg:tt)*) => {
        $crate::kprint_emit(format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! kprintln {
    () => { $crate::kprint!("\n") };
    ($($arg:tt)*) => { $crate::kprint!("{}\n", format_args!($($arg)*)) };
}

/* ==== pre-console debug output: serial COM1 + QEMU debugcon 0xE9 ==== */

fn dbg_write(s: &str) {
    for b in s.bytes() {
        serial::write_byte(b);
        unsafe { Port::<u8>::new(0xE9).write(b); }
    }
}

/* ==== boot ==== */

// Pick the largest usable region above 1 MiB; heap = min(size, 64 MiB)
// mapped at base + hhdm_offset (Limine maps all of physical memory there).
fn pick_heap(bi: &BootInfo) -> (usize, usize) {
    let mut base = 0u64;
    let mut len = 0u64;
    for i in 0..bi.mem_count as usize {
        let r = &bi.mem[i];
        if r.usable == 1 && r.base >= 0x10_0000 && r.length > len {
            base = r.base;
            len = r.length;
        }
    }
    if len == 0 {
        return (0, 0);
    }
    if len > 64 * 1024 * 1024 {
        len = 64 * 1024 * 1024;
    }
    ((base + bi.hhdm_offset) as usize, len as usize)
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> ! {
    // 1. Interrupts off.
    unsafe { core::arch::asm!("cli"); }

    // 2. Serial COM1 (polling), "KIAN: _start" to serial AND debugcon 0xE9.
    serial::init();
    dbg_write("KIAN: _start\n");

    // 3. Boot info from the C boot glue (Limine requests).
    let bi: *const BootInfo = unsafe { kian_boot_info() };
    let valid = unsafe { (*bi).valid == 1 };
    if !valid {
        dbg_write("KIAN: NO FRAMEBUFFER / BAD BASE REVISION\n");
        kian_halt();
    }

    // 4. C framebuffer console + banner.
    unsafe { console_init(&(*bi).fb); }
    CONSOLE_READY.store(true, Ordering::Relaxed);
    kprintln!("Kian OS 0.3.0 (Rust kernel, C console, C++ GUI)");

    // 5. Heap: largest usable region from the Limine memmap.
    let (heap_base, heap_len) = pick_heap(unsafe { &*bi });
    if heap_len == 0 {
        kprintln!("KIAN: no usable memory for the heap!");
        kian_halt();
    }
    memory::heap::init(heap_base, heap_len);
    kprintln!("[Kian] heap: {} MiB at {:#x}", heap_len >> 20, heap_base);

    // 6. Run C++ static constructors.
    unsafe { kian_run_ctors(); }

    // 7. Stage gated inits (ALL false - Limine's GDT/page tables untouched).
    if ENABLE_GDT   { arch::x86_64::gdt::init();   kprintln!("[Kian] GDT OK"); }
    if ENABLE_IDT   { arch::x86_64::idt::init();   kprintln!("[Kian] IDT OK"); }
    if ENABLE_APIC  { arch::x86_64::apic::init();  kprintln!("[Kian] APIC OK"); }
    if ENABLE_PIT   { arch::x86_64::pit::init();   kprintln!("[Kian] PIT OK"); }
    if ENABLE_HPET  { arch::x86_64::hpet::init();  kprintln!("[Kian] HPET OK"); }
    if ENABLE_NET   { net::init();                 kprintln!("[Kian] NET OK"); }
    if ENABLE_SCHED { scheduler::init();           kprintln!("[Kian] SCHED OK"); }

    // 8. C console shell (login + ksh), never returns.
    unsafe { kian_shell_main() }
}

/* ==== ABI exports (implemented in Rust, see kian_abi.h) ==== */

#[unsafe(no_mangle)]
pub extern "C" fn kian_serial_write(s: *const u8, n: usize) {
    if s.is_null() {
        return;
    }
    for i in 0..n {
        serial::write_byte(unsafe { *s.add(i) });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_alloc(size: usize, align: usize) -> *mut u8 {
    memory::heap::kian_alloc(size, align)
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_free(p: *mut u8) {
    memory::heap::kian_free(p)
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_poll_event(out: *mut KianEvent) -> i32 {
    input::poll_event(out)
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_mem_stats(used: *mut u64, total: *mut u64) {
    let (u, t) = memory::heap::stats();
    if !used.is_null() {
        unsafe { *used = u; }
    }
    if !total.is_null() {
        unsafe { *total = t; }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_uptime_ms() -> u64 {
    // TSC-based; fallback calibration assumes ~2 GHz (QEMU/VMware typical).
    // PIT-calibrated refinement comes with ENABLE_PIT.
    (unsafe { core::arch::x86_64::_rdtsc() }) / 2_000_000
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_time_now(out: *mut KianTime) {
    if out.is_null() {
        return;
    }
    let mut t = KianTime {
        year: 1970, month: 1, day: 1, hour: 0, minute: 0, second: 0, weekday: 4,
    };
    time::fill_local(&mut t);
    unsafe { *out = t; }
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_auth_check(user: *const u8, pass: *const u8) -> i32 {
    if user.is_null() || pass.is_null() {
        return 0;
    }
    unsafe {
        let (ulen, ubuf) = read_cstr(user, 32);
        let (plen, pbuf) = read_cstr(pass, 64);
        if security::auth::verify(&ubuf[..ulen], &pbuf[..plen]) {
            1
        } else {
            0
        }
    }
}

unsafe fn read_cstr(p: *const u8, max: usize) -> (usize, [u8; 64]) {
    let mut buf = [0u8; 64];
    let cap = max.min(64);
    let mut len = 0usize;
    while len < cap {
        let b = *p.add(len);
        if b == 0 {
            break;
        }
        buf[len] = b;
        len += 1;
    }
    (len, buf)
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_reboot() -> ! {
    unsafe {
        // 1. PS/2 controller 8042 pulse-reset
        Port::<u8>::new(0x64).write(0xFE);
        for _ in 0..200_000 {
            core::hint::spin_loop();
        }
        // 2. triple fault: zero IDT, then trigger an interrupt
        static ZERO_IDT: [u64; 2] = [0, 0]; // limit=0, base=0
        core::arch::asm!(
            "cli",
            "lidt [{}]",
            "int3",
            in(reg) &ZERO_IDT as *const u64,
            options(noreturn)
        );
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_shutdown() -> ! {
    unsafe {
        // QEMU/Bochs
        Port::<u16>::new(0x604).write(0x2000);
        // Bochs (older) / VMware
        Port::<u16>::new(0xB004).write(0x2000);
        // VirtualBox
        Port::<u16>::new(0x4004).write(0x3400);
    }
    kian_halt()
}

#[unsafe(no_mangle)]
pub extern "C" fn kian_halt() -> ! {
    unsafe { core::arch::asm!("cli"); }
    loop {
        unsafe { core::arch::asm!("hlt"); }
    }
}

#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    unsafe { core::arch::asm!("cli"); }
    crate::kprintln!("");
    crate::kprintln!("*** KIAN KERNEL PANIC ***");
    if let Some(loc) = info.location() {
        crate::kprintln!("at {}:{}", loc.file(), loc.line());
    }
    crate::kprintln!("{}", info.message());
    loop { unsafe { core::arch::asm!("hlt"); } }
}
