#![no_std]
#![no_main]
#![feature(alloc_error_handler)]
#![feature(allocator_api)]
#![feature(naked_functions)]
#![feature(asm_goto)]
#![feature(ptr_as_uninit)]
#![feature(extract_if)]
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
pub mod fs;
use core::panic::PanicInfo;

// Boot banner - ASCII art "KIAN OS" printed at boot.
pub static BOOT_BANNER: &str = r#"
  ______     __   __     ______     __  __     __   __     ______     ______   ______
 /\  __ \   /\ \./\ \   /\  __ \   /\ \/\ \   /\ \/\ \   /\  ___\   /\  ___\ /\  __ \
 \ \ \/\ \  \ \ \_\ \ \  \ \  __ \  \ \ \_\ \  \ \ \_\ \  \ \___  \  \ \  __\ \ \  __ \
  \ \_____\  \ \_____\ \  \ \_\ \_\  \ \_____\  \ \_____\  \/\_____\  \ \_\    \ \_\ \
   \/_____/   \/_____\/_\_\/_\/_/     \/_____/   \/_____/   \/_____/   \/_/     \/_/
                                                                                v0.2.0 "Horizon"
"#;

#[no_mangle]
pub extern "C" fn kernel_main() -> ! {
    // Early console + serial before anything else.
    console::early_init();
    arch::serial::init();

    kprintln!("{}", BOOT_BANNER);
    kprintln!("[Kian] Kernel starting... (x86_64, higher-half)");
    kprintln!("[Kian] Boot time zone: Asia/Tehran (UTC+03:30, no DST)");

    // Initialize architecture (GDT, IDT, paging).
    arch::init();

    // Memory manager: physical + virtual + heap.
    memory::init();

    // Drivers.
    drivers::init();

    // VFS + filesystems.
    vfs::init();
    fs::init();

    // Security / accounts.
    security::init();

    // Scheduler + multitasking.
    scheduler::init();

    // Syscall interface.
    syscall::init();

    // Start userspace (initramfs -> /sbin/init).
    process::start_userspace();

    kprintln!("[Kian] Kernel panic: userspace exited");
    crate::panic::halt("userspace exited");
}

#[macro_export]
macro_rules! kprint {
    ($($arg:tt)*) => ({
        use core::fmt::Write;
        $crate::console::WRITER.lock().write_fmt(format_args!($($arg)*)).unwrap();
    });
}

#[macro_export]
macro_rules! kprintln {
    () => ($crate::kprint!("\n"));
    ($($arg:tt)*) => ($crate::kprint!("{}\n", format_args!($($arg)*)));
}

#[alloc_error_handler]
fn alloc_error_handler(layout: alloc::alloc::Layout) -> ! {
    crate::panic::halt(&format!("allocation error: {:?}", layout));
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    crate::panic::kernel_panic(info);
    loop { unsafe { core::arch::asm!("hlt"); } }
}
