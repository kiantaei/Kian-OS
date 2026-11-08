pub fn init() {
    crate::kprintln!("[Kian] Interrupts enabled");
    x86_64::instructions::interrupts::enable();
}
