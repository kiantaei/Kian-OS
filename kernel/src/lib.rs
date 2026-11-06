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
extern crate limine;

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
pub mod console;
pub mod panic;
pub mod gui;
pub mod font;

use core::panic::PanicInfo;
use crate::arch::x86_64::serial;
use limine::request::FramebufferRequest;
use limine::{BaseRevision, RequestsEndMarker, RequestsStartMarker};
use x86_64::instructions::port::Port;

// Boot banner
pub static BOOT_BANNER: &str = r#"
  ______     __   __     ______     __  __     __   __     ______     ______   ______
 /\  __ \   /\ \./\ \   /\  __ \   /\ \/\ \   /\ \/\ \   /\  ___\   /\  ___\ /\  __ \
 \ \ \/\ \  \ \ \_\ \ \  \ \  __ \  \ \ \_\ \  \ \ \_\ \  \ \___  \  \ \  __\ \ \  __ \
  \ \_____\  \ \_____\ \  \ \_\ \_\  \ \_____\  \ \_____\  \/\_____\  \ \_\    \ \_\ \
   \/_____/   \/_____\/_\_\/_\/_/     \/_____/   \/_____/   \/_____/   \/_/     \/_/
                                                                                v0.2.0 "Horizon"
"#;

// Stage gating — switch to true one at a time after each stage prints OK.
const ENABLE_GDT: bool = false;
const ENABLE_IDT: bool = false;
const ENABLE_HEAP: bool = false;
const ENABLE_APIC: bool = false;
const ENABLE_HPET: bool = false;
const ENABLE_PIT: bool = false;
const ENABLE_RTC: bool = false;
const ENABLE_SCHEDULER: bool = false;
const ENABLE_PROCESS: bool = false;
const ENABLE_SYSCALL: bool = false;
const ENABLE_NET: bool = false;
const ENABLE_SECURITY: bool = false;
const ENABLE_GUI: bool = false;
const ENABLE_DRIVERS: bool = false;

// Static heap: 4 MiB in .bss, no hard-coded physical addresses.
static mut HEAP_MEM: [u8; 4 * 1024 * 1024] = [0; 4 * 1024 * 1024];

/* ==== Limine request statics (limine crate 0.6.5) ====
 * The start/end markers delimit the request region; Limine v8 scans between
 * them. Every request static is #[used] and pinned to .limine_requests so the
 * linker can never drop or relocate it. */

#[used]
#[unsafe(link_section = ".limine_requests_start")]
static LIMINE_START_MARKER: RequestsStartMarker = RequestsStartMarker::new();

#[used]
#[unsafe(link_section = ".limine_requests_end")]
static LIMINE_END_MARKER: RequestsEndMarker = RequestsEndMarker::new();

#[used]
#[unsafe(link_section = ".limine_requests")]
pub static BASE_REVISION: BaseRevision = BaseRevision::new();

#[used]
#[unsafe(link_section = ".limine_requests")]
pub static FRAMEBUFFER_REQUEST: FramebufferRequest = FramebufferRequest::new();

/* ==== Minimal debug output: plain port I/O, no locks, no allocation ==== */

/// Bring up COM1 (115200 8N1, FIFO on) exactly per the init sequence.
fn debug_uart_init() {
    unsafe {
        Port::<u8>::new(0x3F9).write(0x00); // IER: disable UART interrupts
        Port::<u8>::new(0x3FB).write(0x80); // LCR: DLAB=1
        Port::<u8>::new(0x3F8).write(0x01); // divisor low = 1  -> 115200 baud
        Port::<u8>::new(0x3F9).write(0x00); // divisor high = 0
        Port::<u8>::new(0x3FB).write(0x03); // LCR: 8N1, DLAB=0
        Port::<u8>::new(0x3FA).write(0xC7); // FCR: FIFO enabled + cleared
        Port::<u8>::new(0x3FC).write(0x0B); // MCR: DTR | RTS | OUT2
    }
}

/// Send one byte to COM1 (0x3F8) AND the QEMU debug console (0xE9).
fn debug_putc(b: u8) {
    unsafe {
        // Wait for bit 5 (THR empty) of the Line Status Register.
        while (Port::<u8>::new(0x3FD).read() & 0x20) == 0 {
            core::hint::spin_loop();
        }
        Port::<u8>::new(0x3F8).write(b); // COM1 data register
        Port::<u8>::new(0xE9).write(b);  // QEMU debugcon (isa-debugcon)
    }
}

fn debug_write(s: &str) {
    for b in s.bytes() {
        debug_putc(b);
    }
}

fn debug_write_dec(mut v: u64) {
    if v == 0 {
        debug_putc(b'0');
        return;
    }
    let mut buf = [0u8; 20];
    let mut i = buf.len();
    while v > 0 {
        i -= 1;
        buf[i] = b'0' + (v % 10) as u8;
        v /= 10;
    }
    debug_write(core::str::from_utf8(&buf[i..]).unwrap_or("?"));
}

