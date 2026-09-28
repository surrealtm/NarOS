[bits 32]
[extern kernel_entry_point]
[extern __bss_start]
[extern __bss_end]
[global kernel_main]

section .text.entry_point

; This file is put first into the kernel executable, so that the boot loader can reliably jump
; to the first bytes in the kernel executable and expect a valid "entry point".
kernel_main:
    ; Zero-initialize the .bss section for the kernel
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi

    xor eax, eax
    rep stosb

    ; Jump into the C entry point
    call kernel_entry_point
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
