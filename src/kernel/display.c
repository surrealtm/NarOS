#include "display.h"

// These values are hardcoded by the boot loader choosing the corresponding VGA video mode during boot time.
#define DISPLAY_WIDTH 80
#define DISPLAY_HEIGHT 25

// This memory address is hardcoded by the VGA video format
static volatile char * const video_memory = (char *) 0xb8000;

void os_display_get_resolution(s32 * const width, s32 * const height) {
    *width = DISPLAY_WIDTH;
    *height = DISPLAY_HEIGHT;
}

void os_display_clear(const char character, const OS_Display_Color color) {
    const s64 cells = DISPLAY_WIDTH * DISPLAY_HEIGHT;
    const s16 target_value = (s16) character | ((s16) color << 8);
    for(s64 cell = 0; cell < cells; ++cell) {
        ((u16 *) video_memory)[cell] = target_value;
    }
}

void os_display_set_character(const s32 x, const s32 y, const char character, const OS_Display_Color color) {
    if(x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }

    const unsigned int idx = (y * DISPLAY_WIDTH + x) * 2;
    video_memory[idx + 0] = character;
    video_memory[idx + 1] = color;
}
