#pragma once

typedef void (*Interrupt_Callback)();

void write_output_port(u16 port, u8 value);
void initialize_interrupt_handlers(void);
void register_interrupt_callback(u8 signal, Interrupt_Callback callback);
