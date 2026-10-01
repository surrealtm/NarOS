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

    ; Jump into the C entry point. For this, we need to forward the `Boot_Info` struct along, which
    ; is passed on the stack. Since every `call` instruction modifies the stack though, we need to
    ; copy the `Boot_Info` struct from our callee frame into the caller frame...
    mov ecx, [esp + 0x4]
    mov edi, [esp + 0x8]
    sub esp, 0x8
    mov [esp + 0x0], ecx
    mov [esp + 0x4], edi
    call kernel_entry_point
    add esp, 0x8
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
