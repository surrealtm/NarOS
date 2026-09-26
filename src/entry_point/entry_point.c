#include "base.h"
#include "display.h"
#include "ctrl.h"
#include "input.h"

#include "acpi/acpi.h"
#include "input/input_kernel.h"
#include "interrupt/interrupt.h"
#include "terminal/terminal.h"

/**
 * This procedure is called by the `kernel_main` file, once the boot loader has loaded and invoked
 * the kernel.
 */
int kernel_entry_point(void) {
    interrupt_initialize();
    input_initialize();
    acpi_initialize();
    terminal_enter();
    os_ctrl_shut_down();
    return 0;
}
