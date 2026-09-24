//
// Public Header Files
//
#include "core.h"
#include "ctrl.h"
#include "display.h"

//
// Internal Header Files
//
#include "common.h"
#include "runtime.h"
#include "acpi.h"
#include "interrupt.h"

//
// Public Source Files
//
#include "ctrl.c"
#include "core.c"
#include "display.c"

//
// Internal Source Files
//
#include "runtime.c"
#include "acpi.c"
#include "interrupt.c"

static
u32 print_string(const u32 x, const u32 y, const char *text) {
    u32 cursor = x;
    while(*text) {
        os_display_set_character(cursor, y, *text, OS_DISPLAY_White);
        ++cursor;
        ++text;
    }

    return cursor;
}

static
void print_number(const u32 x, const u32 y, const u32 number) {
    const OS_Display_Color color = OS_DISPLAY_Cyan;

    if(number == 0) {
        os_display_set_character(x, y, '0', color);
        return;
    }

    u32 power = 0;

    {
        u32 value = number;
        while(value > 0) {
            value /= 10;
            ++power;
        }
    }

    {
        u32 value = number;
        u32 index = 1;
        while(value > 0) {
            os_display_set_character(x + power - index, y, value % 10 + '0', color);
            value /= 10;
            ++index;
        }
    }
}

static
void app(void) {
    s32 width, height;
    os_display_get_resolution(&width, &height);

    u32 seconds_passed = 0;
    while(!os_ctrl_exit_requested()) {
        os_display_clear(' ', OS_DISPLAY_White);
        const u32 cursor = print_string(0, 0, "Seconds passed: ");
        print_number(cursor, 0, seconds_passed);
        if(seconds_passed == 2) os_ctrl_exit();
        os_ctrl_sleep(1000000000);
        ++seconds_passed;
    }
}

//
// This procedure is called by the `kernel_main` file, once the boot loader has loaded and invoked
// the kernel.
//
int kernel_entry_point(void) {
    initialize_interrupt_handlers();
    initialize_tick_counter();
    acpi_initialize();
    app();
    os_ctrl_shut_down();
    return 0;
}
