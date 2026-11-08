use crate::kprintln;
use alloc::vec::Vec;
use core::convert::TryInto;

#[repr(C, packed)]
pub struct EthernetHeader {
    pub dst_mac: [u8; 6],
    pub src_mac: [u8; 6],
    pub ethertype: u16,
}

pub const ETHERTYPE_ARP: u16 = 0x0806;
pub const ETHERTYPE_IPV4: u16 = 0x0800;
pub const ETHERTYPE_IPV6: u16 = 0x86DD;

pub const BROADCAST_MAC: [u8; 6] = [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF];

pub fn ethertype_to_string(t: u16) -> &'static str {
    match t {
        ETHERTYPE_ARP => "ARP",
        ETHERTYPE_IPV4 => "IPv4",
        ETHERTYPE_IPV6 => "IPv6",
        _ => "Unknown",
    }
}

pub fn init() {
    kprintln!("[Kian] Ethernet layer initialized");
}

pub fn build_frame(src_mac: [u8; 6], dst_mac: [u8; 6], ethertype: u16, payload: &[u8]) -> Vec<u8> {
    let mut frame = Vec::with_capacity(14 + payload.len());
    frame.extend_from_slice(&dst_mac);
    frame.extend_from_slice(&src_mac);
    frame.extend_from_slice(&ethertype.to_be_bytes());
    frame.extend_from_slice(payload);
    frame
}

pub fn parse_header(buf: &[u8]) -> Option<EthernetHeader> {
    if buf.len() < 14 {
        return None;
    }
    Some(EthernetHeader {
        dst_mac: buf[0..6].try_into().unwrap(),
        src_mac: buf[6..12].try_into().unwrap(),
        ethertype: u16::from_be_bytes([buf[12], buf[13]]),
    })
}
