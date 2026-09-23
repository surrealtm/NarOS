#pragma once

#define STATIC_ASSERT(expr) extern int (*__Static_assert_function (void)) [!!sizeof (struct { int static_assertion_failed: (expr) ? 2 : -1; })]
#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define true 1
#define false 0

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef signed long long s64;
typedef signed int s32;
typedef signed short s16;
typedef signed char s8;

typedef double f64;
typedef float f32;

typedef char b8;

const u64 U64_MAX = 0xffffffffffffffff;
const u32 U32_MAX = 0xffffffff;
const u16 U16_MAX = 0xffff;
const u8  U8_MAX = 0xff;

STATIC_ASSERT(sizeof(u64) == 8 && sizeof(s64) == 8);
STATIC_ASSERT(sizeof(u32) == 4 && sizeof(s32) == 4);
STATIC_ASSERT(sizeof(u16) == 2 && sizeof(s16) == 2);
STATIC_ASSERT(sizeof(u8)  == 1 && sizeof(s8)  == 1);
STATIC_ASSERT(sizeof(f64) == 8);
STATIC_ASSERT(sizeof(f32) == 4);
STATIC_ASSERT(sizeof(b8)  == 1);

void set_memory(void *dst, u8 value, u32 size_in_bytes);
