#![no_std]

extern crate alloc;

use alloc::string::{String, ToString};
use alloc::vec::Vec;
use alloc::format;
use core::fmt::Write;

// Re-export syscalls.
use crate::syscalls::{write, read_line, exit, getpid, time, yield_cpu};

const SYS_WRITE: u64 = 1;
const SYS_READ: u64 = 6;
const SYS_OPEN: u64 = 7;
const SYS_CLOSE: u64 = 8;
const SYS_STAT: u64 = 9;
const SYS_GETPID: u64 = 3;
const SYS_TIME: u64 = 4;
const SYS_YIELD: u64 = 5;
const SYS_EXIT: u64 = 2;

#[no_mangle]
pub extern "C" fn write(fd: u64, buf: &[u8]) -> i64 {
    unsafe {
        // Inline syscall: write
        let ret: u64;
        core::arch::asm!(
            "syscall",
            in("rax") SYS_WRITE,
            in("rdi") fd,
            in("rsi") buf.as_ptr(),
            in("rdx") buf.len(),
            lateout("rax") ret,
            options(nostack)
        );
        ret as i64
    }
}

#[no_mangle]
pub extern "C" fn read_line() -> alloc::string::String {
    // Simple line reader: read one byte at a time until newline.
    let mut buf = [0u8; 1];
    let mut result = alloc::string::String::new();
    loop {
        let _ = read(0, &mut buf);
        let c = buf[0] as char;
        if c == '\n' || c == '\r' {
            break;
        }
        if c >= 32 && c <= 126 {
            result.push(c);
        }
    }
    result
}

#[no_mangle]
pub extern "C" fn read(fd: u64, buf: &mut [u8]) -> i64 {
    unsafe {
        let ret: u64;
        core::arch::asm!(
            "syscall",
            in("rax") SYS_READ,
            in("rdi") fd,
            in("rsi") buf.as_ptr(),
            in("rdx") buf.len(),
            lateout("rax") ret,
            options(nostack)
        );
        ret as i64
    }
}

#[no_mangle]
pub extern "C" fn exit(code: i64) -> ! {
    unsafe {
        core::arch::asm!(
            "syscall",
            in("rax") SYS_EXIT,
            in("rdi") code,
            options(nostack)
        );
        loop { core::arch::asm!("hlt"); }
    }
}

#[no_mangle]
pub extern "C" fn getpid() -> u64 {
    unsafe {
        let ret: u64;
        core::arch::asm!(
            "syscall",
            in("rax") SYS_GETPID,
            lateout("rax") ret,
            options(nostack)
        );
        ret
    }
}

#[no_mangle]
pub extern "C" fn time() -> u64 {
    unsafe {
        let ret: u64;
        core::arch::asm!(
            "syscall",
            in("rax") SYS_TIME,
            lateout("rax") ret,
            options(nostack)
        );
        ret
    }
}

#[no_mangle]
pub extern "C" fn yield_cpu() {
    unsafe {
        core::arch::asm!(
            "syscall",
            in("rax") SYS_YIELD,
            options(nostack)
        );
    }
}

#[no_mangle]
pub extern "C" fn exec(path: &str) -> ! {
    unsafe {
        let ret: u64;
        core::arch::asm!(
            "syscall",
            in("rax") 10, // SYS_EXEC
            in("rdi") path.as_ptr(),
            in("rsi") path.len(),
            lateout("rax") ret,
            options(nostack)
        );
        loop { core::arch::asm!("hlt"); }
    }
}
