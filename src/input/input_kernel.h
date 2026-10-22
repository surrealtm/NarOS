#pragma once

#include "input.h"

/**
 * Initializes the entire input system
 */
void input_initialize(void);

/**
 * Pushes an input event registered by another component of the kernel
 * onto the event queue.
 */
void input_push_event(OS_Input_Event event);
