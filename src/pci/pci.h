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

void pci_write_u16(u8 bus, u8 slot, u8 function, u8 offset, u16 data);
void pci_write_u32(u8 bus, u8 slot, u8 function, u8 offset, u32 data);
u16 pci_read_u16(u8 bus, u8 slot, u8 function, u8 offset);
u32 pci_read_u32(u8 bus, u8 slot, u8 function, u8 offset);
void pci_initialize(void);
