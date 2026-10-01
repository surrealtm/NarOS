#include "base.h"
#include "display.h"
#include "ctrl.h"
#include "input.h"

#include "acpi/acpi.h"
#include "ctrl/ctrl_kernel.h"
#include "display/display_kernel.h"
#include "input/input_kernel.h"
#include "interrupt/interrupt.h"
#include "terminal/terminal.h"

typedef struct Boot_Info {
    u32 boot_drive_number;
    u32 vga_display_mode;
} Boot_Info;

/**
 * This procedure is called by the `kernel_main` file, once the boot loader has loaded and invoked
 * the kernel.
 */
int kernel_entry_point(const Boot_Info boot_info) {
    display_initialize(boot_info.vga_display_mode);
    interrupt_initialize();
    input_initialize();
    acpi_initialize();
    terminal_enter();
    os_ctrl_shut_down();
    return 0;
}
