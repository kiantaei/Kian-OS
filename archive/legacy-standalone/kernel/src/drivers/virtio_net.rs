use crate::kprintln;
use alloc::vec::Vec;
use crate::net::send_raw;
use crate::net::ethernet;

pub fn probe() -> bool {
    kprintln!("[Kian] virtio-net probe (stub)");
    false
}

pub fn init() {
    kprintln!("[Kian] virtio-net driver initialized");
}

pub fn link_up() -> bool {
    false
}

pub fn send(_ethertype: u16, _dst_mac: [u8; 6], _payload: &[u8]) -> bool {
    false
}

pub fn recv() -> Option<(u16, Vec<u8>)> {
    None
}
