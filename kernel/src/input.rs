use x86_64::instructions::port::Port;

/* PS/2 keyboard + mouse POLLING (no interrupts, ports 0x60/0x64).
 * Keyboard: set 1 scancodes (i8042 translation enabled), shift/ctrl/alt and
 * caps tracking, E0-prefixed keys. Mouse: 3-byte packets, AUX bit (bit 5 of
 * port 0x64) decides which byte stream a byte belongs to, enable sequence
 * 0xA8, 0x20/0x60 config, 0xD4 0xF4. */

use crate::KianEvent;

pub const EV_NONE: i32 = 0;
pub const EV_KEY_DOWN: i32 = 1;
pub const EV_KEY_UP: i32 = 2;
pub const EV_MOUSE_MOVE: i32 = 3;
pub const EV_MOUSE_BTN: i32 = 4;

const QLEN: usize = 128;

static mut INITED: bool = false;
static mut MODS: i32 = 0; // 1=shift, 2=ctrl, 4=alt
static mut CAPS: bool = false;
static mut EXT: bool = false; // E0 prefix pending
static mut MIDX: usize = 0;
static mut MPKT: [u8; 3] = [0; 3];
static mut LAST_BTN: i32 = 0;
static mut Q: [KianEvent; QLEN] = [KianEvent { kind: 0, a: 0, b: 0, c: 0 }; QLEN];
static mut QHEAD: usize = 0;
static mut QTAIL: usize = 0;

#[inline]
fn out_p(p: u16, v: u8) {
    unsafe { Port::<u8>::new(p).write(v) };
}

#[inline]
fn in_p(p: u16) -> u8 {
    unsafe { Port::<u8>::new(p).read() }
}

#[inline]
fn io_wait() {
    out_p(0x80, 0);
}

unsafe fn ps2_init() {
    // drain output buffers
    for _ in 0..64 {
        if in_p(0x64) & 1 == 0 {
            break;
        }
        in_p(0x60);
    }
    out_p(0x64, 0xAE); io_wait(); // enable first port (keyboard)
    out_p(0x64, 0xA8); io_wait(); // enable second port (mouse)
    // controller config: enable both port interrupts, enable clocks,
    // KEEP translation (bit 6) so the keyboard delivers set-1 scancodes
    out_p(0x64, 0x20); io_wait();
    let cfg = in_p(0x60);
    let cfg = (cfg | 0x03) & !0x30;
    out_p(0x64, 0x60); io_wait();
    out_p(0x60, cfg); io_wait();
    // enable mouse data reporting
    out_p(0x64, 0xD4); io_wait();
    out_p(0x60, 0xF4); io_wait();
    INITED = true;
}

unsafe fn push(kind: i32, a: i32, b: i32, c: i32) {
    let next = (QTAIL + 1) % QLEN;
    if next == QHEAD {
        return; // queue full -> drop
    }
    Q[QTAIL] = KianEvent { kind, a, b, c };
    QTAIL = next;
}

// ASCII for a (non-extended) set-1 make code under current modifiers.
fn map_ascii(code: u8, shift: bool, caps: bool) -> u8 {
    match code {
        0x02..=0x0A => {
            let i = (code - 0x02) as usize;
            if shift { b")!@#$%^&*("[i] } else { b'1' + i as u8 }
        }
        0x0B => if shift { b')' } else { b'0' },
        0x0C => if shift { b'_' } else { b'-' },
        0x0D => if shift { b'+' } else { b'=' },
        0x0E => 8, // backspace
        0x0F => b'\t',
        0x10..=0x19 => {
            let l = b"qwertyuiop"[(code - 0x10) as usize];
            letter_case(l, shift, caps)
        }
        0x1A => if shift { b'{' } else { b'[' },
        0x1B => if shift { b'}' } else { b']' },
        0x1C => b'\n',
        0x1E..=0x26 => {
            let l = b"asdfghjkl"[(code - 0x1E) as usize];
            letter_case(l, shift, caps)
        }
        0x27 => if shift { b':' } else { b';' },
        0x28 => if shift { b'"' } else { b'\'' },
        0x29 => if shift { b'~' } else { b'`' },
        0x2A | 0x36 | 0x1D | 0x38 => 0, // modifier keys
        0x2B => if shift { b'|' } else { b'\\' },
        0x2C..=0x32 => {
            let l = b"zxcvbnm"[(code - 0x2C) as usize];
            letter_case(l, shift, caps)
        }
        0x33 => if shift { b'<' } else { b',' },
        0x34 => if shift { b'>' } else { b'.' },
        0x35 => if shift { b'?' } else { b'/' },
        0x39 => b' ',
        _ => 0,
    }
}

