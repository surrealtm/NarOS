#include "terminal/terminal.h"

#include <stdarg.h>

#define PRINT_BUFFER_CAPACITY 256

// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

static
char character_from_digit(const u8 digit) {
    if (digit >= 0 && digit <= 9) {
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
void print_data_type(Print_Buffer *buffer, const char data_type, va_list *arguments) {
    switch(data_type) {
        case 'u': {
            const unsigned int value = va_arg(*arguments, unsigned int);
            print_unsigned_int(buffer, value);
        } break;
    }
}

void print_format(const Str8 format, va_list arguments) {
    Print_Buffer buffer = { .cursor = 0 };

    for (s32 i = 0; i < format.count;) {
        if (format.data[i] == '%' && i + 1 < format.count) {
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
