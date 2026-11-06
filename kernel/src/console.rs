use core::fmt;
use core::ptr::{read_volatile, write_volatile};
use spin::Once;
use spin::Mutex;

const BUFFER_HEIGHT: usize = 25;
const BUFFER_WIDTH: usize = 80;
const VGA_ADDRESS: *mut Buffer = 0xb8000 as *mut _;

#[allow(dead_code)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum Color {
    Black = 0,
    Blue = 1,
    Green = 2,
    Cyan = 3,
    Red = 4,
    Magenta = 5,
    Brown = 6,
    LightGray = 7,
    DarkGray = 8,
    LightBlue = 9,
    LightGreen = 10,
    LightCyan = 11,
    LightRed = 12,
    Pink = 13,
    Yellow = 14,
    White = 15,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub struct ScreenChar {
    pub ascii: u8,
    pub color_code: ColorCode,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(transparent)]
pub struct ColorCode(u8);

impl ColorCode {
    pub fn new(fg: Color, bg: Color) -> ColorCode {
        ColorCode((fg as u8) | ((bg as u8) << 4))
    }
}

pub struct Buffer {
    pub chars: [[ScreenChar; BUFFER_WIDTH]; BUFFER_HEIGHT],
}

pub struct Writer {
    pub column: usize,
    pub color_code: ColorCode,
    pub buffer: &'static mut Buffer,
}

impl Writer {
    pub fn new(buffer: &'static mut Buffer) -> Self {
        Writer {
            column: 0,
            color_code: ColorCode::new(Color::LightGray, Color::Black),
            buffer,
        }
    }

    pub fn write_byte(&mut self, byte: u8) {
        match byte {
            b'\n' => self.new_line(),
            byte => {
                if self.column >= BUFFER_WIDTH {
                    self.new_line();
                }
                let row = BUFFER_HEIGHT - 1;
                let col = self.column;
                let color = self.color_code;
                unsafe {
                    write_volatile(
                        &mut self.buffer.chars[row][col] as *mut ScreenChar,
                        ScreenChar { ascii: byte, color_code: color },
                    );
                }
                self.column += 1;
            }
        }
    }

    fn new_line(&mut self) {
        for row in 1..BUFFER_HEIGHT {
            for col in 0..BUFFER_WIDTH {
                let c = unsafe {
                    read_volatile(&self.buffer.chars[row][col] as *const ScreenChar)
                };
                unsafe {
                    write_volatile(
                        &mut self.buffer.chars[row - 1][col] as *mut ScreenChar,
                        c,
                    );
                }
            }
        }
        self.clear_row(BUFFER_HEIGHT - 1);
        self.column = 0;
    }

    fn clear_row(&mut self, row: usize) {
        let blank = ScreenChar { ascii: b' ', color_code: self.color_code };
        for col in 0..BUFFER_WIDTH {
            unsafe {
                write_volatile(
                    &mut self.buffer.chars[row][col] as *mut ScreenChar,
                    blank,
                );
            }
        }
    }
}

impl fmt::Write for Writer {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for b in s.bytes() {
            self.write_byte(b);
        }
        Ok(())
    }
}

static WRITER_INIT: Once = Once::new();
static mut WRITER: Option<Mutex<Writer>> = None;

pub fn writer() -> &'static Mutex<Writer> {
    unsafe {
        WRITER_INIT.call_once(|| {
            WRITER = Some(Mutex::new(Writer::new(unsafe { &mut *VGA_ADDRESS })));
        });
        WRITER.as_mut().unwrap()
    }
}

pub fn early_init() {
    let w = writer();
    let mut writer = w.lock();
    for row in 0..BUFFER_HEIGHT {
        writer.clear_row(row);
    }
    writer.column = 0;
}
