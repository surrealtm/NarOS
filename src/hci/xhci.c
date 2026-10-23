#include "xhci.h"
#include "base.h"
#include "ctrl.h"
#include "interrupt/interrupt.h"
#include "usb/hid.h"

#define XHCI_MAX_CONTROLLERS 4

#define WAIT_TIME_NANOSECONDS 1000000 // Wait in 1 millisecond intervals
#define WAIT_MAX_RETRIES 100 // How many iterations in a wait-loop

#define XHCI_ERROR -1
#define XHCI_DEVICE_CAPACITY 8 // Shared across controllers, matching the USB layer's global device limit.
#define XHCI_ENDPOINT_CAPACITY 32
#define XHCI_RING_CAPACITY 64
#define XHCI_CONTEXT_SIZE 64
#define XHCI_SCRATCHPAD_CAPACITY 40


// ---------------------------------------------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------------------------------------------

typedef enum XHCI_USBCMD {
    XHCI_USBCMD_Run_Stop = 1 << 0,
    XHCI_USBCMD_Host_Controller_Reset = 1 << 1,
    XHCI_USBCMD_Interrupt_Enable = 1 << 2,
} XHCI_USBCMD;

typedef enum XHCI_USBSTS {
    XHCI_USBSTS_Halted = 1 << 0,
    XHCI_USBSTS_Event_Interrupt = 1 << 3,
    XHCI_USBSTS_Controller_Not_Ready = 1 << 11,
} XHCI_USBSTS;

typedef enum XHCI_IMAN {
    XHCI_IMAN_Interrupt_Pending = 1 << 0,
    XHCI_IMAN_Interrupt_Enable = 1 << 1,
} XHCI_IMAN;

typedef enum XHCI_Context_Dword {
    XHCI_INPUT_ADD_CONTEXT_FLAGS = 1,
    XHCI_SLOT_ROUTE_AND_SPEED = 0,
    XHCI_SLOT_ROOT_PORT = 1,
    XHCI_ENDPOINT_TYPE_AND_PACKET_SIZE = 1,
    XHCI_ENDPOINT_DEQUEUE_LOW = 2,
    XHCI_ENDPOINT_DEQUEUE_HIGH = 3,
    XHCI_ENDPOINT_AVERAGE_TRB_LENGTH = 4,
} XHCI_Context_Dword;

typedef enum XHCI_PortSC {
    XHCI_PORTSC_Connect_Status = 1 << 0,
    XHCI_PORTSC_Port_Enabled = 1 << 1,
    XHCI_PORTSC_Port_Reset = 1 << 4,
    XHCI_PORTSC_Port_Power = 1 << 9,
    XHCI_PORTSC_CHANGE_BITS =  ((1u << 17) | (1u << 18) | (1u << 19) | (1u << 20) | (1u << 21) | (1u << 22)),
} XHCI_PortSC;

static const u32 XHCI_PORTSC_Warm_Reset = 1u << 31;

typedef enum XHCI_TRB_Control {
    XHCI_TRB_Cycle = 1 << 0,
    XHCI_TRB_ISP = 1 << 2,
    XHCI_TRB_IOC = 1 << 5,
    XHCI_TRB_IDT = 1 << 6,
} XHCI_TRB_Control;

typedef enum XHCI_TRB_TYPE {
    XHCI_TRB_TYPE_Normal = 1 << 10,
    XHCI_TRB_TYPE_Setup_Stage = 2 << 10,
    XHCI_TRB_TYPE_Data_Stage = 3 << 10,
    XHCI_TRB_TYPE_Status_Stage = 4 << 10,
    XHCI_TRB_TYPE_Link = 6 << 10,
    XHCI_TRB_TYPE_Enable_Slot_Command = 9 << 10,
    XHCI_TRB_TYPE_Disable_Slot_Command = 10 << 10,
    XHCI_TRB_TYPE_Address_Device_Command = 11 << 10,
    XHCI_TRB_TYPE_Configure_Endpoint_Command = 12 << 10,
    XHCI_TRB_TYPE_Evaluate_Context_Command = 13 << 10,
    XHCI_TRB_TYPE_Command_Completion_Event = 33 << 10,
    XHCI_TRB_TYPE_Transfer_Event = 32 << 10,
} XHCI_TRB_TYPE;

typedef struct XHCI_TRB {
    u32 parameter_low;
    u32 parameter_high;
    u32 status;
    u32 control;
} PACKED_STRUCT XHCI_TRB;

