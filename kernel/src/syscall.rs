use crate::kprintln;

pub const SYSCALL_WRITE: u64 = 1;
pub const SYSCALL_EXIT: u64 = 2;
pub const SYSCALL_GETPID: u64 = 3;
pub const SYSCALL_TIME: u64 = 4;
pub const SYSCALL_YIELD: u64 = 5;

pub fn init() {
    kprintln!("[Kian] Syscall interface initialized (ring 3 -> ring 0 via syscall instruction)");
}

pub fn handle(syscall_num: u64, arg1: u64, arg2: u64, arg3: u64) -> u64 {
    match syscall_num {
        SYSCALL_WRITE => {
            let _ = (arg1, arg2, arg3);
            0
        }
        SYSCALL_EXIT => {
            let _ = arg1;
            0
        }
        SYSCALL_GETPID => 1,
        SYSCALL_TIME => {
            let _ = (arg1, arg2, arg3);
            0
        }
        SYSCALL_YIELD => {
            crate::scheduler::schedule();
            0
        }
        _ => {
            kprintln!("[Kian] Unknown syscall: {}", syscall_num);
            0xFFFFFFFFFFFFFFFF
        }
    }
}

#[no_mangle]
pub extern "C" fn syscall_handler(num: u64, a1: u64, a2: u64, a3: u64) -> u64 {
    handle(num, a1, a2, a3)
}
