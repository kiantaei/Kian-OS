use crate::kprintln;
use alloc::vec::Vec;
use crate::net::ethernet;
use crate::net::ipv4;
use crate::net::send_raw;

#[repr(C, packed)]
pub struct ArpHeader {
    pub htype: u16,
    pub ptype: u16,
    pub hlen: u8,
    pub plen: u8,
    pub oper: u16,
    pub sha: [u8; 6],
    pub spa: [u8; 4],
    pub tha: [u8; 6],
    pub tpa: [u8; 4],
}

pub const ARP_HTYPE_ETHERNET: u16 = 1;
pub const ARP_PTYPE_IPV4: u16 = 0x0800;
pub const ARP_OP_REQUEST: u16 = 1;
pub const ARP_OP_REPLY: u16 = 2;

static mut ARP_TABLE: Option<[ArpEntry; 32]> = None;
static mut ARP_COUNT: usize = 0;

#[derive(Copy, Clone)]
pub struct ArpEntry {
    pub ip: [u8; 4],
    pub mac: [u8; 6],
    pub expires_at: u64,
}

pub fn init() {
    kprintln!("[Kian] ARP layer initialized");
    unsafe { ARP_TABLE = Some([ArpEntry { ip: [0;4], mac: [0;6], expires_at: 0 }; 32]); }
}

pub fn arp_table() -> &'static mut [ArpEntry; 32] {
    unsafe { ARP_TABLE.as_mut().unwrap() }
}

pub fn arp_count() -> &'static mut usize {
    unsafe { &mut ARP_COUNT }
}

pub fn lookup(ip: &[u8; 4]) -> Option<[u8; 6]> {
    let table = arp_table();
    let count = arp_count();
    let now = crate::drivers::pit::ticks();
    for i in 0..*count {
        if table[i].ip == *ip && table[i].expires_at > now {
            return Some(table[i].mac);
        }
    }
    None
}

pub fn insert(ip: &[u8; 4], mac: &[u8; 6]) {
    let table = arp_table();
    let count = arp_count();
    let now = crate::drivers::pit::ticks();
    // Update if exists.
    for i in 0..*count {
        if table[i].ip == *ip {
            table[i].mac = *mac;
            table[i].expires_at = now + 300; // 300 ticks TTL
            return;
        }
    }
    // Insert new.
    if *count < 32 {
        table[*count] = ArpEntry { ip: *ip, mac: *mac, expires_at: now + 300 };
        *count += 1;
    }
}

pub fn handle_packet(buf: &[u8]) {
    if buf.len() < 28 {
        return;
    }
    let htype = u16::from_be_bytes([buf[0], buf[1]]);
    let ptype = u16::from_be_bytes([buf[2], buf[3]]);
    let hlen = buf[4];
    let plen = buf[5];
    let oper = u16::from_be_bytes([buf[6], buf[7]]);

    if htype != ARP_HTYPE_ETHERNET || ptype != ARP_PTYPE_IPV4 || hlen != 6 || plen != 4 {
        return;
    }

    let sha: [u8; 6] = buf[8..14].try_into().unwrap();
    let spa: [u8; 4] = buf[14..18].try_into().unwrap();
    let tha: [u8; 6] = buf[18..24].try_into().unwrap();
    let tpa: [u8; 4] = buf[24..28].try_into().unwrap();

    // Learn sender.
    insert(&spa, &sha);

    if oper == ARP_OP_REQUEST {
        // We are being asked who has tpa. Reply if tpa is our IP.
        let my_ip = ipv4::our_ip();
        if tpa == my_ip {
            let my_mac = ethernet_build_our_mac();
            let reply = build_reply(sha, spa, my_mac, my_ip);
            let eth_frame = ethernet::build_frame(my_mac, sha, ethernet::ETHERTYPE_ARP, &reply);
            send_raw("eth0", ethernet::ETHERTYPE_ARP, sha, &eth_frame);
        }
    }
}

pub fn request(ip: &[u8; 4]) {
    if let Some(mac) = lookup(ip) {
        return; // Already cached.
    }
    let my_ip = ipv4::our_ip();
    let my_mac = ethernet_build_our_mac();
    let req = build_request(my_mac, my_ip, *ip);
    let eth_frame = ethernet::build_frame(my_mac, ethernet::BROADCAST_MAC, ethernet::ETHERTYPE_ARP, &req);
    send_raw("eth0", ethernet::ETHERTYPE_ARP, ethernet::BROADCAST_MAC, &eth_frame);
}

fn build_request(sha: [u8; 6], spa: [u8; 4], tpa: [u8; 4]) -> Vec<u8> {
    let mut pkt = Vec::new();
    pkt.extend_from_slice(&ARP_HTYPE_ETHERNET.to_be_bytes());
    pkt.extend_from_slice(&ARP_PTYPE_IPV4.to_be_bytes());
    pkt.push(6);
    pkt.push(4);
    pkt.extend_from_slice(&ARP_OP_REQUEST.to_be_bytes());
    pkt.extend_from_slice(&sha);
    pkt.extend_from_slice(&spa);
    pkt.extend_from_slice(&[0u8; 6]);
    pkt.extend_from_slice(&tpa);
    pkt
}

fn build_reply(tha: [u8; 6], tpa: [u8; 4], sha: [u8; 6], spa: [u8; 4]) -> Vec<u8> {
    let mut pkt = Vec::new();
    pkt.extend_from_slice(&ARP_HTYPE_ETHERNET.to_be_bytes());
    pkt.extend_from_slice(&ARP_PTYPE_IPV4.to_be_bytes());
    pkt.push(6);
    pkt.push(4);
    pkt.extend_from_slice(&ARP_OP_REPLY.to_be_bytes());
    pkt.extend_from_slice(&sha);
    pkt.extend_from_slice(&spa);
    pkt.extend_from_slice(&tha);
    pkt.extend_from_slice(&tpa);
    pkt
}

fn ethernet_build_our_mac() -> [u8; 6] {
    // Read from NIC; default fallback.
    [0x52, 0x54, 0x00, 0x12, 0x34, 0x56]
}
