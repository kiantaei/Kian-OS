use crate::console::{writer, Color, ColorCode};
use spin::Mutex;

pub static VGA: Mutex<Option<()>> = Mutex::new(None);

pub fn init() {
    let mut w = writer().lock();
    w.color_code = ColorCode::new(Color::LightGreen, Color::Black);
    *VGA.lock() = Some(());
    crate::kprintln!("[Kian] VGA text mode driver active");
}

pub fn set_color(fg: Color, bg: Color) {
    let mut w = writer().lock();
    w.color_code = ColorCode::new(fg, bg);
}
