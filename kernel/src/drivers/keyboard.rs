use x86_64::instructions::port::Port;
use crate::kprintln;

pub fn init() {
    // Enable keyboard scanning on PS/2 controller (port 0x64 command 0xAE = enable first port)
    let mut ctrl = Port::<u8>::new(0x64);
    unsafe { ctrl.write(0xAE); }
    kprintln!("[Kian] PS/2 keyboard controller enabled");
}

pub fn read_scancode() -> u8 {
    let mut port = Port::<u8>::new(0x60);
    unsafe { port.read() }
}