/// MINIMAL boot: prove out COM1 + debugcon + Limine framebuffer + pixels.
/// No gdt/idt/heap/memory/drivers/net/gui/scheduler init for now.
#[no_mangle]
pub extern "C" fn _start() -> ! {
    // 1. Interrupts off.
    unsafe { core::arch::asm!("cli"); }

    // 2. "KIAN: _start" to COM1 and the QEMU debug console (0xE9).
    debug_uart_init();
    debug_write("KIAN: _start\n");

    // 3. Limine base revision check.
    if BASE_REVISION.is_supported() {
        debug_write("KIAN: base revision supported\n");
    } else {
        debug_write("KIAN: base revision NOT SUPPORTED\n");
    }

    // 4. Limine framebuffer response.
    match FRAMEBUFFER_REQUEST.response() {
        Some(resp) => {
            debug_write("KIAN: fb response present\n");
            if let Some(fb) = resp.framebuffers().first() {
                // 5. Fill the WHOLE screen solid RED (0x00FF0000), 32-bit pixels.
                //    Row start = base + y * pitch (pitch is in BYTES).
                let base = fb.address() as usize;
                let w = fb.width;
                let h = fb.height;
                let pitch = fb.pitch;
                let bpp = fb.bpp as u64;
                for y in 0..h {
                    let row = (base + (y * pitch) as usize) as *mut u32;
                    for x in 0..w {
                        unsafe { row.add(x as usize).write_volatile(0x00FF0000); }
                    }
                }
                debug_write("KIAN: screen filled ");
                debug_write_dec(w);
                debug_write("x");
                debug_write_dec(h);
                debug_write(" bpp=");
                debug_write_dec(bpp);
                debug_write(" pitch=");
                debug_write_dec(pitch);
                debug_write("\n");
            } else {
                debug_write("KIAN: fb response present but ZERO framebuffers\n");
            }
        }
        None => {
            debug_write("KIAN: NO FB RESPONSE\n");
        }
    }

    // 6. Halt forever.
    loop {
        unsafe { core::arch::asm!("hlt"); }
    }
}

/// The old full-init boot path, kept intact but NOT called.
/// Re-enable it later by having _start call full_boot() again.
#[allow(dead_code)]
fn full_boot() -> ! {
    // 1. Interrupts off.
    unsafe { core::arch::asm!("cli"); }

    // 2. Init serial COM1.
    serial::init();
    serial::write_str("KIAN: _start reached\n");

    // 3. Get framebuffer from Limine.
    let fb = match gui::framebuffer::init() {
        Some(fb) => {
            serial::write_str("KIAN: framebuffer OK ");
            serial::write_u32(fb.width);
            serial::write_str("x");
            serial::write_u32(fb.height);
            serial::write_str("\n");
            fb
        }
        None => {
            serial::write_str("KIAN: NO FRAMEBUFFER\n");
            loop { unsafe { core::arch::asm!("hlt"); } }
        }
    };

    // 4. Fill screen dark blue and draw text.
    gui::framebuffer::fill_screen(&fb, 0x001A2B4A);
    console::init();
    console::draw_text(0, 0, "KIAN OS 0.2.0 - kernel alive", 0xFFFFFF);
    serial::write_str("KIAN: kernel alive\n");

    // Stage gated inits (all false for now)
    if ENABLE_GDT  { arch::x86_64::gdt::init();  serial::write_str("[Kian] GDT OK\n"); }
    if ENABLE_IDT  { arch::x86_64::idt::init();  serial::write_str("[Kian] IDT OK\n"); }
    if ENABLE_HEAP { memory::heap::init(); serial::write_str("[Kian] HEAP OK\n"); }
    if ENABLE_APIC { arch::x86_64::apic::init(); serial::write_str("[Kian] APIC OK\n"); }
    if ENABLE_HPET { arch::x86_64::hpet::init(); serial::write_str("[Kian] HPET OK\n"); }
    if ENABLE_PIT  { arch::x86_64::pit::init();  serial::write_str("[Kian] PIT OK\n"); }
    if ENABLE_RTC  { arch::x86_64::rtc::init();  serial::write_str("[Kian] RTC OK\n"); }
    if ENABLE_SCHEDULER { scheduler::init(); serial::write_str("[Kian] SCHED OK\n"); }
    if ENABLE_PROCESS { process::init(); serial::write_str("[Kian] PROC OK\n"); }
    if ENABLE_SYSCALL { syscall::init(); serial::write_str("[Kian] SYSCALL OK\n"); }
    if ENABLE_NET { net::init(); serial::write_str("[Kian] NET OK\n"); }
    if ENABLE_SECURITY { security::init(); serial::write_str("[Kian] SEC OK\n"); }
    if ENABLE_DRIVERS { drivers::init(); serial::write_str("[Kian] DRIVERS OK\n"); }

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

#[macro_export]
macro_rules! kprint {
    ($($arg:tt)*) => {
        $crate::console::_print(format_args!($($arg)*))
    };
}

#[macro_export]
macro_rules! kprintln {
    () => { $crate::kprint!("\n") };
    ($($arg:tt)*) => { $crate::kprint!("{}\n", format_args!($($arg)*)) };
}
