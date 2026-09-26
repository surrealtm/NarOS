#include "base.h"
#include "display.h"
#include "ctrl.h"
#include "input.h"

#include "input/input_kernel.h"
#include "interrupt/interrupt.h"
#include "acpi/acpi.h"

static
void app(void) {
    s32 width, height;
    os_display_get_resolution(&width, &height);
    os_display_clear(' ', OS_DISPLAY_White);

    u32 cursor_x = 0;
    while(!os_ctrl_exit_requested()) {
        OS_Input_Event event;
        while(os_input_pop_event(&event)) {
            if(event.kind == OS_INPUT_EVENT_KIND_Keyboard && event.data.keyboard.down) {
                switch(event.data.keyboard.key_code) {
                    case OS_INPUT_KEY_Backspace:
                        if(cursor_x > 0) --cursor_x;
                        os_display_set_character(cursor_x, 0, ' ', OS_DISPLAY_White);
                        break;

                    case OS_INPUT_KEY_Arrow_Left:
                        if(cursor_x > 0) --cursor_x;
                        break;

                    case OS_INPUT_KEY_Arrow_Right:
                        ++cursor_x;
                    break;

                    case OS_INPUT_KEY_Space:
                        if(event.data.keyboard.right_alt_down) {
                            os_input_set_keyboard_layout(OS_INPUT_KEYBOARD_LAYOUT_Standard_US);
                            break;
                        }

                    default:
                        if(event.data.keyboard.ascii == 0) break;
                        u8 drawn_ascii = event.data.keyboard.ascii;
                        if(event.data.keyboard.ascii >= 'A' && event.data.keyboard.ascii <= 'Z' && !event.data.keyboard.shift_down) {
                            drawn_ascii = drawn_ascii - 'A' + 'a';
                        }
                        os_display_set_character(cursor_x, 0, drawn_ascii, OS_DISPLAY_White);
                        ++cursor_x;
                }
            }
        }
        os_ctrl_sleep(10000000);
    }
}

/**
 * This procedure is called by the `kernel_main` file, once the boot loader has loaded and invoked
 * the kernel.
 */
int kernel_entry_point(void) {
    interrupt_initialize();
    input_initialize();
    acpi_initialize();
    app();
    os_ctrl_shut_down();
    return 0;
}
