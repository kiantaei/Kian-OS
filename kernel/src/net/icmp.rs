use crate::kprintln;
use crate::net::ipv4;
use crate::net::send_raw;
use crate::net::ethernet;

#[repr(C, packed)]
pub struct IcmpHeader {
    pub r#type: u8,
    pub code: u8,
    pub checksum: u16,
    pub rest: [u8; 4],
}

pub const ICMP_ECHO_REPLY: u8 = 0;
pub const ICMP_ECHO_REQUEST: u8 = 8;

pub fn init() {
    kprintln!("[Kian] ICMP layer initialized");
}

pub fn handle_packet(src_ip: [u8; 4], dst_ip: [u8; 4], payload: &[u8]) {
    if payload.len() < 4 {
        return;
    }
    let r#type = payload[0];
    let code = payload[1];

    match r#type {
        ICMP_ECHO_REQUEST => {
            // Respond with echo reply.
            let mut reply = payload.to_vec();
            reply[0] = ICMP_ECHO_REPLY;
            reply[1] = 0;
            reply[2..4].copy_from_slice(&[0, 0]); // checksum recompute
            let csum = ipv4::checksum(&reply);
            reply[2..4].copy_from_slice(&csum.to_be_bytes());
            ipv4::send_packet(src_ip, ipv4::PROTO_ICMP, &reply);
        }
        ICMP_ECHO_REPLY => {
            kprintln!("[Kian] ICMP echo reply from {}.{}.{}.{}", src_ip[0], src_ip[1], src_ip[2], src_ip[3]);
        }
        _ => {}
    }
}

pub fn ping(dst_ip: [u8; 4]) {
    let mut pkt = [0u8; 64];
    pkt[0] = ICMP_ECHO_REQUEST;
    pkt[1] = 0;
    pkt[2..4].copy_from_slice(&[0, 0]);
    pkt[4..8].copy_from_slice(&[0, 0, 0, 1]);
    let csum = ipv4::checksum(&pkt);
    pkt[2..4].copy_from_slice(&csum.to_be_bytes());
    ipv4::send_packet(dst_ip, ipv4::PROTO_ICMP, &pkt);
    kprintln!("[Kian] Ping sent to {}.{}.{}.{}", dst_ip[0], dst_ip[1], dst_ip[2], dst_ip[3]);
}
