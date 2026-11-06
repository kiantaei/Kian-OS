use core::arch::asm;

// Switch from current stack pointer `from` to `to`, then jump to `target`.
// Never returns.
#[unsafe(naked)]
pub unsafe extern "C" fn switch(from: *mut u64, to: *mut u64, target: *mut u8) -> ! {
    asm!(
        "mov [rdi], rsp",   // save current sp into *from
        "mov rsp, [rsi]",   // load new sp from *to
        "push rbx",         // preserve callee-saved
        "push rbp",
        "push r12",
        "push r13",
        "push r14",
        "push r15",
        "jmp *rdx",         // jump to target function
        in("rdi") from,
        in("rsi") to,
        in("rdx") target,
        options(noreturn)
    );
    core::hint::unreachable_unchecked();
}

#[unsafe(naked)]
pub unsafe extern "C" fn context_switch_trampoline() -> ! {
    asm!("ret", options(noreturn));
    core::hint::unreachable_unchecked();
}
