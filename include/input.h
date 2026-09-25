#pragma once

#include "base.h"

typedef enum OS_Input_Event_Kind {
    OS_INPUT_EVENT_KIND_Keyboard,
} OS_Input_Event_Kind;

typedef enum OS_Input_Key_Code {
    OS_INPUT_KEY_Escape,
    OS_INPUT_KEY_Enter,
    OS_INPUT_KEY_Space,
} OS_Input_Key_Code;

typedef struct OS_Input_Keyboard_Event {
    OS_Input_Key_Code key_code;
    b8 down;
} OS_Input_Keyboard_Event;

typedef struct OS_Input_Event {
    OS_Input_Event_Kind kind;

    union data {
        OS_Input_Keyboard_Event keyboard;
    } data;
} OS_Input_Event;

b8 os_input_has_event(void);
b8 os_input_pop_event(OS_Input_Event *event);
