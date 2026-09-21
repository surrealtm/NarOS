#pragma once

typedef void (*Interrupt_Callback)();

typedef enum Interrupt_Signal {
    INTERRUPT_SIGNAL_Timer = 0x20,
} Interrupt_Signal;

void write_output_port(u16 port, u8 value);
void initialize_interrupt_handlers(void);
void register_interrupt_callback(Interrupt_Signal signal, Interrupt_Callback callback);
