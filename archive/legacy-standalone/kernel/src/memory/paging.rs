pub fn init() {
    crate::kprintln!("[Kian] VMM: paging active");
}

pub fn map_page(virt: x86_64::VirtAddr, phys: x86_64::PhysAddr) {
    // TODO: implement page mapping
    let _ = (virt, phys);
}

pub fn identity_map(phys: x86_64::PhysAddr) {
    // TODO: implement identity mapping
    let _ = phys;
}
