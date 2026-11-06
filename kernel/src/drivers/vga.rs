// Kian OS - VGA text-mode driver (DISABLED STUB)
// The framebuffer console in crate::console is in use, so this driver is
// disabled. The public functions are kept as stubs so existing call sites
// (e.g. drivers::init -> vga::init) keep compiling.

pub fn init() {
    crate::kprintln!("[Kian] VGA text driver disabled (framebuffer console in use)");
}

pub fn set_color(fg: u32, bg: u32) {
    let _ = (fg, bg);
}
