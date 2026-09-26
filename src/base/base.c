#include "base.h"

void set_memory(void *dst, const u8 value, const u32 size_in_bytes) {
    for(u64 i = 0; i < size_in_bytes; ++i) {
        ((u8 *) dst)[i] = value;
    }
}

s32 compare_memory(const void *lhs, const void *rhs, u32 size_in_bytes) {
    for(u32 i = 0; i < size_in_bytes; ++i) {
        const u8 diff = ((u8 *) lhs)[i] - ((u8 *) rhs)[i];
        if(diff != 0) return diff;
    }
    return 0;
}

s32 string_length(const char *string) {
    s32 count;
    for(count = 0; string[count]; ++count) {}
    return count;
}

void memset(void *dst, const u8 value, const u32 size_in_bytes) {
    set_memory(dst, value, size_in_bytes);
}
