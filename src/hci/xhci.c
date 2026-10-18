#include "xhci.h"
#include "base.h"
#include "ctrl.h"
#include "output.h"

#define WAIT_TIME_NANOSECONDS 1000000
#define WAIT_MAX_RETRIES 1000

/* Each controller owns statically allocated DMA rings and contexts.  Until
 * the kernel has a memory allocator, only one fits below the early stack. */
#define XHCI_MAX_CONTROLLERS 4

#define XHCI_ERROR -1

#define XHCI_USBCMD_RUN_STOP (1u << 0)
#define XHCI_USBCMD_HOST_CONTROLLER_RESET (1u << 1)
#define XHCI_USBSTS_HALTED (1u << 0)
#define XHCI_USBSTS_CONTROLLER_NOT_READY (1u << 11)
#define XHCI_PORTSC_CONNECT_STATUS (1u << 0)
#define XHCI_PORTSC_PORT_ENABLED (1u << 1)
#define XHCI_PORTSC_PORT_RESET (1u << 4)
#define XHCI_PORTSC_WARM_RESET (1u << 31)
#define XHCI_PORTSC_PORT_POWER (1u << 9)
#define XHCI_PORTSC_CHANGE_BITS ((1u << 17) | (1u << 18) | (1u << 19) | (1u << 20) | (1u << 21) | (1u << 22))

#define XHCI_TRB_CYCLE (1u << 0)
#define XHCI_TRB_ISP (1u << 2)
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
#define XHCI_TRB_TYPE_EVALUATE_CONTEXT_COMMAND 13
#define XHCI_TRB_TYPE_COMMAND_COMPLETION_EVENT 33
#define XHCI_TRB_TYPE_TRANSFER_EVENT 32

static XHCI_Controller xhci_controllers[XHCI_MAX_CONTROLLERS];
static u32 xhci_controller_count;
static u8 scratchpad_pages[XHCI_SCRATCHPAD_CAPACITY][4096] ALIGN_DECLARATION(4096);
static u32 scratchpad_pages_used;

static
void print_decimal_count(u32 count) {
    char digits[4];
    u32 length = 0;
    do {
        digits[sizeof(digits) - 1 - length++] = '0' + count % 10;
        count /= 10;
    } while(count);
    os_output_print((Str8) { digits + sizeof(digits) - length, (s32)length });
}

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
b8 wait_for_controller_ready(const XHCI_Controller *controller) {
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        if(!(mmio_read32(at(controller->operational_base, 0x04)) & XHCI_USBSTS_CONTROLLER_NOT_READY)) return true;
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
        const u8 capability_id = header & 0xff;
        const u8 next_capability_offset = (header >> 8) & 0xff;

        if(capability_id == 1) { /* USB Legacy Support extended capability */
            /* The ownership semaphores share the extended-capability header
             * dword.  The following dword is USBLEGSUP control/status, not
             * the semaphore register. */
            volatile u8 *legacy_support = at(controller->mmio_base, capability_offset);
            mmio_write32(legacy_support, mmio_read32(legacy_support) | (1u << 24));
            for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
                if(!(mmio_read32(legacy_support) & (1u << 16))) return true;
                os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
            }
            return false;
        }

        capability_offset = (u32)next_capability_offset << 2;
    }

    return true;
}

static
XHCI_TRB *context(XHCI_Controller *controller, const b8 input, const u8 index) {
    u8 *base = input ? &controller->input_contexts[0][0] : &controller->device_contexts[0][0];
    return (XHCI_TRB *)(base + (u32)index * controller->context_size);
}

static
u8 endpoint_id(const u8 endpoint, const HCI_Direction direction) {
    return endpoint == 0 ? 1 : (u8)(endpoint * 2 + (direction == HCI_DIRECTION_In ? 1 : 0));
}

