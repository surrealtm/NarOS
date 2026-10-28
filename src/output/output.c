#include "output.h"
#include "print.h"
#include "terminal/terminal.h"

#include <stdarg.h>

void os_output_clear_backlog(void) {
    terminal_clear();
}

void os_output_print(const Str8 format, ...) {
    va_list arguments;
    va_start(arguments, format);
    print_format(format, arguments);
    va_end(arguments);
}
