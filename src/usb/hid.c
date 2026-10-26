#include "hid.h"
#include "hid_tables.h"
#include "input/input_kernel.h"
#include "interrupt/interrupt.h"

#define KEY_REPEAT_DELAY_NANOSECONDS 250000000 // After how many nanoseconds the first repetition shall occur
#define KEY_REPEAT_RATE 20 // How many repetitions per second shall be generated for a held key

typedef struct Key_State {
    b8 down;
    b8 repeated;
    /* Time-tick of when the last press event was generated for this key, either through a physical press or through
     * a repetition event */
    u64 tick_of_last_press;
} Key_State;

static b8 repetition_interrupt_installed = false;
static Key_State key_states[HID_KEY_COUNT] = { 0 };
static HID_Keyboard_Report_Modifiers modifiers = 0;

static inline
const HID_Key_Map *get_active_key_map(void) {
    switch(os_input_get_keyboard_layout()) {
        case OS_INPUT_KEYBOARD_LAYOUT_Standard_US: return hid_layout_standard_us;
        case OS_INPUT_KEYBOARD_LAYOUT_German: return hid_layout_german;
    }

    return hid_layout_standard_us;
}

static
b8 query_keyboard_report(const HID_Keyboard_Report report, const HID_Key_Code key_code) {
    for (u32 key_idx = 0; key_idx < ARRAY_COUNT(report.pressed_keys); ++key_idx) {
        if (key_code == report.pressed_keys[key_idx]) return true;
    }
    return false;
}

static
OS_Input_Key_Code os_input_key_code(const HID_Key_Code hid) {
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
OS_Input_Keyboard_Modifiers os_input_modifiers(void) {
    OS_Input_Keyboard_Modifiers result = 0;

    if (modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Control) {
        result |= OS_INPUT_KEYBOARD_MODIFIERS_Control;
    }

    if (modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Shift) {
        result |= OS_INPUT_KEYBOARD_MODIFIERS_Shift;
    }

    if (modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Right_Alt) {
        result |= OS_INPUT_KEYBOARD_MODIFIERS_Right_Alt;
    }

    if (modifiers & HID_KEYBOARD_REPORT_MODIFIERS_Super) {
        result |= OS_INPUT_KEYBOARD_MODIFIERS_Super;
    }

    return result;
}

static
void check_for_repetitions(void) {
    const u64 now = interrupt_get_tick();
    const u64 initial_delay   = interrupt_ticks_from_nanoseconds(KEY_REPEAT_DELAY_NANOSECONDS);
    const u64 repeat_interval = interrupt_ticks_from_nanoseconds(1000000000 / KEY_REPEAT_RATE);

    for (u32 hid_key_code = 0; hid_key_code < ARRAY_COUNT(key_states); ++hid_key_code) {
        const u64 required_delta = (key_states[hid_key_code].repeated) ? repeat_interval : initial_delay;
        if (key_states[hid_key_code].down && now - key_states[hid_key_code].tick_of_last_press >= required_delta) {
            key_states[hid_key_code].tick_of_last_press = now;
            key_states[hid_key_code].repeated = true;
            const OS_Input_Key_Code input_key_code = os_input_key_code(hid_key_code);
            input_trigger_key_repetition(input_key_code);
        }
    }
}

void hid_parse_keyboard_report(const HID_Keyboard_Report current_keyboard_report) {
    if (!repetition_interrupt_installed) {
        interrupt_register_callback(INTERRUPT_SIGNAL_Timer, check_for_repetitions);
        repetition_interrupt_installed = true;
    }

    const u64 now = interrupt_get_tick();

    //
    // Update the keyboard modifiers
    //
    modifiers = current_keyboard_report.modifiers;
    input_set_current_key_modifiers(os_input_modifiers());

    //
    // Find all keys that have been released in this new report
    //
    for (u32 hid_key_code = 0; hid_key_code < ARRAY_COUNT(key_states); ++hid_key_code) {
        if (!query_keyboard_report(current_keyboard_report, hid_key_code)) {
            key_states[hid_key_code].down = false;

            // The physical key has been released, so all combinations are also released
            const HID_Key_Map key_map = get_active_key_map()[hid_key_code];
            input_set_current_key_state(key_map.normal, false);
            input_set_current_key_state(key_map.shift, false);
            input_set_current_key_state(key_map.altgr, false);
        }
    }

    //
    // Find all keys that have been pressed in this new report
    //
    for (u32 key_idx = 0; key_idx < ARRAY_COUNT(current_keyboard_report.pressed_keys); ++key_idx) {
        const HID_Key_Code hid_key_code = current_keyboard_report.pressed_keys[key_idx];
        if (!key_states[hid_key_code].down) {
            key_states[hid_key_code].down = true;
            key_states[hid_key_code].repeated = false;
            key_states[hid_key_code].tick_of_last_press = now;

            const OS_Input_Key_Code input_key_code = os_input_key_code(hid_key_code);
            input_set_current_key_state(input_key_code, true);
        }
    }
}
