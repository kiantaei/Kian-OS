; Kian OS - Boot stub (Legacy BIOS + Limine entry)
; Assembled with NASM. Sets up a minimal environment and jumps to the Rust kernel.
; For UEFI/Limine, the bootloader provides the entry; this file is the BIOS fallback.

[bits 32]
section .text
global start
extern kernel_main

start:
    ; Disable interrupts
    cli

    ; Set up a temporary stack
    mov esp, stack_top

    ; Clear direction flag
    cld

    ; Jump into the Rust kernel (higher-half entry linked at 0xffffffff80000000)
    ; The linker script places kernel_main at the higher-half address.
    jmp 0x08:kernel_main

.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
