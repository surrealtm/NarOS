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

static
void move_cursor_to_the_left(Text_Input *text_input) {
    if(text_input->cursor == 0) {
        return;
    }
    --text_input->cursor;
}

static
void move_cursor_to_the_right(Text_Input *text_input) {
    if(text_input->cursor == text_input->count) {
        return;
    }
    ++text_input->cursor;
}

static
void remove_character_to_the_right(Text_Input *text_input) {
    if(text_input->cursor == text_input->count) {
        return;
    }
    move_memory(&text_input->buffer[text_input->cursor], &text_input->buffer[text_input->cursor + 1], text_input->count - text_input->cursor);
    text_input->buffer[text_input->count - 1] = 0;
    --text_input->count;
}

static
void remove_character_to_the_left(Text_Input *text_input) {
    if(text_input->cursor == 0) {
        return;
    }
    move_memory(&text_input->buffer[text_input->cursor - 1], &text_input->buffer[text_input->cursor], text_input->count - text_input->cursor + 1);
    text_input->buffer[text_input->count - 1] = 0;
    --text_input->cursor;
    --text_input->count;
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

            case OS_INPUT_KEY_Backspace:
                remove_character_to_the_left(text_input);
                break;

            case OS_INPUT_KEY_Delete:
                remove_character_to_the_right(text_input);
                break;

            case OS_INPUT_KEY_Arrow_Left:
                move_cursor_to_the_left(text_input);
                break;

            case OS_INPUT_KEY_Arrow_Right:
                move_cursor_to_the_right(text_input);
                break;

            default:
                if(event.data.keyboard.ascii != 0) {
                    write_character(text_input, event.data.keyboard.ascii);
                }
        }
    }

    return signal;
}
