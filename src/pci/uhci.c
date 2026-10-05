#include "uhci.h"
#include "base.h"
#include "ctrl.h"
#include "port/port.h"

#define WAIT_TIME_NANOSECONDS 1000000 // Wait in 1 millisecond intervals
#define WAIT_MAX_RETRIES 100 // How many iterations in a wait-loop

#define INVALID_TRANSFER_DESCRIPTOR_INDEX -1
#define TRANSFER_DESCRIPTOR_ENCODED_LENGTH(size_in_bytes)     ((size_in_bytes) > 0 ? (size_in_bytes) - 1 : 0x7ff)
#define TRANSFER_DESCRIPTOR_DECODED_LENGTH(length)            ((length) != 0x7ff ? ((length) + 1) : 0)
#define REQUIRED_TRANSFER_DESCRIPTORS(maximum_packet_size, size_in_bytes) (size_in_bytes > 0  ? (size_in_bytes + maximum_packet_size - 1) / maximum_packet_size : 0)

// To save bit space, addresses in UHCI are assumed to be 16-byte aligned, and the lower four bits are therefore ommitted from
// the bit representation
#define TRANSFER_DESCRIPTOR_ADDRESS(pointer) (PHYSICAL_ADDRESS(pointer) >> 4)
#define TRANSFER_DESCRIPTOR_POINTER(address) (VIRTUAL_ADDRESS(address << 4))

// ---------------------------------------------------------------------------------------------------------------
// UHCI Interaction
// Port Mapping: https://wiki.osdev.org/Universal_Host_Controller_Interface
// ---------------------------------------------------------------------------------------------------------------

typedef b8 (*Status_Check)(const UHCI_Controller *);

static inline
b8 transfer_descriptor_has_error(const UHCI_Transfer_Descriptor *descriptor) {
    return !descriptor->status.active && (descriptor->status.bit_error || descriptor->status.timeout_crc || descriptor->status.babble_detected || descriptor->status.data_buffer_error || descriptor->status.stalled);
}

static inline
b8 transfer_descriptor_is_short_packet(const UHCI_Transfer_Descriptor *descriptor) {
    return !descriptor->status.active && descriptor->packet_header.packet_type == UHCI_PACKET_In && descriptor->status.short_packet_detect && (TRANSFER_DESCRIPTOR_DECODED_LENGTH(descriptor->status.transferred_length) < TRANSFER_DESCRIPTOR_DECODED_LENGTH(descriptor->packet_header.requested_length));
}

static
b8 wait_for_status(const UHCI_Controller *controller, Status_Check check) {
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        if(check(controller)) {
            return true;
        }
        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
    }
    return false;
}

static inline
b8 is_halted(const UHCI_Controller *controller) {
    return (port_read_u16(controller->pci_address + 0x02) & (1 << 5)) != 0;
}

static inline
b8 is_host_reset(const UHCI_Controller *controller) {
    return (port_read_u16(controller->pci_address + 0x00) & (1 << 1)) == 0;
}

static inline
void write_frame_list_pointer(const UHCI_Controller *controller, const u32 *frame_list) {
    port_write_u32(controller->pci_address + 0x08, PHYSICAL_ADDRESS(frame_list));
}

static inline
void write_frame_number(const UHCI_Controller *controller, const u16 frame_number) {
    port_write_u16(controller->pci_address + 0x06, frame_number);
}

static inline
u16 read_frame_number(const UHCI_Controller *controller) {
    return port_read_u16(controller->pci_address + 0x06);
}

static inline
void write_frame_timing(const UHCI_Controller *controller, const u8 value) {
    port_write_u8(controller->pci_address + 0x0c, value);
}

static inline
void toggle_interrupts(const UHCI_Controller *controller, const b8 enabled) {
    port_write_u16(controller->pci_address + 0x4, !!(enabled));
}

static inline
void clear_signals(const UHCI_Controller *controller, const u16 mask) {
    port_write_u16(controller->pci_address + 0x02, mask);
}

