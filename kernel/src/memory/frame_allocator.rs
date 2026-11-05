use x86_64::structures::paging::{PhysFrame, Size4KiB, FrameAllocator};
use x86_64::{PhysAddr};
use core::ops::Range;

const FRAME_SIZE: u64 = 4096;

// Simple bump frame allocator over a reserved physical range (1MB..64MB).
// Production parses the Limine memory map.
pub struct BumpFrameAllocator {
    next: PhysAddr,
    end: PhysAddr,
    used: usize,
    total: usize,
}

static mut FRAME_ALLOCATOR: Option<BumpFrameAllocator> = None;

pub fn init() {
    let start = PhysAddr::new(0x100000);
    let end = PhysAddr::new(0x4000000); // 64MB
    let total = ((end.as_u64() - start.as_u64()) / FRAME_SIZE) as usize;
    unsafe {
        FRAME_ALLOCATOR = Some(BumpFrameAllocator { next: start, end, used: 0, total });
    }
    crate::kprintln!("[Kian] PMM: managing {:.1} MB ({} frames) at 1MB..64MB",
        total as f64 * FRAME_SIZE as f64 / 1024.0 / 1024.0, total);
}

pub fn alloc_frame() -> Option<PhysFrame> {
    let allocator = unsafe { FRAME_ALLOCATOR.as_mut()? };
    if allocator.next.as_u64() + FRAME_SIZE > allocator.end.as_u64() {
        return None;
    }
    let frame = PhysFrame::containing_address(allocator.next);
    allocator.next = PhysAddr::new(allocator.next.as_u64() + FRAME_SIZE);
    allocator.used += 1;
    Some(frame)
}

pub fn used() -> usize {
    unsafe { FRAME_ALLOCATOR.as_ref().map(|a| a.used).unwrap_or(0) }
}
pub fn total() -> usize {
    unsafe { FRAME_ALLOCATOR.as_ref().map(|a| a.total).unwrap_or(0) }
}

pub struct GlobalFrameAllocator;

unsafe impl FrameAllocator<Size4KiB> for GlobalFrameAllocator {
    fn allocate_frame(&mut self) -> Option<PhysFrame> {
        alloc_frame()
    }
}
