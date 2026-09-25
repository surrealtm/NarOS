#pragma once

#include "input.h"
#include "base.h"

#define SUPPORTED_SCAN_CODE_COUNT 0x47
#define NON_PRINTABLE_CHARACTER 0

/**
 * Maps a scan code (identified by the index in the mapping table) to the key code associated with it.
 * If `shift` or `altgr` are held, the user shall use the other members of this struct.
 */
typedef struct Scan_Code_Mapping {
    OS_Input_Key_Code normal;
/*
    OS_Input_Key_Code shift;
    OS_Input_Key_Code altgr;
    */
} Scan_Code_Mapping;

typedef struct Scan_Code_Table {
    Scan_Code_Mapping per_scan_code[SUPPORTED_SCAN_CODE_COUNT];
} Scan_Code_Table;

// @Incomplete: Missing arrow keys...
static const Scan_Code_Table scan_code_table_en = {{
    { OS_INPUT_KEY_Unknown },
    { OS_INPUT_KEY_Escape },
    { OS_INPUT_KEY_1 },
    { OS_INPUT_KEY_2 },
    { OS_INPUT_KEY_3 },
    { OS_INPUT_KEY_4 },
    { OS_INPUT_KEY_5 },
    { OS_INPUT_KEY_6 },
    { OS_INPUT_KEY_7 },
    { OS_INPUT_KEY_8 },
    { OS_INPUT_KEY_9 },
    { OS_INPUT_KEY_0 },
    { OS_INPUT_KEY_Minus },
    { OS_INPUT_KEY_Equals },
    { OS_INPUT_KEY_Backspace },
    { OS_INPUT_KEY_Tab },
    { OS_INPUT_KEY_Q },
    { OS_INPUT_KEY_W },
    { OS_INPUT_KEY_E },
    { OS_INPUT_KEY_R },
    { OS_INPUT_KEY_T },
    { OS_INPUT_KEY_Y },
    { OS_INPUT_KEY_U },
    { OS_INPUT_KEY_I },
    { OS_INPUT_KEY_O },
    { OS_INPUT_KEY_P },
    { OS_INPUT_KEY_Bracket_Open },
    { OS_INPUT_KEY_Bracket_Close },
    { OS_INPUT_KEY_Enter },
    { OS_INPUT_KEY_Left_Control },
    { OS_INPUT_KEY_A },
    { OS_INPUT_KEY_S },
    { OS_INPUT_KEY_D },
    { OS_INPUT_KEY_F },
    { OS_INPUT_KEY_G },
    { OS_INPUT_KEY_H },
    { OS_INPUT_KEY_J },
    { OS_INPUT_KEY_K },
    { OS_INPUT_KEY_L },
    { OS_INPUT_KEY_Semicolon },
    { OS_INPUT_KEY_Single_Quote },
    { OS_INPUT_KEY_Backtick },
    { OS_INPUT_KEY_Left_Shift },
    { OS_INPUT_KEY_Backslash },
    { OS_INPUT_KEY_Z },
    { OS_INPUT_KEY_X },
    { OS_INPUT_KEY_C },
    { OS_INPUT_KEY_V },
    { OS_INPUT_KEY_B },
    { OS_INPUT_KEY_N },
    { OS_INPUT_KEY_M },
    { OS_INPUT_KEY_Comma },
    { OS_INPUT_KEY_Dot },
    { OS_INPUT_KEY_Slash },
    { OS_INPUT_KEY_Right_Shift },
    { OS_INPUT_KEY_Multiply },
    { OS_INPUT_KEY_Left_Alt },
    { OS_INPUT_KEY_Space },
    { OS_INPUT_KEY_Caps_Lock },
    { OS_INPUT_KEY_F1 },
    { OS_INPUT_KEY_F2 },
    { OS_INPUT_KEY_F3 },
    { OS_INPUT_KEY_F4 },
    { OS_INPUT_KEY_F5 },
    { OS_INPUT_KEY_F6 },
    { OS_INPUT_KEY_F7 },
    { OS_INPUT_KEY_F8 },
    { OS_INPUT_KEY_F9 },
    { OS_INPUT_KEY_F10 },
    { OS_INPUT_KEY_Num_Lock },
    { OS_INPUT_KEY_Scroll_Lock },
}};

// Implemented according to: https://aeb.win.tue.nl/linux/kbd/scancodes-1.html
static const u8 ascii_from_keycode[OS_INPUT_KEY_COUNT] = {
    NON_PRINTABLE_CHARACTER,

    /* Non Printable Characters */
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
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,
    NON_PRINTABLE_CHARACTER,

    /* Printable Special Characters */
    ' ',
    '-',
    '*',
    '=',
    '{',
    '}',
    '[',
    ']',
    '\\',
    ';',
    '\'',
    '"',
    '`',
    ',',
    '.',
    '/',

    /* Latin Characters */
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