static inline
void issue_command(const UHCI_Controller *controller, const u16 command) {
    port_write_u16(controller->pci_address + 0x0, command);
}

static inline
void write_port_status(const UHCI_Controller *controller, const u32 port_idx, const UHCI_Port_Status status) {
    port_write_u16(controller->ports[port_idx].register_address, status);
}

static inline
UHCI_Port_Status read_port_status(const UHCI_Controller *controller, const u32 port_idx) {
    return (UHCI_Port_Status) port_read_u16(controller->ports[port_idx].register_address);
}

// ---------------------------------------------------------------------------------------------------------------
// Endpoint Handling
// ---------------------------------------------------------------------------------------------------------------

static
u8 get_data_toggle(UHCI_Endpoint *endpoint, const UHCI_Packet_Type packet_type) {
    switch(packet_type) {
        case UHCI_PACKET_In:  return endpoint->next_data_toggle[0];
        case UHCI_PACKET_Out: return endpoint->next_data_toggle[1];
        default: return 0;
    }
}

static
void set_data_toggle(UHCI_Endpoint *endpoint, const UHCI_Packet_Type packet_type, const u8 desired_toggle) {
    switch(packet_type) {
        case UHCI_PACKET_In:  endpoint->next_data_toggle[0] = desired_toggle; break;
        case UHCI_PACKET_Out: endpoint->next_data_toggle[1] = desired_toggle; break;
        case UHCI_PACKET_Setup: break;
    }
}

static
void reset_data_toggles(UHCI_Endpoint *endpoint, const u8 desired_toggle) {
    for(u32 i = 0; i < ARRAY_COUNT(endpoint->next_data_toggle); ++i) {
        endpoint->next_data_toggle[i] = desired_toggle;
    }
}

static
u8 get_and_advance_data_toggle(UHCI_Endpoint *endpoint, const UHCI_Packet_Type packet_type) {
    const u8 data_toggle = get_data_toggle(endpoint, packet_type);
    set_data_toggle(endpoint, packet_type, !data_toggle);
    return data_toggle;
}

static
u16 get_maximum_packet_size(const UHCI_Endpoint *endpoint, const UHCI_Packet_Type packet_type) {
    switch(packet_type) {
        case UHCI_PACKET_In: return endpoint->maximum_packet_size[0];
        case UHCI_PACKET_Out: return endpoint->maximum_packet_size[1];
        default: return 8;
    }
}

static
UHCI_Endpoint *find_endpoint(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx) {
    return &controller->devices[device_idx].endpoints[endpoint_idx];
}

// ---------------------------------------------------------------------------------------------------------------
// Transfer Descriptor Handling
// ---------------------------------------------------------------------------------------------------------------

static
void clear_frame_list(UHCI_Controller *controller) {
    for(u32 i = 0; i < ARRAY_COUNT(controller->frame_list); ++i) {
        controller->frame_list[i] = 1;
    }
}

