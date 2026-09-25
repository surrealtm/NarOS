[extern interrupt_handler]
[global interrupt_dummy_master]
[global interrupt_dummy_slave]
[global interrupt_20]
[global interrupt_21]

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

interrupt_20:
    cli
    push byte 0
    push byte 0x20
    jmp interrupt_dispatch

interrupt_21:
    cli
    push byte 0
    push byte 0x21
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
    add esp, 0x8
    sti
    iret

section .note.GNU-stack noalloc noexec nowrite progbits
