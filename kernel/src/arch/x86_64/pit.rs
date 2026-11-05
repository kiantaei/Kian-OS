use x86_64::instructions::port::Port;

pub fn init() {
    let mut port = Port::<u8>::new(0x43);
    unsafe { port.write(0x34); } // channel 0, lobyte/hibyte, mode 2, binary
    crate::kprintln!("[Kian] PIT initialized");
}
