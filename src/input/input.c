#include "input.h"
#include "input_kernel.h"
#include "scan_code_tables.h"
#include "interrupt/interrupt.h"
#include "port/port.h"

#define EVENT_QUEUE_CAPACITY 128

typedef struct Event_Buffer {
    OS_Input_Event data[EVENT_QUEUE_CAPACITY];
    u32 read_idx;
    u32 write_idx;
} Event_Buffer;

static Event_Buffer event_buffer;
static const Scan_Code_Table * const active_scan_code_table = &scan_code_table_en;

static
u32 advance_index(const u32 idx) {
    return (idx + 1) % ARRAY_COUNT(event_buffer.data);
}

static
void push_event(const OS_Input_Event event) {
    event_buffer.data[event_buffer.write_idx] = event;
    event_buffer.write_idx = advance_index(event_buffer.write_idx);
    if(event_buffer.read_idx == event_buffer.write_idx) {
        // We've caught back up to the read index, as the reader was too slow.
        // The new value in the `read_idx` is now the *latest* event, but the read index should
        // point at the oldest event in the queue, therefore we forcefully advance the read index here -
        // basically implicitly popping the last event from the queue
        event_buffer.read_idx = advance_index(event_buffer.read_idx);
    }
}

static
OS_Input_Event make_keyboard_event(const OS_Input_Keyboard_Event keyboard) {
    OS_Input_Event event;
    event.kind = OS_INPUT_EVENT_KIND_Keyboard;
    event.data.keyboard = keyboard;
    return event;
}

static
void keyboard_interrupt_handler(void) {
    const u8 scan_code = port_read_u8(0x60);
    const b8 down = (scan_code & 0x80) == 0;
    const u8 normalized_scan_code = scan_code & 0x7f;

    if(normalized_scan_code > 0 && normalized_scan_code < ARRAY_COUNT(active_scan_code_table->per_scan_code)) {
        const OS_Input_Key_Code key_code = active_scan_code_table->per_scan_code[normalized_scan_code].normal;
        const OS_Input_Keyboard_Event keyboard_event = (OS_Input_Keyboard_Event) { key_code, ascii_from_keycode[key_code], down };
        push_event(make_keyboard_event(keyboard_event));
    }
}

void input_initialize(void) {
    interrupt_register_callback(INTERRUPT_SIGNAL_Keyboard, keyboard_interrupt_handler);
}

b8 os_input_has_event(void) {
    return event_buffer.read_idx != event_buffer.write_idx;
}

b8 os_input_pop_event(OS_Input_Event *event) {
    if(!os_input_has_event()) {
        return false;
    }

    *event = event_buffer.data[event_buffer.read_idx];
    event_buffer.read_idx = advance_index(event_buffer.read_idx);
    return true;
}
