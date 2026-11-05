OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
ENTRY(kernel_main)

SECTIONS
{
    /* Higher-half kernel: mapped at 0xffffffff80000000 */
    . = 0xffffffff80000000;

    .text : {
        *(.text .text.*)
        *(.text.unlikely .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
        *(.rodata.*)
    }

    .data : {
        *(.data .data.*)
        *(.data.*)
    }

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4K);
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.note.GNU-stack)
        *(.comment)
    }
}
