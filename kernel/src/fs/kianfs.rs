use crate::kprintln;

pub struct Superblock {
    pub magic: u32,
    pub block_size: u32,
    pub block_count: u32,
    pub inode_count: u32,
}

const KIANFS_MAGIC: u32 = 0x4B49414E; // "KIAN"

pub fn init() {
    kprintln!("[Kian] KianFS driver loaded");
}

pub fn format_magic() -> u32 {
    KIANFS_MAGIC
}
