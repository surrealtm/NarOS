#pragma once

#include "base.h"

typedef enum OS_Input_Event_Kind {
    OS_INPUT_EVENT_KIND_Keyboard,
} OS_Input_Event_Kind;

typedef struct OS_Input_Event {
    OS_Input_Event_Kind kind;
} OS_Input_Event;

b8 os_input_has_event(void);
b8 os_input_pop_event(OS_Input_Event *event);
