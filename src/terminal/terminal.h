#pragma once

#include "display.h"

void terminal_initialize(void);
void terminal_run(void);
void terminal_set_color(OS_Display_Color foreground, OS_Display_Color background);
void terminal_reset_color(void);
void terminal_print_string(Str8 string);
