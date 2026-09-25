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
static const Scan_Code_Table scan_code_table_en = {
    .per_scan_code = {
        [0x00] = { .normal = OS_INPUT_KEY_Unknown },
        [0x01] = { .normal = OS_INPUT_KEY_Escape },
        [0x02] = { .normal = OS_INPUT_KEY_1 },
        [0x03] = { .normal = OS_INPUT_KEY_2 },
        [0x04] = { .normal = OS_INPUT_KEY_3 },
        [0x05] = { .normal = OS_INPUT_KEY_4 },
        [0x06] = { .normal = OS_INPUT_KEY_5 },
        [0x07] = { .normal = OS_INPUT_KEY_6 },
        [0x08] = { .normal = OS_INPUT_KEY_7 },
        [0x09] = { .normal = OS_INPUT_KEY_8 },
        [0x0A] = { .normal = OS_INPUT_KEY_9 },
        [0x0B] = { .normal = OS_INPUT_KEY_0 },
        [0x0C] = { .normal = OS_INPUT_KEY_Minus },
        [0x0D] = { .normal = OS_INPUT_KEY_Equals },
        [0x0E] = { .normal = OS_INPUT_KEY_Backspace },
        [0x0F] = { .normal = OS_INPUT_KEY_Tab },
        [0x10] = { .normal = OS_INPUT_KEY_Q },
        [0x11] = { .normal = OS_INPUT_KEY_W },
        [0x12] = { .normal = OS_INPUT_KEY_E },
        [0x13] = { .normal = OS_INPUT_KEY_R },
        [0x14] = { .normal = OS_INPUT_KEY_T },
        [0x15] = { .normal = OS_INPUT_KEY_Y },
        [0x16] = { .normal = OS_INPUT_KEY_U },
        [0x17] = { .normal = OS_INPUT_KEY_I },
        [0x18] = { .normal = OS_INPUT_KEY_O },
        [0x19] = { .normal = OS_INPUT_KEY_P },
        [0x1A] = { .normal = OS_INPUT_KEY_Bracket_Open },
        [0x1B] = { .normal = OS_INPUT_KEY_Bracket_Close },
        [0x1C] = { .normal = OS_INPUT_KEY_Enter },
        [0x1D] = { .normal = OS_INPUT_KEY_Left_Control },
        [0x1E] = { .normal = OS_INPUT_KEY_A },
        [0x1F] = { .normal = OS_INPUT_KEY_S },
        [0x20] = { .normal = OS_INPUT_KEY_D },
        [0x21] = { .normal = OS_INPUT_KEY_F },
        [0x22] = { .normal = OS_INPUT_KEY_G },
        [0x23] = { .normal = OS_INPUT_KEY_H },
        [0x24] = { .normal = OS_INPUT_KEY_J },
        [0x25] = { .normal = OS_INPUT_KEY_K },
        [0x26] = { .normal = OS_INPUT_KEY_L },
        [0x27] = { .normal = OS_INPUT_KEY_Semicolon },
        [0x28] = { .normal = OS_INPUT_KEY_Single_Quote },
        [0x29] = { .normal = OS_INPUT_KEY_Backtick },
        [0x2A] = { .normal = OS_INPUT_KEY_Left_Shift },
        [0x2B] = { .normal = OS_INPUT_KEY_Backslash },
        [0x2C] = { .normal = OS_INPUT_KEY_Z },
        [0x2D] = { .normal = OS_INPUT_KEY_X },
        [0x2E] = { .normal = OS_INPUT_KEY_C },
        [0x2F] = { .normal = OS_INPUT_KEY_V },
        [0x30] = { .normal = OS_INPUT_KEY_B },
        [0x31] = { .normal = OS_INPUT_KEY_N },
        [0x32] = { .normal = OS_INPUT_KEY_M },
        [0x33] = { .normal = OS_INPUT_KEY_Comma },
        [0x34] = { .normal = OS_INPUT_KEY_Dot },
        [0x35] = { .normal = OS_INPUT_KEY_Slash },
        [0x36] = { .normal = OS_INPUT_KEY_Right_Shift },
        [0x37] = { .normal = OS_INPUT_KEY_Multiply },
        [0x38] = { .normal = OS_INPUT_KEY_Left_Alt },
        [0x39] = { .normal = OS_INPUT_KEY_Space },
        [0x3A] = { .normal = OS_INPUT_KEY_Caps_Lock },
        [0x3B] = { .normal = OS_INPUT_KEY_F1 },
        [0x3C] = { .normal = OS_INPUT_KEY_F2 },
        [0x3D] = { .normal = OS_INPUT_KEY_F3 },
        [0x3E] = { .normal = OS_INPUT_KEY_F4 },
        [0x3F] = { .normal = OS_INPUT_KEY_F5 },
        [0x40] = { .normal = OS_INPUT_KEY_F6 },
        [0x41] = { .normal = OS_INPUT_KEY_F7 },
        [0x42] = { .normal = OS_INPUT_KEY_F8 },
        [0x43] = { .normal = OS_INPUT_KEY_F9 },
        [0x44] = { .normal = OS_INPUT_KEY_F10 },
        [0x45] = { .normal = OS_INPUT_KEY_Num_Lock },
        [0x46] = { .normal = OS_INPUT_KEY_Scroll_Lock },
    },
};

