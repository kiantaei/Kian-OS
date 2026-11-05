use x86_64::structures::idt::{InterruptDescriptorTable, InterruptStackFrame};
use x86_64::structures::gdt::SegmentSelector;
use x86_64::instructions::port::Port;
use crate::{kprintln, serial};

lazy_static! {
    pub static ref IDT: InterruptDescriptorTable = {
        let mut idt = InterruptDescriptorTable::new();
        idt.breakpoint.set_handler_fn(breakpoint_handler);
        idt.page_fault.set_handler_fn(page_fault_handler);
        unsafe {
            idt.double_fault
                .set_handler_fn(double_fault_handler)
                .set_stack_index(crate::arch::gdt::DOUBLE_FAULT_IST_INDEX);
        }
        idt[0x21].set_handler_fn(keyboard_interrupt_handler);
        idt[0x2c].set_handler_fn(mouse_interrupt_handler);
        idt[0x20].set_handler_fn(pit_timer_handler);
        idt
    };
}

pub fn init() {
    IDT.load();
    crate::kprintln!("[Kian] IDT loaded");
}

extern "x86-interrupt" fn breakpoint_handler(stack_frame: &mut InterruptStackFrame) {
    kprintln!("EXCEPTION: BREAKPOINT\n{:#?}", stack_frame);
}

extern "x86-interrupt" fn double_fault_handler(
    stack_frame: &mut InterruptStackFrame,
    _error_code: u64,
) -> ! {
    panic!("EXCEPTION: DOUBLE FAULT\n{:#?}", stack_frame);
}

extern "x86-interrupt" fn page_fault_handler(
    stack_frame: &mut InterruptStackFrame,
    error_code: x86_64::structures::paging::PageFaultErrorCode,
) {
    kprintln!("EXCEPTION: PAGE FAULT");
    kprintln!("Accessed address: {:?}", x86_64::registers::control::Cr2::read());
    kprintln!("Error code: {:?}", error_code);
    kprintln!("{:#?}", stack_frame);
    crate::panic::halt("page fault");
}

extern "x86-interrupt" fn keyboard_interrupt_handler(_stack_frame: &mut InterruptStackFrame) {
    // PS/2 keyboard - read data port 0x60
    let mut port = Port::<u8>::new(0x60);
    let _scancode = unsafe { port.read() };
    unsafe { x86_64::instructions::interrupts::enable() };
    x86_64::instructions::interrupts::eoi();
}

extern "x86-interrupt" fn mouse_interrupt_handler(_stack_frame: &mut InterruptStackFrame) {
    let mut port = Port::<u8>::new(0x60);
    let _data = unsafe { port.read() };
    unsafe { x86_64::instructions::interrupts::enable() };
    x86_64::instructions::interrupts::eoi();
}

extern "x86-interrupt" fn pit_timer_handler(_stack_frame: &mut InterruptStackFrame) {
    crate::scheduler::timer_tick();
    unsafe { x86_64::instructions::interrupts::eoi() };
}

#[naked_function]
pub unsafe extern "C" fn context_switch() { core::arch::asm!("ret"); }
