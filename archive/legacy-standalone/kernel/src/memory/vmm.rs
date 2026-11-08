use crate::kprintln;

pub fn init() {
    crate::kprintln!("[Kian] VMM ready");
}

pub fn identity_map(phys: x86_64::PhysAddr) {
    crate::memory::paging::identity_map(phys);
}
