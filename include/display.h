#pragma once

// @Incomplete: Expose color codes as enum?
// @Incomplete: Provide API to clear the entire screen?

void os_display_get_resolution(s32 *width, s32 *height);
void os_display_set_character(s32 x, s32 y, char character, char color);
