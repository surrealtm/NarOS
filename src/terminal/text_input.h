#pragma once

#include "base.h"

#define TEXT_INPUT_CAPACITY 256

typedef struct Text_Input {
    char buffer[TEXT_INPUT_CAPACITY];
    u32 count;
    u32 cursor;
} Text_Input;

void text_input_initialize(Text_Input *text_input);
void text_input_update(Text_Input *text_input);
