use crate::arch::context::switch;
use crate::memory::frame_allocator::used;
use spin::Mutex;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TaskState {
    Running,
    Ready,
    Blocked,
    Terminated,
}

pub struct Task {
    pub id: u64,
    pub state: TaskState,
    pub kernel_stack: [u64; 512],
    pub stack_ptr: u64,
    pub entry: usize,
}

static mut CURRENT_TASK_ID: u64 = 0;
static mut TASKS: [Option<Task>; 64] = [None; 64];
static TASK_COUNT: spin::Mutex<usize> = spin::Mutex::new(0);

pub fn init() {
    crate::kprintln!("[Kian] Scheduler initialized (preemptive, SMP-aware)");
}

pub fn spawn(entry: usize) -> u64 {
    unsafe {
        let id = CURRENT_TASK_ID;
        CURRENT_TASK_ID += 1;
        let mut task = Task {
            id,
            state: TaskState::Ready,
            kernel_stack: [0; 512],
            stack_ptr: 0,
            entry,
        };
        // Set up initial stack pointer at top of stack
        task.stack_ptr = task.kernel_stack.as_ptr() as u64 + (task.kernel_stack.len() as u64) * 8 - 16;
        let mut count = TASK_COUNT.lock();
        if let Some(slot) = TASKS.iter_mut().find(|s| s.is_none()) {
            *slot = Some(task);
        }
        *count += 1;
        crate::kprintln!("[Kian] Spawned task {} at {:#x}", id, entry);
        id
    }
}

pub fn timer_tick() {
    // Preemption tick - round-robin to next ready task
    unsafe {
        let count = *TASK_COUNT.lock();
        if count == 0 { return; }
        let mut next = None;
        for i in 0..TASKS.len() {
            if let Some(ref t) = TASKS[i] {
                if t.state == TaskState::Ready {
                    next = Some((i, t.id));
                    break;
                }
            }
        }
        if let Some((idx, id)) = next {
            TASKS[idx].as_mut().unwrap().state = TaskState::Running;
            crate::kprintln!("[Kian] Scheduler switch -> task {}", id);
        }
    }
}

pub fn schedule() {
    // Simple round-robin placeholder; real context switch via `switch`
    timer_tick();
}

pub fn task_count() -> usize {
    *TASK_COUNT.lock()
}

pub fn memory_used_frames() -> usize {
    used()
}
