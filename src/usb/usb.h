#pragma once

#include "base.h"

struct UHCI_Controller;

typedef struct USB_Device {
    u8 address;
    u8 configuration_value;
    u8 mass_storage_interface;
    u8 bulk_in_endpoint;
    u8 bulk_out_endpoint;
    u16 bulk_in_packet_size;
    u16 bulk_out_packet_size;
} USB_Device;

b8 usb_initialize_device(USB_Device *device, struct UHCI_Controller *uhci, u8 port_idx);
