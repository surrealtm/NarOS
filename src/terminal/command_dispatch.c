#include "command_dispatch.h"
#include "ctrl.h"
#include "display.h"
#include "output.h"
#include "block_device/block_device.h"

#define COMMAND_MAX_ARGUMENTS 4

// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

#define divider(text) os_output_print(str8("--------------------------- " text " ---------------------------\n"));
#define error_msg(message) os_output_print(str8(OS_OUTPUT_COLOR_Red message OS_OUTPUT_COLOR_White))
#define error_fmt(format, ...) os_output_print(str8(OS_OUTPUT_COLOR_Red format OS_OUTPUT_COLOR_White), __VA_ARGS__)

// ---------------------------------------------------------------------------------------------------------------
// Token Handling
// ---------------------------------------------------------------------------------------------------------------

typedef struct Token {
    Str8 string;

    struct {
        int value;
        b8 valid;
    } integer;
} Token;

static
Token token_from_string(const Str8 string) {
    Token token = { .string = string };
    token.integer.value = s32_from_str8(token.string, &token.integer.valid);
    return token;
}

static
void eat_whitespaces(const Str8 command_line, s32 *cursor) {
    while (*cursor < command_line.count && command_line.data[*cursor] == ' ') {
        ++(*cursor);
    }
}

static
Token find_next_token(const Str8 command_line, s32 *cursor) {
    const b8 enquoted = command_line.data[*cursor] == '"';
    const s32 token_start = *cursor + enquoted;
    const char terminating_character = enquoted ? '"' : ' ';
    const s32 start_of_remaining_line = min(token_start + 1, command_line.count - 1);
    const Str8 remaining_line = str8_substring(command_line, start_of_remaining_line, command_line.count - 1);
    const s32 needle = str8_find_character(remaining_line, terminating_character);
    const s32 token_end = start_of_remaining_line + min(needle - 1, remaining_line.count - 1); // Clamp to the length of the command line in case there's no space in the remaining line
    *cursor = token_end + 1 + enquoted;
    const Str8 string = str8_substring(command_line, token_start, token_end);
    return token_from_string(string);
}

// ---------------------------------------------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------------------------------------------

typedef s32(*Command_Dispatch)(const Token *tokens);

typedef enum Command_Argument_Type {
    COMMAND_ARGUMENT_TYPE_Unused,
    COMMAND_ARGUMENT_TYPE_String,
    COMMAND_ARGUMENT_TYPE_Integer,
} Command_Argument_Type;

typedef struct Command_Argument {
    Str8 name;
    Command_Argument_Type type;
} Command_Argument;

typedef struct Command {
    Str8 name;
    Str8 description;
    Command_Dispatch dispatch;
    Command_Argument arguments[COMMAND_MAX_ARGUMENTS];
} Command;

static Str8 command_argument_type_string[] = {
    [COMMAND_ARGUMENT_TYPE_String] = str8_lit("string"),
    [COMMAND_ARGUMENT_TYPE_Integer] = str8_lit("integer"),
};

// ---------------------------------------------------------------------------------------------------------------
// Command Dispatch
// ---------------------------------------------------------------------------------------------------------------

static s32 cmd_quit(void);
static s32 cmd_version(void);
static s32 cmd_help(void);
static s32 cmd_block_devices(void);
static s32 cmd_read(Str8, s32);
static s32 cmd_write(Str8, Str8);

static
s32 dispatch_quit(const Token *tokens) {
    (void) tokens;
    return cmd_quit();
}

static
s32 dispatch_version(const Token *tokens) {
    (void) tokens;
    return cmd_version();
}

static
s32 dispatch_help(const Token *tokens) {
    (void) tokens;
    return cmd_help();
}

static
s32 dispatch_block_devices(const Token *tokens) {
    (void) tokens;
    return cmd_block_devices();
}

static
s32 dispatch_read(const Token *tokens) {
    return cmd_read(tokens[1].string, tokens[2].integer.value);
}

static
s32 dispatch_write(const Token *tokens) {
    return cmd_write(tokens[1].string, tokens[2].string);
}

