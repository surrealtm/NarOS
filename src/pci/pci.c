#include "pci.h"
#include "port/port.h"

#define PCI_ADDRESS 0xcf8
#define PCI_DATA    0xcfc

static inline
u32 calculate_address(const u8 bus, const u8 slot, const u8 function, const u8 offset) {
    return 0x80000000 | ((u32) bus << 16) | ((u32) slot << 11) | ((u32) function << 8) | ((u32) offset & 0xfc);
}

static
void write_configuration_word(const u8 bus, const u8 slot, const u8 function, const u8 offset, const u32 data) {
    const u32 address = calculate_address(bus, slot, function, offset);
    port_write_u32(PCI_ADDRESS, address);
    port_write_u32(PCI_DATA, data);
}

static
u16 read_configuration_word(const u8 bus, const u8 slot, const u8 function, const u8 offset) {
    const u32 address = calculate_address(bus, slot, function, offset);
    port_write_u32(PCI_ADDRESS, address);
    const u32 data = port_read_u32(PCI_DATA);
    return data >> ((offset & 0x2) * 8) & 0xffff;
}

static
void set_bar_address(const u8 bus, const u8 slot, const u8 function, const u8 bar_number, const u32 result) {
    const u32 first_part = result & 0xffff;
    const u32 second_part = (result >> 16) & 0xffff;
    write_configuration_word(bus, slot, function, bar_number + 0, first_part);
    write_configuration_word(bus, slot, function, bar_number + 2, second_part);
}

static
u32 get_bar_address(const u8 bus, const u8 slot, const u8 function, const u8 bar_number) {
    const u32 first_part = read_configuration_word(bus, slot, function, bar_number + 0);
    const u32 second_part = read_configuration_word(bus, slot, function, bar_number + 2);
    return first_part | (second_part << 16);
}

void pci_initialize(void) {
    (void) set_bar_address;
    (void) get_bar_address;
}