typedef struct XHCI_Ring {
    XHCI_TRB trbs[XHCI_RING_CAPACITY] ALIGN_DECLARATION(64);
    u32 index;
    b8 cycle;
} XHCI_Ring;

typedef struct XHCI_ERST_Entry {
    u64 segment_base;
    u32 segment_size;
    u32 reserved;
} PACKED_STRUCT XHCI_ERST_Entry;

typedef struct XHCI_Device {
    struct XHCI_Controller *controller;
    u8 port;
    u8 device_index;
    u8 slot;
    u8 bulk_endpoint_ids[HCI_DIRECTION_COUNT];
    u8 context_entries;
    u16 ep0_packet_size;
    u8 device_contexts[XHCI_ENDPOINT_CAPACITY][XHCI_CONTEXT_SIZE] ALIGN_DECLARATION(64);
    u8 input_contexts[XHCI_ENDPOINT_CAPACITY + 1][XHCI_CONTEXT_SIZE] ALIGN_DECLARATION(64);
    XHCI_Ring control_ring;
    XHCI_Ring bulk_rings[HCI_DIRECTION_COUNT];
} XHCI_Device;

typedef struct XHCI_Controller {
    volatile u8 *mmio_base;
    volatile u8 *operational_base;
    volatile u8 *runtime_base;
    volatile u8 *doorbell_base;
    u8 context_size;
    u8 port_count;
    u8 slot_count;
    u8 interrupt_signal;
    u8 enumerating_port;
    b8 has_enumerating_port;
    XHCI_Device *pending_device;
    u64 dcbaa[XHCI_DEVICE_CAPACITY + 1] ALIGN_DECLARATION(64);
    u64 scratchpad_array[XHCI_SCRATCHPAD_CAPACITY] ALIGN_DECLARATION(64);
    XHCI_Ring command_ring;
    XHCI_Ring event_ring;
    XHCI_ERST_Entry event_ring_segment_table ALIGN_DECLARATION(64);
    XHCI_Device *keyboard_device;
    XHCI_TRB *keyboard_transfer;
    u8 keyboard_endpoint_id;
    HID_Keyboard_Report keyboard_report;
} XHCI_Controller;

static XHCI_Controller xhci_controllers[XHCI_MAX_CONTROLLERS];
static u32 xhci_controller_count;
static XHCI_Device xhci_devices[XHCI_DEVICE_CAPACITY];
static b8 xhci_interrupt_registered[16];

static u8 scratchpad_pages[XHCI_SCRATCHPAD_CAPACITY][4096] ALIGN_DECLARATION(4096);
static u32 scratchpad_pages_used;


// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

static inline
u32 mmio_read32(const volatile void *address) {
    return *(const volatile u32 *) address;
}

static inline
void mmio_write32(volatile void *address, const u32 value) {
    *(volatile u32 *)address = value;
}

static inline
void mmio_write64(volatile void *address, const u64 value) {
    /* xHCI 64-bit registers are programmed as two ordered 32-bit MMIO
     * accesses.  A compiler-generated volatile u64 store is not required to
     * use this order on a 32-bit target. */
    volatile u8 *base = (volatile u8 *)address;
    mmio_write32(base, (u32)value);
    mmio_write32(base + sizeof(u32), (u32)(value >> 32));
}

static inline
volatile u8 *at(volatile u8 *base, const u32 offset) {
    return base + offset;
}

static inline
u32 trb_type(const XHCI_TRB *trb) {
    return trb->control & (0x3fu << 10);
}

static inline
u32 completion_code(const XHCI_TRB *trb) {
    return (trb->status >> 24) & 0xff;
}

static
void ring_initialize(XHCI_Ring *ring) {
    ZERO_STRUCT(ring);
    ring->cycle = 1;
    ring->trbs[XHCI_RING_CAPACITY - 1] = (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(ring->trbs), .control = XHCI_TRB_TYPE_Link | XHCI_TRB_Cycle | (1u << 1) };
}

static
XHCI_TRB *ring_push(XHCI_Ring *ring, XHCI_TRB trb) {
    if(ring->index == XHCI_RING_CAPACITY - 1) {
        ring->trbs[ring->index].control = XHCI_TRB_TYPE_Link | (ring->cycle ? XHCI_TRB_Cycle : 0) | (1u << 1);
        ring->index = 0;
        ring->cycle = !ring->cycle;
    }

    trb.control |= ring->cycle ? XHCI_TRB_Cycle : 0;
    ring->trbs[ring->index] = trb;
    return &ring->trbs[ring->index++];
}

