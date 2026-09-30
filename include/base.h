#pragma once

#define STATIC_ASSERT(expr) extern int (*__Static_assert_function (void)) [!!sizeof (struct { int static_assertion_failed: (expr) ? 2 : -1; })]
#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define PACKED_STRUCT __attribute__((packed))

#define true 1
#define false 0
#define null 0

#define min(lhs, rhs) ((lhs) < (rhs) ? (lhs) : (rhs))
#define max(lhs, rhs) ((lhs) > (rhs) ? (lhs) : (rhs))

#if ENABLE_ASSERTIONS
# define assert(condition, reason) if(!(condition)) panic(str8(reason))
#else
# define assert(condition, reason) {}
#endif

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

static const u64 U64_MAX = 0xffffffffffffffff;
static const u32 U32_MAX = 0xffffffff;
static const u16 U16_MAX = 0xffff;
static const u8  U8_MAX = 0xff;

STATIC_ASSERT(sizeof(u64) == 8 && sizeof(s64) == 8);
STATIC_ASSERT(sizeof(u32) == 4 && sizeof(s32) == 4);
STATIC_ASSERT(sizeof(u16) == 2 && sizeof(s16) == 2);
STATIC_ASSERT(sizeof(u8)  == 1 && sizeof(s8)  == 1);
STATIC_ASSERT(sizeof(f64) == 8);
STATIC_ASSERT(sizeof(f32) == 4);
STATIC_ASSERT(sizeof(b8)  == 1);

typedef struct Str8 {
    const char *data;
    const s32 count;
} Str8;

#define str8_lit(literal) {literal, sizeof(literal) - 1}
#define str8(literal) (Str8) str8_lit(literal)
Str8 str8_substring(Str8 base, s32 first_idx, s32 last_idx);
b8 str8_equals(Str8 lhs, Str8 rhs);

void panic(Str8 reason);

void set_memory(void *dst, u8 value, u32 size_in_bytes);
void move_memory(void *dst, const void *src, u32 size_in_bytes);
s32 compare_memory(const void *lhs, const void *rhs, u32 size_in_bytes);
s32 compare_cstrings(const char *lhs, const char *rhs);
s32 cstring_length(const char *string);
