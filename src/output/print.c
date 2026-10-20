#include "terminal/terminal.h"

#include <stdarg.h>

#define PRINT_BUFFER_CAPACITY 256

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
void print_unsigned_int(Print_Buffer *buffer, const unsigned int value) {
    print_character(buffer, value + '0');
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
