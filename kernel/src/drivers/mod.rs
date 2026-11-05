use crate::kprintln;

pub mod vga;
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

pub fn init() {
    kprintln!("[Kian] Driver framework initialized");
    vga::init();
    keyboard::init();
    crate::kprintln!("[Kian] Core drivers: VGA, PS/2 keyboard loaded");
}
