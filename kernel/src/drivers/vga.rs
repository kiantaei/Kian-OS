use crate::console::{WRITER, Color, ColorCode};
use spin::Mutex;

pub static VGA: Mutex<Option<VgaText>> = Mutex::new(None);

pub struct VgaText;

pub fn init() {
    let mut w = WRITER.lock();
    w.color_code = ColorCode::new(Color::LightGreen, Color::Black);
    *VGA.lock() = Some(VgaText);
    crate::kprintln!("[Kian] VGA text mode driver active");
}

pub fn set_color(fg: Color, bg: Color) {
    let mut w = WRITER.lock();
    w.color_code = ColorCode::new(fg, bg);
}
