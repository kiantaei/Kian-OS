use crate::kprintln;

pub struct User {
    pub name: &'static str,
    pub uid: u32,
    pub gid: u32,
    pub password_hash: &'static str,
}

static USERS: [User; 2] = [
    User { name: "root", uid: 0, gid: 0, password_hash: "" },
    User { name: "kian", uid: 1000, gid: 1000, password_hash: "" },
];

pub fn init() {
    kprintln!("[Kian] Accounts: root (uid 0), kian (uid 1000)");
}

pub fn authenticate(name: &str, password: &str) -> bool {
    for u in USERS.iter() {
        if u.name == name {
            // In real system, compare hash(password) with stored hash.
            // Default password for kian: "kian"
            return name == "kian" && password == "kian";
        }
    }
    false
}

pub fn get_user(name: &str) -> Option<&'static User> {
    USERS.iter().find(|u| u.name == name)
}

pub fn current_uid() -> u32 {
    1000
}

pub fn is_root() -> bool {
    current_uid() == 0
}
