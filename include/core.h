#pragma once

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

typedef bool b8;

static_assert(sizeof(u64) == 8 && sizeof(s64) == 8);
static_assert(sizeof(u32) == 4 && sizeof(s32) == 4);
static_assert(sizeof(u16) == 2 && sizeof(s16) == 2);
static_assert(sizeof(u8)  == 1 && sizeof(s8)  == 1);
static_assert(sizeof(f64) == 8);
static_assert(sizeof(f32) == 4);
static_assert(sizeof(b8) == 1);
