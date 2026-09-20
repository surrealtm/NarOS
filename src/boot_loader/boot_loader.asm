[bits 16]
[org 0x7c00]

;
; Entry Point
;
start:
    ; INT instructions use the stack to save FLAGS, CS and IP.
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    mov si, INITIALIZATION_MSG
    call print_string
    call halt

;
; Prints a string using BIOS interrupts.
;
print_string:
    mov ah, 0x0e
    mov bx, 0x0007
.next_character:
    mov al, [si]
    test al, al
    jz .end_of_string
    int 0x10
    inc si
    jmp .next_character
.end_of_string:
    xor ax, ax
    mov dx, ax
    mov es, ax
    ret

;
; Stops any code execution on the CPU and never returns.
;
halt:
    sti
.loop:
    hlt ; If interrupted, just go back to halting
    jmp .loop

;
; Declare data
;
KERNEL_OFFSET equ 0x1000
INITIALIZATION_MSG db "Initializing NarOS...", 0xd, 0xa, 0x0
BOOT_DRIVE db 0
times 510 - ($ - $$) db 0 ; Align the entire boot loader binary to 510 bytes
dw 0xaa55 ; Magic number at the end identifying this as a boot loader program and aligning the entire binary to 512 bytes
