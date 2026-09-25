#include "input.h"
#include "input_kernel.h"
#include "interrupt/interrupt.h"
#include "port/port.h"

#define EVENT_QUEUE_CAPACITY 128
#define SUPPORTED_SCAN_CODE_COUNT 0x47
#define NON_PRINTABLE_CHARACTER 0

// Implemented according to: https://aeb.win.tue.nl/linux/kbd/scancodes-1.html
static
OS_Input_Key_Code scan_code_table[SUPPORTED_SCAN_CODE_COUNT] = {
    OS_INPUT_KEY_Unknown,
    OS_INPUT_KEY_Escape,
    OS_INPUT_KEY_1,
    OS_INPUT_KEY_2,
    OS_INPUT_KEY_3,
    OS_INPUT_KEY_4,
    OS_INPUT_KEY_5,
    OS_INPUT_KEY_6,
    OS_INPUT_KEY_7,
    OS_INPUT_KEY_8,
    OS_INPUT_KEY_9,
    OS_INPUT_KEY_0,
    OS_INPUT_KEY_Minus,
    OS_INPUT_KEY_Equals,
    OS_INPUT_KEY_Backspace,
    OS_INPUT_KEY_Tab,
    OS_INPUT_KEY_Q,
    OS_INPUT_KEY_W,
    OS_INPUT_KEY_E,
    OS_INPUT_KEY_R,
    OS_INPUT_KEY_T,
    OS_INPUT_KEY_Y,
    OS_INPUT_KEY_U,
    OS_INPUT_KEY_I,
    OS_INPUT_KEY_O,
    OS_INPUT_KEY_P,
    OS_INPUT_KEY_Brace_Close,
    OS_INPUT_KEY_Brace_Open,
    OS_INPUT_KEY_Enter,
    OS_INPUT_KEY_Left_Control,
    OS_INPUT_KEY_A,
    OS_INPUT_KEY_S,
    OS_INPUT_KEY_D,
    OS_INPUT_KEY_F,
    OS_INPUT_KEY_G,
    OS_INPUT_KEY_H,
    OS_INPUT_KEY_J,
    OS_INPUT_KEY_K,
    OS_INPUT_KEY_L,
    OS_INPUT_KEY_Semicolon,
    OS_INPUT_KEY_Quote,
    OS_INPUT_KEY_Backtick,
    OS_INPUT_KEY_Left_Shift,
    OS_INPUT_KEY_Backslash,
    OS_INPUT_KEY_Z,
    OS_INPUT_KEY_X,
    OS_INPUT_KEY_C,
    OS_INPUT_KEY_V,
    OS_INPUT_KEY_B,
    OS_INPUT_KEY_N,
    OS_INPUT_KEY_M,
    OS_INPUT_KEY_Comma,
    OS_INPUT_KEY_Dot,
    OS_INPUT_KEY_Slash,
    OS_INPUT_KEY_Right_Shift,
    OS_INPUT_KEY_Unknown,
    OS_INPUT_KEY_Left_Alt,
    OS_INPUT_KEY_Space,
    OS_INPUT_KEY_Caps_Lock,
    OS_INPUT_KEY_F1,
    OS_INPUT_KEY_F2,
    OS_INPUT_KEY_F3,
    OS_INPUT_KEY_F4,
    OS_INPUT_KEY_F5,
    OS_INPUT_KEY_F6,
    OS_INPUT_KEY_F7,
    OS_INPUT_KEY_F8,
    OS_INPUT_KEY_F9,
    OS_INPUT_KEY_F10,
    OS_INPUT_KEY_Num_Lock,
    OS_INPUT_KEY_Scroll_Lock,
};

static
u8 ascii_table[OS_INPUT_KEY_COUNT] = {
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    '-',
    '=',
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    '{',
    '}',
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    '\\',
    ';',
    '"',
    '`',
    NON_PRINTABLE_CHARACTER,
    ',',
    '.',
    '/',
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    ' ',
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    '0',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    'A',
    'B',
    'C',
    'D',
    'E',
    'F',
    'G',
    'H',
    'I',
    'J',
    'K',
    'L',
    'M',
    'N',
    'O',
    'P',
    'Q',
    'R',
    'S',
    'T',
    'U',
    'V',
    'W',
    'X',
    'Y',
    'Z',
};

typedef struct Event_Buffer {
    OS_Input_Event data[EVENT_QUEUE_CAPACITY];
    u32 read_idx;
    u32 write_idx;
} Event_Buffer;

static Event_Buffer event_buffer;

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
    const u8 down_indicator = 0x80;
    const u8 scan_code = port_read_u8(0x60);
    const b8 down = scan_code >= down_indicator;
    const u8 normalized_scan_code = (down) ? scan_code - down_indicator : scan_code;

    if(normalized_scan_code > 0 && normalized_scan_code < ARRAY_COUNT(scan_code_table)) {
        const OS_Input_Key_Code key_code = scan_code_table[normalized_scan_code];
        const OS_Input_Keyboard_Event keyboard_event = (OS_Input_Keyboard_Event) { key_code, ascii_table[key_code], down };
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
