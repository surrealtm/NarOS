#pragma once

#include "pci.h"

#define UHCI_FRAME_LIST_CAPACITY 1024
#define UHCI_PORT_CAPACITY 2

typedef struct UHCI_Controller {
    u32 pci_address;
    u32 frame_list[UHCI_FRAME_LIST_CAPACITY] ALIGN_DECLARATION(16);
    u32 ports[UHCI_PORT_CAPACITY];
    u8 current_data_toggle;
} UHCI_Controller;

b8 uhci_initialize_controller(UHCI_Controller *controller, const u32 pci_address);
b8 uhci_control(UHCI_Controller *controller, void *header_data, u32 header_size_in_bytes, void *payload, u32 payload_size_in_bytes);
b8 uhci_bulk_write(UHCI_Controller *controller, u8 device, u8 endpoint, const void *data, u32 size_in_bytes);
b8 uhci_bulk_read(UHCI_Controller *controller, u8 device, u8 endpoint, void *data, u32 size_in_bytes);
