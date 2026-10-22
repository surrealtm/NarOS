#include "hid.h"
#include "input/input_kernel.h"

static
OS_Input_Keyboard_Modifiers input_modifiers_from_report(const HID_Keyboard_Report report) {
    OS_Input_Keyboard_Modifiers modifiers = 0;

    if (report.modifiers & (HID_KEYBOARD_REPORT_MODIFIERS_Left_Control | HID_KEYBOARD_REPORT_MODIFIERS_Right_Control)) {
        modifiers |= OS_INPUT_KEYBOARD_MODIFIERS_Control;
    }

    if (report.modifiers & (HID_KEYBOARD_REPORT_MODIFIERS_Left_Shift | HID_KEYBOARD_REPORT_MODIFIERS_Right_Shift)) {
        modifiers |= OS_INPUT_KEYBOARD_MODIFIERS_Shift;
    }

    if (report.modifiers & (HID_KEYBOARD_REPORT_MODIFIERS_Right_Alt)) {
        modifiers |= OS_INPUT_KEYBOARD_MODIFIERS_Right_Alt;
    }

    if (report.modifiers & (HID_KEYBOARD_REPORT_MODIFIERS_Left_Super | HID_KEYBOARD_REPORT_MODIFIERS_Right_Super)) {
        // @Incomplete: Super modifier missing in the input system
    }

    return modifiers;
}

void hid_parse_keyboard_report(const HID_Keyboard_Report report) {
    const OS_Input_Keyboard_Modifiers modifiers = input_modifiers_from_report(report);
    input_set_current_key_modifiers(modifiers);

    for (u32 key_idx = 0; key_idx < ARRAY_COUNT(report.keys); ++key_idx) {
        if (report.keys[key_idx] == 0) continue;

        // @Incomplete: Convert the hid code into a key code. What about different keyboard layouts?
        input_set_current_key_state(OS_INPUT_KEY_A, true);
        input_set_current_key_state(OS_INPUT_KEY_A, false);
    }
}
