#![no_std]
#![no_main]

extern crate alloc;

use alloc::string::{String, ToString};
use alloc::vec::Vec;
use alloc::collections::BTreeMap;
use core::fmt::Write;

mod syscalls;
mod cmdlets;
mod pipeline;
mod parser;

static mut PROMPT: &str = "PS Kian:\\Users\\kian>";

#[no_mangle]
pub extern "C" fn kian_start() -> ! {
    // Clear screen and show banner.
    syscalls::write(1, b"\x1b[2J\x1b[H");
    syscalls::write(1, b"BlueShell v0.2.0 (PowerShell-style) on Kian OS\n");
    syscalls::write(1, b"Type 'Get-Help' for available cmdlets.\n\n");

    loop {
        // Print prompt.
        let p = unsafe { PROMPT };
        syscalls::write(1, p.as_bytes());
        syscalls::write(1, b" ");

        // Read line.
        let line = syscalls::read_line();
        if line.is_empty() {
            continue;
        }

        // Handle built-in shell commands.
        let trimmed = line.trim();
        match trimmed {
            "exit" | "quit" => {
                syscalls::write(1, b"Goodbye.\n");
                syscalls::exit(0);
            }
            "cls" | "clear" => {
                syscalls::write(1, b"\x1b[2J\x1b[H");
                continue;
            }
            "switch-shell" => {
                syscalls::write(1, b"Switching to ksh...\n");
                syscalls::exec("/sbin/ksh");
                continue;
            }
            _ => {}
        }

        // Parse and execute.
        if let Err(e) = parser::execute(&line) {
            let msg = format!("Error: {}\n", e);
            syscalls::write(1, msg.as_bytes());
        }
    }
}
