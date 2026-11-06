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
pub mod console;
pub mod panic;
pub mod gui;
pub mod font;

use core::panic::PanicInfo;
use crate::arch::x86_64::serial;

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

#[no_mangle]
pub extern "C" fn _start() -> ! {
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
