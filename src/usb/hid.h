#pragma once

#include "base.h"

typedef enum HID_Keyboard_Report_Modifiers {
    HID_KEYBOARD_REPORT_MODIFIERS_Left_Control  = 1 << 0,
    HID_KEYBOARD_REPORT_MODIFIERS_Left_Shift    = 1 << 1,
    HID_KEYBOARD_REPORT_MODIFIERS_Left_Alt      = 1 << 2,
    HID_KEYBOARD_REPORT_MODIFIERS_Left_Super    = 1 << 3,
    HID_KEYBOARD_REPORT_MODIFIERS_Right_Control = 1 << 4,
    HID_KEYBOARD_REPORT_MODIFIERS_Right_Shift   = 1 << 5,
    HID_KEYBOARD_REPORT_MODIFIERS_Right_Alt     = 1 << 6,
    HID_KEYBOARD_REPORT_MODIFIERS_Right_Super   = 1 << 7,
} Hid_Keyboard_Report_Modifiers;

typedef struct HID_Keyboard_Report {
    const u8 modifiers; // Combination of HID_Keyboard_Report_Modifiers
    const u8 reserved;
    const u8 keys[6]; // A set of currently pressed keys
} ALIGN_DECLARATION(16) HID_Keyboard_Report;

void hid_parse_keyboard_report(HID_Keyboard_Report report);
