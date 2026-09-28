#pragma once

#include "base.h"

/**
 * This file implements some math helpers.
 * The X86 instruction set does not support these natively, so compilers usually provide their own
 * implementation. Since we're compiling in free-standing mode, we have to provide our own
 * implementations for them.
 */

u64 __udivdi3(u64 dividend, u64 divisor);
u64 __udivmoddi4(u64 dividend, u64 divisor, u64 *remainder);
u64 __umodi3(u64 divident, u64 divisor);
