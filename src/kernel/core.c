void set_memory(void *dst, u8 value, u64 size_in_bytes) {
    for(u64 i = 0; i < size_in_bytes; ++i) {
        ((u8 *) dst)[i] = value;
    }
}
