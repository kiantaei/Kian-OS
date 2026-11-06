use core::alloc::{GlobalAlloc, Layout};
use alloc::format;
use spin::Mutex;

/* Free-list heap allocator over ONE region chosen at boot from the Limine
 * memory map (virtual address = physical base + hhdm_offset).
 *
 * Block layout (16-byte aligned block starts):
 *   [size: u64][meta: u64][payload ...]
 *   size = total block size including header
 *   meta = bit0: used flag, bits 8..23: payload offset (align_up(16, align))
 *   payload offset > 16 additionally stores a footer word at payload-8 with
 *   the same meta value so dealloc can find the block for any alignment.
 * Free coalescing merges every adjacent free pair after each free. */

const HDR_SIZE: usize = 16;
const MIN_SPLIT: usize = 32;
const MAX_ALIGN: usize = 4096;

pub struct HeapState {
    base: usize,
    end: usize,
    ready: bool,
}

impl HeapState {
    const fn new() -> Self {
        HeapState { base: 0, end: 0, ready: false }
    }

    #[inline]
    unsafe fn read_u64(p: usize) -> u64 {
        (p as *const u64).read_volatile()
    }

    #[inline]
    unsafe fn write_u64(p: usize, v: u64) {
        (p as *mut u64).write_volatile(v)
    }

    #[inline]
    unsafe fn block_size(p: usize) -> usize {
        Self::read_u64(p) as usize
    }

    #[inline]
    unsafe fn block_meta(p: usize) -> u64 {
        Self::read_u64(p + 8)
    }

    #[inline]
    unsafe fn set_block(p: usize, size: usize, meta: u64) {
        Self::write_u64(p, size as u64);
        Self::write_u64(p + 8, meta);
    }

    unsafe fn init(&mut self, base: usize, size: usize) {
        let size = size & !0xF;
        self.base = base;
        self.end = base + size;
        if size >= MIN_SPLIT {
            // one big free block covers the whole region
            Self::set_block(base, size, 0);
        }
        self.ready = true;
    }

    unsafe fn alloc(&mut self, layout: Layout) -> *mut u8 {
        if !self.ready || self.base == 0 {
            return core::ptr::null_mut();
        }
        if layout.align() > MAX_ALIGN || !layout.align().is_power_of_two() {
            return core::ptr::null_mut();
        }
        let align = layout.align().max(8);
        let size = layout.size().max(1);
        // payload offset: header must not overlap the aligned payload
        let pay_off = ((HDR_SIZE + align - 1) / align) * align;
        let need = pay_off + size;

        let mut cur = self.base;
        while cur + HDR_SIZE <= self.end {
            let bsize = Self::block_size(cur);
            if bsize < HDR_SIZE || cur + bsize > self.end {
                return core::ptr::null_mut(); // corrupted header, give up
            }
            let meta = Self::block_meta(cur);
            if meta & 1 == 0 && bsize >= need {
                if bsize - need >= MIN_SPLIT {
                    // split: remainder block keeps payload offset 16
                    Self::set_block(cur + need, bsize - need, 0);
                    Self::set_block(cur, need, ((pay_off as u64) << 8) | 1);
                } else {
                    Self::set_block(cur, bsize, ((pay_off as u64) << 8) | 1);
                }
                if pay_off > HDR_SIZE {
                    // footer so dealloc can find this block
                    Self::write_u64(cur + pay_off - 8, ((pay_off as u64) << 8) | 1);
                }
                return (cur + pay_off) as *mut u8;
            }
            cur += bsize;
        }
        core::ptr::null_mut()
    }

    unsafe fn free(&mut self, ptr: *mut u8) {
        if !self.ready || ptr.is_null() || self.base == 0 {
            return;
        }
        let p = ptr as usize;
        if p < self.base + HDR_SIZE || p > self.end {
            return;
        }
        let word = Self::read_u64(p - 8); // meta word (payload offset 16) or footer
        let pay_off = ((word >> 8) & 0xFFFF) as usize;
        if pay_off < HDR_SIZE || pay_off > 0x10000 {
            return;
        }
        let block = p - pay_off;
        if block < self.base {
            return;
        }
        let meta = word & !1;
        Self::write_u64(block + 8, meta);
        if pay_off > HDR_SIZE {
            Self::write_u64(p - 8, meta);
        }

        // coalesce every adjacent free pair
        let mut cur = self.base;
        while cur + HDR_SIZE <= self.end {
            let bsize = Self::block_size(cur) as usize;
            if bsize < HDR_SIZE || cur + bsize > self.end {
                return;
            }
            if Self::block_meta(cur) & 1 == 0 {
                let next = cur + bsize;
                if next + HDR_SIZE <= self.end && Self::block_meta(next) & 1 == 0 {
                    let nsize = Self::block_size(next) as usize;
                    if nsize >= HDR_SIZE && next + nsize <= self.end {
                        Self::set_block(cur, bsize + nsize, Self::block_meta(cur) & !1);
                        continue; // try to merge further
                    }
                }
            }
            cur += bsize;
        }
    }
}

struct KianHeapAllocator;

static HEAP: Mutex<HeapState> = Mutex::new(HeapState::new());

#[global_allocator]
static ALLOCATOR: KianHeapAllocator = KianHeapAllocator;

unsafe impl GlobalAlloc for KianHeapAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        unsafe { HEAP.lock().alloc(layout) }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        unsafe { HEAP.lock().free(ptr) }
    }
}

/// Called from _start once the boot info is available.
pub fn init(base: usize, size: usize) {
    unsafe { HEAP.lock().init(base, size) }
}

/// ABI: void *kian_alloc(size_t size, size_t align)
pub fn kian_alloc(size: usize, align: usize) -> *mut u8 {
    if size == 0 || align == 0 || align > MAX_ALIGN || !align.is_power_of_two() {
        return core::ptr::null_mut();
    }
    match Layout::from_size_align(size, align) {
        Ok(l) => unsafe { HEAP.lock().alloc(l) },
        Err(_) => core::ptr::null_mut(),
    }
}

/// ABI: void kian_free(void *p)
pub fn kian_free(p: *mut u8) {
    unsafe { HEAP.lock().free(p) }
}

#[alloc_error_handler]
fn alloc_error_handler(layout: core::alloc::Layout) -> ! {
    crate::panic::halt(&format!("heap allocation error: {:?}", layout));
}
