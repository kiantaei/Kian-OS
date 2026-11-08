pub mod heap;
pub mod paging;
pub mod vmm;

use crate::kprintln;

pub fn init() {
    paging::init();
    kprintln!("[Kian] Memory manager initialized (VMM + heap)");
}
