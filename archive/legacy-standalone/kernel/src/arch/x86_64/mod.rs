pub mod gdt;
pub mod idt;
pub mod interrupts;
pub mod paging;
pub mod pic;
pub mod serial;
pub mod context;
pub mod tss;
pub mod apic;
pub mod hpet;
pub mod pit;
pub mod rtc;

pub fn init() {
    gdt::init();
    idt::init();
    interrupts::init();
    paging::init();
    serial::init();
    apic::init();
    hpet::init();
    pit::init();
    rtc::init();
    crate::kprintln!("[Kian] x86_64 arch initialized (GDT/IDT/paging/serial/APIC/HPET/PIT/RTC)");
}
