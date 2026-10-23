#include "hid.h"
#include "hid_tables.h"
#include "input/input_kernel.h"

typedef struct Parsed_Keyboard_Report {
    OS_Input_Keyboard_Modifiers modifiers;
    OS_Input_Key_Code pressed_keys[HID_KEYBOARD_REPORT_KEY_COUNT];
} Parsed_Keyboard_Report;

static Parsed_Keyboard_Report previous_keyboard_report = { 0 };

static inline
const HID_Key_Map *get_active_key_map(void) {
    switch(os_input_get_keyboard_layout()) {
        case OS_INPUT_KEYBOARD_LAYOUT_Standard_US: return hid_layout_standard_us;
        case OS_INPUT_KEYBOARD_LAYOUT_German: return hid_layout_german;
    }

    return hid_layout_standard_us;
}

static
b8 is_key_pressed(const Parsed_Keyboard_Report report, const OS_Input_Key_Code key_code) {
    for (u32 key_idx = 0; key_idx < ARRAY_COUNT(report.pressed_keys); ++key_idx) {
        if (key_code == report.pressed_keys[key_idx]) return true;
    }
    return false;
}

static
OS_Input_Key_Code key_code_from_hid(const u8 hid, const HID_Keyboard_Report_Modifiers modifiers) {
    const HID_Key_Map map = get_active_key_map()[hid];

    if (modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Shift) {
        return map.shift;
    } else if (modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Right_Alt) {
        return map.altgr;
    } else {
        return map.normal;
    }
}

static
OS_Input_Keyboard_Modifiers input_modifiers_from_report(const HID_Keyboard_Report report) {
    OS_Input_Keyboard_Modifiers modifiers = 0;

    if (report.modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Control) {
        modifiers |= OS_INPUT_KEYBOARD_MODIFIERS_Control;
    }

    if (report.modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Shift) {
        modifiers |= OS_INPUT_KEYBOARD_MODIFIERS_Shift;
    }

    if (report.modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Right_Alt) {
        modifiers |= OS_INPUT_KEYBOARD_MODIFIERS_Right_Alt;
    }

    if (report.modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Super) {
        modifiers |= OS_INPUT_KEYBOARD_MODIFIERS_Super;
    }

    return modifiers;
}

static
Parsed_Keyboard_Report parse_keyboard_report(const HID_Keyboard_Report report) {
    Parsed_Keyboard_Report result = { 0 };
    result.modifiers = input_modifiers_from_report(report);

    for (u32 key_idx = 0; key_idx < ARRAY_COUNT(report.keys); ++key_idx) {
        result.pressed_keys[key_idx] = key_code_from_hid(report.keys[key_idx], report.modifiers);
    }

    return result;
}

void hid_parse_keyboard_report(const HID_Keyboard_Report hid_report) {
    const Parsed_Keyboard_Report current_keyboard_report = parse_keyboard_report(hid_report);

    //
    // Find all keys that have been released in this new report
    //
    for (u32 key_idx = 0; key_idx < ARRAY_COUNT(previous_keyboard_report.pressed_keys); ++key_idx) {
        if (!is_key_pressed(current_keyboard_report, previous_keyboard_report.pressed_keys[key_idx])) {
            input_set_current_key_state(previous_keyboard_report.pressed_keys[key_idx], false);
        }
    }

    input_set_current_key_modifiers(current_keyboard_report.modifiers);

    //
    // Find all keys that have been pressed in this new report
    //
    for (u32 key_idx = 0; key_idx < ARRAY_COUNT(current_keyboard_report.pressed_keys); ++key_idx) {
        if (!is_key_pressed(previous_keyboard_report, current_keyboard_report.pressed_keys[key_idx])) {
            input_set_current_key_state(current_keyboard_report.pressed_keys[key_idx], true);
        }
    }

    previous_keyboard_report = current_keyboard_report;
}
