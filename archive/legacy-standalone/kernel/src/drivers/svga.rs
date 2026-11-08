use crate::kprintln;
use x86_64::instructions::port::Port;

pub fn init() {
    kprintln!("[Kian] VMware SVGA II initialized (stub)");
}

pub fn set_mode(_width: u32, _height: u32, _bpp: u32) -> bool {
    kprintln!("[Kian] SVGA II set_mode (stub)");
    false
}

pub fn draw_pixel(_x: u32, _y: u32, _color: u32) {
    // Stub.
}

pub fn framebuffer_addr() -> *mut u32 {
    0 as *mut u32
}
