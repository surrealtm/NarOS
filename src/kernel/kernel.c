#include "core.h"
#include "display.h"

#include "display.c"

static
void clear_screen() {
    s32 width, height;
    os_display_get_resolution(&width, &height);
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            os_display_set_character(x, y, 'X', 0x0f);
        }
    }
}

// This procedure is called by the `kernel_main` file, once the boot loader has loaded and invoked
// the kernel.
int kernel_entry_point(void) {
    clear_screen();
    while(true) {}
    return 0;
}
