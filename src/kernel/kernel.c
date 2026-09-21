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

static
void app(void) {
    s32 index = 0;
    s32 width, height;
    os_display_get_resolution(&width, &height);
    while(true) {
        os_display_clear(' ', OS_DISPLAY_White);

        for(int y = 0; y < height; ++y) {
            for(int x = 0; x < width; ++x) {
                const b8 is_cell = (y * width + x) == index;
                os_display_set_character(x, y, is_cell ? 'X' : '.', is_cell ? OS_DISPLAY_White : OS_DISPLAY_Red);
            }
        }

        os_ctrl_sleep(16000000);

        ++index;
    }
}

//
// This procedure is called by the `kernel_main` file, once the boot loader has loaded and invoked
// the kernel.
//
int kernel_entry_point(void) {
    initialize_timer();
    app();
    return 0;
}
