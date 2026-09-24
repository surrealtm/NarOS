#pragma once

#include "base.h"

void os_ctrl_sleep(u64 nanoseconds);
void os_ctrl_halt(void);
void os_ctrl_exit(void);

// @Incomplete: Should this be moved into internal kernel api only?
b8 os_ctrl_exit_requested(void);
void os_ctrl_shut_down(void);
