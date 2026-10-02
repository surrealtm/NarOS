#pragma once

#include "base.h"

typedef struct PCI_Device {
    const u8 bus;
    const u8 slot;
    const u8 function;
    const u8 class_code;
    const u8 subclass;
    const u8 interface;
    const u8 revision;
    const u16 vendor;
    const u16 device_id;
} PCI_Device;

void pci_set_bar_address(u8 bus, u8 slot, u8 function, u8 bar_number, u32 data);
u32 pci_get_bar_address(u8 bus, u8 slot, u8 bar_number, u8 function);
void pci_initialize(void);
