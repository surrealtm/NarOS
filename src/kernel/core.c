void set_memory(void *dst, const u8 value, const u32 size_in_bytes) {
    for(u64 i = 0; i < size_in_bytes; ++i) {
        ((u8 *) dst)[i] = value;
    }
}
