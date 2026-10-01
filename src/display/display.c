#include "display.h"
#include "port/port.h"

#define VGA_CURSOR_ADDRESS_LOW_REGISTER  0x0f
#define VGA_CURSOR_ADDRESS_HIGH_REGISTER 0x0e
#define VGA_CONTROL_REGISTER             0x3d4
#define VGA_DATA_REGISTER                0x3d5

static s32 display_width = 0;
static s32 display_height = 0;
static volatile char * video_memory = null;

void display_initialize(const u32 vga_display_mode) {
    switch(vga_display_mode) {
        case 0x2:
            display_width = 80;
            display_height = 25;
            video_memory = (char *) 0xb8000;
            break;

        default:
            panic(str8("Unsupported VGA display mode set by the boot loader. Cannot initialize the display!"));
            break;
    }
}

void os_display_get_resolution(s32 * const width, s32 * const height) {
    *width = display_width;
    *height = display_height;
}

void os_display_clear(const char character, const OS_Display_Color color) {
    if(video_memory == null) {
        return;
    }

    const s64 cells = display_width * display_height;
    const s16 target_value = (s16) character | ((s16) color << 8);
    for(s64 cell = 0; cell < cells; ++cell) {
        ((u16 *) video_memory)[cell] = target_value;
    }
}

void os_display_set_character(const s32 x, const s32 y, const char character, const OS_Display_Color foreground, const OS_Display_Color background) {
    if(video_memory == null) {
        return;
    }

    if(x < 0 || x >= display_width || y < 0 || y >= display_height) {
        return;
    }

    const u32 idx = (y * display_width + x) * 2;
    video_memory[idx + 0] = character;
    video_memory[idx + 1] = (background << 4) | foreground;
}

void os_display_set_cursor_position(const s32 x, const s32 y) {
    const u16 cursor_address = ((y * display_width) + x);
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
    *x = cursor_address % display_width;
    *y = cursor_address / display_width;
}
