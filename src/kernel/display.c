#include "display.h"

// These values are hardcoded by the boot loader choosing the corresponding VGA video mode during boot time.
#define DISPLAY_WIDTH 80
#define DISPLAY_HEIGHT 25

// This memory address is hardcoded by the VGA video format
static volatile char * const video_memory = (char *) 0xb8000;

void os_display_get_resolution(s32 *width, s32 *height) {
    *width = DISPLAY_WIDTH;
    *height = DISPLAY_HEIGHT;
}

void os_display_set_character(s32 x, s32 y, char character, OS_Display_Color color) {
    if(x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }

    unsigned int idx = (y * DISPLAY_WIDTH + x) * 2;
    video_memory[idx++] = character;
    video_memory[idx++] = color;
}
