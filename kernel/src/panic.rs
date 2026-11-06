use crate::kprintln;
use alloc::format;

pub fn halt(message: &str) -> ! {
    crate::kprintln!("");
    crate::kprintln!("========================================");
    crate::kprintln!(" KIAN OS KERNEL PANIC");
    crate::kprintln!("========================================");
    crate::kprintln!("{}", message);
    crate::kprintln!("Version: 0.2.0 \"Horizon\"");
    crate::kprintln!("Arch: x86_64");
    crate::kprintln!("========================================");
    crate::kprintln!("System halted. Please reboot.");
    loop {
        unsafe { core::arch::asm!("cli; hlt"); }
    }
}

pub fn kernel_panic(info: &core::panic::PanicInfo) -> ! {
    let msg = match info.location() {
        Some(loc) => format!("panic at {}:{}:{} - {}", loc.file(), loc.line(), loc.column(), info.message()),
        None => format!("panic: {}", info.message()),
    };
    halt(&msg);
}