static
b8 wait_for_bits(const volatile void *address, const u32 mask, const b8 set) {
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        if((mmio_read32(address) & mask) == (set ? mask : 0)) return true;
        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
    }
    return false;
}

/* Firmware is permitted to retain an xHCI controller until the OS-owned
 * semaphore is set.  In particular, SeaBIOS can leave this capability
 * present on QEMU's qemu-xhci device. */
static
b8 take_legacy_ownership(XHCI_Controller *controller, const u32 hccparams1) {
    u32 capability_offset = ((hccparams1 >> 16) & 0xffff) << 2;

    for(u32 capability_count = 0; capability_offset && capability_count < 64; ++capability_count) {
        const u32 header = mmio_read32(at(controller->mmio_base, capability_offset));
        if((header & 0xff) == 1) { /* USB Legacy Support extended capability */
            /* The ownership semaphores share the extended-capability header
             * dword.  The following dword is USBLEGSUP control/status, not
             * the semaphore register. */
            volatile u8 *legacy_support = at(controller->mmio_base, capability_offset);
            mmio_write32(legacy_support, mmio_read32(legacy_support) | (1u << 24));
            return wait_for_bits(legacy_support, 1u << 16, false);
        }

        capability_offset = ((header >> 8) & 0xff) << 2;
    }

    return true;
}

static
u32 *input_context(const XHCI_Controller *controller, XHCI_Device *device, const u8 index) {
    return (u32 *)(&device->input_contexts[0][0] + (u32)index * controller->context_size);
}

static
XHCI_Device *find_device(XHCI_Controller *controller, const u8 device_index) {
    if(device_index == 0) return controller->pending_device;
    for(u32 i = 0; i < ARRAY_COUNT(xhci_devices); ++i) {
        XHCI_Device *device = &xhci_devices[i];
        if(device->controller == controller && device->slot && device->device_index == device_index) return device;
    }
    return null;
}

static
XHCI_Device *free_device(void) {
    for(u32 i = 0; i < ARRAY_COUNT(xhci_devices); ++i) {
        if(!xhci_devices[i].slot) return &xhci_devices[i];
    }
    return null;
}

static
u8 endpoint_id(const u8 endpoint, const HCI_Direction direction) {
    return endpoint == 0 ? 1 : (u8)(endpoint * 2 + (direction == HCI_DIRECTION_In ? 1 : 0));
}

static
b8 event_pop(XHCI_Controller *controller, XHCI_TRB *event) {
    XHCI_Ring *ring = &controller->event_ring;
    volatile XHCI_TRB *trb = &ring->trbs[ring->index];
    memory_barrier();
    if(((trb->control & XHCI_TRB_Cycle) != 0) != ring->cycle) return false;

    event->parameter_low = trb->parameter_low;
    event->parameter_high = trb->parameter_high;
    event->status = trb->status;
    event->control = trb->control;
    ++ring->index;
    if(ring->index == XHCI_RING_CAPACITY) {
        ring->index = 0;
        ring->cycle = !ring->cycle;
    }

    mmio_write64(at(controller->runtime_base, 0x38), PHYSICAL_ADDRESS(&ring->trbs[ring->index]) | (1u << 3));
    return true;
}

static
b8 wait_for_event(XHCI_Controller *controller, const u32 expected_type, const u64 expected_pointer, XHCI_TRB *result) {
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        XHCI_TRB event;
        while(event_pop(controller, &event)) {
            const u64 pointer = ((u64)event.parameter_high << 32) | event.parameter_low;

            if(trb_type(&event) != expected_type) continue;
            if(expected_pointer && pointer != expected_pointer) {
                if(expected_type == XHCI_TRB_TYPE_Transfer_Event && completion_code(&event) != 1 && completion_code(&event) != 13) {
                    return false;
                }
                continue;
            }

            if(result) *result = event;
            return completion_code(&event) == 1 || completion_code(&event) == 13;
        }

        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
    }
    return false;
}

static
b8 submit_command(XHCI_Controller *controller, XHCI_TRB command, XHCI_TRB *event) {
    XHCI_TRB *submitted = ring_push(&controller->command_ring, command);
    memory_barrier();
    mmio_write32(at(controller->doorbell_base, 0), 0);
    return wait_for_event(controller, XHCI_TRB_TYPE_Command_Completion_Event, PHYSICAL_ADDRESS(submitted), event);
}

