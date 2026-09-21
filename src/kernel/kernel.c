//
// Public Header Files
//
#include "core.h"
#include "display.h"

//
// Internal Header Files
//
#include "common.h"
#include "interrupt.h"

//
// Kernel Source Files
//
#include "interrupt.c"
#include "core.c"
#include "display.c"

volatile u32 idx = 0;
volatile u32 seconds_passed = 0;

static
void timer_handle() {
    ++idx;
    if(idx % 100 == 0) {
        ++seconds_passed;
    }
}

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
void app(void) {
    s32 width, height;
    os_display_get_resolution(&width, &height);

    while(true) {
        const u32 seconds_started = seconds_passed;
        while(seconds_started == seconds_passed) {};

        os_display_clear(' ', OS_DISPLAY_White);
        const u32 cursor = print_string(0, 0, "Seconds passed: ");

        u32 power = 1;

        {
            u32 value = seconds_passed;
            while(value > 0) {
                value /= 10;
                ++power;
            }
        }

        {
            u32 value = seconds_passed;
            u32 index = 0;
            while(value > 0) {
                os_display_set_character(cursor + power - index, 0, value % 10 + '0', OS_DISPLAY_Cyan);
                value /= 10;
                ++index;
            }
        }
    }
}

//
// This procedure is called by the `kernel_main` file, once the boot loader has loaded and invoked
// the kernel.
//
int kernel_entry_point(void) {
    initialize_interrupt_handlers();

    int divisor = 1193180 / 100;
    write_output_port(0x43, 0x36);
    write_output_port(0x40, divisor & 0xff);
    write_output_port(0x40, divisor >> 8);

    register_interrupt_callback(INTERRUPT_SIGNAL_Timer, timer_handle);
    app();
    return 0;
}
