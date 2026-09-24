#include "math.h"

typedef struct {
    u32 hi;
    u32 lo;
} u64_word;

static inline
u64_word to_word(const u64 in) {
    return (u64_word) { (in >> 32) & 0xffffffff, (in >> 0) & 0xffffffff };
}

static inline
u64 from_word(const u64_word word) {
    return ((u64) word.hi << 32ULL) | (u64) word.lo;
}

static inline
b8 compare_words(const u64_word lhs, const u64_word rhs) {
    return lhs.hi > rhs.hi || (lhs.hi == rhs.hi && lhs.lo >= rhs.lo);
}

static inline
u32 index_word(const u64_word word, const u64 idx) {
    const u32 mask = (idx < 32) ? (word.hi >> (31 - idx)) : (word.lo >> (63 - idx));
    return mask & 1;
}

u64 __udivdi3(const u64 dividend, const u64 divisor) {
    const u64_word dividend_word = to_word(dividend);
    const u64_word divisor_word  = to_word(divisor);
    u64_word quotient_word  = { 0, 0 };
    u64_word remainder_word = { 0, 0 };

    for(u64 i = 0; i < 64; ++i) {
        remainder_word.hi = (remainder_word.hi << 1) | (remainder_word.lo >> 31);
        remainder_word.lo <<= 1;
        remainder_word.lo |= index_word(dividend_word, i);

        if(compare_words(remainder_word, divisor_word)) {
            remainder_word.lo -= divisor_word.lo;
            remainder_word.hi -= divisor_word.hi;
            if(remainder_word.lo > U32_MAX - divisor_word.lo) --remainder_word.hi;

            if(i < 32) {
                quotient_word.hi |= (u32)1 << (31 - i);
            } else {
                quotient_word.lo |= (u32)1 << (63 - i);
            }
        }
    }

    return from_word(quotient_word);
}