static
b8 xhci_initialize_controller(XHCI_Controller *controller, const u32 pci_address, const u8 interrupt_signal) {
    if(interrupt_signal < 0x20 || interrupt_signal >= 0x30) return false;
    controller->mmio_base = (volatile u8 *)(u32)(pci_address & ~0xfu);
    controller->interrupt_signal = interrupt_signal;

    const u8 capability_length = *controller->mmio_base;
    const u32 hcsparams1 = mmio_read32(at(controller->mmio_base, 0x04));
    const u32 hcsparams2 = mmio_read32(at(controller->mmio_base, 0x08));
    const u32 hccparams1 = mmio_read32(at(controller->mmio_base, 0x10));
    if(capability_length < 0x20 || (hcsparams1 & 0xff) == 0) return false;

    controller->operational_base = at(controller->mmio_base, capability_length);
    controller->runtime_base = at(controller->mmio_base, mmio_read32(at(controller->mmio_base, 0x18)));
    controller->doorbell_base = at(controller->mmio_base, mmio_read32(at(controller->mmio_base, 0x14)));
    controller->context_size = (hccparams1 & (1u << 2)) ? 64 : 32;
    controller->port_count = (hcsparams1 >> 24) & 0xff;
    controller->slot_count = min(hcsparams1 & 0xff, XHCI_DEVICE_CAPACITY);
    if(!take_legacy_ownership(controller, hccparams1)) return false;
    if(!wait_for_bits(at(controller->operational_base, 0x04), XHCI_USBSTS_Controller_Not_Ready, false)) return false;

    mmio_write32(at(controller->operational_base, 0x00), 0);
    if(!wait_for_bits(at(controller->operational_base, 0x04), XHCI_USBSTS_Halted, true)) return false;

    mmio_write32(at(controller->operational_base, 0x00), XHCI_USBCMD_Host_Controller_Reset);
    if(!wait_for_bits(at(controller->operational_base, 0x00), XHCI_USBCMD_Host_Controller_Reset, false)) return false;
    if(!wait_for_bits(at(controller->operational_base, 0x04), XHCI_USBSTS_Controller_Not_Ready, false)) return false;

    /* A real controller may require private DMA pages.  QEMU commonly asks
     * for none, so leaving DCBAA[0] empty hides this initialization error. */
    const u32 scratchpad_count = ((hcsparams2 >> 27) & 0x1f) | (((hcsparams2 >> 21) & 0x1f) << 5);
    if(scratchpad_count) {
        if(!(mmio_read32(at(controller->operational_base, 0x08)) & 1u)) return false;
        if(scratchpad_count > XHCI_SCRATCHPAD_CAPACITY - scratchpad_pages_used) return false;

        for(u32 i = 0; i < scratchpad_count; ++i) {
            controller->scratchpad_array[i] = PHYSICAL_ADDRESS(scratchpad_pages[scratchpad_pages_used++]);
        }
        controller->dcbaa[0] = PHYSICAL_ADDRESS(controller->scratchpad_array);
    }

    ring_initialize(&controller->command_ring);
    ZERO_STRUCT(&controller->event_ring);
    controller->event_ring.cycle = 1;
    controller->event_ring_segment_table = (XHCI_ERST_Entry) { PHYSICAL_ADDRESS(controller->event_ring.trbs), XHCI_RING_CAPACITY, 0 };

    mmio_write64(at(controller->operational_base, 0x30), PHYSICAL_ADDRESS(controller->dcbaa));
    mmio_write64(at(controller->operational_base, 0x18), PHYSICAL_ADDRESS(controller->command_ring.trbs) | 1);
    mmio_write32(at(controller->operational_base, 0x38), controller->slot_count);
    mmio_write32(at(controller->runtime_base, 0x28), 1);
    mmio_write64(at(controller->runtime_base, 0x30), PHYSICAL_ADDRESS(&controller->event_ring_segment_table));
    mmio_write64(at(controller->runtime_base, 0x38), PHYSICAL_ADDRESS(controller->event_ring.trbs));
    memory_barrier();

    mmio_write32(at(controller->operational_base, 0x00), XHCI_USBCMD_Run_Stop);
    return wait_for_bits(at(controller->operational_base, 0x04), XHCI_USBSTS_Halted, false);
}

// ---------------------------------------------------------------------------------------------------------------
// Keyboard Report Handling
// ---------------------------------------------------------------------------------------------------------------

