pub fn init() {
    crate::kprintln!("[Kian] Paging: higher-half kernel active");
}

pub fn map_page(virt: x86_64::VirtAddr, phys: x86_64::PhysAddr) {
    crate::memory::paging::map_page(virt, phys);
}