static
b8 event_pop(XHCI_Controller *controller, XHCI_TRB *event) {
    XHCI_Ring *ring = &controller->event_ring;
    volatile XHCI_TRB *trb = &ring->trbs[ring->dequeue_index];
    memory_barrier();
    if(((trb->control & XHCI_TRB_CYCLE) != 0) != ring->consumer_cycle) return false;

    event->parameter_low = trb->parameter_low;
    event->parameter_high = trb->parameter_high;
    event->status = trb->status;
    event->control = trb->control;
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
            if(trb_type(&event) == expected_type && expected_pointer && pointer != expected_pointer) {
                if(expected_type == XHCI_TRB_TYPE_TRANSFER_EVENT && completion_code(&event) != 1 && completion_code(&event) != 13) {
                    os_output_print(str8("xHCI: earlier transfer TRB error, code "));
                    print_decimal_count(completion_code(&event));
                    os_output_print(str8(".\n"));
                    return false;
                }
                continue;
            }
            if(trb_type(&event) == expected_type) {
                if(result) *result = event;
                if(completion_code(&event) == 1 || completion_code(&event) == 13) return true;
                os_output_print(str8("xHCI: completion error, code "));
                print_decimal_count(completion_code(&event));
                os_output_print(str8(".\n"));
                return false;
            }
        }
        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
    }
    os_output_print(expected_type == XHCI_TRB_TYPE_TRANSFER_EVENT ? str8("xHCI: transfer event timeout.\n") : str8("xHCI: command event timeout.\n"));
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
    const u32 hcsparams2 = mmio_read32(at(controller->mmio_base, 0x08));
    const u32 hccparams1 = mmio_read32(at(controller->mmio_base, 0x10));
    const u32 dboff = mmio_read32(at(controller->mmio_base, 0x14));
    const u32 rtsoff = mmio_read32(at(controller->mmio_base, 0x18));
    if(capability_length < 0x20 || (hcsparams1 & 0xff) == 0) {
        os_output_print(str8("xHCI: invalid capabilities.\n"));
        return false;
    }

    controller->operational_base = at(controller->mmio_base, capability_length);
    controller->runtime_base = at(controller->mmio_base, rtsoff);
    controller->doorbell_base = at(controller->mmio_base, dboff);
    controller->context_size = (hccparams1 & (1u << 2)) ? 64 : 32;
    controller->port_count = (hcsparams1 >> 24) & 0xff;
    if(!take_legacy_ownership(controller, hccparams1)) {
        os_output_print(str8("xHCI: firmware ownership timeout.\n"));
        return false;
    }
    if(!wait_for_controller_ready(controller)) {
        os_output_print(str8("xHCI: controller not ready.\n"));
        return false;
    }

    mmio_write32(at(controller->operational_base, 0x00), 0);
    if(!wait_for_halted(controller, true)) {
        os_output_print(str8("xHCI: halt timeout.\n"));
        return false;
    }
    mmio_write32(at(controller->operational_base, 0x00), XHCI_USBCMD_HOST_CONTROLLER_RESET);
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        if(!(mmio_read32(at(controller->operational_base, 0x00)) & XHCI_USBCMD_HOST_CONTROLLER_RESET)) break;
        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
        if(i + 1 == WAIT_MAX_RETRIES) {
            os_output_print(str8("xHCI: reset timeout.\n"));
            return false;
        }
    }
    if(!wait_for_controller_ready(controller)) {
        os_output_print(str8("xHCI: not ready after reset.\n"));
        return false;
    }

    /* A real controller may require private DMA pages.  QEMU commonly asks
     * for none, so leaving DCBAA[0] empty hides this initialization error. */
    const u32 scratchpad_count = ((hcsparams2 >> 27) & 0x1f) | (((hcsparams2 >> 21) & 0x1f) << 5);
    if(scratchpad_count) {
        const u32 page_size = mmio_read32(at(controller->operational_base, 0x08));
        if(!(page_size & 1u)) {
            os_output_print(str8("xHCI: controller does not support 4 KiB pages.\n"));
            return false;
        }
        if(scratchpad_count > XHCI_SCRATCHPAD_CAPACITY - scratchpad_pages_used) {
            os_output_print(str8("xHCI: scratchpad pool too small; needs "));
            print_decimal_count(scratchpad_count);
            os_output_print(str8(" pages, has "));
            print_decimal_count(XHCI_SCRATCHPAD_CAPACITY - scratchpad_pages_used);
            os_output_print(str8(".\n"));
            return false;
        }
        for(u32 i = 0; i < scratchpad_count; ++i) {
            controller->scratchpad_array[i] = PHYSICAL_ADDRESS(scratchpad_pages[scratchpad_pages_used++]);
        }
        controller->dcbaa[0] = PHYSICAL_ADDRESS(controller->scratchpad_array);
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
    if(!wait_for_halted(controller, false)) {
        os_output_print(str8("xHCI: start timeout.\n"));
        return false;
    }
    return true;
}