static Command commands[] = {
    { str8_lit(":quit"),    str8_lit("Exits the kernel"), dispatch_quit, { 0 } },
    { str8_lit(":version"), str8_lit("Prints the version of the kernel"), dispatch_version, { 0 } },
    { str8_lit(":help"),    str8_lit("Prints an overview of all available commands"), dispatch_help, { 0 } },
    { str8_lit(":block_devices"), str8_lit("Prints all connected block devices"), dispatch_block_devices, { 0 } },
    { str8_lit(":read"),    str8_lit("Reads the last block of the connected block device"), dispatch_read, { { .name = str8_lit("Device"), .type = COMMAND_ARGUMENT_TYPE_String }, { .name = str8_lit("Blocks"), .type = COMMAND_ARGUMENT_TYPE_Integer } } },
    { str8_lit(":write"),   str8_lit("Writes a string into the last block of the connected block device"), dispatch_write, { { .name = str8_lit("Device"), .type = COMMAND_ARGUMENT_TYPE_String }, { .name = str8_lit("Content"), .type = COMMAND_ARGUMENT_TYPE_String } } },
};


// ---------------------------------------------------------------------------------------------------------------
// Command Implementations
// ---------------------------------------------------------------------------------------------------------------

static
s32 cmd_quit(void) {
    os_ctrl_exit();
    return 0;
}

static
s32 cmd_version(void) {
    const Str8 source_info = str8_lit("The kernel is running version " KERNEL_BUILD_TAG "." KERNEL_BUILD_COMMIT ".\n");
    const Str8 build_info = str8_lit("It was built on " KERNEL_BUILD_TIMESTAMP ".\n");
    os_output_print(str8(OS_OUTPUT_COLOR_Cyan));
    os_output_print(source_info);
    os_output_print(str8(OS_OUTPUT_COLOR_White));
    os_output_print(build_info);
    return 0;
}

static
s32 cmd_help(void) {
    divider("Help");
    for(u32 cmd_idx = 0; cmd_idx < ARRAY_COUNT(commands); ++cmd_idx) {
        const Command command = commands[cmd_idx];
        os_output_print(str8(OS_OUTPUT_COLOR_Bright_White "%s" OS_OUTPUT_COLOR_White " // %s\n"), command.name, command.description);
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
        os_output_print(str8("Device: %c (%s)\n"), device->key.identifier, str8_from_array(device->device_name));
        os_output_print(str8("  Block Size: %d, Block Count: %d\n"), device->block_size_in_bytes, device->block_count);
        os_output_print(str8("   -> Capacity: %d\n"), (u64) device->block_size_in_bytes * (u64) device->block_count);
    }

    divider("Block Devices");
    return 0;
}

static
s32 cmd_read(const Str8 device_identifier, const s32 number_of_blocks) {
    if (device_identifier.count != 1) {
        error_msg("Expected a single character as the device identifier.\n");
        return -1;
    }

    const Block_Device_Key key = { device_identifier.data[0] };
    const Block_Device *device = block_device_find(key);

    if (!block_device_is_valid(device)) {
        error_fmt("No block device for %c could be found.\n", key.identifier);
        return -1;
    }

    const s32 bytes_to_read = device->block_size_in_bytes * number_of_blocks;
    char buffer[bytes_to_read];
    if (!block_device_read(device, device->block_count - number_of_blocks, buffer)) {
        error_msg("Failed to read from the block device.\n");
        return -1;
    }

    const Str8 buffer_view = str8(buffer);

    s32 width, height;
    os_display_get_resolution(&width, &height);

    divider("Read");

    for (s32 offset = 0; offset < buffer_view.count; ++offset) {
        s32 batch_size = min(width, buffer_view.count - offset);
        os_output_print(str8_substring(buffer_view, offset, offset + batch_size - 1));
        os_output_print(str8("\n"));
        offset += batch_size;
    }

    divider("Read");
    return 0;
}

