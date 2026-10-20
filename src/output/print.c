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
void print_character(Print_Buffer *buffer, char character) {
    reserve(buffer, 1);
    buffer->characters[buffer->cursor] = character;
    ++buffer->cursor;
}

void print_format(const Str8 format, ...) {
    va_list arguments;
    va_start(arguments, format);

    Print_Buffer buffer = { .cursor = 0 };

    for (s32 i = 0; i < format.count;) {
        if (format.data[i] == '%') {

        } else {
            print_character(&buffer, format.data[i]);
            ++i;
        }
    }

    flush(&buffer);

    va_end(arguments);
}
