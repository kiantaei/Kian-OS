use x86_64::instructions::port::Port;

pub unsafe fn eoi(vector: u8) {
    if vector >= 0x28 {
        Port::<u8>::new(0xA0).write(0x20);
    }
    Port::<u8>::new(0x20).write(0x20);
}
