use core::arch::asm;

// Switch from current stack pointer `from` to `to`, then call `switch(target)`.
// Returns in `from` context.
#[naked]
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
        "mov rsp, [rsi]",   // reload rsp (in case)
        "jmp *rdx",         // jump to target function
        :
        : "{rdi}" (from), "{rsi}" (to), "{rdx}" (target)
        : "memory"
        : "intel", "noreturn"
    );
}

#[naked]
pub unsafe extern "C" fn context_switch_trampoline() -> ! {
    asm!("ret", : : : : "intel", "noreturn");
}
