#include "output.h"
#include "terminal/terminal.h"

void os_output_print(const char *string) {
    terminal_print_string(string);
}
