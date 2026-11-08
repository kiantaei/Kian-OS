use crate::kprintln;
use alloc::vec::Vec;
use crate::net::ipv4;
use crate::net::udp;

pub struct DhcpState {
    pub server_ip: [u8; 4],
    pub offered_ip: [u8; 4],
    pub lease_seconds: u32,
}

static mut DHCP: Option<DhcpState> = None;

pub fn init() {
    kprintln!("[Kian] DHCP client initialized");
}

pub fn start() {
    kprintln!("[Kian] DHCP client: discovering...");
    discover();
}

pub fn discover() {
    let mut pkt = Vec::new();
    // BOOTP/DHCP Discover minimal
    pkt.push(1); // op
    pkt.push(1); // htype
    pkt.push(6); // hlen
    pkt.push(0); // hops
    pkt.extend_from_slice(&[0, 0, 0, 0]); // xid
    pkt.extend_from_slice(&[0, 0]); // secs
    pkt.extend_from_slice(&[0, 0]); // flags
    pkt.extend_from_slice(&[0, 0, 0, 0]); // ciaddr
    pkt.extend_from_slice(&[0, 0, 0, 0]); // yiaddr
    pkt.extend_from_slice(&[0, 0, 0, 0]); // siaddr
    pkt.extend_from_slice(&[0, 0, 0, 0]); // giaddr
    pkt.extend_from_slice(&[0u8; 16]); // chaddr
    pkt.extend_from_slice(&[0u8; 64]); // sname
    pkt.extend_from_slice(&[0u8; 128]); // file
    pkt.extend_from_slice(&[99, 130, 83, 99]); // magic cookie
    pkt.push(53); pkt.push(1); pkt.push(1); // DHCP Discover
    pkt.push(255); // end

    udp::send([255, 255, 255, 255], 68, 67, &pkt);
    kprintln!("[Kian] DHCP Discover sent");
}

pub fn handle_offer(body: &[u8]) {
    if body.len() < 16 {
        return;
    }
    let yiaddr: [u8; 4] = body[16..20].try_into().unwrap();
    kprintln!("[Kian] DHCP Offer: {}.{}.{}.{}", yiaddr[0], yiaddr[1], yiaddr[2], yiaddr[3]);
    ipv4::set_our_ip(yiaddr);
    unsafe { DHCP = Some(DhcpState { server_ip: [0,0,0,0], offered_ip: yiaddr, lease_seconds: 86400 }); }
    request(yiaddr);
}

pub fn request(requested: [u8; 4]) {
    let mut pkt = Vec::new();
    pkt.push(1); pkt.push(1); pkt.push(6); pkt.push(0);
    pkt.extend_from_slice(&[0, 0, 0, 0]);
    pkt.extend_from_slice(&[0, 0]); pkt.extend_from_slice(&[0, 0]);
    pkt.extend_from_slice(&[0, 0, 0, 0]);
    pkt.extend_from_slice(&[0, 0, 0, 0]);
    pkt.extend_from_slice(&[0, 0, 0, 0]);
    pkt.extend_from_slice(&[0, 0, 0, 0]);
    pkt.extend_from_slice(&[0u8; 16]);
    pkt.extend_from_slice(&[0u8; 64]);
    pkt.extend_from_slice(&[0u8; 128]);
    pkt.extend_from_slice(&[99, 130, 83, 99]);
    pkt.push(53); pkt.push(1); pkt.push(3); // DHCP Request
    pkt.push(50); pkt.push(4); pkt.extend_from_slice(&requested); // requested IP
    pkt.push(255);
    udp::send([255, 255, 255, 255], 68, 67, &pkt);
    kprintln!("[Kian] DHCP Request sent for {}.{}.{}.{}", requested[0], requested[1], requested[2], requested[3]);
    ipv4::set_gateway(yiaddr_gateway());
}

fn yiaddr_gateway() -> [u8; 4] {
    [192, 168, 1, 1] // typical VMware NAT gateway
}

pub fn has_lease() -> bool {
    unsafe { DHCP.is_some() }
}
