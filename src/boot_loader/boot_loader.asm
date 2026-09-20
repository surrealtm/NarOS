[bits 16]
[org 0x7c00]

; Jump over the FAT BPB
jmp short boot_start
nop

; ------------------------------------------------
; FAT12/16 BIOS PARAMETER BLOCK
; ------------------------------------------------

db "NAROS   " ; OEM name, exactly 8 bytes
dw 512;       ; Bytes per sector
db 1          ; Sectors per cluster
dw 10         ; Reserved sectors
db 2          ; Number of FATs
dw 224        ; Root directory of entries
dw 2880       ; Total sectors, 1.44 MiB
db 0xf0       ; Removable-media descriptor
dw 9          ; Sectors per FAT
dw 18         ; Sectors per track
dw 2          ; Number of heads
dd 0          ; Hidden sectors
dd 0          ; Large total-sector count

; ------------------------------------------------
; FAT12/16 EXTENDED BPB
; ------------------------------------------------

db 0x00          ; Bios drive number
db 0x00          ; Reserved
db 0x29          ; Extended boot signature
dd 0x20260920    ; Arbitrary volume serial number
db "NAROS      " ; Volume label, exactly 11 bytes
db "FAT12   "    ; Filesystem name, exactly 8 bytes

boot_start:
    ; Normalize CS if firmware entered as 07c0:0000
    jmp 0x0000:entry_point

; Verify the size of the BPB
%if (boot_start - $$) != 0x3e
    %error "Boot code must start at byte 0x3e - the BPB was expected to take this much space."
%endif

; ------------------------------------------------
; Entry Point
; ------------------------------------------------
entry_point:
    ; INT instructions use the stack to save FLAGS, CS and IP.
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [BOOT_DRIVE], dl

    mov si, INITIALIZATION_MSG
    call print_string

    call load_kernel_from_disk

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
; Loads the kernel code from the boot drive into memory
;
load_kernel_from_disk:
    pusha
    mov ah, 0x2 ; Read Mode
    mov al, KERNEL_SECTOR_COUNT ; Number of sectors to read
    mov bx, KERNEL_OFFSET ; Target offset into which to read
    mov cl, 0x02 ; Start sector (sector 1 is the boot sector)
    mov ch, 0x00 ; Cylinder 0
    mov dl, [BOOT_DRIVE] ; Source drive identifier
    mov dh, 0x00 ; Head 0
    int 0x13
    jc .error ; If the carry bit is set, the loading has failed
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov si, DISK_SUCCESS_MSG
    call print_string
    popa
    ret
.error:
    mov si, DISK_FAILURE_MSG
    call print_string
    call halt

;
; Declare data
;
KERNEL_OFFSET equ 0x1000
KERNEL_SECTOR_COUNT equ 0x9
INITIALIZATION_MSG db "Initializing NarOS...", 0xd, 0xa, 0x0
DISK_SUCCESS_MSG   db "Successfully read the kernel from disk...", 0xd, 0xa, 0x0
DISK_FAILURE_MSG   db "Failed to read the kernel from disk...", 0xd, 0xa, 0x0
BOOT_DRIVE db 0
times 510 - ($ - $$) db 0 ; Align the entire boot loader binary to 510 bytes
dw 0xaa55 ; Magic number at the end identifying this as a boot loader program and aligning the entire binary to 512 bytes
