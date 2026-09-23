#pragma once

#include "core.h"

void os_ctrl_sleep(u64 nanoseconds);
void os_ctrl_halt(void);
void os_ctrl_exit(void);
b8 os_ctrl_exit_requested(void);
