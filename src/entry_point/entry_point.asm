[bits 32]
[extern kernel_entry_point]
[extern __bss_start]
[extern __bss_end]
[global kernel_main]

section .text.entry_point

; This file is put first into the kernel executable, so that the boot loader can reliably jump
; to the first bytes in the kernel executable and expect a valid "entry point".
; Note: This implicitly forwards the `Boot_Info` struct from the boot loader to the entry point in `C`
kernel_main:
    ; Zero-initialize the .bss section for the kernel
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    ; Copy the `Boot_Info` struct from this call frame into the call frame of the kernel entry point.
    sub esp, BOOT_INFO_STRUCT_SIZE

    mov edi, esp
    mov esi, esp
    add esi, BOOT_INFO_STRUCT_SIZE + 0x4
    mov ecx, BOOT_INFO_STRUCT_SIZE
    cld
    rep movsb

    call kernel_entry_point
    add esp, BOOT_INFO_STRUCT_SIZE
    ret

BOOT_INFO_STRUCT_SIZE equ 0x10

section .note.GNU-stack noalloc noexec nowrite progbits
