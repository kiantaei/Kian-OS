use x86_64::instructions::port::Port;

const CMOS_ADDR: u16 = 0x70;
const CMOS_DATA: u16 = 0x71;

fn cmos_read(reg: u8) -> u8 {
    let mut addr_port = Port::<u8>::new(CMOS_ADDR);
    let mut data_port = Port::<u8>::new(CMOS_DATA);
    unsafe {
        addr_port.write(reg | 0x80); // disable NMI
        let _ = data_port.read(); // small delay
        addr_port.write(reg);
        data_port.read()
    }
}

fn bcd_to_bin(bcd: u8) -> u8 {
    (bcd & 0x0f) + ((bcd >> 4) * 10)
}

pub struct RtcTime {
    pub second: u8,
    pub minute: u8,
    pub hour: u8,
    pub day: u8,
    pub month: u8,
    pub year: u8,
}

pub fn read() -> RtcTime {
    // Wait for update cycle not in progress
    let mut addr_port = Port::<u8>::new(CMOS_ADDR);
    let mut data_port = Port::<u8>::new(CMOS_DATA);
    unsafe {
        while (addr_port.read() & 0x80) != 0 {}
    }
    RtcTime {
        second: bcd_to_bin(cmos_read(0x00)),
        minute: bcd_to_bin(cmos_read(0x02)),
        hour: bcd_to_bin(cmos_read(0x04)),
        day: bcd_to_bin(cmos_read(0x07)),
        month: bcd_to_bin(cmos_read(0x08)),
        year: bcd_to_bin(cmos_read(0x09)),
    }
}

pub fn init() {
    let t = read();
    crate::kprintln!("[Kian] RTC: {:02}/{:02}/20{:02} {:02}:{:02}:{:02} (UTC from hardware)",
        t.month, t.day, t.year, t.hour, t.minute, t.second);
}
