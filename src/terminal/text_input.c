#include "text_input.h"
#include "input.h"

static
void write_character(Text_Input *text_input, char character) {
    if(text_input->count >= ARRAY_COUNT(text_input->buffer) - 1) {
        // Ensure that the last character in the buffer remains 0 as we're working with null-terminated strings...
        return;
    }
    move_memory(&text_input->buffer[text_input->cursor + 1], &text_input->buffer[text_input->cursor], text_input->count - text_input->cursor);
    text_input->buffer[text_input->cursor] = character;
    ++text_input->cursor;
    ++text_input->count;
}

void text_input_initialize(Text_Input *text_input) {
    text_input_clear(text_input);
}

void text_input_clear(Text_Input *text_input) {
    text_input->count = 0;
    text_input->cursor = 0;
    set_memory(text_input->buffer, 0, sizeof(text_input->buffer));
}

Text_Input_Signal text_input_update(Text_Input *text_input) {
    Text_Input_Signal signal = TEXT_INPUT_SIGNAL_None;

    OS_Input_Event event;
    while(os_input_pop_event(&event)) {
        if(event.kind != OS_INPUT_EVENT_KIND_Keyboard || !event.data.keyboard.down) continue;

        switch(event.data.keyboard.key_code) {
            case OS_INPUT_KEY_Enter:
                signal = TEXT_INPUT_SIGNAL_Entered;
                break;

            default:
                if(event.data.keyboard.ascii != 0) {
                    write_character(text_input, event.data.keyboard.ascii);
                }
        }
    }

    return signal;
}
