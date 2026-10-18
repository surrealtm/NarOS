#include "xhci.h"
#include "base.h"
#include "ctrl.h"

#define XHCI_ERROR -1
#define WAIT_TIME_NANOSECONDS 1000000
#define WAIT_MAX_RETRIES 1000

#define XHCI_USBCMD_RUN_STOP (1u << 0)
#define XHCI_USBCMD_HOST_CONTROLLER_RESET (1u << 1)
#define XHCI_USBSTS_HALTED (1u << 0)
#define XHCI_PORTSC_CONNECT_STATUS (1u << 0)
#define XHCI_PORTSC_PORT_ENABLED (1u << 1)
#define XHCI_PORTSC_PORT_RESET (1u << 4)
#define XHCI_PORTSC_PORT_POWER (1u << 9)
#define XHCI_PORTSC_CHANGE_BITS ((1u << 17) | (1u << 18) | (1u << 19) | (1u << 20) | (1u << 21) | (1u << 22))

#define XHCI_TRB_CYCLE (1u << 0)
#define XHCI_TRB_CHAIN (1u << 4)
#define XHCI_TRB_IOC (1u << 5)
#define XHCI_TRB_IDT (1u << 6)
#define XHCI_TRB_TYPE(type) ((u32)(type) << 10)
#define XHCI_TRB_TYPE_NORMAL 1
#define XHCI_TRB_TYPE_SETUP_STAGE 2
#define XHCI_TRB_TYPE_DATA_STAGE 3
#define XHCI_TRB_TYPE_STATUS_STAGE 4
#define XHCI_TRB_TYPE_LINK 6
#define XHCI_TRB_TYPE_ENABLE_SLOT_COMMAND 9
#define XHCI_TRB_TYPE_ADDRESS_DEVICE_COMMAND 11
#define XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_COMMAND 12
#define XHCI_TRB_TYPE_COMMAND_COMPLETION_EVENT 33
#define XHCI_TRB_TYPE_TRANSFER_EVENT 32

static XHCI_Controller xhci_controllers[XHCI_MAX_CONTROLLERS];
static u32 xhci_controller_count;

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
    *(volatile u64 *)address = value;
}

static inline
volatile u8 *at(volatile u8 *base, const u32 offset) {
    return base + offset;
}

static inline
u32 trb_type(const XHCI_TRB *trb) {
    return (trb->control >> 10) & 0x3f;
}

static inline
u32 completion_code(const XHCI_TRB *trb) {
    return (trb->status >> 24) & 0xff;
}

static
void ring_initialize(XHCI_Ring *ring) {
    ZERO_STRUCT(ring);
    ring->producer_cycle = 1;
    ring->consumer_cycle = 1;
    ring->trbs[XHCI_RING_CAPACITY - 1] = (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(ring->trbs), .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_LINK) | XHCI_TRB_CYCLE | (1u << 1) };
}

static
void event_ring_initialize(XHCI_Ring *ring) {
    ZERO_STRUCT(ring);
    ring->producer_cycle = 1;
    ring->consumer_cycle = 1;
}

static
XHCI_TRB *ring_push(XHCI_Ring *ring, XHCI_TRB trb) {
    if(ring->enqueue_index == XHCI_RING_CAPACITY - 1) {
        ring->trbs[ring->enqueue_index].control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_LINK) | (ring->producer_cycle ? XHCI_TRB_CYCLE : 0) | (1u << 1);
        ring->enqueue_index = 0;
        ring->producer_cycle = !ring->producer_cycle;
    }

    trb.control |= ring->producer_cycle ? XHCI_TRB_CYCLE : 0;
    ring->trbs[ring->enqueue_index] = trb;
    return &ring->trbs[ring->enqueue_index++];
}

static
b8 controller_is_halted(const XHCI_Controller *controller) {
    return (mmio_read32(at(controller->operational_base, 0x04)) & XHCI_USBSTS_HALTED) != 0;
}

static
b8 wait_for_halted(const XHCI_Controller *controller, const b8 halted) {
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        if(controller_is_halted(controller) == halted) return true;
        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
    }
    return false;
}

static
XHCI_TRB *context(XHCI_Controller *controller, const u8 device, const b8 input, const u8 index) {
    u8 *base = input ? &controller->input_contexts[device][0][0] : &controller->device_contexts[device][0][0];
    return (XHCI_TRB *)(base + (u32)index * controller->context_size);
}

static
u8 endpoint_id(const u8 endpoint, const HCI_Direction direction) {
    return endpoint == 0 ? 1 : (u8)(endpoint * 2 + (direction == HCI_DIRECTION_In ? 2 : 1));
}

