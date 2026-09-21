[extern interrupt_handler]
[global interrupt_00]
[global interrupt_0e]
[global interrupt_20]

; The interrupt procedures are installed by the kernel into the Interrupt Descriptor Table.
; When an exception / interrupt occurs, these assembly procedures are invoked by the CPU.
; We then construct a `Interrupt_Register_State` struct on the stack in assembly, and call the
; `interrupt_handler` procedure with a pointer to the state as an argument.
; The `interrupt_handler` then has access to the interrupt signal, as well as all the registers.

interrupt_00:
    cli
    push byte 0
    push byte 0x00
    jmp interrupt_dispatch

interrupt_0e:
    cli
    push byte 0
    push byte 0x0e
    jmp interrupt_dispatch

interrupt_20:
    cli
    push byte 0
    push byte 0x20
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
    add esp, 8
    sti
    iret

