use core::fmt;
use crate::arch::x86_64::serial;

const CONSOLE_WIDTH: usize = 80;
const CONSOLE_HEIGHT: usize = 30;

static mut CURSOR_X: usize = 0;
static mut CURSOR_Y: usize = 0;
static mut CONSOLE_INIT: bool = false;

// Text buffer in .bss for the scrolling console
static mut CONSOLE_BUFFER: [[u8; CONSOLE_WIDTH]; CONSOLE_HEIGHT] = [[b' '; CONSOLE_WIDTH]; CONSOLE_HEIGHT];

pub fn init() {
    unsafe { CONSOLE_INIT = true; }
}

fn put_byte(byte: u8) {
    // Always write to serial
    serial::write_byte(byte);

    if !unsafe { CONSOLE_INIT } {
        return;
    }

    match byte {
        b'\n' => {
            unsafe {
                CURSOR_X = 0;
                CURSOR_Y += 1;
                if CURSOR_Y >= CONSOLE_HEIGHT {
                    scroll_up();
                    CURSOR_Y = CONSOLE_HEIGHT - 1;
                }
            }
        }
        b'\r' => {
            unsafe { CURSOR_X = 0; }
        }
        byte => {
            unsafe {
                CONSOLE_BUFFER[CURSOR_Y][CURSOR_X] = byte;
                CURSOR_X += 1;
                if CURSOR_X >= CONSOLE_WIDTH {
                    CURSOR_X = 0;
                    CURSOR_Y += 1;
                    if CURSOR_Y >= CONSOLE_HEIGHT {
                        scroll_up();
                        CURSOR_Y = CONSOLE_HEIGHT - 1;
                    }
                }
            }
            redraw();
        }
    }
}

fn scroll_up() {
    unsafe {
        for y in 0..CONSOLE_HEIGHT - 1 {
            for x in 0..CONSOLE_WIDTH {
                CONSOLE_BUFFER[y][x] = CONSOLE_BUFFER[y + 1][x];
            }
        }
        for x in 0..CONSOLE_WIDTH {
            CONSOLE_BUFFER[CONSOLE_HEIGHT - 1][x] = b' ';
        }
    }
}

fn redraw() {
    let fb = match crate::gui::framebuffer::framebuffer() {
        Some(f) => f,
        None => return,
    };

    // Clear the whole screen to black first
    unsafe {
        let bytes_per_px = 4usize;
        for y in 0..fb.height {
            let row_base = fb.address.add((y as usize) * (fb.pitch as usize)) as *mut u8;
            for x in 0..fb.width {
                let px = row_base.add((x as usize) * bytes_per_px) as *mut u32;
                *px = 0x001A2B4A; // dark blue
            }
        }
    }

    // Redraw all visible characters
    for y in 0..CONSOLE_HEIGHT {
        for x in 0..CONSOLE_WIDTH {
            let byte = unsafe { CONSOLE_BUFFER[y][x] };
            if byte != b' ' {
                draw_char_at(&fb, x as u32 * 8, y as u32 * 8, byte, 0xFFFFFF);
            }
        }
    }
}

fn draw_char_at(fb: &crate::gui::framebuffer::Framebuffer, x: u32, y: u32, ch: u8, color: u32) {
    let glyph = if ch >= 32 && ch <= 126 {
        crate::font::FONT_8X8[(ch - 32) as usize]
    } else {
        crate::font::FONT_8X8[0] // space
    };

    for row in 0..8 {
        let pixels = glyph[row];
        for col in 0..8 {
            if (pixels >> (7 - col)) & 1 == 1 {
                let px_addr = (y as usize + row as usize) * (fb.pitch as usize) + (x as usize + col as usize) * 4;
                let px = unsafe { fb.address.add(px_addr) as *mut u32 };
                unsafe { *px = color; }
            }
        }
    }
}

pub fn draw_text(x: u32, y: u32, text: &str, color: u32) {
    let fb = match crate::gui::framebuffer::framebuffer() {
        Some(f) => f,
        None => return,
    };

    for (i, ch) in text.bytes().enumerate() {
        draw_char_at(&fb, x + (i as u32) * 8, y, ch, color);
    }
}

pub struct Writer;

impl fmt::Write for Writer {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for byte in s.bytes() {
            put_byte(byte);
        }
        Ok(())
    }
}

pub fn writer() -> &'static Writer {
    &Writer
}

pub fn write_str(s: &str) {
    for byte in s.bytes() {
        put_byte(byte);
    }
}

struct ConsoleWriter;

impl fmt::Write for ConsoleWriter {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for b in s.bytes() {
            put_byte(b);
        }
        Ok(())
    }
}

pub fn _print(args: fmt::Arguments) {
    use core::fmt::Write;
    let _ = ConsoleWriter.write_fmt(args);
}
