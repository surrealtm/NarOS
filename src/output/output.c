#include "output.h"
#include "print.h"

#include <stdarg.h>

void os_output_print(const Str8 format, ...) {
    va_list arguments;
    va_start(arguments, format);
    print_format(format, arguments);
    va_end(arguments);
}
