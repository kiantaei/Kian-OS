use crate::kprintln;

pub fn init() {
    kprintln!("[Kian] DNS resolver initialized (cache enabled)");
}

static mut DNS_CACHE: Option<[DnsEntry; 16]> = None;
static mut DNS_COUNT: usize = 0;

pub struct DnsEntry {
    pub name: [u8; 64],
    pub name_len: usize,
    pub ip: [u8; 4],
    pub ttl: u32,
}

pub fn resolve(name: &str) -> Option<[u8; 4]> {
    // Check cache first.
    let cache = dns_cache();
    let count = dns_count();
    let name_bytes = name.as_bytes();
    for i in 0..*count {
        if cache[i].name_len == name_bytes.len()
            && cache[i].name[..name_bytes.len()] == name_bytes[..]
        {
            return Some(cache[i].ip);
        }
    }
    // Send DNS query (UDP 53).
    let query = build_query(name);
    crate::net::udp::send([8, 8, 8, 8], 12345, 53, &query);
    None
}

pub fn handle_response(body: &[u8]) {
    if body.len() < 12 {
        return;
    }
    let mut i = 12;
    while i < body.len() {
        let len = body[i] as usize;
        if len == 0 { i += 1; break; }
        i += 1 + len;
    }
    i += 4;
    if i + 10 < body.len() {
        let ip = [body[i + 8], body[i + 9], body[i + 10], body[i + 11]];
        kprintln!("[Kian] DNS resolved -> {}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]);
    }
}

fn build_query(name: &str) -> Vec<u8> {
    let mut pkt = Vec::new();
    pkt.extend_from_slice(&[0, 1]);
    pkt.extend_from_slice(&[0x01, 0x00]);
    pkt.extend_from_slice(&[0, 1]);
    pkt.extend_from_slice(&[0, 0]);
    pkt.extend_from_slice(&[0, 0]);
    pkt.extend_from_slice(&[0, 0]);
    for label in name.split('.') {
        pkt.push(label.len() as u8);
        pkt.extend_from_slice(label.as_bytes());
    }
    pkt.push(0);
    pkt.extend_from_slice(&[0, 1]);
    pkt.extend_from_slice(&[0, 1]);
    pkt
}

fn dns_cache() -> &'static mut [DnsEntry; 16] {
    unsafe { DNS_CACHE.as_mut().unwrap() }
}

fn dns_count() -> &'static mut usize {
    unsafe { &mut DNS_COUNT }
}
