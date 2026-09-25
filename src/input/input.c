#include "input.h"
#include "input_kernel.h"
#include "interrupt/interrupt.h"
#include "port/port.h"

static
void keyboard_interrupt_handler(void) {
    const u8 scan_code = port_read_u8(0x60);
    (void) scan_code;
}

void input_initialize(void) {
    interrupt_register_callback(INTERRUPT_SIGNAL_Keyboard, keyboard_interrupt_handler);
}

b8 os_input_has_event(void) {
    return false;
}

b8 os_input_pop_event(OS_Input_Event *event) {
    (void) event;
    return false;
}
