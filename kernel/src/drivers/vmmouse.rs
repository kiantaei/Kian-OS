use crate::kprintln;

pub fn init() {
    kprintln!("[Kian] VMware SVGA II / absolute-pointer (vmmouse) stub loaded");
}

pub fn read_mouse() -> (i32, i32, i32) {
    (0, 0, 0)
}
