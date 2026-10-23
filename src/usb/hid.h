#pragma once

#include "base.h"

#define HID_KEYBOARD_REPORT_KEY_COUNT 6

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
    const u8 modifiers; // Combination of HID_Keyboard_Report_Modifiers
    const u8 reserved;
    const u8 keys[HID_KEYBOARD_REPORT_KEY_COUNT]; // A set of currently pressed keys
} ALIGN_DECLARATION(16) HID_Keyboard_Report; // @Cleanup: Move the alignment to the storage in the host controller interface. Other references of this struct don't need to be aligned

void hid_parse_keyboard_report(HID_Keyboard_Report report);