static
void add_single_transfer_descriptor(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const UHCI_Packet_Type packet_type, const u32 requested_length, const void *data) {
    assert(controller->active_transfer_descriptors < UHCI_TRANSFER_DESCRIPTOR_CAPACITY, "Reached the capacity of the UHCI transfer descriptor list.");

    UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);

    if(controller->active_transfer_descriptors > 0) {
        const UHCI_Transfer_Descriptor_Link link = (UHCI_Transfer_Descriptor_Link) { .terminate = false, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor, .pointer = TRANSFER_DESCRIPTOR_ADDRESS(&controller->transfer_descriptors[controller->active_transfer_descriptors]) };
        controller->transfer_descriptors[controller->active_transfer_descriptors - 1].link = link;
    }

    const u32 error_retry_counter = 3;
    const u32 short_packet_detect = (packet_type == UHCI_PACKET_In && requested_length > 0) ? 1 : 0;

    const UHCI_Transfer_Descriptor_Link link = (UHCI_Transfer_Descriptor_Link) { .terminate = true };
    const UHCI_Transfer_Descriptor_Status status = (UHCI_Transfer_Descriptor_Status) { .transferred_length = TRANSFER_DESCRIPTOR_ENCODED_LENGTH(0), .error_retry_counter = error_retry_counter, .active = 1, .short_packet_detect = short_packet_detect };
    const UHCI_Transfer_Descriptor_Packet_Header packet_header = (UHCI_Transfer_Descriptor_Packet_Header) { .packet_type = packet_type, .device = device_idx, .endpoint = endpoint_idx, .data_toggle = get_and_advance_data_toggle(endpoint, packet_type), .requested_length = TRANSFER_DESCRIPTOR_ENCODED_LENGTH(requested_length) };
    controller->transfer_descriptors[controller->active_transfer_descriptors] = (UHCI_Transfer_Descriptor) { .link = link, .status = status, .packet_header = packet_header, .buffer_address = PHYSICAL_ADDRESS(data) };

    ++controller->active_transfer_descriptors;
}

static
void add_transfer_descriptors(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const UHCI_Packet_Type packet_type, const u32 size_in_bytes, const void *data) {
    const UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);
    const u16 maximum_packet_size = get_maximum_packet_size(endpoint, packet_type);
    const u32 descriptor_count = REQUIRED_TRANSFER_DESCRIPTORS(maximum_packet_size, size_in_bytes);
    for(u32 descriptor_idx = 0; descriptor_idx < descriptor_count; ++descriptor_idx) {
        const u32 offset_in_bytes = descriptor_idx * maximum_packet_size;
        const u32 packet_size_in_bytes = min(size_in_bytes - offset_in_bytes, maximum_packet_size);
        add_single_transfer_descriptor(controller, device_idx, endpoint_idx, packet_type, packet_size_in_bytes, (const char *) data + offset_in_bytes);
    }
}

static
void update_queue_head_pointer(UHCI_Controller *controller, const u32 transfer_descriptor_idx) {
    controller->queue_head.vertical_pointer = (UHCI_Frame_List_Entry) { .terminate = 0, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor, .pointer = TRANSFER_DESCRIPTOR_ADDRESS(&controller->transfer_descriptors[transfer_descriptor_idx]) };
    memory_barrier(); // Make sure the hardware sees this change before continuing with the execution
}

static
s32 submit(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const s32 status_descriptor_idx) {
    STATIC_ASSERT(sizeof(controller->queue_head) == sizeof(UHCI_Queue_Head));

    //
    // Prepare the queue head for this descriptor table
    //
    update_queue_head_pointer(controller, 0);

    //
    // Make this queue live on the controller so that the descriptors should be executed
    //
    UHCI_Transfer_Descriptor_Link link_to_queue = (UHCI_Transfer_Descriptor_Link) { .terminate = false, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Queue_Head, .pointer = TRANSFER_DESCRIPTOR_ADDRESS(&controller->queue_head) };
    for(u32 i = 0; i < ARRAY_COUNT(controller->frame_list); ++i) {
        STATIC_ASSERT(sizeof(link_to_queue) == sizeof(u32));
        move_memory(&controller->frame_list[i], &link_to_queue, sizeof(u32));
    }

    //
    // Wait until the controller has executed all descriptors in our queue
    //
    b8 successful = true;
    while(successful && !controller->queue_head.vertical_pointer.terminate) {
        const UHCI_Transfer_Descriptor *current_descriptor = TRANSFER_DESCRIPTOR_POINTER(controller->queue_head.vertical_pointer.pointer);
        if(transfer_descriptor_has_error(current_descriptor)) {
            successful = false;
            UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);
            set_data_toggle(endpoint, current_descriptor->packet_header.packet_type, current_descriptor->packet_header.data_toggle);
            break;
        } else if(transfer_descriptor_is_short_packet(current_descriptor)) {
            UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);
            set_data_toggle(endpoint, current_descriptor->packet_header.packet_type, !current_descriptor->packet_header.data_toggle);
            if(status_descriptor_idx != INVALID_TRANSFER_DESCRIPTOR_INDEX) {
                // Detected a short packet, but the status transfer still needs to complete for a valid hardware interaction.
                // Update the queue so that it points at that transfer descriptor...
                update_queue_head_pointer(controller, status_descriptor_idx);
            } else {
                // Reached the end of this transmission, exit...
                break;
            }
        } else {
            os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
        }
    }

    //
    // Reset the frame list so that the controller does not attempt to re-execute these descriptors again.
    // Wait until the controller has moved onto a new frame number (to which we've already written the dummy
    // terminate frame), to make sure that it is not still trying to read from our current queue
    //
    clear_frame_list(controller);
    memory_barrier();
    const u16 current_frame_number = read_frame_number(controller);
    while(read_frame_number(controller) == current_frame_number) {
        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
    }

    s32 number_of_bytes_transferred = 0;
    for(u32 i = 0; i < controller->active_transfer_descriptors; ++i) {
        if(controller->transfer_descriptors[i].packet_header.packet_type != UHCI_PACKET_Setup) {
            number_of_bytes_transferred += TRANSFER_DESCRIPTOR_DECODED_LENGTH(controller->transfer_descriptors[i].status.transferred_length);
        }
    }

    controller->active_transfer_descriptors = 0;
    return successful ? number_of_bytes_transferred : -1;
}

