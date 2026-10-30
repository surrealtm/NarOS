#include "base.h"
#include "display.h"
#include "ctrl.h"
#include "input.h"
#include "output.h"
#include "alloc.h"

#include "acpi/acpi.h"
#include "alloc/alloc_kernel.h"
#include "ctrl/ctrl_kernel.h"
#include "display/display_kernel.h"
#include "input/input_kernel.h"
#include "interrupt/interrupt.h"
#include "pci/pci.h"
#include "terminal/terminal.h"

typedef struct Boot_Info {
    u32 boot_drive_number;
    u32 vga_display_mode;
    void *dynamic_memory_region_low;
    void *dynamic_memory_region_high;
} Boot_Info;

/**
 * This procedure is called by the `kernel_main` file, once the boot loader has loaded and invoked
 * the kernel.
 */
int kernel_entry_point(const Boot_Info boot_info) {
    display_initialize(boot_info.vga_display_mode);
    terminal_initialize();
    interrupt_initialize();
    acpi_initialize();
    pci_initialize();
    alloc_initialize_memory_region(boot_info.dynamic_memory_region_low, boot_info.dynamic_memory_region_high);

    os_input_set_keyboard_layout(OS_INPUT_KEYBOARD_LAYOUT_German);
    os_output_print(str8("The available dynamic memory is %d bytes.\n"), (u64) ((u8 *) boot_info.dynamic_memory_region_high - (u8 *) boot_info.dynamic_memory_region_low));

    Heap_Allocator allocator;
    heap_allocator_create(&allocator, MEMORY_UNIT_Megabyte);

    Boot_Info *allocated = heap_allocator_allocate(&allocator, sizeof(Boot_Info), STRUCT_ALIGNMENT(Boot_Info));
    os_output_print(str8("Allocated: %p\n"), allocated);

    terminal_run();
    os_ctrl_shut_down();
    return 0;
}
