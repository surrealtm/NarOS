#pragma once

#include "base.h"
#include "hci/hci.h"

typedef struct USB_Device {
    HCI_Controller hci;
    u8 address;
    u8 configuration_value;
    u8 mass_storage_interface;
    u8 bulk_in_endpoint;
    u8 bulk_out_endpoint;
    u16 bulk_in_packet_size;
    u16 bulk_out_packet_size;
} USB_Device;

/**
 * Returns an uninitialized USB device structure, or null if the capacity for devices was reached
 */
USB_Device *usb_allocate_device(void);
void usb_discard_device(USB_Device *device);

/**
 * Initializes a new USB device for the given interface and port idx.
 * Returns true if the hardware device was successfully configured for the USB protocol
 */
b8 usb_initialize_device(USB_Device *device, HCI_Controller hci, u8 port_idx);
