#include "input.h"
#include "input_kernel.h"
#include "input/tables.h"
#include "port/port.h"

#define EVENT_QUEUE_CAPACITY 128

typedef struct Event_Buffer {
    OS_Input_Event data[EVENT_QUEUE_CAPACITY];
    u32 read_idx;
    u32 write_idx;
} Event_Buffer;

typedef struct Key_State {
    b8 down;
} Key_State;

typedef struct Keyboard_State {
    OS_Input_Keyboard_Layout layout;
    OS_Input_Keyboard_Modifiers modifiers;
    Key_State keys[OS_INPUT_KEY_COUNT];
} Keyboard_State;

static Event_Buffer   event_buffer   = { 0 };
static Keyboard_State keyboard_state = { 0 };

static
u32 advance_index(const u32 idx) {
    return (idx + 1) % ARRAY_COUNT(event_buffer.data);
}

static
u32 utf32_from_key_code(const OS_Input_Key_Code key_code) {
    const u32 utf32 = UTF32_FROM_KEY_CODE[key_code];
    if(keyboard_state.modifiers & OS_INPUT_KEYBOARD_MODIFIERS_Shift) {
        return utf32;
    }
    if(utf32 >= 'A' && utf32 <= 'Z') {
        return utf32 - 'A' + 'a';
    }
    if(utf32 == UMLAUT_A || utf32 == UMLAUT_O || utf32 == UMLAUT_U) {
        return utf32 + 0x20;
    }
    return utf32;
}

static
void push_keyboard_event(const OS_Input_Key_Code key_code, const b8 down) {
    OS_Input_Event event;
    event.kind = OS_INPUT_EVENT_KIND_Keyboard;
    event.data.keyboard = (OS_Input_Keyboard_Event) {
        .key_code = key_code,
        .modifiers = keyboard_state.modifiers,
        .utf32 = utf32_from_key_code(key_code),
        .down = down
    };
    input_push_event(event);
}

void input_push_event(const OS_Input_Event event) {
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

void input_set_current_key_modifiers(const OS_Input_Keyboard_Modifiers modifiers) {
    keyboard_state.modifiers = modifiers;
}

void input_set_current_key_state(const OS_Input_Key_Code key_code, const b8 down) {
    if (key_code < 0 || key_code >= ARRAY_COUNT(keyboard_state.keys)) return;

    if (keyboard_state.keys[key_code].down != down) {
        keyboard_state.keys[key_code].down = down;
        push_keyboard_event(key_code, down);
    }
}

void input_trigger_key_repetition(const OS_Input_Key_Code key_code) {
    push_keyboard_event(key_code, true);
}

void os_input_set_keyboard_layout(const OS_Input_Keyboard_Layout layout) {
    keyboard_state.layout = layout;
}

OS_Input_Keyboard_Layout os_input_get_keyboard_layout(void) {
    return keyboard_state.layout;
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