static
b8 event_pop(XHCI_Controller *controller, XHCI_TRB *event) {
    XHCI_Ring *ring = &controller->event_ring;
    XHCI_TRB *trb = &ring->trbs[ring->dequeue_index];
    if(((trb->control & XHCI_TRB_CYCLE) != 0) != ring->consumer_cycle) return false;

    *event = *trb;
    ++ring->dequeue_index;
    if(ring->dequeue_index == XHCI_RING_CAPACITY) {
        ring->dequeue_index = 0;
        ring->consumer_cycle = !ring->consumer_cycle;
    }

    mmio_write64(at(controller->runtime_base, 0x38), PHYSICAL_ADDRESS(&ring->trbs[ring->dequeue_index]) | (1u << 3));
    return true;
}

static
b8 wait_for_event(XHCI_Controller *controller, const u32 expected_type, const u64 expected_pointer, XHCI_TRB *result) {
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        XHCI_TRB event;
        while(event_pop(controller, &event)) {
            const u64 pointer = ((u64)event.parameter_high << 32) | event.parameter_low;
            if(trb_type(&event) == expected_type && (!expected_pointer || pointer == expected_pointer)) {
                if(result) *result = event;
                return completion_code(&event) == 1 || completion_code(&event) == 13;
            }
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
    return wait_for_event(controller, XHCI_TRB_TYPE_COMMAND_COMPLETION_EVENT, PHYSICAL_ADDRESS(submitted), event);
}

static
b8 xhci_initialize_controller(XHCI_Controller *controller, const u32 pci_address) {
    controller->mmio_base = (volatile u8 *)(u32)(pci_address & ~0xfu);

    const u8 capability_length = *(volatile u8 *)controller->mmio_base;
    const u32 hcsparams1 = mmio_read32(at(controller->mmio_base, 0x04));
    const u32 hccparams1 = mmio_read32(at(controller->mmio_base, 0x10));
    const u32 dboff = mmio_read32(at(controller->mmio_base, 0x14));
    const u32 rtsoff = mmio_read32(at(controller->mmio_base, 0x18));
    if(capability_length < 0x20 || (hcsparams1 & 0xff) == 0) return false;

    controller->operational_base = at(controller->mmio_base, capability_length);
    controller->runtime_base = at(controller->mmio_base, rtsoff);
    controller->doorbell_base = at(controller->mmio_base, dboff);
    controller->context_size = (hccparams1 & (1u << 2)) ? 64 : 32;
    controller->port_count = (hcsparams1 >> 24) & 0xff;

    mmio_write32(at(controller->operational_base, 0x00), 0);
    if(!wait_for_halted(controller, true)) return false;
    mmio_write32(at(controller->operational_base, 0x00), XHCI_USBCMD_HOST_CONTROLLER_RESET);
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        if(!(mmio_read32(at(controller->operational_base, 0x00)) & XHCI_USBCMD_HOST_CONTROLLER_RESET)) break;
        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
        if(i + 1 == WAIT_MAX_RETRIES) return false;
    }

    ring_initialize(&controller->command_ring);
    event_ring_initialize(&controller->event_ring);
    controller->event_ring_segment_table = (XHCI_ERST_Entry) { PHYSICAL_ADDRESS(controller->event_ring.trbs), XHCI_RING_CAPACITY, 0 };

    mmio_write64(at(controller->operational_base, 0x30), PHYSICAL_ADDRESS(controller->dcbaa));
    mmio_write64(at(controller->operational_base, 0x18), PHYSICAL_ADDRESS(controller->command_ring.trbs) | 1);
    mmio_write32(at(controller->operational_base, 0x38), min((hcsparams1 & 0xff), XHCI_DEVICE_CAPACITY));
    mmio_write32(at(controller->runtime_base, 0x28), 1);
    mmio_write64(at(controller->runtime_base, 0x30), PHYSICAL_ADDRESS(&controller->event_ring_segment_table));
    mmio_write64(at(controller->runtime_base, 0x38), PHYSICAL_ADDRESS(controller->event_ring.trbs));
    memory_barrier();

    mmio_write32(at(controller->operational_base, 0x00), XHCI_USBCMD_RUN_STOP);
    return wait_for_halted(controller, false);
}

static
s32 xhci_get_port_capacity(const XHCI_Controller *controller) {
    return controller->port_count;
}

static
HCI_Device_Speed xhci_get_port_speed(const XHCI_Controller *controller, const u8 port_idx) {
    if(port_idx >= controller->port_count) return HCI_DEVICE_SPEED_Error;

    const u32 portsc = mmio_read32(at(controller->operational_base, 0x400 + port_idx * 0x10));
    return ((portsc >> 10) & 0xf) == 2 ? HCI_DEVICE_SPEED_Low : HCI_DEVICE_SPEED_Full;
}

