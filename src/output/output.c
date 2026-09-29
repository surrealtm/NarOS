#include "output.h"
#include "terminal/terminal.h"

void os_output_print(Str8 string) {
    terminal_print_string(string);
}
