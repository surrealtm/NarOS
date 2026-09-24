#pragma once

#define packed_struct __attribute__((packed))

/**
 * Implements division of two unsigned 64 bit integers.
 * The X86 instruction set does not support this natively, so compilers usually provide their own
 * implementation.
 * Since we're compiling in free-standing mode, we have to provide our own division function.
 */
u64 __udivdi3(u64 divident, u64 divisor);