// Implemented according to: https://aeb.win.tue.nl/linux/kbd/scancodes-1.html
static const u8 ascii_from_keycode[OS_INPUT_KEY_COUNT] = {
    [OS_INPUT_KEY_Unknown] = NON_PRINTABLE_CHARACTER,

    /* Non Printable Characters */
    [OS_INPUT_KEY_Escape] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Enter] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Backspace] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Tab] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Left_Control] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Left_Shift] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Right_Shift] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Left_Alt] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Caps_Lock] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Num_Lock] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_Scroll_Lock] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F1] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F2] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F3] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F4] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F5] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F6] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F7] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F8] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F9] = NON_PRINTABLE_CHARACTER,
    [OS_INPUT_KEY_F10] = NON_PRINTABLE_CHARACTER,

    /* Printable Special Characters */
    [OS_INPUT_KEY_Space] = ' ',
    [OS_INPUT_KEY_Minus] = '-',
    [OS_INPUT_KEY_Multiply] = '*',
    [OS_INPUT_KEY_Equals] = '=',
    [OS_INPUT_KEY_Brace_Open] = '{',
    [OS_INPUT_KEY_Brace_Close] = '}',
    [OS_INPUT_KEY_Bracket_Open] = '[',
    [OS_INPUT_KEY_Bracket_Close] = ']',
    [OS_INPUT_KEY_Backslash] = '\\',
    [OS_INPUT_KEY_Semicolon] = ';',
    [OS_INPUT_KEY_Single_Quote] = '\'',
    [OS_INPUT_KEY_Double_Quote] = '"',
    [OS_INPUT_KEY_Backtick] = '`',
    [OS_INPUT_KEY_Comma] = ',',
    [OS_INPUT_KEY_Dot] = '.',
    [OS_INPUT_KEY_Slash] = '/',

    /* Latin Characters */
    [OS_INPUT_KEY_0] = '0',
    [OS_INPUT_KEY_1] = '1',
    [OS_INPUT_KEY_2] = '2',
    [OS_INPUT_KEY_3] = '3',
    [OS_INPUT_KEY_4] = '4',
    [OS_INPUT_KEY_5] = '5',
    [OS_INPUT_KEY_6] = '6',
    [OS_INPUT_KEY_7] = '7',
    [OS_INPUT_KEY_8] = '8',
    [OS_INPUT_KEY_9] = '9',
    [OS_INPUT_KEY_A] = 'A',
    [OS_INPUT_KEY_B] = 'B',
    [OS_INPUT_KEY_C] = 'C',
    [OS_INPUT_KEY_D] = 'D',
    [OS_INPUT_KEY_E] = 'E',
    [OS_INPUT_KEY_F] = 'F',
    [OS_INPUT_KEY_G] = 'G',
    [OS_INPUT_KEY_H] = 'H',
    [OS_INPUT_KEY_I] = 'I',
    [OS_INPUT_KEY_J] = 'J',
    [OS_INPUT_KEY_K] = 'K',
    [OS_INPUT_KEY_L] = 'L',
    [OS_INPUT_KEY_M] = 'M',
    [OS_INPUT_KEY_N] = 'N',
    [OS_INPUT_KEY_O] = 'O',
    [OS_INPUT_KEY_P] = 'P',
    [OS_INPUT_KEY_Q] = 'Q',
    [OS_INPUT_KEY_R] = 'R',
    [OS_INPUT_KEY_S] = 'S',
    [OS_INPUT_KEY_T] = 'T',
    [OS_INPUT_KEY_U] = 'U',
    [OS_INPUT_KEY_V] = 'V',
    [OS_INPUT_KEY_W] = 'W',
    [OS_INPUT_KEY_X] = 'X',
    [OS_INPUT_KEY_Y] = 'Y',
    [OS_INPUT_KEY_Z] = 'Z',
};
