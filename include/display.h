#pragma once

#include "core.h"

typedef enum OS_Display_Color {
    OS_DISPLAY_Black         = 0x00,
    OS_DISPLAY_Blue          = 0x01,
    OS_DISPLAY_Green         = 0x02,
    OS_DISPLAY_Cyan          = 0x03,
    OS_DISPLAY_Red           = 0x04,
    OS_DISPLAY_Magenta       = 0x05,
    OS_DISPLAY_Brown         = 0x06,
    OS_DISPLAY_White         = 0x07,
    OS_DISPLAY_Gray          = 0x08,
    OS_DISPLAY_Light_Blue    = 0x09,
    OS_DISPLAY_Light_Green   = 0x0a,
    OS_DISPLAY_Light_Cyan    = 0x0b,
    OS_DISPLAY_Light_Red     = 0x0c,
    OS_DISPLAY_Light_Magenta = 0x0d,
    OS_DISPLAY_Yellow        = 0x0e,
    OS_DISPLAY_Bright_White  = 0x0f,
} OS_Display_Color;

void os_display_get_resolution(s32 *width, s32 *height);
void os_display_clear(char character, OS_Display_Color color);
void os_display_set_character(s32 x, s32 y, char character, OS_Display_Color color);
