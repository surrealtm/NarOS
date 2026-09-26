#include "display.h"
#include "port/port.h"

// These values are hardcoded by the boot loader choosing the corresponding VGA video mode during boot time.
#define DISPLAY_WIDTH 80
#define DISPLAY_HEIGHT 25

#define VGA_CURSOR_ADDRESS_LOW_REGISTER  0x0f
#define VGA_CURSOR_ADDRESS_HIGH_REGISTER 0x0e
#define VGA_CONTROL_REGISTER             0x3d4
#define VGA_DATA_REGISTER                0x3d5

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

    const u32 idx = (y * DISPLAY_WIDTH + x) * 2;
    video_memory[idx + 0] = character;
    video_memory[idx + 1] = color;
}

void os_display_set_cursor_position(const s32 x, const s32 y) {
    const u16 cursor_address = ((y * DISPLAY_WIDTH) + x);
    port_write_u8(VGA_CONTROL_REGISTER, VGA_CURSOR_ADDRESS_HIGH_REGISTER);
    port_write_u8(VGA_DATA_REGISTER, (cursor_address >> 8) & 0xff);
    port_write_u8(VGA_CONTROL_REGISTER, VGA_CURSOR_ADDRESS_LOW_REGISTER);
    port_write_u8(VGA_DATA_REGISTER, (cursor_address >> 0) & 0xff);
}

void os_display_get_cursor_position(s32 *x, s32 *y) {
    port_write_u8(VGA_CONTROL_REGISTER, VGA_CURSOR_ADDRESS_LOW_REGISTER);
    const u8 cursor_address_low = port_read_u8(VGA_DATA_REGISTER);
    port_write_u8(VGA_CONTROL_REGISTER, VGA_CURSOR_ADDRESS_HIGH_REGISTER);
    const u8 cursor_address_high = port_read_u8(VGA_DATA_REGISTER);
    const u16 cursor_address = ((u16) cursor_address_high << 8) | (u16) cursor_address_low;
    *x = cursor_address % DISPLAY_WIDTH;
    *y = cursor_address / DISPLAY_WIDTH;
}
