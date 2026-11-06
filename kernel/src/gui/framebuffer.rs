// Kian OS - Framebuffer driver (STEP 1)
// Requests a framebuffer from Limine and provides basic drawing.

use crate::arch::x86_64::serial;
use core::ptr;
use alloc::format;

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
#[used]
#[unsafe(link_section = ".limine_requests")]
static mut FRAMEBUFFER_REQUEST_ENTRY: LimineFramebufferRequest = LimineFramebufferRequest {
    id: [0u8; 16], // 16 bytes — MUST be set to Limine's framebuffer request GUID (see limine.h)
    revision: 0,
    response: ptr::null_mut(),
};

#[used]
#[unsafe(link_section = ".limine_requests")]
static mut FB_RESPONSE_PTR: *mut *mut LimineFramebufferResponse = ptr::null_mut();

/// Request a framebuffer from Limine. Returns Some(Framebuffer) if one is available.
pub fn init() -> Option<Framebuffer> {
    serial::write_str("KIAN GUI: requesting framebuffer from Limine...\n");

    // Wire the request's response pointer to our storage.
    unsafe {
        FRAMEBUFFER_REQUEST_ENTRY.response = FB_RESPONSE_PTR;
    }

    // After Limine has filled the response, check if we got a framebuffer.
    let fb_resp = unsafe { FB_RESPONSE_PTR.as_ref() };
    match fb_resp {
        Some(resp) if unsafe { (*(*resp)).framebuffer_count > 0 } => {
            let fb_ptr = unsafe { (*(*resp)).framebuffers.as_ref() };
            if let Some(fb) = fb_ptr {
                let fb: &LimineFramebuffer = unsafe { &**fb };
                let addr = fb.addr as *mut u32;
                let width = fb.width as u32;
                let height = fb.height as u32;
                let pitch = fb.pitch as u32;
                let bpp = fb.bpp as u32;

                let fb_desc = Framebuffer {
                    address: addr,
                    width,
                    height,
                    pitch,
                    bpp,
                    available: true,
                };

                unsafe { FB = Some(fb_desc.clone()); }

                serial::write_str("KIAN GUI: framebuffer ");
                serial::write_u32(width);
                serial::write_str("x");
                serial::write_u32(height);
                serial::write_str(" pitch=");
                serial::write_u32(pitch);
                serial::write_str(" bpp=");
                serial::write_u32(bpp);
                serial::write_str("\n");

                return Some(fb_desc);
            }
        }
        _ => {}
    }

    serial::write_str("KIAN GUI: no framebuffer from Limine\n");
    None
}

pub fn framebuffer() -> Option<&'static Framebuffer> {
    unsafe { FB.as_ref() }
}

/// Fill the entire screen with a 32-bit color (0xRRGGBB).
pub fn fill_screen(fb: &Framebuffer, color: u32) {
    let bpp = fb.bpp;
    if bpp != 32 {
        serial::write_str("KIAN GUI: unsupported bpp\n");
        return;
    }

    unsafe {
        for y in 0..fb.height {
            let row_offset = (y as usize) * (fb.pitch as usize);
            let row_base = fb.address as *mut u8;
            for x in 0..fb.width {
                let byte_offset = row_offset + (x as usize) * 4;
                let px = row_base.add(byte_offset) as *mut u32;
                *px = color;
            }
        }
    }
}

/// Draw a single pixel at (x, y) with 32-bit color.
pub fn draw_pixel(fb: &Framebuffer, x: u32, y: u32, color: u32) {
    if x >= fb.width || y >= fb.height {
        return;
    }
    if fb.bpp != 32 {
        return;
    }
    unsafe {
        let byte_offset = (y as usize) * (fb.pitch as usize) + (x as usize) * 4;
        let px = fb.address as *mut u8;
        let px = px.add(byte_offset) as *mut u32;
        *px = color;
    }
}

impl Clone for Framebuffer {
    fn clone(&self) -> Self {
        Framebuffer {
            address: self.address,
            width: self.width,
            height: self.height,
            pitch: self.pitch,
            bpp: self.bpp,
            available: self.available,
        }
    }
}
