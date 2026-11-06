use crate::kprintln;

pub mod keyboard;
pub mod ps2_mouse;
pub mod svga;
pub mod vmmouse;
pub mod ahci;
pub mod nvme;
pub mod lsi_scsi;
pub mod e1000;
pub mod e1000e;
pub mod vmxnet3;
pub mod rtc;
pub mod hpet;
pub mod pit;
pub mod apic;
pub mod rtl8139;
pub mod virtio_net;

pub fn init() {
    kprintln!("[Kian] Driver framework initialized");
    keyboard::init();
    ps2_mouse::init();
    svga::init();
    vmmouse::init();
    e1000::init();
    e1000e::init();
    vmxnet3::init();
    rtl8139::init();
    virtio_net::init();
    crate::kprintln!("[Kian] Core drivers: VGA, PS/2 keyboard+mouse loaded");
}
