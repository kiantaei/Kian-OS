pub mod kianfs;
pub mod fat32;

use crate::kprintln;
use crate::vfs;

pub fn init() {
    kprintln!("[Kian] Filesystems registered: KianFS, FAT32, ramdisk/initramfs");
    // Mount initramfs as root
    vfs::mount("/", crate::vfs::FsType::Ramdisk, 0, 0);
}
