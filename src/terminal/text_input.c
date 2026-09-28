#include "text_input.h"
#include "input.h"

static
b8 is_empty_character(const char character) {
    return character == 0 || character == ' ';
}

static
u32 find_control_point_to_the_left(const Text_Input *text_input) {
    u32 idx = text_input->cursor;

    // Skip all leading empty characters until the start of a word
    while(idx > 0 && is_empty_character(text_input->buffer[idx])) --idx;

    // Skip until the end of this word
    while(idx > 0 && !is_empty_character(text_input->buffer[idx])) --idx;

    return idx;
}

static
u32 find_control_point_to_the_right(const Text_Input *text_input) {
    u32 idx = text_input->cursor;

    // Skip all leading empty characters until the start of a word
    while(idx + 1 < text_input->count && is_empty_character(text_input->buffer[idx])) ++idx;

    // Skip until the end of this word
    while(idx + 1 < text_input->count && !is_empty_character(text_input->buffer[idx])) ++idx;

    // Skip all trailing empty characters until the start of the next word
    while(idx < text_input->count && is_empty_character(text_input->buffer[idx + 1])) ++idx;

    return idx;
}

static
void insert_text(Text_Input *text_input, const char *bytes, const u32 requested_byte_count) {
    const u32 byte_count = min(requested_byte_count, ARRAY_COUNT(text_input->buffer) - text_input->count - 1); // Ensure that the last character in the buffer remains 0 as we're working with null-terminated strings...
    move_memory(&text_input->buffer[text_input->cursor + byte_count], &text_input->buffer[text_input->cursor], text_input->count - text_input->cursor);
    move_memory(&text_input->buffer[text_input->cursor], bytes, byte_count);
    text_input->cursor += byte_count;
    text_input->count += byte_count;
}

static
void erase_text(Text_Input *text_input, const u32 first_to_remove, const u32 last_to_remove) {
    if (last_to_remove < first_to_remove || first_to_remove >= text_input->count) return;

    const u32 chars_to_remove = last_to_remove - first_to_remove + 1;
    move_memory(&text_input->buffer[first_to_remove], &text_input->buffer[last_to_remove + 1], text_input->count - (last_to_remove + 1));
    set_memory(&text_input->buffer[text_input->count - chars_to_remove], 0, chars_to_remove);
    text_input->count -= chars_to_remove;
    text_input->cursor = first_to_remove;
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

        const b8 control = !!(event.data.keyboard.modifiers & OS_INPUT_KEYBOARD_MODIFIERS_Control);

        switch(event.data.keyboard.key_code) {
            case OS_INPUT_KEY_Enter:
                signal = TEXT_INPUT_SIGNAL_Entered;
                break;

            case OS_INPUT_KEY_Backspace: {
                const u32 last_idx = text_input->cursor - 1;
                const u32 first_idx = control ? find_control_point_to_the_left(text_input) : last_idx;
                erase_text(text_input, first_idx, last_idx);
            } break;

            case OS_INPUT_KEY_Delete: {
                const u32 first_idx = text_input->cursor;
                const u32 last_idx = control ? min(find_control_point_to_the_right(text_input), text_input->count - 1) : first_idx;
                erase_text(text_input, first_idx, last_idx);
            } break;

            case OS_INPUT_KEY_Arrow_Left: {
                if(text_input->cursor == 0) break;
                const u32 target_cursor = control ? find_control_point_to_the_left(text_input) : text_input->cursor - 1;
                text_input->cursor = target_cursor;
            } break;

            case OS_INPUT_KEY_Arrow_Right: {
                if(text_input->cursor == text_input->count) break;
                const u32 target_cursor = control ? find_control_point_to_the_right(text_input) : text_input->cursor + 1;
                text_input->cursor = target_cursor;
            } break;

            default:
                if(event.data.keyboard.key_code == OS_INPUT_KEY_C && event.data.keyboard.modifiers & OS_INPUT_KEYBOARD_MODIFIERS_Control) {
                    text_input_clear(text_input);
                } else if(event.data.keyboard.utf32 >= 0x20 && event.data.keyboard.utf32 < 0xff) {
                    insert_text(text_input, (const char *) &event.data.keyboard.utf32, 1);
                }
        }
    }

    return signal;
}
