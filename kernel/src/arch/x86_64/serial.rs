use x86_64::instructions::port::Port;
use crate::kprintln;

const COM1_DATA: u16 = 0x3F8;
const COM1_IER: u16 = 0x3F9;
const COM1_FCR: u16 = 0x3FA;
const COM1_LCR: u16 = 0x3FB;
const COM1_MCR: u16 = 0x3FC;
const COM1_LSR: u16 = 0x3FD;

pub fn init() {
    let mut port = Port::<u8>::new(COM1_LCR);
    // DLAB enable to set baud rate divisor
    unsafe { port.write(0x80); }        // line control: DLAB=1
    let mut dll_port = Port::<u8>::new(COM1_DATA);
    unsafe { dll_port.write(0x01); }    // DLL low = 1 (115200)
    let mut dlm_port = Port::<u8>::new(COM1_IER);
    unsafe { dlm_port.write(0x00); }    // DLL high = 0
    let mut lcr_port = Port::<u8>::new(COM1_LCR);
    unsafe { lcr_port.write(0x03); }    // line control: 8N1, DLAB=0
    let mut ier_port = Port::<u8>::new(COM1_IER);
    unsafe { ier_port.write(0x00); }    // IER: no interrupts
    crate::kprintln!("[Kian] Serial (COM1) initialized");
}

pub fn write_byte(b: u8) {
    let mut port = Port::<u8>::new(COM1_LSR);
    unsafe {
        // Wait for THRE empty
        while (port.read() & 0x20) == 0 {}
        let mut data_port = Port::<u8>::new(COM1_DATA);
        data_port.write(b);
    }
}
