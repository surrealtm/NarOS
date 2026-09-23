void os_ctrl_sleep(const u64 nanoseconds) {
    const u64 target_tick_counter = current_tick_counter() + ticks_from_nanoseconds(nanoseconds);
    while(current_tick_counter() < target_tick_counter) {
        os_ctrl_halt();
    }
}

void os_ctrl_halt() {
    __asm__ volatile("hlt");
}
