[bits 32]
[extern kernel_entry_point]
[global kernel_main]

section .text

; This file is put first into the kernel executable, so that the boot loader can reliably jump
; to the first bytes in the kernel executable and expect a valid "entry point".
kernel_main:
    call kernel_entry_point
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
