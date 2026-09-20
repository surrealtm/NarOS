#include "display.h"

// These values are hardcoded by the boot loader choosing the corresponding VGA video mode during boot time.
#define DISPLAY_WIDTH 80
#define DISPLAY_HEIGHT 25

static inline
char *video_memory() {
    return (char *) 0xb8000;
}

void os_display_get_resolution(int *width, int *height) {
    *width = DISPLAY_WIDTH;
    *height = DISPLAY_HEIGHT;
}

void os_display_set_character(int x, int y, char character, char color) {
    if(x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }

    unsigned int idx = (y * DISPLAY_WIDTH + x) * 2;
    char *memory = video_memory();
    memory[idx++] = character;
    memory[idx++] = color;
}
