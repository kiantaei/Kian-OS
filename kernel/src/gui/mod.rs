// Kian OS GUI module
// Step 1: Framebuffer support via Limine

pub mod framebuffer;

pub fn init() {
    framebuffer::init();
}
