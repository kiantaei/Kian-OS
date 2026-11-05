use x86_64::instructions::port::Port;
use crate::kprintln;

pub fn init() {
    let mut port = Port::<u8>::new(0x3F8);
    // DLAB enable to set baud rate divisor
    unsafe { port.write(0x80); }        // line control: DLAB=1
    unsafe { port.offset(0).write(0x01); } // DLL low = 1 (115200)
    unsafe { port.offset(1).write(0x00); } // DLL high = 0
    unsafe { port.write(0x03); }        // line control: 8N1, DLAB=0
    unsafe { port.offset(1).write(0x00); } // IER: no interrupts
    crate::kprintln!("[Kian] Serial (COM1) initialized");
}

pub fn write_byte(b: u8) {
    let mut port = Port::<u8>::new(0x3F8);
    unsafe {
        // Wait for THRE empty
        let mut port_lsr = Port::<u8>::new(0x3FD);
        while (port_lsr.read() & 0x20) == 0 {}
        port.write(b);
    }
}