static
b8 xhci_reset_and_enable_port(XHCI_Controller *controller, const u8 port_idx) {
    if(port_idx >= controller->port_count) return false;

    volatile u8 *port = at(controller->operational_base, 0x400 + port_idx * 0x10);
    const u32 initial = mmio_read32(port);
    if(!(initial & XHCI_PORTSC_CONNECT_STATUS)) return false;

    controller->current_port = port_idx;
    mmio_write32(port, (initial & XHCI_PORTSC_PORT_POWER) | XHCI_PORTSC_PORT_POWER | XHCI_PORTSC_PORT_RESET | XHCI_PORTSC_CHANGE_BITS);

    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        const u32 status = mmio_read32(port);
        if((status & XHCI_PORTSC_CONNECT_STATUS) && (status & XHCI_PORTSC_PORT_ENABLED)) {
            mmio_write32(port, XHCI_PORTSC_PORT_POWER | (status & XHCI_PORTSC_CHANGE_BITS));
            return true;
        }
        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
    }

    return false;
}

static
void initialize_endpoint_context(XHCI_Controller *controller, const u8 device, const u8 id, const u16 maximum_packet_size) {
    XHCI_Ring *ring = &controller->transfer_rings[device][id];
    ring_initialize(ring);

    XHCI_TRB *endpoint = context(controller, device, true, id + 1);
    endpoint->parameter_low = PHYSICAL_ADDRESS(ring->trbs) | 1;
    endpoint->parameter_high = 0;
    endpoint->status = ((u32)maximum_packet_size << 16) | 3;
    endpoint->control = (id == 1 ? 4u : 3u) << 3; /* control endpoint, otherwise bulk */
}

static
b8 xhci_configure_device(XHCI_Controller *controller, const u8 device_idx, const HCI_Device_Speed speed) {
    if(device_idx >= XHCI_DEVICE_CAPACITY || speed == HCI_DEVICE_SPEED_Error) return false;

    if(device_idx == 0) {
        XHCI_TRB event;
        if(!submit_command(controller, (XHCI_TRB) { .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_ENABLE_SLOT_COMMAND) }, &event)) return false;

        const u8 slot = (event.control >> 24) & 0xff;
        if(slot == 0 || slot > XHCI_DEVICE_CAPACITY) return false;

        controller->slot_for_device[0] = slot;

        XHCI_TRB *control = context(controller, 0, true, 0);
        control->control = (1u << 1) | (1u << 0); /* add slot and EP0 contexts */

        XHCI_TRB *slot_context = context(controller, 0, true, 1);
        slot_context->status = ((u32)(speed == HCI_DEVICE_SPEED_Low ? 2 : 1) << 20) | ((u32)(controller->current_port + 1) << 16);
        slot_context->control = 1u << 27; /* context entries = EP0 */

        initialize_endpoint_context(controller, 0, 1, 8);

        controller->dcbaa[slot] = PHYSICAL_ADDRESS(controller->device_contexts[0]);
        memory_barrier();

        return submit_command(controller, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(controller->input_contexts[0]), .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_ADDRESS_DEVICE_COMMAND) | (1u << 9) | ((u32)slot << 24) }, null);
    } else {
        if(!controller->slot_for_device[0]) return false;

        controller->slot_for_device[device_idx] = controller->slot_for_device[0];

        // Address Device assigns the xHCI slot's USB address.  The current USB
        // layer uses port 0 first, for which its address and QEMU's first slot agree.
        return submit_command(controller, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(controller->input_contexts[0]), .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_ADDRESS_DEVICE_COMMAND) | ((u32)controller->slot_for_device[device_idx] << 24) }, null);
    }
}

static
b8 xhci_configure_endpoint_direction(XHCI_Controller *controller, const u8 device_idx, const u8 endpoint, const HCI_Direction direction, const u16 maximum_packet_size) {
    if(device_idx >= XHCI_DEVICE_CAPACITY || endpoint >= 16 || direction >= HCI_DIRECTION_COUNT || maximum_packet_size == 0 || !controller->slot_for_device[device_idx]) return false;

    const u8 id = endpoint_id(endpoint, direction);
    controller->maximum_packet_size[device_idx][id] = maximum_packet_size;
    if(endpoint == 0) {
        initialize_endpoint_context(controller, device_idx, id, maximum_packet_size);
        return true;
    }

    XHCI_TRB *control = context(controller, device_idx, true, 0);
    control->control = (1u << id) | (1u << 1);
    XHCI_TRB *slot_context = context(controller, device_idx, true, 1);
    slot_context->control = (u32)id << 27;
    initialize_endpoint_context(controller, device_idx, id, maximum_packet_size);
    memory_barrier();

    return submit_command(controller, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(controller->input_contexts[device_idx]), .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_COMMAND) | ((u32)controller->slot_for_device[device_idx] << 24) }, null);
}

