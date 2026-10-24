#include "command_dispatch.h"
#include "terminal.h"
#include "ctrl.h"
#include "output.h"
#include "block_device/block_device.h"

// @Cleanup: This should use the os_output_print functionality for formatting, or terminal_print() should support formatting...

#define divider(text) terminal_print_string(str8("--------------------------- " text " ---------------------------\n"));

typedef s32(*Command_Callback)(void);

typedef struct Command {
    Str8 name;
    Str8 description;
    Command_Callback callback;
} Command;

static s32 cmd_quit(void);
static s32 cmd_version(void);
static s32 cmd_help(void);
static s32 cmd_block_devices(void);

static Command commands[] = {
    { str8_lit(":quit"),    str8_lit("Exits the kernel"), cmd_quit },
    { str8_lit(":version"), str8_lit("Prints the version of the kernel"), cmd_version },
    { str8_lit(":help"),    str8_lit("Prints an overview of all available commands"), cmd_help },
    { str8_lit(":block_devices"), str8_lit("Prints all connected block devices"), cmd_block_devices },
};

static
s32 cmd_quit(void) {
    os_ctrl_exit();
    return 0;
}

static
s32 cmd_version(void) {
    const Str8 source_info = str8_lit("The kernel is running version " KERNEL_BUILD_TAG "." KERNEL_BUILD_COMMIT ".\n");
    const Str8 build_info = str8_lit("It was built on " KERNEL_BUILD_TIMESTAMP ".\n");
    terminal_set_color(OS_DISPLAY_Cyan, OS_DISPLAY_Black);
    terminal_print_string(source_info);
    terminal_print_string(build_info);
    terminal_reset_color();
    return 0;
}

static
s32 cmd_help(void) {
    divider("Help");
    for(u32 cmd_idx = 0; cmd_idx < ARRAY_COUNT(commands); ++cmd_idx) {
        const Command command = commands[cmd_idx];
        terminal_set_color(OS_DISPLAY_Bright_White, OS_DISPLAY_Black);
        terminal_print_string(command.name);
        terminal_reset_color();
        terminal_print_string(str8(" // "));
        terminal_print_string(command.description);
        terminal_print_string(str8("\n"));
    }
    divider("Help");
    return 0;
}

static
s32 cmd_block_devices(void) {
    divider("Block Devices");

    s32 device_count;
    const Block_Device *devices = block_device_enumerate(&device_count);

    for (s32 device_idx = 0; device_idx < device_count; ++device_idx) {
        const Block_Device *device = &devices[device_idx];
        Block_Device_Metadata metadata;
        if (block_device_get_metadata(device, &metadata)) {
            os_output_print(str8("Device: %c (%s)\n"), device->key.identifier, str8_from_array(metadata.device_name));
            os_output_print(str8("  Block Size: %d, Block Count: %d\n"), metadata.block_size_in_bytes, metadata.block_count);
            os_output_print(str8("   -> Capacity: %d\n"), metadata.block_size_in_bytes * metadata.block_count);
        } else {
            os_output_print(str8(" (Misconfigured device)\n"));
        }
    }

    divider("Block Devices");
    return 0;
}

b8 dispatch_command(const Str8 input_line) {
    for(u32 cmd_idx = 0; cmd_idx < ARRAY_COUNT(commands); ++cmd_idx) {
        if(str8_equals(commands[cmd_idx].name, input_line)) {
            return commands[cmd_idx].callback() == 0;
        }
    }
    return false;
}
