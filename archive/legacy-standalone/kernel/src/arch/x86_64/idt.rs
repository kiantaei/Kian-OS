use x86_64::structures::idt::{InterruptDescriptorTable, InterruptStackFrame, PageFaultErrorCode};
use x86_64::instructions::port::Port;
use crate::kprintln;
use crate::arch::x86_64::serial;

static mut IDT: Option<InterruptDescriptorTable> = None;

pub fn init() {
    let mut idt = InterruptDescriptorTable::new();
    idt.breakpoint.set_handler_fn(breakpoint_handler);
    idt.page_fault.set_handler_fn(page_fault_handler);
    unsafe {
        idt.double_fault
            .set_handler_fn(double_fault_handler)
            .set_stack_index(crate::arch::x86_64::gdt::DOUBLE_FAULT_IST_INDEX);
    }
    idt[0x21].set_handler_fn(keyboard_interrupt_handler);
    idt[0x2c].set_handler_fn(mouse_interrupt_handler);
    idt[0x20].set_handler_fn(pit_timer_handler);

    unsafe {
        IDT = Some(idt);
        IDT.as_mut().unwrap().load();
    }
    crate::kprintln!("[Kian] IDT loaded");
}

extern "x86-interrupt" fn breakpoint_handler(stack_frame: InterruptStackFrame) {
    kprintln!("EXCEPTION: BREAKPOINT\n{:#?}", stack_frame);
}

extern "x86-interrupt" fn double_fault_handler(
    stack_frame: InterruptStackFrame,
    _error_code: u64,
) -> ! {
    panic!("EXCEPTION: DOUBLE FAULT\n{:#?}", stack_frame);
}

extern "x86-interrupt" fn page_fault_handler(
    stack_frame: InterruptStackFrame,
    error_code: PageFaultErrorCode,
) {
    kprintln!("EXCEPTION: PAGE FAULT");
    kprintln!("Accessed address: {:?}", x86_64::registers::control::Cr2::read());
    kprintln!("Error code: {:?}", error_code);
    kprintln!("{:#?}", stack_frame);
    crate::panic::halt("page fault");
}

extern "x86-interrupt" fn keyboard_interrupt_handler(_stack_frame: InterruptStackFrame) {
    let mut port = Port::<u8>::new(0x60);
    let _scancode = unsafe { port.read() };
    unsafe { crate::arch::x86_64::pic::eoi(0x21) };
}

extern "x86-interrupt" fn mouse_interrupt_handler(_stack_frame: InterruptStackFrame) {
    let mut port = Port::<u8>::new(0x60);
    let _data = unsafe { port.read() };
    unsafe { crate::arch::x86_64::pic::eoi(0x2c) };
}

extern "x86-interrupt" fn pit_timer_handler(_stack_frame: InterruptStackFrame) {
    crate::scheduler::timer_tick();
    unsafe { crate::arch::x86_64::pic::eoi(0x20) };
}
