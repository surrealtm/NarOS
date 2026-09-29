/**
 * This linker script defines the section layout of the kernel binary.
 * It contains two important aspects:
 * - Reserve the first 0x7dff bytes for the boot loader segment. This includes both code and data.
 *   The BIOS can load the boot loader into address 0x0000 or 0x7c00. The boot loader occupies a
 *   total of 512 bytes, to the highest address reserved by the boot segment at run-time is 0x7dff.
 *   For simplicity, we just align the kernel to start at address 0x8000 so that the boot segment
 *   and all its danger will not be overridden.
 * - Assign the appropriate access flags for the different sections in the kernel. This is only
 *   relevant for the `elf` file, which is only used for debugging, but avoids a linker warning.
 * - Emit symbols for the start and end of the BSS section. We must manually zero the memory
 *   for this address, so that all of our global variables are zero-initialized (since there's
 *   no CRT to do this for us...)
 */

PHDRS {
    text   PT_LOAD FLAGS(5); /* R + X */
    rodata PT_LOAD FLAGS(4); /* R     */
    data   PT_LOAD FLAGS(6); /* R + W */
}

SECTIONS {
    . = 0x8000;

    .text : {
        *(.text.entry_point) /* Corresponds to the entry_point.asm file - ensure that this function is exactly at the expected address */
        *(.text*)
    } :text

    .rodata : { *(.rodata*) } :rodata
    .data : { *(.data*) } :data

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } :data
}
