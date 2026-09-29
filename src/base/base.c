#include "base.h"

Str8 str8_substring(Str8 base, const s32 first_idx, const s32 last_idx) {
    // @Incomplete: Assert that the string indices are correct.
    return (Str8) { base.data + first_idx, (last_idx - first_idx ) + 1 };
}

b8 str8_equals(const Str8 lhs, const Str8 rhs) {
    return lhs.count == rhs.count && compare_memory(lhs.data, rhs.data, lhs.count) == 0;
}

void set_memory(void *dst, const u8 value, const u32 size_in_bytes) {
    for(u64 i = 0; i < (u64) size_in_bytes; ++i) {
        ((u8 *) dst)[i] = value;
    }
}

void move_memory(void *dst, const void *src, const u32 size_in_bytes) {
    if(dst < src) {
        for(s64 i = 0; i < (s64) size_in_bytes; ++i) {
            ((u8 *) dst)[i] = ((u8 *) src)[i];
        }
    } else {
        for(s64 i = (s64) size_in_bytes - 1; i >= 0; --i) {
            ((u8 *) dst)[i] = ((u8 *) src)[i];
        }
    }
}

s32 compare_memory(const void *lhs, const void *rhs, const u32 size_in_bytes) {
    for(u32 i = 0; i < size_in_bytes; ++i) {
        const u8 diff = ((u8 *) lhs)[i] - ((u8 *) rhs)[i];
        if(diff != 0) return diff;
    }
    return 0;
}

s32 compare_cstrings(const char *lhs, const char *rhs) {
    u32 i;
    for(i = 0; lhs[i] != 0 && rhs[i] != 0; ++i) {
        const u8 diff = ((u8 *) lhs)[i] - ((u8 *) rhs)[i];
        if(diff != 0) return diff;
    }
    return lhs[i] - rhs[i];
}

s32 cstring_length(const char *string) {
    s32 count;
    for(count = 0; string[count]; ++count) {}
    return count;
}

void memset(void *dst, const u8 value, const u32 size_in_bytes) {
    set_memory(dst, value, size_in_bytes);
}

void memcpy(void *dst, const void *src, const u32 size_in_bytes) {
    move_memory(dst, src, size_in_bytes);
}
