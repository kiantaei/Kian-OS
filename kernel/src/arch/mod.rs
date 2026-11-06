use crate::kprintln;

#[cfg(target_arch = "x86_64")]
pub mod x86_64;

pub fn init() {
    kprintln!("[Kian] Console initialized");
}

#[cfg(target_arch = "x86_64")]
pub fn init_x86_64() {
    x86_64::init();
}