static
void enqueue_keyboard_report(XHCI_Controller *controller) {
    XHCI_Device *device = controller->keyboard_device;
    XHCI_Ring *ring = &device->bulk_rings[HCI_DIRECTION_In];

    controller->keyboard_transfer = ring_push(ring, (XHCI_TRB) {
        .parameter_low = PHYSICAL_ADDRESS(&controller->keyboard_report),
        .status = sizeof(controller->keyboard_report),
        .control = XHCI_TRB_TYPE_Normal | XHCI_TRB_IOC | XHCI_TRB_ISP,
    });
    memory_barrier();
    mmio_write32(at(controller->doorbell_base, device->slot * 4), controller->keyboard_endpoint_id);
}

static
void poll_keyboard_reports(XHCI_Controller *controller) {
    XHCI_TRB event;
    while(event_pop(controller, &event)) {
        const u64 transfer_pointer = ((u64)event.parameter_high << 32) | event.parameter_low;
        if(trb_type(&event) != XHCI_TRB_TYPE_Transfer_Event ||
            transfer_pointer != PHYSICAL_ADDRESS(controller->keyboard_transfer)) continue;

        const u32 code = completion_code(&event);
        if(code == 1 || code == 13) { // Success or short packet
            hid_parse_keyboard_report(controller->keyboard_report);
        }
        enqueue_keyboard_report(controller);
    }
}

static
void xhci_interrupt(void) {
    for(u32 i = 0; i < xhci_controller_count; ++i) {
        XHCI_Controller *controller = &xhci_controllers[i];
        if(!controller->keyboard_transfer) continue;

        volatile u8 *iman = at(controller->runtime_base, 0x20);
        if(!(mmio_read32(iman) & XHCI_IMAN_Interrupt_Pending)) continue;

        poll_keyboard_reports(controller);
        mmio_write32(iman, XHCI_IMAN_Interrupt_Pending | XHCI_IMAN_Interrupt_Enable);
        mmio_write32(at(controller->operational_base, 0x04), XHCI_USBSTS_Event_Interrupt);
    }
}

static
b8 xhci_initialize_keyboard_report(XHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx) {
    XHCI_Device *device = find_device(controller, device_idx);
    const u8 id = endpoint_id(endpoint_idx, HCI_DIRECTION_In);
    if(!device || endpoint_idx == 0 || endpoint_idx >= 16 || device->bulk_endpoint_ids[HCI_DIRECTION_In] != id) return false;

    controller->keyboard_device = device;
    controller->keyboard_endpoint_id = id;
    enqueue_keyboard_report(controller);

    const u8 interrupt_index = controller->interrupt_signal - 0x20;
    if(!xhci_interrupt_registered[interrupt_index]) {
        interrupt_register_callback(controller->interrupt_signal, xhci_interrupt);
        xhci_interrupt_registered[interrupt_index] = true;
    }

    volatile u8 *iman = at(controller->runtime_base, 0x20);
    mmio_write32(iman, XHCI_IMAN_Interrupt_Pending | XHCI_IMAN_Interrupt_Enable);
    mmio_write32(at(controller->operational_base, 0x00), mmio_read32(at(controller->operational_base, 0x00)) | XHCI_USBCMD_Interrupt_Enable);
    return true;
}

static s32 xhci_get_port_capacity(const XHCI_Controller *controller) { return controller->port_count; }

static
HCI_Device_Speed xhci_get_port_speed(const XHCI_Controller *controller, const u8 port_idx) {
    if(port_idx >= controller->port_count) return HCI_DEVICE_SPEED_Error;

    const u32 portsc = mmio_read32(at(controller->operational_base, 0x400 + port_idx * 0x10));
    switch((portsc >> 10) & 0xf) {
        case 1: return HCI_DEVICE_SPEED_Full;
        case 2: return HCI_DEVICE_SPEED_Low;
        case 3: return HCI_DEVICE_SPEED_High;
        case 4: return HCI_DEVICE_SPEED_Super;
        case 5:
        case 6:
        case 7: return HCI_DEVICE_SPEED_Super_Plus;
        default: return HCI_DEVICE_SPEED_Error;
    }
}

