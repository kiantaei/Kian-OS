// Kian OS - Framebuffer driver (STEP 1)
// Requests a framebuffer from Limine and provides basic drawing.

use crate::kprintln;
use core::ptr;

/// Limine framebuffer request (16-byte ID + revision + response pointer).
/// The ID must match Limine's framebuffer request GUID (see limine.h).
/// Placed in a custom ELF section so Limine finds it at boot time.
#[repr(C, align(4))]
pub struct LimineFramebufferRequest {
    pub id: [u8; 16],
    pub revision: u32,
    pub response: *mut *mut LimineFramebufferResponse,
}

/// Limine framebuffer response (filled by bootloader).
#[repr(C)]
pub struct LimineFramebufferResponse {
    pub revision: u32,
    pub framebuffer_count: u64,
    pub framebuffers: *mut *mut LimineFramebuffer,
}

/// A single framebuffer described by Limine.
#[repr(C)]
pub struct LimineFramebuffer {
    pub addr: *mut u8,
    pub width: u64,
    pub height: u64,
    pub pitch: u64,
    pub bpp: u64,
    pub memory_model: u8,
    pub red_mask_size: u8,
    pub red_mask_shift: u8,
    pub green_mask_size: u8,
    pub green_mask_shift: u8,
    pub blue_mask_size: u8,
    pub blue_mask_shift: u8,
    pub _reserved: [u8; 7],
}

/// Our framebuffer descriptor.
pub struct Framebuffer {
    pub address: *mut u32,
    pub width: u32,
    pub height: u32,
    pub pitch: u32,
    pub bpp: u32,
    pub available: bool,
}

static mut FB: Option<Framebuffer> = None;

// Section markers for Limine request scanning.
extern "C" {
    static mut __limine_requests_start: u8;
    static mut __limine_requests_end: u8;
}

/// The Limine framebuffer request entry. Limine scans the .limine_requests
/// section and fills in the `response` pointer for matching IDs.
#[link_section = ".limine_requests"]
static mut FRAMEBUFFER_REQUEST_ENTRY: LimineFramebufferRequest = LimineFramebufferRequest {
    id: [0u8; 16], // 16 bytes — MUST be set to Limine's framebuffer request GUID (see limine.h)
    revision: 0,
    response: ptr::null_mut(),
};

static mut FB_RESPONSE_PTR: *mut *mut LimineFramebufferResponse = ptr::null_mut();

pub fn init() {
    kprintln!("[Kian GUI] Requesting framebuffer from Limine...");

    // Wire the request's response pointer to our storage.
    unsafe {
        FRAMEBUFFER_REQUEST_ENTRY.response = &mut FB_RESPONSE_PTR;
    }

    // After Limine has filled the response, check if we got a framebuffer.
    let fb_resp = unsafe { FB_RESPONSE_PTR.as_ref() };
    match fb_resp {
        Some(resp) if resp.framebuffer_count > 0 => {
            let fb_ptr = unsafe { resp.framebuffers.as_ref() };
            if let Some(fb) = fb_ptr {
                let fb = unsafe { &*fb };
                let addr = fb.addr as *mut u32;
                let width = fb.width as u32;
                let height = fb.height as u32;
                let pitch = fb.pitch as u32;
                let bpp = fb.bpp as u32;

                unsafe {
                    FB = Some(Framebuffer {
                        address: addr,
                        width,
                        height,
                        pitch,
                        bpp,
                        available: true,
                    });
                }

                kprintln!(
                    "[Kian GUI] Framebuffer: {}x{} @ {:p}, pitch={}, bpp={}",
                    width, height, addr, pitch, bpp
                );
                return;
            }
        }
        _ => {}
    }

    kprintln!("[Kian GUI] No framebuffer from Limine; GUI disabled, staying in text mode.");
}

pub fn framebuffer() -> Option<&'static Framebuffer> {
    unsafe { FB.as_ref() }
}

/// Fill the entire screen with a 32-bit color (0xRRGGBB).
pub fn fill_screen(color: u32) {
    let fb = match framebuffer() {
        Some(f) if f.available => f,
        _ => return,
    };

    let bpp = fb.bpp;
    if bpp != 32 {
        kprintln!("[Kian GUI] fill_screen: unsupported bpp={}, skipping", bpp);
        return;
    }

    let bytes_per_px = 4usize;
    unsafe {
        for y in 0..fb.height {
            let row_base = fb.address.offset((y * fb.pitch as isize) as isize) as *mut u8;
            for x in 0..fb.width {
                let px = row_base.offset((x * bytes_per_px) as isize) as *mut u32;
                *px = color;
            }
        }
    }
    kprintln!("[Kian GUI] Screen filled with 0x{:08X}", color);
}
