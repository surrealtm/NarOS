#include "test.h"
#include <stdio.h>

#define COLOR_RED    "\033[0;31m"
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_RESET  "\033[0m"

Test_Definition *first_test_definition = 0;

void register_test_definition(Test_Definition *definition) {
    if (first_test_definition == 0) {
        first_test_definition = definition;
    } else {
        definition->next = first_test_definition;
        first_test_definition = definition;
    }
}

int main(void) {
    int failed_test_count = 0;

    Test_Definition *current_test_definition = first_test_definition;
    while (current_test_definition) {
        printf("  - %s.%s: ", current_test_definition->suite, current_test_definition->name);
        current_test_definition->function(); // @Incomplete: How to detect whether this test actually passed?
        printf(" " COLOR_GREEN "OK" COLOR_RESET ".\n");
        current_test_definition = current_test_definition->next;
    }

    return failed_test_count;
}