static
s32 submit_transfer(XHCI_Controller *controller, const u8 device, const u8 id, XHCI_TRB *last) {
    const u8 slot = controller->slot_for_device[device];
    if(!slot) return XHCI_ERROR;

    memory_barrier();
    mmio_write32(at(controller->doorbell_base, slot * 4), id);

    XHCI_TRB event;
    if(!wait_for_event(controller, XHCI_TRB_TYPE_TRANSFER_EVENT, PHYSICAL_ADDRESS(last), &event)) return XHCI_ERROR;
    return (s32)(event.status & 0xffffff);
}

static
s32 xhci_control(XHCI_Controller *controller, const u8 device, const HCI_Direction direction, const void *header, const u32 header_size, void *payload, const u32 payload_size) {
    if(device >= XHCI_DEVICE_CAPACITY || !header || header_size != 8 || (payload_size && !payload)) return XHCI_ERROR;

    XHCI_Ring *ring = &controller->transfer_rings[device][1];
    u64 setup = 0;
    move_memory(&setup, header, 8);

    XHCI_TRB *last = ring_push(ring, (XHCI_TRB) { .parameter_low = (u32)setup, .parameter_high = (u32)(setup >> 32), .status = 8, .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_SETUP_STAGE) | XHCI_TRB_IDT | XHCI_TRB_CHAIN | ((payload_size ? (direction == HCI_DIRECTION_In ? 3u : 2u) : 0u) << 16) });

    if(payload_size) last = ring_push(ring, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(payload), .status = payload_size, .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_DATA_STAGE) | XHCI_TRB_CHAIN | (direction == HCI_DIRECTION_In ? (1u << 16) : 0) });

    last = ring_push(ring, (XHCI_TRB) { .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_STATUS_STAGE) | XHCI_TRB_IOC | (direction == HCI_DIRECTION_Out ? (1u << 16) : 0) });

    const s32 residual = submit_transfer(controller, device, 1, last);
    return residual < 0 ? XHCI_ERROR : (s32)payload_size - residual;
}

static
s32 xhci_bulk(XHCI_Controller *controller, const u8 device, const u8 endpoint, const HCI_Direction direction, void *data, const u32 size) {
    if(!size || !data) return 0;
    if(device >= XHCI_DEVICE_CAPACITY || endpoint == 0 || endpoint >= 16) return XHCI_ERROR;

    const u8 id = endpoint_id(endpoint, direction);
    XHCI_Ring *ring = &controller->transfer_rings[device][id];
    XHCI_TRB *last = ring_push(ring, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(data), .status = size, .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_NORMAL) | XHCI_TRB_IOC });

    const s32 residual = submit_transfer(controller, device, id, last);
    return residual < 0 ? XHCI_ERROR : (s32)size - residual;
}

static
s32 xhci_control_read(XHCI_Controller *c, u8 d, void *h, u32 hs, void *p, u32 ps) {
    return xhci_control(c, d, HCI_DIRECTION_In, h, hs, p, ps);
}

static
s32 xhci_control_write(XHCI_Controller *c, u8 d, const void *h, u32 hs, const void *p, u32 ps) {
    return xhci_control(c, d, HCI_DIRECTION_Out, h, hs, (void *)p, ps);
}

static
s32 xhci_bulk_read(XHCI_Controller *c, u8 d, u8 e, void *p, u32 s) {
    return xhci_bulk(c, d, e, HCI_DIRECTION_In, p, s);
}

static
s32 xhci_bulk_write(XHCI_Controller *c, u8 d, u8 e, const void *p, u32 s) {
    return xhci_bulk(c, d, e, HCI_DIRECTION_Out, (void *)p, s);
}

XHCI_Controller *xhci_allocate_controller(void) {
    if(xhci_controller_count >= XHCI_MAX_CONTROLLERS) return null;
    XHCI_Controller *controller = &xhci_controllers[xhci_controller_count++];
    ZERO_STRUCT(controller);
    return controller;
}

HCI_Controller hci_controller_from_xhci(XHCI_Controller *xhci) {
    return (HCI_Controller) { xhci,
        (HCI_Initialize_Controller) xhci_initialize_controller,
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
