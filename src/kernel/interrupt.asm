[extern interrupt_handler]
[global interrupt_0]
[global interrupt_1]
[global interrupt_2]

interrupt_0:
    cli
    push byte 0
    push byte 0
    jmp interrupt_dispatch

interrupt_1:
    cli
    push byte 0
    push byte 14
    jmp interrupt_dispatch

interrupt_2:
    cli
    push byte 0
    push byte 32
    jmp interrupt_dispatch

interrupt_dispatch:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov eax, esp
    push eax
    call interrupt_handler
    pop eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    sti
    iret

