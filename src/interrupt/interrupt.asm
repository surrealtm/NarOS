[extern interrupt_handler]
[global interrupt_dummy_master]
[global interrupt_dummy_slave]

; The interrupt procedures are installed by the kernel into the Interrupt Descriptor Table.
; When an exception / interrupt occurs, these assembly procedures are invoked by the CPU.
;
; For signals we care about:
; We then construct a `Interrupt_Register_State` struct on the stack in assembly, and call the
; `interrupt_handler` procedure with a pointer to the state as an argument.
; The `interrupt_handler` then has access to the interrupt signal, as well as all the registers.
;
; For signals we don't care about:
; We'll just implement the most barebones interrupt handler that we can (so that the CPU can
; call *something*, but we'll just discard the data).
; We need different handlers for signals coming from the master or the slave interrupt controller,
; since we need to respond differently to them

interrupt_dummy_master:
    pusha
    mov eax, 0x20
    out 0x20, eax
    popa
    iret

interrupt_dummy_slave:
    pusha
    mov eax, 0x20
    out 0xa0, eax
    out 0x20, eax
    popa
    iret

; Pass the vector as hexadecimal digits without 0x (for example, 20 or 2b).
%macro interrupt_stub 1
[global interrupt_%1]
interrupt_%1:
    cli
    push byte 0
    push byte 0x%+%1
    jmp interrupt_dispatch
%endmacro

interrupt_stub 20
interrupt_stub 21
interrupt_stub 22
interrupt_stub 23
interrupt_stub 24
interrupt_stub 25
interrupt_stub 26
interrupt_stub 27
interrupt_stub 28
interrupt_stub 29
interrupt_stub 2a
interrupt_stub 2b
interrupt_stub 2c
interrupt_stub 2d
interrupt_stub 2e
interrupt_stub 2f

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
    add esp, 0x8
    sti
    iret

section .note.GNU-stack noalloc noexec nowrite progbits
