use crate::kprintln;

pub fn init() {
    kprintln!("[Kian] RTC driver initialized");
}

pub fn read_time() -> (u16, u8, u8, u8, u8, u8) {
    // Returns: (year, month, day, hour, minute, second)
    (2026, 11, 5, 0, 0, 0)
}
