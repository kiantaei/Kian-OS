use crate::kprintln;
use alloc::vec::Vec;
use crate::net::ipv4;

#[repr(C, packed)]
pub struct UdpHeader {
    pub src_port: u16,
    pub dst_port: u16,
    pub len: u16,
    pub checksum: u16,
}

pub fn init() {
    kprintln!("[Kian] UDP layer initialized");
}

pub fn handle_packet(src_ip: [u8; 4], dst_ip: [u8; 4], payload: &[u8]) {
    if payload.len() < 8 {
        return;
    }
    let src_port = u16::from_be_bytes([payload[0], payload[1]]);
    let dst_port = u16::from_be_bytes([payload[2], payload[3]]);
    let len = u16::from_be_bytes([payload[4], payload[5]]) as usize;
    let body = &payload[8..len.min(payload.len())];

    kprintln!("[Kian] UDP packet {}.{}.{}.{} len={}",
        src_ip[0], src_ip[1], src_ip[2], src_ip[3], len);

    match dst_port {
        68 => dhcp::handle_offer(body),
        53 => dns::handle_response(body),
        _ => {}
    }
}

pub fn send(dst_ip: [u8; 4], src_port: u16, dst_port: u16, payload: &[u8]) {
    let len: u16 = (8 + payload.len()) as u16;
    let mut header = [0u8; 8];
    header[0..2].copy_from_slice(&src_port.to_be_bytes());
    header[2..4].copy_from_slice(&dst_port.to_be_bytes());
    header[4..6].copy_from_slice(&len.to_be_bytes());
    header[6..8].copy_from_slice(&[0, 0]);

    let mut packet = Vec::with_capacity(8 + payload.len());
    packet.extend_from_slice(&header);
    packet.extend_from_slice(payload);

    ipv4::send_packet(dst_ip, ipv4::PROTO_UDP, &packet);
}

mod dhcp {
    use super::*;
    pub fn handle_offer(_body: &[u8]) {
        // DHCP offer handling - stub; full client below.
    }
}

mod dns {
    use super::*;
    pub fn handle_response(_body: &[u8]) {
        // DNS response handling - stub; full resolver below.
    }
}
