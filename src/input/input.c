#include "input.h"
#include "interrupt/interrupt.h"

b8 os_input_has_event(void) {
    return false;
}

b8 os_input_pop_event(OS_Input_Event *event) {
    (void) event;
    return false;
}
