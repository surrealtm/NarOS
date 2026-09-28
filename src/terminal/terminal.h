#pragma once

#include "display.h"

void terminal_enter(void);
void terminal_set_color(OS_Display_Color foreground, OS_Display_Color background);
void terminal_print_string(const char *string);
