use crate::kprintln;
use x86_64::instructions::port::Port;

pub fn init() {
    kprintln!("[Kian] PS/2 mouse initialized");
    // Enable mouse on PS/2 controller.
    let mut ctrl = Port::<u8>::new(0x64);
    unsafe { ctrl.write(0xA8); } // enable second port
    unsafe { ctrl.write(0xD4); } // write to mouse
    let mut data = Port::<u8>::new(0x60);
    unsafe { data.write(0xF4); } // enable mouse streaming
    kprintln!("[Kian] PS/2 mouse streaming enabled");
}

pub fn read_packet() -> Option<(i8, i8, i8, u8)> {
    let mut status = Port::<u8>::new(0x64);
    let data = Port::<u8>::new(0x60);
    let s = unsafe { status.read() };
    if s & 0x01 == 0 {
        return None;
    }
    let bx = unsafe { data.read() } as i8;
    let by = unsafe { data.read() } as i8;
    let flags = unsafe { data.read() };
    // Y axis is inverted in screen coords.
    Some((bx, -by, 0, flags))
}

pub fn buttons(flags: u8) -> (bool, bool, bool) {
    let left = flags & 0x01 != 0;
    let right = flags & 0x02 != 0;
    let middle = flags & 0x04 != 0;
    (left, right, middle)
}
