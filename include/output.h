#pragma once

#include "base.h"

#define OS_OUTPUT_COLOR_Bright_White "$bright_white$"
#define OS_OUTPUT_COLOR_White "$white$"
#define OS_OUTPUT_COLOR_Red   "$red$"
#define OS_OUTPUT_COLOR_Cyan  "$cyan$"

void os_output_print(const Str8 format, ...);
