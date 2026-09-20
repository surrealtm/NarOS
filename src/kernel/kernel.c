static
void print_string(const unsigned int row, const char *string) {
    const char color = 0x0f;
    char *video_memory = (char *) 0xb8000;
    unsigned int idx = row * 80 * 2;
    while(*string) {
        video_memory[idx++] = *string;
        video_memory[idx++] = color;
        ++string;
    }
}

// This procedure is called by the `kernel_main` file, once the boot loader has loaded and invoked
// the kernel.
int kernel_entry_point(void) {
    print_string(5, "Hello from the kernel!");
    while(true) {}
    return 0;
}
