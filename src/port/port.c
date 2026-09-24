#include "port.h"

void port_write_u8(const u16 port, const u8 value) {
    __asm__ volatile ("outb %0, %1" :: "a" (value), "Nd" (port));
}

void port_write_u16(const u16 port, const u16 value) {
    __asm__ volatile ("outw %0, %1" :: "a" (value), "Nd" (port));
}

u16 port_read_u16(const u16 port) {
    u16 value;
    __asm__ volatile ("inw %w1, %0" : "=a" (value) : "Nd" (port));
    return value;
}


