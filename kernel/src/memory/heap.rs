use core::alloc::{GlobalAlloc, Layout};
use alloc::format;
use spin::Mutex;

const HEAP_START: usize = 0x_4444_4444_0000;
const HEAP_SIZE: usize = 4 * 1024 * 1024;

struct BumpAllocator {
    next: usize,
    end: usize,
    initialized: bool,
}

impl BumpAllocator {
    const fn empty() -> Self {
        BumpAllocator { next: 0, end: 0, initialized: false }
    }

    unsafe fn init(&mut self, start: usize, size: usize) {
        self.next = start;
        self.end = start + size;
        self.initialized = true;
    }

    unsafe fn alloc(&mut self, layout: Layout) -> *mut u8 {
        if !self.initialized {
            return core::ptr::null_mut();
        }
        let align = layout.align();
        let size = layout.size();

        // Align next up.
        let aligned_next = (self.next + align - 1) & !(align - 1);
        let next = aligned_next.checked_add(size).expect("overflow");
        if next > self.end {
            return core::ptr::null_mut();
        }
        let ptr = aligned_next as *mut u8;
        self.next = next;
        ptr
    }

    unsafe fn dealloc(&mut self, _ptr: *mut u8, _layout: Layout) {
        // Bump allocator does not free.
    }
}

#[global_allocator]
static ALLOCATOR: Mutex<BumpAllocator> = Mutex::new(BumpAllocator::empty());

unsafe impl GlobalAlloc for Mutex<BumpAllocator> {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        self.lock().alloc(layout)
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        self.lock().dealloc(ptr, layout);
    }
}

pub fn init() {
    unsafe {
        ALLOCATOR.lock().init(HEAP_START, HEAP_SIZE);
    }
    crate::kprintln!("[Kian] Heap: {} bytes ({:.1} MiB) at {:#x}", HEAP_SIZE, HEAP_SIZE as f64 / 1024.0 / 1024.0, HEAP_START);
}

#[alloc_error_handler]
fn alloc_error_handler(layout: core::alloc::Layout) -> ! {
    crate::panic::halt(&format!("heap allocation error: {:?}", layout));
}
