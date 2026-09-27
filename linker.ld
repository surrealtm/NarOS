/**
 * This linker script defines the section layout of the kernel binary.
 * It contains two important aspects:
 * - Align the code to start at 0x1000 - the boot loader will call into this address exactly.
 * - Emit symbols for the start and end of the BSS section. We must manually zero the memory
 *   for this address, so that all of our global variables are zero-initialized (since there's
 *   no CRT to do this for us...)
 */
SECTIONS {
    . = 0x1000;

    .text : {
        *(.text.entry_point) /* Corresponds to the entry_point.asm file - ensure that this function is exactly at the expected address */
        *(.text*)
    }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }
}
