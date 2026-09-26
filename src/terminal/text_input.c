#include "text_input.h"
#include "input.h"

void text_input_initialize(Text_Input *text_input) {
    text_input->count = 0;
    text_input->cursor = 0;
}

static
void write_character(Text_Input *text_input, char character) {
    move_memory(&text_input->buffer[text_input->cursor + 1], &text_input->buffer[text_input->cursor], text_input->count - text_input->cursor);
    text_input->buffer[text_input->cursor] = character;
    ++text_input->cursor;
    ++text_input->count;
}

void text_input_update(Text_Input *text_input) {
    OS_Input_Event event;
    while(os_input_pop_event(&event)) {
        if(event.kind != OS_INPUT_EVENT_KIND_Keyboard) continue;

        switch(event.data.keyboard.key_code) {
            default:
                if(event.data.keyboard.ascii != 0) {
                    write_character(text_input, event.data.keyboard.ascii);
                }
        }
    }
}
