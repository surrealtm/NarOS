#include "port.h"

void port_write_u8(const u16 port, const u8 value) {
    __asm__ volatile ("outb %0, %1" :: "a" (value), "Nd" (port));
}

void port_write_u16(const u16 port, const u16 value) {
    __asm__ volatile ("outw %0, %1" :: "a" (value), "Nd" (port));
}

void port_write_u32(const u16 port, const u32 value) {
    __asm__ volatile ("outl %0, %1" :: "a" (value), "Nd" (port));
}

u8 port_read_u8(const u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

u16 port_read_u16(const u16 port) {
    u16 value;
    __asm__ volatile ("inw %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

u32 port_read_u32(const u16 port) {
    u32 value;
    __asm__ volatile ("inl %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}
