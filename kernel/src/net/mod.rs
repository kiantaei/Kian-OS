use crate::kprintln;

pub mod ethernet;
pub mod arp;
pub mod ipv4;
pub mod icmp;
pub mod udp;
pub mod tcp;
pub mod dhcp;
pub mod dns;
pub mod ipv6;

pub struct NetStack {
    pub initialized: bool,
    pub default_iface: Option<&'static str>,
}

static mut NET_STACK: Option<NetStack> = None;

pub fn init() {
    kprintln!("[Kian] Initializing TCP/IP stack (Ethernet, ARP, IPv4, ICMP, UDP, DHCP, DNS)");

    // Network drivers are initialized by drivers::init(); protocol layers start here.
    arp::init();
    ipv4::init();
    icmp::init();
    udp::init();
    tcp::init();

    // Start DHCP client on default interface.
    dhcp::start();

    unsafe {
        NET_STACK = Some(NetStack {
            initialized: true,
            default_iface: Some("eth0"),
        });
    }

    kprintln!("[Kian] TCP/IP stack initialized");
}

pub fn stack() -> &'static mut NetStack {
    unsafe { NET_STACK.as_mut().unwrap() }
}

pub fn send_raw(iface: &str, ethertype: u16, dst_mac: [u8; 6], payload: &[u8]) -> bool {
    let _ = (iface, ethertype, dst_mac, payload);
    false
}

pub fn recv_raw() -> Option<(u16, Vec<u8>)> {
    None
}
