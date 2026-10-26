#pragma once

#include "base.h"

#define HID_KEYBOARD_REPORT_KEY_COUNT 6

typedef u8 HID_Key_Code;

typedef enum HID_Keyboard_Report_Modifiers {
    HID_KEYBOARD_REPORT_MODIFIERS_Left_Control  = 1 << 0,
    HID_KEYBOARD_REPORT_MODIFIERS_Left_Shift    = 1 << 1,
    HID_KEYBOARD_REPORT_MODIFIERS_Left_Alt      = 1 << 2,
    HID_KEYBOARD_REPORT_MODIFIERS_Left_Super    = 1 << 3,
    HID_KEYBOARD_REPORT_MODIFIERS_Right_Control = 1 << 4,
    HID_KEYBOARD_REPORT_MODIFIERS_Right_Shift   = 1 << 5,
    HID_KEYBOARD_REPORT_MODIFIERS_Right_Alt     = 1 << 6,
    HID_KEYBOARD_REPORT_MODIFIERS_Right_Super   = 1 << 7,

    HID_KEYBOARD_REPORT_MODIFIERS_Shift = HID_KEYBOARD_REPORT_MODIFIERS_Left_Shift | HID_KEYBOARD_REPORT_MODIFIERS_Right_Shift,
    HID_KEYBOARD_REPORT_MODIFIERS_Control = HID_KEYBOARD_REPORT_MODIFIERS_Left_Control | HID_KEYBOARD_REPORT_MODIFIERS_Right_Control,
    HID_KEYBOARD_REPORT_MODIFIERS_Super = HID_KEYBOARD_REPORT_MODIFIERS_Left_Super | HID_KEYBOARD_REPORT_MODIFIERS_Right_Super,
} HID_Keyboard_Report_Modifiers;

typedef struct HID_Keyboard_Report {
    u8 modifiers; // Combination of HID_Keyboard_Report_Modifiers
    u8 reserved;
    HID_Key_Code pressed_keys[HID_KEYBOARD_REPORT_KEY_COUNT];
} HID_Keyboard_Report;

void hid_parse_keyboard_report(HID_Keyboard_Report report);