#[inline]
fn letter_case(l: u8, shift: bool, caps: bool) -> u8 {
    if caps ^ shift {
        l - 32 // uppercase
    } else {
        l
    }
}

unsafe fn kbd_byte(code: u8) {
    if code == 0xE0 {
        EXT = true;
        return;
    }
    let release = code & 0x80 != 0;
    let real = code & 0x7F;
    let _ = EXT; // extended keys use the same modifier handling
    EXT = false;

    // update modifiers (left+right share bits)
    match real {
        0x2A | 0x36 => {
            if release { MODS &= !1; } else { MODS |= 1; }
        }
        0x1D => {
            if release { MODS &= !2; } else { MODS |= 2; }
        }
        0x38 => {
            if release { MODS &= !4; } else { MODS |= 4; }
        }
        0x3A => {
            if !release { CAPS = !CAPS; }
        }
        _ => {}
    }

    if release {
        push(EV_KEY_UP, real as i32, 0, MODS);
    } else {
        let ascii = map_ascii(real, MODS & 1 != 0, CAPS) as i32;
        push(EV_KEY_DOWN, real as i32, ascii, MODS);
    }
}

unsafe fn mouse_byte(b: u8) {
    match MIDX {
        0 => {
            if b & 0x08 != 0 {
                // sync bit must be set on the first packet byte
                MPKT[0] = b;
                MIDX = 1;
            }
        }
        1 => {
            MPKT[1] = b;
            MIDX = 2;
        }
        2 => {
            MPKT[2] = b;
            MIDX = 0;
            mouse_packet();
        }
        _ => MIDX = 0,
    }
}

unsafe fn mouse_packet() {
    let flags = MPKT[0];
    let mut dx = MPKT[1] as i32;
    let mut dy = MPKT[2] as i32;
    if flags & 0x10 != 0 {
        dx |= !0xFF; // 9-bit two's complement
    }
    if flags & 0x20 != 0 {
        dy |= !0xFF;
    }
    dy = -dy; // PS/2 Y axis is inverted relative to the screen
    push(EV_MOUSE_MOVE, dx, dy, 0);
    let btn = ((flags & 1) | ((flags >> 1) & 2) | ((flags >> 2) & 4)) as i32;
    if btn != LAST_BTN {
        LAST_BTN = btn;
        push(EV_MOUSE_BTN, btn, 0, 0);
    }
}

unsafe fn drain() {
    for _ in 0..256 {
        let st = in_p(0x64);
        if st & 1 == 0 {
            break; // output buffer empty
        }
        let b = in_p(0x60);
        if st & 0x20 != 0 {
            mouse_byte(b); // AUX bit set -> mouse byte
        } else {
            kbd_byte(b); // keyboard byte
        }
    }
}

/// ABI: int kian_poll_event(kian_event_t *out) -> 1 if an event was written.
pub fn poll_event(out: *mut KianEvent) -> i32 {
    unsafe {
        if !INITED {
            ps2_init();
        }
        drain();
        if QHEAD == QTAIL {
            return 0;
        }
        if !out.is_null() {
            *out = Q[QHEAD];
        }
        QHEAD = (QHEAD + 1) % QLEN;
        1
    }
}
