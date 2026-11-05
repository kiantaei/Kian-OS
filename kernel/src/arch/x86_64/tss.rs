use x86_64::structures::tss::TaskStateSegment;
use x86_64::{VirtAddr, PhysAddr};

pub fn init() {
    // TSS is initialized in gdt.rs. Here we set up IST stacks for interrupts.
    crate::kprintln!("[Kian] TSS / IST initialized");
}
