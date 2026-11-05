use crate::kprintln;
use alloc::string::String;
use alloc::vec::Vec;
use spin::Mutex;

pub struct Vnode {
    pub name: String,
    pub fs_type: FsType,
    pub inode: u64,
    pub size: u64,
    pub readonly: bool,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FsType {
    KianFS,
    FAT32,
    Ramdisk,
    Tmpfs,
}

static MOUNTS: Mutex<Vec<Vnode>> = Mutex::new(Vec::new());

pub fn init() {
    kprintln!("[Kian] VFS initialized");
}

pub fn mount(name: &str, fs_type: FsType, inode: u64, size: u64) {
    MOUNTS.lock().push(Vnode {
        name: name.to_string(),
        fs_type,
        inode,
        size,
        readonly: false,
    });
    kprintln!("[Kian] VFS mounted {} as {:?}", name, fs_type);
}

pub fn lookup(path: &str) -> Option<Vnode> {
    let mounts = MOUNTS.lock();
    for m in mounts.iter() {
        if m.name == path || path.starts_with(&m.name) {
            return Some(m.clone());
        }
    }
    None
}

pub fn read(vnode: &Vnode, offset: u64, buf: &mut [u8]) -> usize {
    let _ = (vnode, offset, buf);
    0
}

pub fn write(vnode: &Vnode, offset: u64, buf: &[u8]) -> usize {
    let _ = (vnode, offset, buf);
    buf.len()
}
