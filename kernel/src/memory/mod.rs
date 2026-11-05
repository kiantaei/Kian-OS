pub mod frame_allocator;
pub mod heap;
pub mod paging;
pub mod vmm;

use crate::kprintln;

pub fn init() {
    frame_allocator::init();
    heap::init();
    paging::init();
    kprintln!("[Kian] Memory manager initialized (PMM + VMM + heap)");
}
