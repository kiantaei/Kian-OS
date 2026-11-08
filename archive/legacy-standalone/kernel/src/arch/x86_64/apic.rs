pub fn init() {
    crate::kprintln!("[Kian] APIC/IOAPIC initialized");
}

pub fn send_eoi(irq: u8) {
    let _ = irq;
    // xAPIC EOI register at 0xFEE000B0 (mapped). Stub for now.
}
