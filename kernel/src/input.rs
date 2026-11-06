use x86_64::instructions::port::Port;

/* PS/2 keyboard + mouse POLLING (no interrupts, ports 0x60/0x64).
 * Keyboard: set 1 scancodes (i8042 translation enabled), shift/ctrl/alt/win
 * and caps tracking, E0-prefixed extended keys mapped to KIAN_KEY_* codes.
 * Mouse: 3-byte packets, AUX bit (bit 5 of port 0x64) decides which byte
 * stream a byte belongs to; IntelliMouse wheel unlock (rates 200/100/80 then
 * read id) switches to 4-byte packets with a wheel delta when supported.
 * enable sequence 0xA8, 0x20/0x60 config, unlock, 0xD4 0xF4. */

use crate::KianEvent;

pub const EV_NONE: i32 = 0;
pub const EV_KEY_DOWN: i32 = 1;
pub const EV_KEY_UP: i32 = 2;
pub const EV_MOUSE_MOVE: i32 = 3;
pub const EV_MOUSE_BTN: i32 = 4;
pub const EV_MOUSE_WHEEL: i32 = 5;

/* mirrors of the KIAN_KEY_* enum in include/kian_abi.h */
pub const K_UP: i32 = 0x100;
pub const K_DOWN: i32 = 0x101;
pub const K_LEFT: i32 = 0x102;
pub const K_RIGHT: i32 = 0x103;
pub const K_HOME: i32 = 0x104;
pub const K_END: i32 = 0x105;
pub const K_DEL: i32 = 0x106;
pub const K_PGUP: i32 = 0x107;
pub const K_PGDN: i32 = 0x108;
pub const K_F1: i32 = 0x110;
pub const K_WIN: i32 = 0x120;
pub const K_ESC: i32 = 0x121;
pub const K_TAB: i32 = 0x122;
pub const K_ENTER: i32 = 0x123;
pub const K_BACKSPACE: i32 = 0x124;

const QLEN: usize = 128;
const DRAIN_MAX: usize = 32; // kian_poll_event NEVER blocks; drains at most 32 bytes per call

static mut INITED: bool = false;
static mut MODS: i32 = 0; // 1=shift, 2=ctrl, 4=alt, 8=win
static mut CAPS: bool = false;
static mut CAPS_DOWN: bool = false; // guard: toggle once per physical press
static mut EXT: bool = false; // E0 prefix pending
static mut WHEEL_MODE: bool = false; // IntelliMouse 4-byte packets
static mut MIDX: usize = 0;
static mut MPKT: [u8; 4] = [0; 4];
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

/* wait until the controller input buffer (bit 1) is free, bounded */
unsafe fn wait_write() {
    for _ in 0..100_000 {
        if in_p(0x64) & 2 == 0 {
            return;
        }
    }
}

/* wait for output data (bit 0) with timeout; true if a byte is waiting */
unsafe fn wait_read() -> bool {
    for _ in 0..100_000 {
        if in_p(0x64) & 1 != 0 {
            return true;
        }
    }
    false
}

/* send a command to the AUX device (0xD4 prefix) and swallow its ACK */
unsafe fn mouse_cmd(cmd: u8) {
    wait_write();
    out_p(0x64, 0xD4);
    io_wait();
    wait_write();
    out_p(0x60, cmd);
    io_wait();
    // swallow the 0xFA acknowledge so it never reaches the packet parser
    for _ in 0..1000 {
        if in_p(0x64) & 1 == 0 {
            return;
        }
        let _ = in_p(0x60);
    }
}

/* IntelliMouse unlock: rates 200, 100, 80 then read the device id.
 * Returns true if the device answered id 3 (wheel present). */
