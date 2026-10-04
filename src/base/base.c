#include "base.h"

const s32 backlog_width = 80;
const s32 backlog_height = 25;
static volatile char * const video_memory = (char *) 0xb8000;

static
void render_character(const s32 cursor_x, const s32 cursor_y, const char character, const char color) {
    const s32 buffer_idx = (cursor_x + cursor_y * backlog_width) * 2;
    video_memory[buffer_idx + 0] = character;
    video_memory[buffer_idx + 1] = color;
}

static
void clear_text(void) {
    for(s32 y = 0; y < backlog_height; ++y) {
        for(s32 x = 0; x < backlog_width; ++x) {
            render_character(x, y, ' ', 0x0f);
        }
    }
}

static
u32 render_text_line(const Str8 text, const u32 cursor_y, const char color) {
    for(s32 char_idx = 0; char_idx < text.count; ++char_idx) {
        render_character(char_idx % backlog_width, cursor_y + (char_idx / backlog_width), text.data[char_idx], color);
    }

    return text.count / (backlog_width + 1) + 1;
}

static
void render_panic_text(const Str8 text) {
    const s32 white_color = 0x0f;
    const s32 red_color   = 0x0c;
    s32 cursor_y = 0;
    clear_text();
    cursor_y += render_text_line(str8("The kernel encountered a critical error"), cursor_y, white_color);
    cursor_y += render_text_line(text, cursor_y, red_color);
    cursor_y += render_text_line(str8("Please restart your machine."), cursor_y, white_color);
}

Str8 str8_substring(Str8 base, const s32 first_idx, const s32 last_idx) {
    assert(first_idx >= 0 && first_idx < base.count && last_idx >= first_idx && last_idx < base.count, "String indices are out of bounds.");
    return (Str8) { base.data + first_idx, (last_idx - first_idx ) + 1 };
}

b8 str8_equals(const Str8 lhs, const Str8 rhs) {
    return lhs.count == rhs.count && compare_memory(lhs.data, rhs.data, lhs.count) == 0;
}

void memory_barrier(void) {
    __asm__ volatile("" ::: "memory");
}

void panic(const Str8 reason) {
    render_panic_text(reason);
    for (;;) {
        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
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
