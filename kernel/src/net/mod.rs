use crate::kprintln;
use crate::drivers::e1000;
use crate::drivers::e1000e;
use crate::drivers::vmxnet3;
use crate::drivers::rtl8139;

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

    // Probe and init network drivers in priority order for VMware NAT.
    if e1000e::probe() {
        e1000e::init();
        kprintln!("[Kian] Intel e1000e NIC detected and initialized");
    } else if e1000::probe() {
        e1000::init();
        kprintln!("[Kian] Intel e1000 NIC detected and initialized");
    } else if vmxnet3::probe() {
        vmxnet3::init();
        kprintln!("[Kian] VMware vmxnet3 NIC detected and initialized");
    } else if rtl8139::probe() {
        rtl8139::init();
        kprintln!("[Kian] Realtek RTL8139/8169 NIC detected and initialized");
    } else if virtio_net::probe() {
        virtio_net::init();
        kprintln!("[Kian] virtio-net NIC detected and initialized");
    } else {
        kprintln!("[Kian] No Ethernet NIC detected; network disabled");
    }

    // Initialize protocol layers.
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
    match iface {
        "eth0" => {
            if e1000e::link_up() { e1000e::send(ethertype, dst_mac, payload) }
            else if e1000::link_up() { e1000::send(ethertype, dst_mac, payload) }
            else if vmxnet3::link_up() { vmxnet3::send(ethertype, dst_mac, payload) }
            else if rtl8139::link_up() { rtl8139::send(ethertype, dst_mac, payload) }
            else if virtio_net::link_up() { virtio_net::send(ethertype, dst_mac, payload) }
            else { false }
        }
        _ => false,
    }
}

pub fn recv_raw() -> Option<(u16, Vec<u8>)> {
    if e1000e::link_up() { e1000e::recv() }
    else if e1000::link_up() { e1000::recv() }
    else if vmxnet3::link_up() { vmxnet3::recv() }
    else if rtl8139::link_up() { rtl8139::recv() }
    else if virtio_net::link_up() { virtio_net::recv() }
    else { None }
}
