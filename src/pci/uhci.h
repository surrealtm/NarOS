#pragma once

#include "pci.h"

#define UHCI_FRAME_LIST_CAPACITY 1024
#define UHCI_PORT_CAPACITY 2

typedef struct UHCI_Controller {
    u32 pci_address;
    u32 frame_list[UHCI_FRAME_LIST_CAPACITY] ALIGN_DECLARATION(4096);
    u32 ports[UHCI_PORT_CAPACITY];
} UHCI_Controller;

b8 uhci_initialize_controller(UHCI_Controller *controller, const u32 pci_address);
void uhci_schedule_bulk_out(UHCI_Controller *controller, const void *data, u32 size_in_bytes);