static
s32 control(UHCI_Controller *controller, const u8 device_idx, const UHCI_Packet_Type payload_direction, const void *header_data, const u32 header_size_in_bytes, const void *payload, const u32 payload_size_in_bytes) {
    assert(header_size_in_bytes == 8, "The header size for a control transaction was expected to be 8 bytes long.");

    const u8 endpoint_idx = 0;

    UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);
    reset_data_toggles(endpoint, 1); // A setup transfer descriptor resets all data toggles for this endpoint

    // Setup transfer
    {
        add_single_transfer_descriptor(controller, device_idx, endpoint_idx, UHCI_PACKET_Setup, header_size_in_bytes, header_data);
    }

    // Payload data transfer
    {
        add_transfer_descriptors(controller, device_idx, endpoint_idx, payload_direction, payload_size_in_bytes, payload);
    }

    // Status transfer
    {
        const UHCI_Packet_Type packet_type = (payload_direction == UHCI_PACKET_In) ? UHCI_PACKET_Out : UHCI_PACKET_In; // Inverse direction to the payload transfer
        add_single_transfer_descriptor(controller, device_idx, endpoint_idx, packet_type, 0, null);
    }

    return submit(controller, device_idx, endpoint_idx, controller->active_transfer_descriptors - 1);
}



// ---------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------

b8 uhci_initialize_controller(UHCI_Controller *controller, const u32 pci_address) {
    controller->pci_address = pci_address;
    controller->queue_head  = (UHCI_Queue_Head) { (UHCI_Frame_List_Entry) { .terminate = 1 }, (UHCI_Frame_List_Entry) { .terminate = 1 } };

    for(u32 device_idx = 0; device_idx < UHCI_DEVICE_CAPACITY; ++device_idx) {
        for(u32 endpoint_idx = 0; endpoint_idx < UHCI_ENDPOINT_CAPACITY; ++endpoint_idx) {
            // This is really only the default for the 0th endpoint, all other endpoints need to be configured before they may
            // be used
            controller->devices[device_idx].endpoints[endpoint_idx].maximum_packet_size[0] = 8;
            controller->devices[device_idx].endpoints[endpoint_idx].maximum_packet_size[1] = 8;
            reset_data_toggles(&controller->devices[device_idx].endpoints[endpoint_idx], 0);
        }
    }

    for(u32 i = 0; i < UHCI_PORT_CAPACITY; ++i) {
        controller->ports[i] = (UHCI_Port) { .register_address = controller->pci_address + 0x10 + (i * 2), .connected = false };
    }

    clear_frame_list(controller);

    issue_command(controller, UHCI_COMMAND_Halt);
    if(!wait_for_status(controller, is_halted)) {
        return false;
    }

    issue_command(controller, UHCI_COMMAND_Host_Reset);
    if(!wait_for_status(controller, is_host_reset)) {
        return false;
    }

    clear_signals(controller, 0x1f); // Clear all signals except `HCHalted`
    toggle_interrupts(controller, false);
    write_frame_list_pointer(controller, controller->frame_list);
    write_frame_number(controller, 0x0);
    write_frame_timing(controller, 0x40);
    issue_command(controller, UHCI_COMMAND_Start);

    return true;
}

