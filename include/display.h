#pragma once

// @Incomplete: Expose color codes as enum?
// @Incomplete: Provide API to clear the entire screen?

void os_display_get_resolution(int *width, int *height);
void os_display_set_character(int x, int y, char character, char color);
