pub fn init() {
    crate::kprintln!("[Kian] Interrupts enabled");
    unsafe { x86_64::instructions::interrupts::enable() };
}
