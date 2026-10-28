#pragma once

#define STATIC_ASSERT(expr) extern int (*__Static_assert_function (void)) [!!sizeof (struct { int static_assertion_failed: (expr) ? 2 : -1; })]
#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define PACKED_STRUCT __attribute__((packed))
#define ALIGN_DECLARATION(alignment) __attribute__((aligned(alignment)))

#define LITTLE_ENDIAN 1
#define BIG_ENDIAN (!LITTLE_ENDIAN)

// The kernel is currently compiled and running without any memory mapping, but maybe that will change later.
// These helpers shall be used when interacting with hardware that does direct memory access - whereas the
// kernel code might use virtual paging later
#define PHYSICAL_ADDRESS(pointer) ((u32) (pointer))
#define VIRTUAL_ADDRESS(address)  ((void *) (u32) (address))

#define ZERO_STRUCT(pointer) set_memory(pointer, 0, sizeof(*pointer))

#define true 1
#define false 0
#define null 0

#define min(lhs, rhs) ((lhs) < (rhs) ? (lhs) : (rhs))
#define max(lhs, rhs) ((lhs) > (rhs) ? (lhs) : (rhs))

#define align(value, alignment) ((value) > 0 ? ((value) + (alignment) - 1) / (alignment) : 0)

// @Incomplete: Provide a `sanity_check` macro which is only active in debug builds
#define assert(condition, reason) if(!(condition)) panic(str8(reason))

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

static const s64 S64_MAX = (s64) 0x7fffffffffffffff;
static const s64 S64_MIN = 0x8000000000000000;
static const s32 S32_MAX = 0x7fffffff;
static const s32 S32_MIN = (s32) 0x80000000;
static const s16 S16_MAX = 0x7fff;
static const s16 S16_MIN = (s16) 0x8000;
static const s8  S8_MAX  = 0x7f;
static const s8  S8_MIN  = (s8) 0x80;

STATIC_ASSERT(sizeof(u64) == 8 && sizeof(s64) == 8);
STATIC_ASSERT(sizeof(u32) == 4 && sizeof(s32) == 4);
STATIC_ASSERT(sizeof(u16) == 2 && sizeof(s16) == 2);
STATIC_ASSERT(sizeof(u8)  == 1 && sizeof(s8)  == 1);
STATIC_ASSERT(sizeof(f64) == 8);
STATIC_ASSERT(sizeof(f32) == 4);
STATIC_ASSERT(sizeof(b8)  == 1);

/* --------------------------------------------------------------------------------------------------------------- */
/* String API                                                                                                      */
/* --------------------------------------------------------------------------------------------------------------- */

typedef struct Str8 {
    const char *data;
    s32 count;
} Str8;

#define str8_lit(literal) {literal, sizeof(literal) - 1}
#define str8(literal) (Str8) {literal, cstring_length(literal)}
#define str8_from_array(array) (Str8) {array, cstring_length_bounded(array, ARRAY_COUNT(array))}
Str8 str8_substring(Str8 base, s32 first_idx, s32 last_idx);
b8 str8_equals(Str8 lhs, Str8 rhs);

/**
 * Returns the index of the first occurance of the needle in the string.
 * If this character does not appear in the string, `hay.count` is returned.
 */
s32 str8_find_character(Str8 hay, char needle);

/**
 * Parses an integer from the given string and returns the value.
 * If `success` is not a null-pointer, the parsing success is stored into this pointer.
 */
s32 s32_from_str8(Str8 string, b8 *success);

/* --------------------------------------------------------------------------------------------------------------- */
/* Memory Utilities                                                                                                */
/* --------------------------------------------------------------------------------------------------------------- */

void memory_barrier(void);
void set_memory(void *dst, u8 value, u32 size_in_bytes); // @Cleanup: Provide a clear_memory?
void move_memory(void *dst, const void *src, u32 size_in_bytes); // @Cleanup: Rename this to copy_memory?
s32 compare_memory(const void *lhs, const void *rhs, u32 size_in_bytes);
s32 compare_cstrings(const char *lhs, const char *rhs);
s32 cstring_length(const char *string);
s32 cstring_length_bounded(const char *string, const s32 max_size);

void to_big_endian(void *dst, const void *src, u32 size_in_bytes);
u64 u64_to_big_endian(u64 src);
u32 u32_to_big_endian(u32 src);
u16 u16_to_big_endian(u16 src);

void from_big_endian(void *dst, const void *src, u32 size_in_bytes);
u64 u64_from_big_endian(u64 src);
u32 u32_from_big_endian(u32 src);
u16 u16_from_big_endian(u16 src);

/* --------------------------------------------------------------------------------------------------------------- */
/* Base                                                                                                            */
/* --------------------------------------------------------------------------------------------------------------- */

void panic(Str8 reason);