static
s32 xhci_get_port_capacity(const XHCI_Controller *controller) {
    return controller->port_count;
}

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
    if(!(initial & XHCI_PORTSC_CONNECT_STATUS)) return false;

    controller->current_port = port_idx;
    for(u32 attempt = 0; attempt < 2; ++attempt) {
        const u32 reset = attempt == 0 ? XHCI_PORTSC_PORT_RESET : XHCI_PORTSC_WARM_RESET;
        mmio_write32(port, XHCI_PORTSC_PORT_POWER | reset | XHCI_PORTSC_CHANGE_BITS);

        for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
            const u32 status = mmio_read32(port);
            if((status & XHCI_PORTSC_CONNECT_STATUS) && (status & XHCI_PORTSC_PORT_ENABLED)) {
                mmio_write32(port, XHCI_PORTSC_PORT_POWER | (status & XHCI_PORTSC_CHANGE_BITS));
                return true;
            }
            os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
        }
    }

    os_output_print(str8("xHCI: connected port did not enable.\n"));
    return false;
}

static
void initialize_endpoint_context(XHCI_Controller *controller, const u8 id, const u16 maximum_packet_size) {
    XHCI_Ring *ring = &controller->transfer_rings[id];
    ring_initialize(ring);

    XHCI_TRB *endpoint = context(controller, true, id + 1);
    /* Endpoint Context: DW1 holds packet size, error count and type;
     * DW2/DW3 hold the transfer-ring dequeue pointer and cycle state. */
    endpoint->parameter_high = ((u32)maximum_packet_size << 16) |
        ((id == 1 ? 4u : (id & 1 ? 7u : 3u)) << 3) | (3u << 1);
    endpoint->status = PHYSICAL_ADDRESS(ring->trbs) | 1;
    endpoint->control = 0;
    ((u32 *)endpoint)[4] = id == 1 ? 8 : 3072; /* Average TRB Length, Endpoint Context DW4 */
}

static
b8 xhci_configure_device(XHCI_Controller *controller, const u8 device_idx, const HCI_Device_Speed speed) {
    if(speed == HCI_DEVICE_SPEED_Error) return false;

    if(device_idx == 0) {
        const u32 speed_code = (mmio_read32(at(controller->operational_base, 0x400 + controller->current_port * 0x10)) >> 10) & 0xf;
        if(speed_code == 0) return false;
        XHCI_TRB event;
        if(!submit_command(controller, (XHCI_TRB) { .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_ENABLE_SLOT_COMMAND) }, &event)) return false;

        const u8 slot = (event.control >> 24) & 0xff;
        if(slot == 0 || slot > XHCI_DEVICE_CAPACITY) return false;

        controller->slot_for_device[0] = slot;

        XHCI_TRB *control = context(controller, true, 0);
        control->parameter_high = (1u << 1) | (1u << 0); /* add slot and EP0 contexts */

        XHCI_TRB *slot_context = context(controller, true, 1);
        slot_context->parameter_low = (speed_code << 20) | (1u << 27);
        slot_context->parameter_high = (u32)(controller->current_port + 1) << 16;

        const u16 ep0_packet_size = (speed == HCI_DEVICE_SPEED_Super || speed == HCI_DEVICE_SPEED_Super_Plus) ? 512 : speed == HCI_DEVICE_SPEED_High ? 64 : 8;
        initialize_endpoint_context(controller, 1, ep0_packet_size);
        controller->maximum_packet_size[1] = ep0_packet_size;

        controller->dcbaa[slot] = PHYSICAL_ADDRESS(controller->device_contexts);
        memory_barrier();

        return submit_command(controller, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(controller->input_contexts), .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_ADDRESS_DEVICE_COMMAND) | ((u32)slot << 24) }, null);
    } else {
        if(!controller->slot_for_device[0]) return false;

        controller->slot_for_device[device_idx] = controller->slot_for_device[0];
        return true;
    }
}