static
b8 xhci_reset_and_enable_port(XHCI_Controller *controller, const u8 port_idx) {
    if(port_idx >= controller->port_count) return false;

    volatile u8 *port = at(controller->operational_base, 0x400 + port_idx * 0x10);
    const u32 initial = mmio_read32(port);
    if(!(initial & XHCI_PORTSC_Connect_Status)) return false;

    for(u32 attempt = 0; attempt < 2; ++attempt) {
        const u32 reset = attempt == 0 ? XHCI_PORTSC_Port_Reset : XHCI_PORTSC_Warm_Reset;
        mmio_write32(port, XHCI_PORTSC_Port_Power | reset | XHCI_PORTSC_CHANGE_BITS);

        if(wait_for_bits(port, XHCI_PORTSC_Connect_Status | XHCI_PORTSC_Port_Enabled, true)) {
            mmio_write32(port, XHCI_PORTSC_Port_Power | (mmio_read32(port) & XHCI_PORTSC_CHANGE_BITS));
            controller->enumerating_port = port_idx;
            controller->has_enumerating_port = true;
            controller->pending_device = null;
            return true;
        }
    }

    return false;
}

static
void initialize_endpoint_context(XHCI_Controller *controller, XHCI_Device *device, const u8 id, XHCI_Ring *ring, const u16 maximum_packet_size) {
    u32 *endpoint = input_context(controller, device, id + 1);
    endpoint[XHCI_ENDPOINT_TYPE_AND_PACKET_SIZE] = ((u32)maximum_packet_size << 16) |
        ((id == 1 ? 4u : (id & 1 ? 7u : 3u)) << 3) | (3u << 1);
    endpoint[XHCI_ENDPOINT_DEQUEUE_LOW] = PHYSICAL_ADDRESS(ring->trbs) | 1;
    endpoint[XHCI_ENDPOINT_DEQUEUE_HIGH] = 0;
    endpoint[XHCI_ENDPOINT_AVERAGE_TRB_LENGTH] = id == 1 ? 8 : 3072;
}

static
b8 xhci_configure_device(XHCI_Controller *controller, const u8 device_idx, const HCI_Device_Speed speed) {
    if(speed == HCI_DEVICE_SPEED_Error) return false;

    if(device_idx == 0) {
        if(!controller->has_enumerating_port || controller->pending_device) return false;
        XHCI_Device *device = free_device();
        if(!device) return false;

        const u8 port = controller->enumerating_port;
        const u32 speed_code = (mmio_read32(at(controller->operational_base, 0x400 + port * 0x10)) >> 10) & 0xf;
        if(speed_code == 0) return false;
        XHCI_TRB event;
        if(!submit_command(controller, (XHCI_TRB) { .control = XHCI_TRB_TYPE_Enable_Slot_Command }, &event)) return false;

        const u8 slot = (event.control >> 24) & 0xff;
        if(slot == 0 || slot > controller->slot_count) return false;

        ZERO_STRUCT(device);
        device->controller = controller;
        device->port = port;
        device->slot = slot;
        device->context_entries = 1;

        u32 *control = input_context(controller, device, 0);
        control[XHCI_INPUT_ADD_CONTEXT_FLAGS] = (1u << 1) | (1u << 0);

        u32 *slot_context = input_context(controller, device, 1);
        slot_context[XHCI_SLOT_ROUTE_AND_SPEED] = (speed_code << 20) | (1u << 27);
        slot_context[XHCI_SLOT_ROOT_PORT] = (u32)(port + 1) << 16;

        const u16 ep0_packet_size = (speed == HCI_DEVICE_SPEED_Super || speed == HCI_DEVICE_SPEED_Super_Plus) ? 512 : speed == HCI_DEVICE_SPEED_High ? 64 : 8;
        ring_initialize(&device->control_ring);
        initialize_endpoint_context(controller, device, 1, &device->control_ring, ep0_packet_size);
        device->ep0_packet_size = ep0_packet_size;

        controller->dcbaa[slot] = PHYSICAL_ADDRESS(device->device_contexts);
        memory_barrier();

        if(!submit_command(controller, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(device->input_contexts), .control = XHCI_TRB_TYPE_Address_Device_Command | ((u32)slot << 24) }, null)) {
            if(submit_command(controller, (XHCI_TRB) { .control = XHCI_TRB_TYPE_Disable_Slot_Command | ((u32)slot << 24) }, null)) {
                controller->dcbaa[slot] = 0;
                ZERO_STRUCT(device);
            }
            return false;
        }
        controller->pending_device = device;
        controller->has_enumerating_port = false;
        return true;
    } else {
        XHCI_Device *device = controller->pending_device;
        if(!device || device_idx != (u8)(device->port + 1)) return false;
        device->device_index = device_idx;
        controller->pending_device = null;
        return true;
    }
}

