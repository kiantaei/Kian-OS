use x86_64::structures::paging::OffsetPageTable;
use crate::memory::paging::OFFSET_PAGE_TABLE;

pub fn init() {
    crate::kprintln!("[Kian] Paging: higher-half kernel active");

    // Initialize the global offset page table for heap mapping.
    // The kernel is already identity-mapped at higher-half; we create an
    // OffsetPageTable starting at the physical start of the page tables.
    let phys_memory_offset = VirtAddr::new(0);
    let l4_table = unsafe { &mut *(0 as *mut x86_64::structures::paging::PageTable) };
    let offset_table = unsafe { OffsetPageTable::new(l4_table, phys_memory_offset) };
    *OFFSET_PAGE_TABLE.lock() = Some(offset_table);
}

use x86_64::VirtAddr;