static
s32 cmd_write(const Str8 device_identifier, const Str8 content) {
    if (device_identifier.count != 1) {
        error_msg("Expected a single character as the device identifier.\n");
        return -1;
    }

    const Block_Device_Key key = { device_identifier.data[0] };
    const Block_Device *device = block_device_find(key);

    if (!block_device_is_valid(device)) {
        error_fmt("No block device for %c could be found.\n", key.identifier);
        return -1;
    }

    const s32 number_of_blocks = align(content.count, device->block_size_in_bytes);
    char buffer[device->block_size_in_bytes];
    s32 block_idx = 0;
    for (s32 offset_in_content = 0; offset_in_content < content.count; ) {
        const s32 batch_size = min(content.count - offset_in_content, (s32) device->block_size_in_bytes);
        move_memory(buffer, content.data + offset_in_content, batch_size);
        set_memory(buffer + batch_size, 0, device->block_size_in_bytes - batch_size);

        if (!block_device_write(device, device->block_count - number_of_blocks + block_idx, buffer)) {
            error_msg("Failed to read from the block device.\n");
            return -1;
        }

        offset_in_content += batch_size;
        block_idx += 1;
    }

    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------

static
const Command *find_command(const Str8 name) {
    for (u32 cmd_idx = 0; cmd_idx < ARRAY_COUNT(commands); ++cmd_idx) {
        if (str8_equals(commands[cmd_idx].name, name)) {
            return &commands[cmd_idx];
        }
    }

    return null;
}

static
s32 get_number_of_arguments(const Command *command) {
    s32 count = 0;

    for (u32 argument_idx = 0; argument_idx < ARRAY_COUNT(command->arguments); ++argument_idx) {
        if (command->arguments[argument_idx].type != COMMAND_ARGUMENT_TYPE_Unused) ++count;
    }

    return count;
}

b8 dispatch_command_line(const Str8 command_line) {
    //
    // Split up the command line into separate tokens
    //
    s32 token_count = 0;

    {
        s32 cursor = 0;
        eat_whitespaces(command_line, &cursor);
        while (cursor < command_line.count) {
            find_next_token(command_line, &cursor);
            eat_whitespaces(command_line, &cursor);
            ++token_count;
        }
    }

    if (token_count == 0) return false;

    Token tokens[token_count];

    {
        s32 cursor = 0, token_idx = 0;
        eat_whitespaces(command_line, &cursor);
        while (cursor < command_line.count) {
            tokens[token_idx++] = find_next_token(command_line, &cursor);
            eat_whitespaces(command_line, &cursor);
        }
    }

    //
    // Find the invoked command
    //
    const Command *command = find_command(tokens[0].string);
    if (!command) {
        error_fmt("The command '%s' is not known. Try `:help`.\n", tokens[0].string);
        return false;
    }

    //
    // Validate the number of arguments
    //
    if (token_count - 1 != get_number_of_arguments(command)) {
        error_fmt("The command '%s' expected %d arguments, but %d were passed.\n", command->name, get_number_of_arguments(command), token_count - 1);
        return false;
    }

    //
    // Type check the command arguments
    //
    {
        s32 token_idx = 1;

        for (u32 argument_idx = 0; argument_idx < ARRAY_COUNT(command->arguments); ++argument_idx) {
            const Command_Argument argument = command->arguments[argument_idx];
            if (argument.type == COMMAND_ARGUMENT_TYPE_Unused) continue;

            assert(token_idx < token_count, "Mismatch in the token <-> argument handling.");
            const Token token = tokens[token_idx++];

            b8 valid = false;

            switch (argument.type) {
                case COMMAND_ARGUMENT_TYPE_Unused: break;
                case COMMAND_ARGUMENT_TYPE_String: valid = true; break;
                case COMMAND_ARGUMENT_TYPE_Integer: valid = token.integer.valid; break;
            }

            if (!valid) {
                error_fmt("The command '%s' expected a '%s' for argument %d, but it received '%s'.\n", command->name, command_argument_type_string[argument.type], token_idx, token.string);
                return false;
            }
        }
    }

    //
    // Invoke the command dispatcher
    //
    return command->dispatch(tokens) == 0;
}
