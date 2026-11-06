use crate::scheduler;
use crate::memory;
use crate::kprintln;
use alloc::vec::Vec;

pub struct Process {
    pub pid: u32,
    pub name: &'static str,
    pub state: ProcessState,
    pub uid: u32,
    pub gid: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ProcessState {
    Running,
    Sleeping,
    Zombie,
}

static mut PROCS: [Option<Process>; 256] = [None; 256];
static PROC_COUNT: spin::Mutex<u32> = spin::Mutex::new(0);

pub fn init() {
    kprintln!("[Kian] Process manager initialized");
}

pub fn spawn_user(name: &'static str, entry: usize, uid: u32) -> u32 {
    unsafe {
        let pid = *PROC_COUNT.lock();
        *PROC_COUNT.lock() += 1;
        let proc = Process { pid, name, state: ProcessState::Running, uid, gid: 0 };
        let slot = PROCS.iter_mut().find(|s| s.is_none()).unwrap();
        *slot = Some(proc);
        let _ = scheduler::spawn(entry);
        kprintln!("[Kian] Process {} ({}) pid={} uid={}", pid, name, pid, uid);
        pid
    }
}

pub fn start_userspace() -> ! {
    kprintln!("[Kian] Starting userspace /sbin/init...");
    // Load /sbin/init from initramfs (VFS) and spawn as pid 1
    let pid = spawn_user("init", 0x100000, 0);
    kprintln!("[Kian] init spawned as pid {}", pid);
    // Wait forever (userspace runs on its own stack via scheduler)
    loop {
        crate::arch::x86_64::interrupts::init();
        unsafe { core::arch::asm!("hlt"); }
    }
}

pub fn list() -> Vec<(u32, &'static str)> {
    let mut out = Vec::new();
    unsafe {
        for p in PROCS.iter() {
            if let Some(ref proc) = p {
                out.push((proc.pid, proc.name));
            }
        }
    }
    out
}

pub fn kill(pid: u32) -> bool {
    unsafe {
        for p in PROCS.iter_mut() {
            if let Some(ref mut proc) = p {
                if proc.pid == pid {
                    proc.state = ProcessState::Zombie;
                    return true;
                }
            }
        }
    }
    false
}
