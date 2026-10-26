#pragma once

#include "input.h"

/**
 * Pushes a new input event onto the back of the queue
 */
void input_push_event(OS_Input_Event event);

/**
 * Overrides the current keyboard modifiers state inside the input subsystem.
 */
void input_set_current_key_modifiers(OS_Input_Keyboard_Modifiers modifiers);

/**
 * Overrides the current key state for the given key. Might emit input events
 * if the key state has changed
 */
void input_set_current_key_state(OS_Input_Key_Code key_code, b8 down);

/**
 * Returns true if the specified key is currently considered `down`
 */
b8 input_get_current_key_state(OS_Input_Key_Code key_code);
