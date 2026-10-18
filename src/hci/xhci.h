#pragma once

#include "base.h"
#include "hci.h"

#define XHCI_DEVICE_CAPACITY 8
#define XHCI_USB_ADDRESS_CAPACITY 256
#define XHCI_ENDPOINT_CAPACITY 32
#define XHCI_RING_CAPACITY 64
#define XHCI_CONTEXT_SIZE 64
#define XHCI_SCRATCHPAD_CAPACITY 40

typedef struct XHCI_TRB {
    u32 parameter_low;
    u32 parameter_high;
    u32 status;
    u32 control;
} PACKED_STRUCT XHCI_TRB;

typedef struct XHCI_Ring {
    XHCI_TRB trbs[XHCI_RING_CAPACITY] ALIGN_DECLARATION(64);
    u32 enqueue_index;
    u32 dequeue_index;
    b8 producer_cycle;
    b8 consumer_cycle;
} XHCI_Ring;

typedef struct XHCI_ERST_Entry {
    u64 segment_base;
    u32 segment_size;
    u32 reserved;
} PACKED_STRUCT XHCI_ERST_Entry;

typedef struct XHCI_Controller {
    volatile u8 *mmio_base;
    volatile u8 *operational_base;
    volatile u8 *runtime_base;
    volatile u8 *doorbell_base;
    u8 context_size;
    u8 port_count;
    u8 current_port;
    u8 slot_for_device[XHCI_USB_ADDRESS_CAPACITY];
    /* This driver currently manages one attached device per controller. */
    u16 maximum_packet_size[XHCI_ENDPOINT_CAPACITY];
    u64 dcbaa[256] ALIGN_DECLARATION(64);
    u64 scratchpad_array[XHCI_SCRATCHPAD_CAPACITY] ALIGN_DECLARATION(64);
    u8 device_contexts[XHCI_ENDPOINT_CAPACITY][XHCI_CONTEXT_SIZE] ALIGN_DECLARATION(64);
    u8 input_contexts[XHCI_ENDPOINT_CAPACITY + 1][XHCI_CONTEXT_SIZE] ALIGN_DECLARATION(64);
    XHCI_Ring command_ring;
    XHCI_Ring event_ring;
    XHCI_Ring transfer_rings[XHCI_ENDPOINT_CAPACITY];
    XHCI_ERST_Entry event_ring_segment_table ALIGN_DECLARATION(64);
} XHCI_Controller;

XHCI_Controller *xhci_allocate_controller(void);
HCI_Controller hci_controller_from_xhci(XHCI_Controller *xhci);
