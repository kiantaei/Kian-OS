use crate::kprintln;
use alloc::vec::Vec;
use crate::net::ethernet;
use crate::net::arp;
use crate::net::icmp;
use crate::net::udp;
use crate::net::tcp;
use crate::net::send_raw;

#[repr(C, packed)]
pub struct Ipv4Header {
    pub ver_ihl: u8,
    pub dscp_ecn: u8,
    pub total_len: u16,
    pub identification: u16,
    pub flags_frag: u16,
    pub ttl: u8,
    pub protocol: u8,
    pub checksum: u16,
    pub src_ip: [u8; 4],
    pub dst_ip: [u8; 4],
}

pub const PROTO_ICMP: u8 = 1;
pub const PROTO_TCP: u8 = 6;
pub const PROTO_UDP: u8 = 17;

static mut OUR_IP: Option<[u8; 4]> = None;
static mut OUR_NETMASK: Option<[u8; 4]> = None;
static mut OUR_GATEWAY: Option<[u8; 4]> = None;

pub fn init() {
    kprintln!("[Kian] IPv4 layer initialized");
}

pub fn our_ip() -> [u8; 4] {
    unsafe { OUR_IP.unwrap_or([0, 0, 0, 0]) }
}

pub fn set_our_ip(ip: [u8; 4]) {
    unsafe { OUR_IP = Some(ip); }
}

pub fn our_netmask() -> [u8; 4] {
    unsafe { OUR_NETMASK.unwrap_or([255, 255, 255, 0]) }
}

pub fn our_gateway() -> [u8; 4] {
    unsafe { OUR_GATEWAY.unwrap_or([0, 0, 0, 0]) }
}

pub fn set_gateway(gw: [u8; 4]) {
    unsafe { OUR_GATEWAY = Some(gw); }
}

pub fn checksum(buf: &[u8]) -> u16 {
    let mut sum: u32 = 0;
    let mut i = 0;
    while i + 1 < buf.len() {
        sum += u16::from_be_bytes([buf[i], buf[i + 1]]) as u32;
        i += 2;
    }
    if i < buf.len() {
        sum += (buf[i] as u32) << 8;
    }
    while sum >> 16 != 0 {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    !(sum as u16) as u16
}

pub fn handle_packet(buf: &[u8]) {
    if buf.len() < 20 {
        return;
    }
    let ver_ihl = buf[0];
    let version = ver_ihl >> 4;
    let ihl = (ver_ihl & 0x0F) * 4;
    if version != 4 || buf.len() < ihl as usize {
        return;
    }
    let total_len = u16::from_be_bytes([buf[2], buf[3]]) as usize;
    let protocol = buf[9];
    let src_ip = buf[12..16].try_into().unwrap();
    let dst_ip = buf[16..20].try_into().unwrap();

    let my_ip = our_ip();
    if dst_ip != my_ip && dst_ip != [255, 255, 255, 255] {
        return; // Not for us.
    }

    let payload = &buf[ihl as usize..total_len];

    match protocol {
        PROTO_ICMP => icmp::handle_packet(src_ip, dst_ip, payload),
        PROTO_UDP => udp::handle_packet(src_ip, dst_ip, payload),
        PROTO_TCP => tcp::handle_packet(src_ip, dst_ip, payload),
        _ => {}
    }
}

pub fn send_packet(dst_ip: [u8; 4], protocol: u8, payload: &[u8]) -> bool {
    let ihl: u8 = 20;
    let ver_ihl: u8 = 0x45;
    let total_len: u16 = (20 + payload.len()) as u16;
    let mut header = [0u8; 20];
    header[0] = ver_ihl;
    header[1] = 0;
    header[2..4].copy_from_slice(&total_len.to_be_bytes());
    header[4..6].copy_from_slice(&[0, 0]);
    header[6..8].copy_from_slice(&[0, 0]);
    header[8] = 64; // TTL
    header[9] = protocol;
    header[10..12].copy_from_slice(&[0, 0]); // checksum placeholder
    header[12..16].copy_from_slice(&our_ip());
    header[16..20].copy_from_slice(&dst_ip);
    header[10..12].copy_from_slice(&checksum(&header).to_be_bytes());

    let mut packet = Vec::with_capacity(20 + payload.len());
    packet.extend_from_slice(&header);
    packet.extend_from_slice(payload);

    // Resolve MAC via ARP.
    let gw = if dst_ip == [255, 255, 255, 255] {
        dst_ip
    } else {
        our_gateway()
    };
    let target_ip = if gw == [0, 0, 0, 0] { dst_ip } else { gw };
    arp::request(&target_ip);
    if let Some(mac) = arp::lookup(&target_ip) {
        let eth_frame = ethernet::build_frame(
            [0x52, 0x54, 0x00, 0x12, 0x34, 0x56],
            mac,
            ethernet::ETHERTYPE_IPV4,
            &packet,
        );
        send_raw("eth0", ethernet::ETHERTYPE_IPV4, mac, &eth_frame)
    } else {
        false
    }
}
