#pragma once

#include "pci.h"

#define UHCI_FRAME_LIST_CAPACITY 1024
#define UHCI_DEVICE_CAPACITY 128
#define UHCI_PORT_CAPACITY 2
#define UHCI_ENDPOINT_CAPACITY 16

typedef struct UHCI_Endpoint {
    u8 next_data_toggle[2]; // Track this separately for each direction
} UHCI_Endpoint;

typedef struct UHCI_Device {
    UHCI_Endpoint endpoints[UHCI_ENDPOINT_CAPACITY];
} UHCI_Device;

typedef struct UHCI_Port {
    u32 register_address;
} UHCI_Port;

typedef struct UHCI_Controller {
    u32 frame_list[UHCI_FRAME_LIST_CAPACITY] ALIGN_DECLARATION(4096);
    u32 pci_address;
    u8 queue_head[16]; // Of type UHCI_Queue_Head, but I don't want to declare all of these structures publicly. Stored on the Controller itself as the hardware might access it asynchronously
    UHCI_Port ports[UHCI_PORT_CAPACITY];
    UHCI_Device devices[UHCI_DEVICE_CAPACITY];
} UHCI_Controller;

b8 uhci_initialize_controller(UHCI_Controller *controller, const u32 pci_address);
b8 uhci_control(UHCI_Controller *controller, u8 device_idx, void *header_data, u32 header_size_in_bytes, void *payload, u32 payload_size_in_bytes);
b8 uhci_bulk_write(UHCI_Controller *controller, u8 device_idx, u8 endpoint_idx, const void *data, u32 size_in_bytes);
b8 uhci_bulk_read(UHCI_Controller *controller, u8 device_idx, u8 endpoint_idx, void *data, u32 size_in_bytes);
