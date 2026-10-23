#pragma once

#include "input.h"

#define HID_KEY_COUNT 0x100

// @Cleanup: Get consistent about capitalization of `Hid` vs `HID`

typedef struct Hid_Key_Map {
    OS_Input_Key_Code normal;
    OS_Input_Key_Code shift;
    OS_Input_Key_Code altgr;
} Hid_Key_Map;

// @Incomplete: Actually fill this in
// @Incomplete Provide a different layout for german keyboards, make active layout depend on the setting in the input module
static Hid_Key_Map hid_layout_us[HID_KEY_COUNT] = {
    [0x04] = { .normal = OS_INPUT_KEY_A, .shift = OS_INPUT_KEY_A, .altgr = OS_INPUT_KEY_A },
};