static
b8 xhci_configure_endpoint_direction(XHCI_Controller *controller, const u8 device_idx, const u8 endpoint, const HCI_Direction direction, const u16 maximum_packet_size) {
    XHCI_Device *device = find_device(controller, device_idx);
    if(endpoint >= 16 || (u32)direction >= HCI_DIRECTION_COUNT || maximum_packet_size == 0 || !device) return false;

    const u8 id = endpoint_id(endpoint, direction);
    if(endpoint == 0) {
        /* SuperSpeed bMaxPacketSize0 is an exponent.  Keep the live EP0 ring
         * while updating the packet size in the input context. */
        const HCI_Device_Speed speed = xhci_get_port_speed(controller, device->port);
        const b8 superspeed = speed == HCI_DEVICE_SPEED_Super || speed == HCI_DEVICE_SPEED_Super_Plus;
        if(superspeed && maximum_packet_size != 9) return false;
        const u16 packet_size = superspeed ? (u16)(1u << maximum_packet_size) : maximum_packet_size;
        if(device->ep0_packet_size == packet_size) return true;

        u32 *input_control = input_context(controller, device, 0);
        input_control[XHCI_INPUT_ADD_CONTEXT_FLAGS] = 1u << 1;
        u32 *ep0_context = input_context(controller, device, 2);
        ep0_context[XHCI_ENDPOINT_TYPE_AND_PACKET_SIZE] =
            (ep0_context[XHCI_ENDPOINT_TYPE_AND_PACKET_SIZE] & 0xffffu) | ((u32)packet_size << 16);
        memory_barrier();
        if(!submit_command(controller, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(device->input_contexts), .control = XHCI_TRB_TYPE_Evaluate_Context_Command | ((u32)device->slot << 24) }, null)) return false;
        device->ep0_packet_size = packet_size;
        return true;
    }

    const u8 bulk_index = (u8)direction;
    if(device->bulk_endpoint_ids[bulk_index] && device->bulk_endpoint_ids[bulk_index] != id) return false;
    XHCI_Ring *ring = &device->bulk_rings[bulk_index];
    if(!device->bulk_endpoint_ids[bulk_index]) ring_initialize(ring);

    u32 *control = input_context(controller, device, 0);
    control[XHCI_INPUT_ADD_CONTEXT_FLAGS] = (1u << id) | 1u;
    u32 *slot_context = input_context(controller, device, 1);
    const u8 context_entries = max(device->context_entries, id);
    slot_context[XHCI_SLOT_ROUTE_AND_SPEED] =
        (slot_context[XHCI_SLOT_ROUTE_AND_SPEED] & ~(31u << 27)) | ((u32)context_entries << 27);
    initialize_endpoint_context(controller, device, id, ring, maximum_packet_size);
    memory_barrier();

    if(!submit_command(controller, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(device->input_contexts), .control = XHCI_TRB_TYPE_Configure_Endpoint_Command | ((u32)device->slot << 24) }, null)) return false;
    device->bulk_endpoint_ids[bulk_index] = id;
    device->context_entries = context_entries;
    return true;
}

static
s32 submit_transfer(XHCI_Controller *controller, const XHCI_Device *device, const u8 id, XHCI_TRB *last) {
    memory_barrier();
    mmio_write32(at(controller->doorbell_base, device->slot * 4), id);

    XHCI_TRB event;
    if(!wait_for_event(controller, XHCI_TRB_TYPE_Transfer_Event, PHYSICAL_ADDRESS(last), &event)) return XHCI_ERROR;
    return (s32)(event.status & 0xffffff);
}

