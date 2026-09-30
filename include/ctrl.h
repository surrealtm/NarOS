#pragma once

#include "base.h"

/**
 * Halts any execution of code for the passed amount of nanoseconds.
 */
void os_ctrl_sleep(u64 nanoseconds);

/**
 * Halts any execution of code until an interrupt is received.
 */
void os_ctrl_halt(void);

/**
 * Requests the kernel to shut down in a clean manner.
 */
void os_ctrl_exit(void);

