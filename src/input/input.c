#include "input.h"
#include "input_kernel.h"
#include "interrupt/interrupt.h"
#include "port/port.h"

#define SUPPORTED_SCAN_CODE_COUNT 0x39

static
OS_Input_Key_Code scan_code_table[SUPPORTED_SCAN_CODE_COUNT] = {
    0xff, // A scan code of 0 represents an error
    OS_INPUT_KEY_Escape,
};

static
OS_Input_Event make_keyboard_event(const OS_Input_Keyboard_Event keyboard) {
    OS_Input_Event event;
    event.kind = OS_INPUT_EVENT_KIND_Keyboard;
    event.data.keyboard = keyboard;
    return event;
}

static
void push_event(const OS_Input_Event event) {
    (void) event;
}

static
void keyboard_interrupt_handler(void) {
    const u8 down_indicator = 0x80;
    const u8 scan_code = port_read_u8(0x60);
    const b8 down = scan_code >= down_indicator;
    const u8 normalized_scan_code = (down) ? scan_code - down_indicator : scan_code;

    if(normalized_scan_code > 0 && normalized_scan_code < ARRAY_COUNT(scan_code_table)) {
        const OS_Input_Key_Code key_code = scan_code_table[normalized_scan_code];
        const OS_Input_Keyboard_Event keyboard_event = (OS_Input_Keyboard_Event) { key_code, down };
        push_event(make_keyboard_event(keyboard_event));
    }
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
