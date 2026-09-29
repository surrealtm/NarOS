#pragma once

#include "base.h"

#define TEXT_INPUT_CAPACITY 256

typedef enum Text_Input_Signal {
    TEXT_INPUT_SIGNAL_None,
    TEXT_INPUT_SIGNAL_Entered,
} Text_Input_Signal;

typedef struct Text_Input {
    char buffer[TEXT_INPUT_CAPACITY];
    u32 count;
    u32 cursor;
} Text_Input;

void text_input_initialize(Text_Input *text_input);
void text_input_clear(Text_Input *text_input);
Text_Input_Signal text_input_update(Text_Input *text_input);
Str8 text_input_content(const Text_Input *text_input);
