#pragma once

/* ----------------------------------------------- Interrupts ----------------------------------------------- */

typedef void (*Interrupt_Callback)(void);

typedef enum Interrupt_Signal {
    INTERRUPT_SIGNAL_Timer = 0x20,
} Interrupt_Signal;

void write_output_port(u16 port, u8 value);
void initialize_interrupt_handlers(void);
void register_interrupt_callback(Interrupt_Signal signal, Interrupt_Callback callback);

/* ------------------------------------------------- Timing ------------------------------------------------- */

void initialize_tick_counter(void);
u64 current_tick_counter(void);
u64 ticks_from_nanoseconds(u64 nanoseconds);