static
b8 xhci_configure_endpoint_direction(XHCI_Controller *controller, const u8 device_idx, const u8 endpoint, const HCI_Direction direction, const u16 maximum_packet_size) {
    if(endpoint >= 16 || direction >= HCI_DIRECTION_COUNT || maximum_packet_size == 0 || !controller->slot_for_device[device_idx]) return false;

    const u8 id = endpoint_id(endpoint, direction);
    if(endpoint == 0) {
        /* SuperSpeed bMaxPacketSize0 is an exponent.  Keep the live EP0 ring
         * while updating the packet size in the input context. */
        const HCI_Device_Speed speed = xhci_get_port_speed(controller, controller->current_port);
        const b8 superspeed = speed == HCI_DEVICE_SPEED_Super || speed == HCI_DEVICE_SPEED_Super_Plus;
        if(superspeed && maximum_packet_size != 9) return false;
        const u16 packet_size = superspeed ? (u16)(1u << maximum_packet_size) : maximum_packet_size;
        if(controller->maximum_packet_size[1] == packet_size) return true;

        XHCI_TRB *input_control = context(controller, true, 0);
        input_control->parameter_high = 1u << 1;
        XHCI_TRB *ep0_context = context(controller, true, 2);
        ep0_context->parameter_high = (ep0_context->parameter_high & 0xffffu) | ((u32)packet_size << 16);
        memory_barrier();
        if(!submit_command(controller, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(controller->input_contexts), .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_EVALUATE_CONTEXT_COMMAND) | ((u32)controller->slot_for_device[device_idx] << 24) }, null)) return false;
        controller->maximum_packet_size[1] = packet_size;
        return true;
    }

    XHCI_TRB *control = context(controller, true, 0);
    control->parameter_high = (1u << id) | 1u; /* add endpoint and slot contexts */
    XHCI_TRB *slot_context = context(controller, true, 1);
    slot_context->parameter_low = (slot_context->parameter_low & ~(31u << 27)) | ((u32)id << 27);
    initialize_endpoint_context(controller, id, maximum_packet_size);
    memory_barrier();

    if(!submit_command(controller, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(controller->input_contexts), .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_COMMAND) | ((u32)controller->slot_for_device[device_idx] << 24) }, null)) return false;
    controller->maximum_packet_size[id] = maximum_packet_size;
    return true;
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
    if(!header || header_size != 8 || (payload_size && !payload)) return XHCI_ERROR;

    /* Address Device already sends SET_ADDRESS through the controller. */
    const u8 *setup_bytes = header;
    if(direction == HCI_DIRECTION_Out && setup_bytes[0] == 0 && setup_bytes[1] == 5 && payload_size == 0) return 0;

    XHCI_Ring *ring = &controller->transfer_rings[1];
    u64 setup = 0;
    move_memory(&setup, header, 8);

    ring_push(ring, (XHCI_TRB) { .parameter_low = (u32)setup, .parameter_high = (u32)(setup >> 32), .status = 8, .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_SETUP_STAGE) | XHCI_TRB_IDT | ((payload_size ? (direction == HCI_DIRECTION_In ? 3u : 2u) : 0u) << 16) });

    XHCI_TRB *data_stage = null;
    if(payload_size) data_stage = ring_push(ring, (XHCI_TRB) { .parameter_low = PHYSICAL_ADDRESS(payload), .status = payload_size, .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_DATA_STAGE) | XHCI_TRB_IOC | (direction == HCI_DIRECTION_In ? XHCI_TRB_ISP | (1u << 16) : 0) });

    XHCI_TRB *status_stage = ring_push(ring, (XHCI_TRB) { .control = XHCI_TRB_TYPE(XHCI_TRB_TYPE_STATUS_STAGE) | XHCI_TRB_IOC | (direction == HCI_DIRECTION_Out ? (1u << 16) : 0) });

    const s32 residual = submit_transfer(controller, device, 1, data_stage ? data_stage : status_stage);
    if(residual < 0) return XHCI_ERROR;
    if(data_stage && !wait_for_event(controller, XHCI_TRB_TYPE_TRANSFER_EVENT, PHYSICAL_ADDRESS(status_stage), null)) return XHCI_ERROR;
    return (s32)payload_size - residual;
}

static
s32 xhci_bulk(XHCI_Controller *controller, const u8 device, const u8 endpoint, const HCI_Direction direction, void *data, const u32 size) {
    if(!size || !data) return 0;
    if(endpoint == 0 || endpoint >= 16) return XHCI_ERROR;

    const u8 id = endpoint_id(endpoint, direction);
    XHCI_Ring *ring = &controller->transfer_rings[id];
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
