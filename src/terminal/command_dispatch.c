#include "command_dispatch.h"
#include "terminal.h"
#include "ctrl.h"

typedef s32(*Command_Callback)(void);

typedef struct Command {
    Str8 name;
    Str8 description;
    Command_Callback callback;
} Command;

static s32 cmd_quit(void);
static s32 cmd_version(void);
static s32 cmd_help(void);

static Command commands[] = {
    { str8_lit(":quit"),    str8_lit("Exits the kernel"), cmd_quit },
    { str8_lit(":version"), str8_lit("Prints the version of the kernel"), cmd_version },
    { str8_lit(":help"),    str8_lit("Prints an overview of all available commands"), cmd_help },
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
    terminal_print_string(str8_lit("--------------------------- Help ---------------------------\n"));
    for(u32 cmd_idx = 0; cmd_idx < ARRAY_COUNT(commands); ++cmd_idx) {
        const Command command = commands[cmd_idx];
        terminal_set_color(OS_DISPLAY_Bright_White, OS_DISPLAY_Black);
        terminal_print_string(command.name);
        terminal_reset_color();
        terminal_print_string(str8_lit(" // "));
        terminal_print_string(command.description);
        terminal_print_string(str8_lit("\n"));
    }
    terminal_print_string(str8_lit("--------------------------- Help ---------------------------\n"));
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
