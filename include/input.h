#pragma once

#include "base.h"

typedef enum OS_Input_Keyboard_Layout {
    OS_INPUT_KEYBOARD_LAYOUT_Standard_US,
    OS_INPUT_KEYBOARD_LAYOUT_German,
} OS_Input_Keyboard_Layout;

typedef enum OS_Input_Event_Kind {
    OS_INPUT_EVENT_KIND_Keyboard,
} OS_Input_Event_Kind;

typedef enum OS_Input_Key_Code {
    OS_INPUT_KEY_Unknown,

    /* Non Printable Characters */
    OS_INPUT_KEY_Escape,
    OS_INPUT_KEY_Enter,
    OS_INPUT_KEY_Backspace,
    OS_INPUT_KEY_Delete,
    OS_INPUT_KEY_Tab,
    OS_INPUT_KEY_Left_Control,
    OS_INPUT_KEY_Right_Control,
    OS_INPUT_KEY_Left_Shift,
    OS_INPUT_KEY_Right_Shift,
    OS_INPUT_KEY_Left_Alt,
    OS_INPUT_KEY_Right_Alt,
    OS_INPUT_KEY_Caps_Lock,
    OS_INPUT_KEY_Num_Lock,
    OS_INPUT_KEY_Scroll_Lock,
    OS_INPUT_KEY_Arrow_Up,
    OS_INPUT_KEY_Arrow_Right,
    OS_INPUT_KEY_Arrow_Down,
    OS_INPUT_KEY_Arrow_Left,
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

    /* Printable Special Characters */
    OS_INPUT_KEY_Space,
    OS_INPUT_KEY_Minus,
    OS_INPUT_KEY_Multiply,
    OS_INPUT_KEY_Equals,
    OS_INPUT_KEY_Exclamation_Mark,
    OS_INPUT_KEY_At,
    OS_INPUT_KEY_Number_Sign,
    OS_INPUT_KEY_Section,
    OS_INPUT_KEY_Dollar,
    OS_INPUT_KEY_Percent,
    OS_INPUT_KEY_Caret,
    OS_INPUT_KEY_Ampersand,
    OS_INPUT_KEY_Parenthesis_Open,
    OS_INPUT_KEY_Parenthesis_Close,
    OS_INPUT_KEY_Underscore,
    OS_INPUT_KEY_Plus,
    OS_INPUT_KEY_Brace_Open,
    OS_INPUT_KEY_Brace_Close,
    OS_INPUT_KEY_Bracket_Open,
    OS_INPUT_KEY_Bracket_Close,
    OS_INPUT_KEY_Backslash,
    OS_INPUT_KEY_Vertical_Bar,
    OS_INPUT_KEY_Semicolon,
    OS_INPUT_KEY_Colon,
    OS_INPUT_KEY_Single_Quote,
    OS_INPUT_KEY_Double_Quote,
    OS_INPUT_KEY_Backtick,
    OS_INPUT_KEY_Tilde,
    OS_INPUT_KEY_Comma,
    OS_INPUT_KEY_Less_Than,
    OS_INPUT_KEY_Dot,
    OS_INPUT_KEY_Greater_Than,
    OS_INPUT_KEY_Slash,
    OS_INPUT_KEY_Question_Mark,
    OS_INPUT_KEY_Acute_Accent,

    /* German Keys */
    OS_INPUT_KEY_Sharp_S,
    OS_INPUT_KEY_Umlaut_A,
    OS_INPUT_KEY_Umlaut_O,
    OS_INPUT_KEY_Umlaut_U,

    /* Latin Characters */
    OS_INPUT_KEY_0,
    OS_INPUT_KEY_1,
    OS_INPUT_KEY_2,
    OS_INPUT_KEY_3,
    OS_INPUT_KEY_4,
    OS_INPUT_KEY_5,
    OS_INPUT_KEY_6,
    OS_INPUT_KEY_7,
    OS_INPUT_KEY_8,
    OS_INPUT_KEY_9,
    OS_INPUT_KEY_A,
    OS_INPUT_KEY_B,
    OS_INPUT_KEY_C,
    OS_INPUT_KEY_D,
    OS_INPUT_KEY_E,
    OS_INPUT_KEY_F,
    OS_INPUT_KEY_G,
    OS_INPUT_KEY_H,
    OS_INPUT_KEY_I,
    OS_INPUT_KEY_J,
    OS_INPUT_KEY_K,
    OS_INPUT_KEY_L,
    OS_INPUT_KEY_M,
    OS_INPUT_KEY_N,
    OS_INPUT_KEY_O,
    OS_INPUT_KEY_P,
    OS_INPUT_KEY_Q,
    OS_INPUT_KEY_R,
    OS_INPUT_KEY_S,
    OS_INPUT_KEY_T,
    OS_INPUT_KEY_U,
    OS_INPUT_KEY_V,
    OS_INPUT_KEY_W,
    OS_INPUT_KEY_X,
    OS_INPUT_KEY_Y,
    OS_INPUT_KEY_Z,

    /* Terminator */
    OS_INPUT_KEY_COUNT,
} OS_Input_Key_Code;

typedef struct OS_Input_Keyboard_Event {
    OS_Input_Key_Code key_code;
    u8 ascii;
    b8 down;
    b8 shift_down;
    b8 right_alt_down;
} OS_Input_Keyboard_Event;

typedef struct OS_Input_Event {
    OS_Input_Event_Kind kind;

    union data {
        OS_Input_Keyboard_Event keyboard;
    } data;
} OS_Input_Event;

/**
 * Sets the scan code mapping table to reflect the requested keyboard layout.
 */
void os_input_set_keyboard_layout(OS_Input_Keyboard_Layout layout);

/**
 * Checks if there's currently any input event available to be popped
 */
b8 os_input_has_event(void);

/**
 * If any input event is available, that event is written into the passed pointer and true is returned.
 * Otherwise, the pointer is untouched and false is returned.
 */
b8 os_input_pop_event(OS_Input_Event *event);
