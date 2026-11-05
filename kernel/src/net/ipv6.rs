use crate::kprintln;

pub fn init() {
    kprintln!("[Kian] IPv6 stub initialized");
}

pub fn handle_packet(_buf: &[u8]) {
    kprintln!("[Kian] IPv6 packet received (stub, not yet implemented)");
}
