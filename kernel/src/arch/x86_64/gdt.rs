use x86_64::structures::gdt::{Descriptor, GlobalDescriptorTable, SegmentSelector};
use x86_64::structures::tss::TaskStateSegment;
use x86_64::{VirtAddr};

pub const DOUBLE_FAULT_IST_INDEX: u16 = 0;

pub struct Selectors {
    pub code_selector: SegmentSelector,
    pub data_selector: SegmentSelector,
    pub tss_selector: SegmentSelector,
}

static mut GDT: Option<(GlobalDescriptorTable, Selectors)> = None;
static mut TSS: Option<TaskStateSegment> = None;

pub fn init() {
    let mut gdt = GlobalDescriptorTable::new();
    let code_selector = gdt.add_entry(Descriptor::kernel_code_segment());
    let data_selector = gdt.add_entry(Descriptor::kernel_data_segment());

    let mut tss = TaskStateSegment::new();
    tss.interrupt_stack_table[DOUBLE_FAULT_IST_INDEX as usize] = {
        const STACK_SIZE: usize = 4096 * 5;
        static mut STACK: [u8; STACK_SIZE] = [0; STACK_SIZE];
        let stack_start = VirtAddr::from_ptr(unsafe { &STACK });
        let stack_end = stack_start + STACK_SIZE;
        stack_end
    };
    let tss_selector = gdt.add_entry(Descriptor::tss_segment(&tss));

    unsafe {
        GDT = Some((gdt, Selectors { code_selector, data_selector, tss_selector }));
        TSS = Some(tss);

        let (gdt, selectors) = GDT.as_mut().unwrap();
        gdt.load();
        selectors.code_selector.set_cs();
        selectors.data_selector.set_ds();
        selectors.data_selector.set_es();
        selectors.data_selector.set_fs();
        selectors.data_selector.set_gs();
        selectors.data_selector.set_ss();
        x86_64::instructions::tables::load_tss(selectors.tss_selector);
    }
    crate::kprintln!("[Kian] GDT + TSS loaded");
}
