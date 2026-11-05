use linked_list_allocator::{Heap, LockedHeap};
use x86_64::{VirtAddr, PhysAddr};
use x86_64::structures::paging::{Mapper, Page, Size4KiB, PageTableFlags, OffsetPageTable};
use crate::memory::frame_allocator::GlobalFrameAllocator;
use lazy_static::lazy_static;
use spin::Mutex;

const HEAP_START: usize = 0x_4444_4444_0000;
const HEAP_SIZE: usize = 4 * 1024 * 1024;

#[global_allocator]
static ALLOCATOR: LockedHeap = LockedHeap::empty();

pub static HEAP: Mutex<Heap> = Mutex::new(Heap::empty());

pub fn init() {
    // Map heap pages into the higher-half address space
    let mut mapper = unsafe { x86_64::memory::mapper::remap_the_kernel() };
    let mut allocator = GlobalFrameAllocator;
    let mut mapped = 0;
    for i in 0..(HEAP_SIZE / 4096) {
        let virt = VirtAddr::new(HEAP_START + i * 4096);
        let page = Page::containing_address(virt);
        if let Some(frame) = allocator.allocate_frame() {
            let flags = PageTableFlags::PRESENT | PageTableFlags::WRITABLE;
            let _ = mapper.map_to(page, frame, flags, &mut allocator);
            mapped += 1;
        }
    }
    unsafe {
        HEAP.lock().init(HEAP_START as *mut u8, HEAP_SIZE);
    }
    crate::kprintln!("[Kian] Heap: {} pages ({:.1} MiB) at {:#x}", mapped, HEAP_SIZE as f64 / 1024.0 / 1024.0, HEAP_START);
}

#[alloc_error_handler]
fn alloc_error_handler(layout: core::alloc::Layout) -> ! {
    crate::panic::halt(&format!("heap allocation error: {:?}", layout));
}
