#pragma once

#include "base.h"

typedef void (*Interrupt_Callback)(void);

typedef enum Interrupt_Signal {
    INTERRUPT_SIGNAL_Timer    = 0x20,
    INTERRUPT_SIGNAL_UHCI     = 0x2b, // This value is hardcoded since it works for Qemu, but for robustness it should be determined at run-time
} Interrupt_Signal;

void interrupt_initialize(void);
void interrupt_register_callback(Interrupt_Signal signal, Interrupt_Callback callback);
u64 interrupt_get_tick(void);
u64 interrupt_ticks_from_nanoseconds(u64 nanoseconds);

