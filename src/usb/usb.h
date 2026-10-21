#pragma once

#include "base.h"
#include "hci/hci.h"

typedef struct USB_Mass_Storage_Interface {
    b8 supported;
    u8 interface_number;
    u8 bulk_in_endpoint;
    u8 bulk_out_endpoint;
    u16 bulk_in_packet_size;
    u16 bulk_out_packet_size;
} USB_Mass_Storage_Interface;

typedef struct USB_Device {
    HCI_Controller hci;
    u8 address;
    u8 configuration_value;
    USB_Mass_Storage_Interface mass_storage;
} USB_Device;

/**
 * Returns an uninitialized USB device structure, or null if the capacity for devices was reached
 */
struct USB_Device *usb_allocate_device(void);

/**
 * Initializes a new USB device for the given interface and port idx.
 * Returns true if the hardware device was successfully configured for the USB protocol
 */
b8 usb_initialize_device(struct USB_Device *device, HCI_Controller hci, u8 port_idx);
