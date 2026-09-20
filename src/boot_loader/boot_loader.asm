[bits 16]
[org 0x7c00]

start:
    ; INT instructions use the stack to save FLAGS, CS and IP.
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    call halt

; Function that stops any code execution on the CPU and never returns.
halt:
    sti
.loop:
    hlt ; If interrupted, just go back to halting
    jmp .loop

;
; Declare data
;
KERNEL_OFFSET equ 0x1000
BOOT_DRIVE db 0
times 510 - ($ - $$) db 0 ; Align the entire boot loader binary to 510 bytes
dw 0xaa55 ; Magic number at the end identifying this as a boot loader program and aligning the entire binary to 512 bytes