b8 uhci_is_port_enabled(UHCI_Controller *controller, const u8 port_idx) {
    if(port_idx > 1) return false;
    return read_port_status(controller, port_idx);
}

b8 uhci_reset_and_enable_port(UHCI_Controller *controller, const u8 port_idx) {
    if(port_idx > 1) return false;

    {
        // Assert the `RESET` status for 50 milliseconds for the port to read it.
        write_port_status(controller, port_idx, UHCI_PORT_STATUS_Reset);
        os_ctrl_sleep(NANOSECONDS_FROM_MILLISECONDS(50));
    }

    {
        // Deassert the `RESET` status
        const UHCI_Port_Status desired_status = read_port_status(controller, port_idx) & ~(UHCI_PORT_STATUS_Reset | UHCI_PORT_STATUS_STATUS_FLAGS);
        write_port_status(controller, port_idx, desired_status);
        os_ctrl_sleep(NANOSECONDS_FROM_MILLISECONDS(10));
    }

    {
        // Enable the port and reset all status bits
        const UHCI_Port_Status desired_status = read_port_status(controller, port_idx) | UHCI_PORT_STATUS_Connection_Changed | UHCI_PORT_STATUS_Enabled_Changed;
        write_port_status(controller, port_idx, desired_status);
        write_port_status(controller, port_idx, desired_status | UHCI_PORT_STATUS_Currently_Enabled);
    }

    {
        // Wait for a valid startup for this port
        for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
            const UHCI_Port_Status current_status = read_port_status(controller, port_idx);
            if(!!(current_status & UHCI_PORT_STATUS_Currently_Connected) && !!(current_status & UHCI_PORT_STATUS_Currently_Enabled) && !(current_status & UHCI_PORT_STATUS_Suspended)) {
                return true;
            }
            os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
        }
        return false;
    }
}

s32 uhci_control_read(UHCI_Controller *controller, const u8 device_idx, void *header_data, u32 header_size_in_bytes, void *payload, u32 payload_size_in_bytes) {
    return control(controller, device_idx, UHCI_PACKET_In, header_data, header_size_in_bytes, payload, payload_size_in_bytes);
}

s32 uhci_control_write(UHCI_Controller *controller, const u8 device_idx, const void *header_data, u32 header_size_in_bytes, const void *payload, u32 payload_size_in_bytes) {
    return control(controller, device_idx, UHCI_PACKET_Out, header_data, header_size_in_bytes, payload, payload_size_in_bytes);
}

s32 uhci_bulk_write(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const void *data, u32 size_in_bytes) {
    if(size_in_bytes == 0) return 0;
    add_transfer_descriptors(controller, device_idx, endpoint_idx, UHCI_PACKET_Out, size_in_bytes, data);
    return submit(controller, device_idx, endpoint_idx, INVALID_TRANSFER_DESCRIPTOR_INDEX);
}

s32 uhci_bulk_read(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, void *data, const u32 size_in_bytes) {
    if(size_in_bytes == 0) return 0;
    add_transfer_descriptors(controller, device_idx, endpoint_idx, UHCI_PACKET_In, size_in_bytes, data);
    return submit(controller, device_idx, endpoint_idx, INVALID_TRANSFER_DESCRIPTOR_INDEX);
}

