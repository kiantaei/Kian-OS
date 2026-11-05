use x86_64::structures::paging::{Mapper, Page, Size4KiB, PageTableFlags, PhysFrame, OffsetPageTable};
use x86_64::{VirtAddr, PhysAddr};
use crate::memory::frame_allocator::GlobalFrameAllocator;
use lazy_static::lazy_static;
use spin::Mutex;

lazy_static! {
    pub static ref OFFSET_PAGE_TABLE: Mutex<OffsetPageTable<'static>> = Mutex::new(unsafe {
        x86_64::memory::mapper::remap_the_kernel()
    });
}

pub fn init() {
    crate::kprintln!("[Kian] VMM: offset page table active");
}

pub fn map_page(virt: VirtAddr, phys: PhysAddr) {
    let page = Page::containing_address(virt);
    let frame = PhysFrame::containing_address(phys);
    let flags = PageTableFlags::PRESENT | PageTableFlags::WRITABLE;
    let mut allocator = GlobalFrameAllocator;
    let _ = OFFSET_PAGE_TABLE.lock().map_to(page, frame, flags, &mut allocator);
}

pub fn identity_map(phys: PhysAddr) {
    let virt = VirtAddr::new(phys.as_u64());
    map_page(virt, phys);
}
