#include "test.h"
#include <stdio.h>
#include <string.h>

#define OUTPUT_WIDTH 80

#define COLOR_RED    "\033[0;31m"
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_RESET  "\033[0m"

Test_Definition *first_test_definition = 0;

static unsigned current_failure_indicator;
static char current_failure_reason[1024];

void fail_current_test(const char *file, unsigned int line, const char *reason) {
    snprintf(current_failure_reason, sizeof(current_failure_reason), "%s:%d: %s", file, line, reason);
    current_failure_indicator = 1;
}

void register_test_definition(Test_Definition *definition) {
    if (first_test_definition == 0) {
        first_test_definition = definition;
    } else {
        definition->next = first_test_definition;
        first_test_definition = definition;
    }
}

static
void pad(const char character, const int count) {
    for (int i = 0; i < count; ++i) {
        printf("%c", character);
    }
}

int main(void) {
    int failed_test_count = 0;

    Test_Definition *current_test_definition = first_test_definition;
    while (current_test_definition) {
        const int name_width = printf("  - %s.%s:", current_test_definition->suite, current_test_definition->name);

        current_failure_indicator = 0;
        current_test_definition->function(); // @Incomplete: How to detect whether this test actually passed?

        if (current_failure_indicator) {
            pad('.', OUTPUT_WIDTH - name_width - strlen("FAIL"));
            printf(COLOR_RED "FAIL" COLOR_RESET "\n");
            printf("    -> %s\n", current_failure_reason);
        } else {
            pad('.', OUTPUT_WIDTH - name_width - strlen("OK"));
            printf(COLOR_GREEN "OK" COLOR_RESET "\n");
        }

        failed_test_count += current_failure_indicator;
        current_test_definition = current_test_definition->next;
    }

    return failed_test_count;
}
