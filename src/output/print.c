#include "print.h"
#include "output.h"
#include "terminal/terminal.h"

#include <stdarg.h>

#define PRINT_BUFFER_CAPACITY 256

// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

static
char character_from_digit(const u8 digit) {
    if (digit <= 9) {
        return '0' + digit;
    } else if (digit >= 10 && digit <= 16) {
        return 'a' + digit;
    } else {
        return '.';
    }
}



// ---------------------------------------------------------------------------------------------------------------
// Print Buffer
// ---------------------------------------------------------------------------------------------------------------

typedef struct Print_Buffer {
    char characters[PRINT_BUFFER_CAPACITY];
    u32 cursor;
} Print_Buffer;

static
void flush(Print_Buffer *buffer) {
    terminal_print_string((Str8) { buffer->characters, buffer->cursor });
    buffer->cursor = 0;
}

static
void reserve(Print_Buffer *buffer, const u32 count) {
    assert(count < PRINT_BUFFER_CAPACITY, "Tried to reserve more characters than the print buffer's capacity.");
    if (count + buffer->cursor > PRINT_BUFFER_CAPACITY) {
        flush(buffer);
    }
}

static
void print_character(Print_Buffer *buffer, const char character) {
    reserve(buffer, 1);
    buffer->characters[buffer->cursor] = character;
    ++buffer->cursor;
}

static
void print_character_at_offset(Print_Buffer *buffer, const char character, const s32 offset_from_cursor) {
    assert(buffer->cursor + offset_from_cursor < PRINT_BUFFER_CAPACITY, "Tried to write out-of-bounds of the print buffer.");
    buffer->characters[buffer->cursor + offset_from_cursor] = character;
}

static
s32 calculate_number_of_digits_unsigned(u64 value, const u64 radix) {
    if(value == 0) return 1;

    s32 number_of_digits = 0;

    while (value > 0) {
        ++number_of_digits;
        value /= radix;
    }

    return number_of_digits;
}

static
void print_unsigned_int(Print_Buffer *buffer, unsigned int value) {
    const u64 radix = 10;

    const s32 number_of_digits = calculate_number_of_digits_unsigned(value, radix);

    reserve(buffer, number_of_digits);

    for (s32 digit_idx = 0; digit_idx < number_of_digits; ++digit_idx) {
        const s32 digit = value % radix;
        const char character = character_from_digit(digit);
        const s32 digit_offset = (number_of_digits - 1) - digit_idx;
        print_character_at_offset(buffer, character, digit_offset);
        value /= radix;
    }

    buffer->cursor += number_of_digits;
}

static
void print_signed_int(Print_Buffer *buffer, const signed int signed_value) {
    unsigned int unsigned_value = (unsigned int) signed_value;

    if (signed_value < 0) {
        print_character(buffer, '-');
        unsigned_value = -signed_value;
    }

    print_unsigned_int(buffer, unsigned_value);
}

static
void print_string(Print_Buffer *buffer, const Str8 string) {
    for (s32 i = 0; i < string.count; ++i) {
        print_character(buffer, string.data[i]);
    }
}

static
void print_data_type(Print_Buffer *buffer, const char data_type, va_list *arguments) {
    switch(data_type) {
        case 'c': {
            const char value = va_arg(*arguments, int);
            print_character(buffer, value);
        } break;

        case 'u': {
            const unsigned int value = va_arg(*arguments, unsigned int);
            print_unsigned_int(buffer, value);
        } break;

        case 'd': {
            const signed int value = va_arg(*arguments, signed int);
            print_signed_int(buffer, value);
        } break;

        case 's': {
            const Str8 value = va_arg(*arguments, Str8);
            print_string(buffer, value);
        } break;

        default: {
            va_arg(*arguments, unsigned int);
            print_character(buffer, '%');
        } break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Color Handling
// ---------------------------------------------------------------------------------------------------------------

typedef struct Color_Mapping {
    const Str8 identifier;
    const OS_Display_Color display_color;
} Color_Mapping;

static Color_Mapping color_mapping[] = {
    { str8_lit(OS_OUTPUT_COLOR_White), OS_DISPLAY_White },
    { str8_lit(OS_OUTPUT_COLOR_Red),   OS_DISPLAY_Red },
    { str8_lit(OS_OUTPUT_COLOR_Cyan),  OS_DISPLAY_Cyan },
};

static
void set_foreground_color_based_on_string(Print_Buffer *buffer, const Str8 identifier) {
    for (u32 map_idx = 0; map_idx < ARRAY_COUNT(color_mapping); ++map_idx) {
        if (str8_equals(color_mapping[map_idx].identifier, identifier)) {
            flush(buffer);
            terminal_set_color(color_mapping[map_idx].display_color, OS_DISPLAY_Black);
            return;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------

void print_format(const Str8 format, va_list arguments) {
    Print_Buffer buffer = { .cursor = 0 };

    for (s32 i = 0; i < format.count;) {
        if (format.data[i] == '$' && i + 1 < format.count) {
            const Str8 remaining_string = str8_substring(format, i + 1, format.count - 1);
            const s32 end_of_color = str8_find_character(remaining_string, '$');
            if (end_of_color < remaining_string.count) {
                const Str8 color_identifier = str8_substring(format, i, i + end_of_color + 1);
                set_foreground_color_based_on_string(&buffer, color_identifier);
            }
            i += end_of_color + 2;
        } else if (format.data[i] == '%' && i + 1 < format.count) {
            const char data_type = format.data[i + 1];
            print_data_type(&buffer, data_type, &arguments);
            i += 2;
        } else {
            print_character(&buffer, format.data[i]);
            ++i;
        }
    }

    flush(&buffer);
}
