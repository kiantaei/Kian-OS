use crate::kprintln;

pub mod auth;
pub mod hash;

pub fn init() {
    kprintln!("[Kian] Security subsystem initialized (permissions, hashed passwords)");
    auth::init();
}