static
s32 xhci_control(XHCI_Controller *controller, const u8 device_idx, const HCI_Direction direction, const void *header, const u32 header_size, void *payload, const u32 payload_size) {
    if(!header || header_size != 8 || (payload_size && !payload)) return XHCI_ERROR;
    XHCI_Device *device = find_device(controller, device_idx);
    if(!device) return XHCI_ERROR;

    /* Address Device already sends SET_ADDRESS through the controller. */
    const u8 *setup_bytes = header;
    if(direction == HCI_DIRECTION_Out && setup_bytes[0] == 0 && setup_bytes[1] == 5 && payload_size == 0) return 0;

    XHCI_Ring *ring = &device->control_ring;
    u64 setup = 0;
    move_memory(&setup, header, 8);

    ring_push(ring, (XHCI_TRB) { .parameter_low = (u32)setup, .parameter_high = (u32)(setup >> 32), .status = 8, .control = XHCI_TRB_TYPE_Setup_Stage | XHCI_TRB_IDT | ((payload_size ? (direction == HCI_DIRECTION_In ? 3u : 2u) : 0u) << 16) });

    XHCI_TRB *data_stage = null;
    if(payload_size) data_stage = ring_push(ring, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(payload), .status = payload_size, .control = XHCI_TRB_TYPE_Data_Stage | XHCI_TRB_IOC | (direction == HCI_DIRECTION_In ? XHCI_TRB_ISP | (1u << 16) : 0) });

    XHCI_TRB *status_stage = ring_push(ring, (XHCI_TRB) { .control = XHCI_TRB_TYPE_Status_Stage | XHCI_TRB_IOC | (direction == HCI_DIRECTION_Out ? (1u << 16) : 0) });

    const s32 residual = submit_transfer(controller, device, 1, data_stage ? data_stage : status_stage);
    if(residual < 0) return XHCI_ERROR;
    if(data_stage && !wait_for_event(controller, XHCI_TRB_TYPE_Transfer_Event, PHYSICAL_ADDRESS(status_stage), null)) return XHCI_ERROR;
    return (s32)payload_size - residual;
}

static
s32 xhci_bulk(XHCI_Controller *controller, const u8 device_idx, const u8 endpoint, const HCI_Direction direction, void *data, const u32 size) {
    if(!size || !data) return 0;
    if(endpoint == 0 || endpoint >= 16 || (u32)direction >= HCI_DIRECTION_COUNT) return XHCI_ERROR;

    XHCI_Device *device = find_device(controller, device_idx);
    if(!device) return XHCI_ERROR;
    const u8 id = endpoint_id(endpoint, direction);
    if(device->bulk_endpoint_ids[direction] != id) return XHCI_ERROR;
    XHCI_Ring *ring = &device->bulk_rings[direction];
    XHCI_TRB *last = ring_push(ring, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(data), .status = size, .control = XHCI_TRB_TYPE_Normal | XHCI_TRB_IOC });

    const s32 residual = submit_transfer(controller, device, id, last);
    return residual < 0 ? XHCI_ERROR : (s32)size - residual;
}

static s32 xhci_control_read(XHCI_Controller *c, u8 d, void *h, u32 hs, void *p, u32 ps) { return xhci_control(c, d, HCI_DIRECTION_In, h, hs, p, ps); }
static s32 xhci_control_write(XHCI_Controller *c, u8 d, const void *h, u32 hs, const void *p, u32 ps) { return xhci_control(c, d, HCI_DIRECTION_Out, h, hs, (void *)p, ps); }
static s32 xhci_bulk_read(XHCI_Controller *c, u8 d, u8 e, void *p, u32 s) { return xhci_bulk(c, d, e, HCI_DIRECTION_In, p, s); }
static s32 xhci_bulk_write(XHCI_Controller *c, u8 d, u8 e, const void *p, u32 s) { return xhci_bulk(c, d, e, HCI_DIRECTION_Out, (void *)p, s); }

XHCI_Controller *xhci_allocate_controller(void) {
    if(xhci_controller_count >= XHCI_MAX_CONTROLLERS) return null;
    XHCI_Controller *controller = &xhci_controllers[xhci_controller_count++];
    ZERO_STRUCT(controller);
    return controller;
}

HCI_Controller hci_controller_from_xhci(XHCI_Controller *xhci) {
    return (HCI_Controller) { xhci,
        (HCI_Initialize_Controller) xhci_initialize_controller,
        (HCI_Initialize_Keyboard_Report) xhci_initialize_keyboard_report,
        (HCI_Get_Port_Capacity) xhci_get_port_capacity,
        (HCI_Get_Port_Speed) xhci_get_port_speed,
        (HCI_Reset_And_Enable_Port) xhci_reset_and_enable_port,
        (HCI_Configure_Device) xhci_configure_device,
        (HCI_Configure_Endpoint_Direction) xhci_configure_endpoint_direction,
        (HCI_Control_Read) xhci_control_read,
        (HCI_Control_Write) xhci_control_write,
        (HCI_Bulk_Read) xhci_bulk_read,
        (HCI_Bulk_Write) xhci_bulk_write
    };
}