unsafe fn mouse_try_wheel() -> bool {
    mouse_cmd(0xF5); // disable data reporting while configuring
    mouse_cmd(0xF3);
    mouse_cmd(200);
    mouse_cmd(0xF3);
    mouse_cmd(100);
    mouse_cmd(0xF3);
    mouse_cmd(80);
    mouse_cmd(0xF2); // read id
    let mut id: u8 = 0;
    // swallow ACK, then read the id byte
    for _ in 0..1000 {
        if in_p(0x64) & 1 == 0 {
            break;
        }
        let _ = in_p(0x60);
    }
    if wait_read() {
        id = in_p(0x60);
    }
    mouse_cmd(0xF4); // (re)enable data reporting
    id == 3
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
    // IntelliMouse wheel probe; keeps 3-byte packets when unsupported
    if mouse_try_wheel() {
        WHEEL_MODE = true;
    }
    MIDX = 0;
    EXT = false;
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
    let ext = EXT;
    EXT = false;

    // update modifiers (left+right share bits; win keys are E0-prefixed)
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
        0x5B | 0x5C if ext => {
            if release { MODS &= !8; } else { MODS |= 8; }
        }
        0x3A => {
            if !release && !CAPS_DOWN {
                CAPS = !CAPS;
                CAPS_DOWN = true;
            } else if release {
                CAPS_DOWN = false;
            }
        }
        _ => {}
    }

    // pick (special key code, ascii). kcode == 0 -> plain scancode in a.
    let (kcode, ascii) = if ext {
        match real {
            0x48 => (K_UP, 0),
            0x50 => (K_DOWN, 0),
            0x4B => (K_LEFT, 0),
            0x4D => (K_RIGHT, 0),
            0x47 => (K_HOME, 0),
            0x4F => (K_END, 0),
            0x53 => (K_DEL, 0),
            0x49 => (K_PGUP, 0),
            0x51 => (K_PGDN, 0),
            0x5B | 0x5C => (K_WIN, 0),
            0x1C => (K_ENTER, b'\n' as i32), // keypad Enter
            0x35 => (0, b'/' as i32),        // keypad slash
            _ => (0, 0),
        }
    } else {
        match real {
            0x01 => (K_ESC, 27),
            0x0E => (K_BACKSPACE, 8),
            0x0F => (K_TAB, b'\t' as i32),
            0x1C => (K_ENTER, b'\n' as i32),
            0x3B..=0x44 => (K_F1 + (real - 0x3B) as i32, 0), // F1..F10
            0x57 => (K_F1 + 10, 0),                          // F11
            0x58 => (K_F1 + 11, 0),                          // F12
            /* Keypad digits/operators: NumLock-on keypad keys arrive WITHOUT
             * the E0 prefix, while arrows/Home/End/etc. always carry E0.
             * Shift/Caps apply nothing here (keypad has no case). */
            0x47..=0x53 => {
                let kp: u8 = match real {
                    0x47 => b'7', 0x48 => b'8', 0x49 => b'9', 0x4A => b'-',
                    0x4B => b'4', 0x4C => b'5', 0x4D => b'6', 0x4E => b'+',
                    0x4F => b'1', 0x50 => b'2', 0x51 => b'3',
                    0x52 => b'0', 0x53 => b'.',
                    _ => 0,
                };
                (0, kp as i32)
            }
            _ => (0, map_ascii(real, MODS & 1 != 0, CAPS) as i32),
        }
    };

    if release {
        push(EV_KEY_UP, kcode, 0, MODS);
    } else {
        let a = if kcode != 0 { kcode } else { real as i32 };
        push(EV_KEY_DOWN, a, ascii, MODS);
    }
}

unsafe fn mouse_byte(b: u8) {
    let plen = if WHEEL_MODE { 4 } else { 3 };
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
            if plen == 3 {
                MIDX = 0;
                mouse_packet(false);
            } else {
                MIDX = 3;
            }
        }
        3 => {
            MPKT[3] = b;
            MIDX = 0;
            mouse_packet(true);
        }
        _ => MIDX = 0,
    }
}

unsafe fn mouse_packet(has_wheel: bool) {
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
    if has_wheel {
        // 4-bit two's complement wheel delta; +1 = away (up), -1 = down
        let w4 = (MPKT[3] & 0x0F) as i32;
        let wv = if w4 & 0x08 != 0 { w4 | !0x0F } else { w4 };
        if wv != 0 {
            push(EV_MOUSE_WHEEL, wv, 0, 0);
        }
    }
}

unsafe fn drain() {
    for _ in 0..DRAIN_MAX {
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
/// Never blocks; drains at most DRAIN_MAX bytes from the controller per call.
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
