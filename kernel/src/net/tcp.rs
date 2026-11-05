use crate::kprintln;
use crate::net::ipv4;

pub fn init() {
    kprintln!("[Kian] TCP layer initialized");
}

pub fn handle_packet(src_ip: [u8; 4], dst_ip: [u8; 4], payload: &[u8]) {
    if payload.len() < 20 {
        return;
    }
    let src_port = u16::from_be_bytes([payload[0], payload[1]]);
    let dst_port = u16::from_be_bytes([payload[2], payload[3]]);
    let seq = u32::from_be_bytes([payload[4], payload[5], payload[6], payload[7]]);
    let ack = u32::from_be_bytes([payload[8], payload[9], payload[10], payload[11]]);
    let data_offset = ((payload[12] >> 4) & 0x0F) as usize * 4;
    let flags = payload[13];
    let body = &payload[data_offset..payload.len().min(payload.len())];

    kprintln!("[Kian] TCP {}:{} -> {}:{} seq={} ack={} flags=0x{:02x} body={}",
        src_ip[0], src_ip[1], src_ip[2], src_ip[3],
        src_port, dst_port, seq, ack, flags, body.len());

    // Handle SYN -> SYN-ACK
    if flags & 0x02 != 0 {
        let mut reply = [0u8; 20];
        reply[0..2].copy_from_slice(&dst_port.to_be_bytes());
        reply[2..4].copy_from_slice(&src_port.to_be_bytes());
        reply[4..8].copy_from_slice(&0u32.to_be_bytes());
        reply[8..12].copy_from_slice(&(seq.wrapping_add(1)).to_be_bytes());
        reply[12] = 0x50; // data offset 5
        reply[13] = 0x12; // SYN+ACK
        reply[14..16].copy_from_slice(&[0, 0]); // window
        reply[16..18].copy_from_slice(&[0, 0]); // checksum
        reply[18..20].copy_from_slice(&[0, 0]); // urgent
        ipv4::send_packet(src_ip, ipv4::PROTO_TCP, &reply);
    }
}
