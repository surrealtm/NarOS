#pragma once

#include "base.h"

typedef void (*Interrupt_Callback)(void);

typedef enum Interrupt_Signal {
    INTERRUPT_SIGNAL_Timer    = 0x20,
} Interrupt_Signal;

void interrupt_initialize(void);
void interrupt_register_callback(u32 signal, Interrupt_Callback callback);
u64 interrupt_get_tick(void);
u64 interrupt_ticks_from_nanoseconds(u64 nanoseconds);
u64 interrupt_nanoseconds_from_ticks(u64 ticks);

