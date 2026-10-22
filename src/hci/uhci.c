#include "uhci.h"
#include "base.h"
#include "ctrl.h"
#include "port/port.h"

#define UHCI_MAX_CONTROLLERS 1

#define WAIT_TIME_NANOSECONDS 1000000 // Wait in 1 millisecond intervals
#define WAIT_MAX_RETRIES 100 // How many iterations in a wait-loop

#define INVALID_TRANSFER_DESCRIPTOR_INDEX UHCI_ERROR
#define TRANSFER_DESCRIPTOR_ENCODED_LENGTH(size_in_bytes)     ((size_in_bytes) > 0 ? (size_in_bytes) - 1 : 0x7ff)
#define TRANSFER_DESCRIPTOR_DECODED_LENGTH(length)            ((length) != 0x7ff ? ((length) + 1) : 0)

// To save bit space, addresses in UHCI are assumed to be 16-byte aligned, and the lower four bits are therefore ommitted from
// the bit representation
#define LINK_ADDRESS_FROM_POINTER(pointer) (PHYSICAL_ADDRESS(pointer) >> 4)
#define LINK_POINTER_FROM_ADDRESS(address) (VIRTUAL_ADDRESS(address << 4))

#define UHCI_BULK_TRANSFER_DESCRIPTOR_CAPACITY 32

#define UHCI_ERROR -1
#define UHCI_FRAME_LIST_CAPACITY 1024
#define UHCI_DEVICE_CAPACITY 128
#define UHCI_PORT_CAPACITY 2
#define UHCI_ENDPOINT_CAPACITY 16

// ---------------------------------------------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------------------------------------------

typedef enum UHCI_Command {
    UHCI_COMMAND_Halt        = 0x0,
    UHCI_COMMAND_Start       = 1 << 0,
    UHCI_COMMAND_Host_Reset  = 1 << 1,
    UHCI_COMMAND_Configure   = 1 << 6,
    UHCI_COMMAND_MaxPacket64 = 1 << 7,
} UHCI_Command;

typedef enum UHCI_Packet_Type {
    UHCI_PACKET_Setup = 0x2d,
    UHCI_PACKET_In    = 0x69,
    UHCI_PACKET_Out   = 0xe1,
} UHCI_Packet_Type;

typedef enum UHCI_Memory_Structure_Type {
    UHCI_MEMORY_STRUCTURE_Transfer_Descriptor = 0x0,
    UHCI_MEMORY_STRUCTURE_Queue_Head          = 0x1,
} UHCI_Memory_Structure_Type;

typedef enum UHCI_Port_Status {
    UHCI_PORT_STATUS_Currently_Connected = 1 << 0,
    UHCI_PORT_STATUS_Connection_Changed  = 1 << 1,
    UHCI_PORT_STATUS_Currently_Enabled   = 1 << 2,
    UHCI_PORT_STATUS_Enabled_Changed     = 1 << 3,
    UHCI_PORT_STATUS_Line                = 3 << 4,
    UHCI_PORT_STATUS_Resume_Detected     = 1 << 6,
    UHCI_PORT_STATUS_Low_Speed           = 1 << 8,
    UHCI_PORT_STATUS_Reset               = 1 << 9,
    UHCI_PORT_STATUS_Suspended           = 1 << 12,

    UHCI_PORT_STATUS_STATUS_FLAGS = (UHCI_PORT_STATUS_Connection_Changed | UHCI_PORT_STATUS_Enabled_Changed),
} UHCI_Port_Status;

typedef struct UHCI_Transfer_Descriptor_Link {
    u32 terminate : 1;
    UHCI_Memory_Structure_Type memory_structure_type : 1;
    u32 depth_first : 1;
    u32 reserved : 1;
    u32 address : 28;
} UHCI_Transfer_Descriptor_Link;

typedef struct UHCI_Transfer_Descriptor_Status {
    u32 transferred_length : 11;
    u32 reserved0 : 6;
    u32 bit_error : 1;
    u32 timeout_crc : 1;
    u32 non_acknowledged : 1;
    u32 babble_detected : 1;
    u32 data_buffer_error : 1;
    u32 stalled : 1;
    u32 active : 1;
    u32 interrupt_on_complete : 1;
    u32 is_isochronous : 1;
    u32 low_speed : 1;
    u32 error_retry_counter : 2;
    u32 short_packet_detect : 1;
    u32 reserved1 : 2;
} UHCI_Transfer_Descriptor_Status;

typedef struct UHCI_Transfer_Descriptor_Packet_Header {
    UHCI_Packet_Type packet_type : 8;
    u32 device : 7;
    u32 endpoint : 4;
    u32 data_toggle : 1;
    u32 reserved : 1;
    u32 requested_length : 11;
} UHCI_Transfer_Descriptor_Packet_Header;

typedef struct UHCI_Transfer_Descriptor {
    UHCI_Transfer_Descriptor_Link link;
    UHCI_Transfer_Descriptor_Status status;
    UHCI_Transfer_Descriptor_Packet_Header packet_header;
    u32 buffer_address;
    u8  reserved[16];
} ALIGN_DECLARATION(16) UHCI_Transfer_Descriptor;

typedef struct UHCI_Frame_List_Entry {
    u32 terminate : 1;
    UHCI_Memory_Structure_Type memory_structure_type : 1;
    u32 reserved : 2;
    u32 address : 28;
} UHCI_Frame_List_Entry;

typedef struct UHCI_Queue_Head {
    volatile UHCI_Frame_List_Entry horizontal_pointer;
    volatile UHCI_Frame_List_Entry vertical_pointer;
} ALIGN_DECLARATION(16) UHCI_Queue_Head;

typedef struct UHCI_Endpoint {
    u8 next_data_toggle[HCI_DIRECTION_COUNT]; // Track this separately for each direction
    u16 maximum_packet_size[HCI_DIRECTION_COUNT];
} UHCI_Endpoint;

typedef struct UHCI_Device {
    UHCI_Endpoint endpoints[UHCI_ENDPOINT_CAPACITY];
    HCI_Device_Speed speed;
} UHCI_Device;

typedef struct UHCI_Port {
    u32 register_address;
    b8 connected;
} UHCI_Port;

typedef struct UHCI_Controller {
    UHCI_Frame_List_Entry frame_list[UHCI_FRAME_LIST_CAPACITY] ALIGN_DECLARATION(4096);
    u32 pci_address;
    UHCI_Port ports[UHCI_PORT_CAPACITY];
    UHCI_Device devices[UHCI_DEVICE_CAPACITY];

    UHCI_Queue_Head bulk_queue_head;
    UHCI_Transfer_Descriptor bulk_transfer_descriptors[UHCI_BULK_TRANSFER_DESCRIPTOR_CAPACITY] ALIGN_DECLARATION(16);
    u32 active_bulk_transfer_descriptors;

    UHCI_Queue_Head interrupt_queue_head;
    UHCI_Transfer_Descriptor interrupt_transfer_descriptor ALIGN_DECLARATION(16);
    u8 keyboard_report[8] ALIGN_DECLARATION(16);
} UHCI_Controller;

static UHCI_Controller uhci_controllers[UHCI_MAX_CONTROLLERS];
static u32 uhci_controller_count = 0;

// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

static inline
b8 validate_address(const u8 device_idx, const u8 endpoint_idx) {
    return device_idx < UHCI_DEVICE_CAPACITY && endpoint_idx < UHCI_ENDPOINT_CAPACITY;
}

static
b8 validate_device_speed(const HCI_Device_Speed speed) {
    switch(speed) {
        case HCI_DEVICE_SPEED_Low:   return true;
        case HCI_DEVICE_SPEED_Full:  return true;
        case HCI_DEVICE_SPEED_High:  return false;
        case HCI_DEVICE_SPEED_Super: return false;
        case HCI_DEVICE_SPEED_Super_Plus: return false;
        case HCI_DEVICE_SPEED_Error: return false;
    }
    return false;
}

static
b8 validate_direction(const HCI_Direction direction) {
    switch(direction) {
        case HCI_DIRECTION_In:    return true;
        case HCI_DIRECTION_Out:   return true;
        case HCI_DIRECTION_COUNT: return false;
    }
    return false;
}

static inline
b8 transfer_descriptor_has_error(const UHCI_Transfer_Descriptor *descriptor) {
    return !descriptor->status.active && (descriptor->status.bit_error || descriptor->status.timeout_crc || descriptor->status.babble_detected || descriptor->status.data_buffer_error || descriptor->status.stalled);
}

static inline
b8 transfer_descriptor_is_short_packet(const UHCI_Transfer_Descriptor *descriptor) {
    return !descriptor->status.active && descriptor->packet_header.packet_type == UHCI_PACKET_In && descriptor->status.short_packet_detect && (TRANSFER_DESCRIPTOR_DECODED_LENGTH(descriptor->status.transferred_length) < TRANSFER_DESCRIPTOR_DECODED_LENGTH(descriptor->packet_header.requested_length));
}

typedef b8 (*Status_Check)(const UHCI_Controller *);

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

// ---------------------------------------------------------------------------------------------------------------
// UHCI Interaction
// Port Mapping: https://wiki.osdev.org/Universal_Host_Controller_Interface
// ---------------------------------------------------------------------------------------------------------------

static inline
b8 is_halted(const UHCI_Controller *controller) {
    return (port_read_u16(controller->pci_address + 0x02) & (1 << 5)) != 0;
}

static inline
b8 is_host_reset(const UHCI_Controller *controller) {
    return (port_read_u16(controller->pci_address + 0x00) & (1 << 1)) == 0;
}

static inline
void write_frame_list_pointer(const UHCI_Controller *controller, const UHCI_Frame_List_Entry *frame_list) {
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

static
b8 is_port_enabled(const UHCI_Controller *controller, const u8 port_idx) {
    if(port_idx >= UHCI_PORT_CAPACITY) return false;
    const UHCI_Port_Status current_status = read_port_status(controller, port_idx);
    return !!(current_status & UHCI_PORT_STATUS_Currently_Connected) && !!(current_status & UHCI_PORT_STATUS_Currently_Enabled) && !(current_status & UHCI_PORT_STATUS_Suspended);
}


// ---------------------------------------------------------------------------------------------------------------
// Endpoint Handling
// ---------------------------------------------------------------------------------------------------------------

static
HCI_Direction direction_for_packet_type(const UHCI_Packet_Type packet_type) {
    switch(packet_type) {
        case UHCI_PACKET_In: return HCI_DIRECTION_In;
        case UHCI_PACKET_Out: return HCI_DIRECTION_Out;
        default: return HCI_DIRECTION_COUNT;
    }
}

static
b8 is_valid_direction(const HCI_Direction direction) {
    return direction >= 0 && direction < HCI_DIRECTION_COUNT;
}

static
void set_data_toggle(UHCI_Endpoint *endpoint, const HCI_Direction direction, const u8 desired_toggle) {
    if(!is_valid_direction(direction)) return;
    endpoint->next_data_toggle[direction] = desired_toggle;
}

static
void set_data_toggles(UHCI_Endpoint *endpoint, const u8 desired_toggle) {
    for(u32 i = 0; i < ARRAY_COUNT(endpoint->next_data_toggle); ++i) {
        endpoint->next_data_toggle[i] = desired_toggle;
    }
}

static
u8 get_and_advance_data_toggle(UHCI_Endpoint *endpoint, const HCI_Direction direction) {
    if(!is_valid_direction(direction)) return 0;
    const u8 data_toggle = endpoint->next_data_toggle[direction];
    endpoint->next_data_toggle[direction] = !data_toggle;
    return data_toggle;
}

static
u16 get_maximum_packet_size(const UHCI_Endpoint *endpoint, const HCI_Direction direction) {
    if(!is_valid_direction(direction)) return 0;
    return endpoint->maximum_packet_size[direction];
}

static
void reset_endpoint(UHCI_Endpoint *endpoint, const u8 desired_toggle) {
    // A max size of 8 is really only the default for the 0th endpoint as that one is readable for the device descriptors
    // are read for the other endpoints, but eh.
    for(u32 i = 0; i < ARRAY_COUNT(endpoint->maximum_packet_size); ++i) {
        endpoint->maximum_packet_size[i] = 8;
    }
    set_data_toggles(endpoint, desired_toggle);
}

static
UHCI_Endpoint *find_endpoint(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx) {
    return &controller->devices[device_idx].endpoints[endpoint_idx];
}

static
UHCI_Device *find_device(UHCI_Controller *controller, const u8 device_idx) {
    return &controller->devices[device_idx];
}

// ---------------------------------------------------------------------------------------------------------------
// Transfer Descriptor Handling
// ---------------------------------------------------------------------------------------------------------------

static
void add_single_transfer_descriptor(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const UHCI_Packet_Type packet_type, const u32 requested_length, const void *data) {
    assert(controller->active_bulk_transfer_descriptors < ARRAY_COUNT(controller->bulk_transfer_descriptors), "Reached the capacity of the UHCI bulk transfer descriptor list.");

    const UHCI_Device *device = find_device(controller, device_idx);
    UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);

    if(controller->active_bulk_transfer_descriptors > 0) {
        const UHCI_Transfer_Descriptor_Link link = (UHCI_Transfer_Descriptor_Link) { .terminate = false, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor, .address = LINK_ADDRESS_FROM_POINTER(&controller->bulk_transfer_descriptors[controller->active_bulk_transfer_descriptors]) };
        controller->bulk_transfer_descriptors[controller->active_bulk_transfer_descriptors - 1].link = link;
    }

    const u32 error_retry_counter = 3;
    const u32 short_packet_detect = (packet_type == UHCI_PACKET_In && requested_length > 0) ? 1 : 0;
    const HCI_Direction direction = direction_for_packet_type(packet_type);

    const UHCI_Transfer_Descriptor_Link link = (UHCI_Transfer_Descriptor_Link) { .terminate = true };
    const UHCI_Transfer_Descriptor_Status status = (UHCI_Transfer_Descriptor_Status) { .transferred_length = TRANSFER_DESCRIPTOR_ENCODED_LENGTH(0), .error_retry_counter = error_retry_counter, .active = 1, .short_packet_detect = short_packet_detect, .low_speed = device->speed == HCI_DEVICE_SPEED_Low };
    const UHCI_Transfer_Descriptor_Packet_Header packet_header = (UHCI_Transfer_Descriptor_Packet_Header) { .packet_type = packet_type, .device = device_idx, .endpoint = endpoint_idx, .data_toggle = get_and_advance_data_toggle(endpoint, direction), .requested_length = TRANSFER_DESCRIPTOR_ENCODED_LENGTH(requested_length) };
    controller->bulk_transfer_descriptors[controller->active_bulk_transfer_descriptors] = (UHCI_Transfer_Descriptor) { .link = link, .status = status, .packet_header = packet_header, .buffer_address = PHYSICAL_ADDRESS(data) };

    ++controller->active_bulk_transfer_descriptors;
}

static
void add_transfer_descriptors(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const UHCI_Packet_Type packet_type, const u32 size_in_bytes, const void *data) {
    const UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);
    const HCI_Direction direction = direction_for_packet_type(packet_type);
    const u16 maximum_packet_size = get_maximum_packet_size(endpoint, direction);
    const u32 descriptor_count = align(size_in_bytes, maximum_packet_size);
    for(u32 descriptor_idx = 0; descriptor_idx < descriptor_count; ++descriptor_idx) {
        const u32 offset_in_bytes = descriptor_idx * maximum_packet_size;
        const u32 packet_size_in_bytes = min(size_in_bytes - offset_in_bytes, maximum_packet_size);
        add_single_transfer_descriptor(controller, device_idx, endpoint_idx, packet_type, packet_size_in_bytes, (const char *) data + offset_in_bytes);
    }
}

static
void update_bulk_queue_head_pointer(UHCI_Controller *controller, const s32 transfer_descriptor_idx) {
    controller->bulk_queue_head.vertical_pointer = (UHCI_Frame_List_Entry) { .terminate = 0, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor, .address = LINK_ADDRESS_FROM_POINTER(&controller->bulk_transfer_descriptors[transfer_descriptor_idx]) };
    memory_barrier(); // Make sure the hardware sees this change before continuing with the execution
}

static
void clear_bulk_queue(UHCI_Controller *controller) {
    controller->bulk_queue_head = (UHCI_Queue_Head) { (UHCI_Frame_List_Entry) { .terminate = true }, (UHCI_Frame_List_Entry) { .terminate = true } };
    memory_barrier();
}

static
s32 submit(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const s32 status_descriptor_idx) {
    UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);

    //
    // Insert a queue head which points at the first bulk transfer
    //
    update_bulk_queue_head_pointer(controller, 0);

    //
    // Wait until the controller has executed all descriptors in our queue
    //
    b8 successful = true;
    while(successful && !controller->bulk_queue_head.vertical_pointer.terminate) {
        const UHCI_Transfer_Descriptor *current_descriptor = LINK_POINTER_FROM_ADDRESS(controller->bulk_queue_head.vertical_pointer.address);
        if(transfer_descriptor_has_error(current_descriptor)) {
            successful = false;
            set_data_toggle(endpoint, direction_for_packet_type(current_descriptor->packet_header.packet_type), current_descriptor->packet_header.data_toggle);
            break;
        } else if(transfer_descriptor_is_short_packet(current_descriptor)) {
            set_data_toggle(endpoint, direction_for_packet_type(current_descriptor->packet_header.packet_type), !current_descriptor->packet_header.data_toggle);
            if(status_descriptor_idx != INVALID_TRANSFER_DESCRIPTOR_INDEX) {
                // Detected a short packet, but the status transfer still needs to complete for a valid hardware interaction.
                // Update the queue so that it points at that transfer descriptor...
                update_bulk_queue_head_pointer(controller, status_descriptor_idx);
            } else {
                // Reached the end of this transmission, exit...
                break;
            }
        } else {
            os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
        }
    }

    //
    // Wait until the controller has completely read the current frame, so that we can safely make modifications
    // to the transfer descriptor list (as the controller will no longer access them from now on).
    //
    clear_bulk_queue(controller);

    const u16 current_frame_number = read_frame_number(controller);
    while(read_frame_number(controller) == current_frame_number) {
        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
    }

    s32 number_of_bytes_transferred = 0;
    for(u32 i = 0; i < controller->active_bulk_transfer_descriptors; ++i) {
        if(controller->bulk_transfer_descriptors[i].packet_header.packet_type != UHCI_PACKET_Setup) {
            number_of_bytes_transferred += TRANSFER_DESCRIPTOR_DECODED_LENGTH(controller->bulk_transfer_descriptors[i].status.transferred_length);
        }
    }

    controller->active_bulk_transfer_descriptors = 0;
    return successful ? number_of_bytes_transferred : UHCI_ERROR;
}

static
s32 control(UHCI_Controller *controller, const u8 device_idx, const UHCI_Packet_Type payload_direction, const void *header_data, const u32 header_size_in_bytes, const void *payload, const u32 payload_size_in_bytes) {
    assert(header_size_in_bytes == 8, "The header size for a control transaction was expected to be 8 bytes long.");

    const u8 endpoint_idx = 0;

    UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);
    set_data_toggles(endpoint, 1); // A setup transfer descriptor resets all data toggles for this endpoint

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

    return submit(controller, device_idx, endpoint_idx, controller->active_bulk_transfer_descriptors - 1);
}


// ---------------------------------------------------------------------------------------------------------------
// HCI Interface
// ---------------------------------------------------------------------------------------------------------------

static
b8 uhci_initialize_controller(UHCI_Controller *controller, const u32 pci_address) {
    controller->pci_address     = pci_address;

    for(u32 device_idx = 0; device_idx < UHCI_DEVICE_CAPACITY; ++device_idx) {
        for(u32 endpoint_idx = 0; endpoint_idx < UHCI_ENDPOINT_CAPACITY; ++endpoint_idx) {
            reset_endpoint(&controller->devices[device_idx].endpoints[endpoint_idx], 0);
        }
    }

    for(u32 i = 0; i < UHCI_PORT_CAPACITY; ++i) {
        controller->ports[i] = (UHCI_Port) { .register_address = controller->pci_address + 0x10 + (i * 2), .connected = false };
    }

    clear_bulk_queue(controller);

    for (u32 i = 0; i < ARRAY_COUNT(controller->frame_list); ++i) {
        controller->frame_list[i] = (UHCI_Frame_List_Entry) { .terminate = false, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Queue_Head, .address = LINK_ADDRESS_FROM_POINTER(&controller->bulk_queue_head) };
    }

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
    issue_command(controller, UHCI_COMMAND_Start | UHCI_COMMAND_Configure | UHCI_COMMAND_MaxPacket64);

    return true;
}

static
b8 uhci_initialize_keyboard_report(UHCI_Controller *controller, const u8 device, const u8 endpoint) {
    controller->interrupt_transfer_descriptor = (UHCI_Transfer_Descriptor) {
        .link = { .terminate = true },
        .status = { .active = true, .interrupt_on_complete = 1, .error_retry_counter = 3 },
        .packet_header = { UHCI_PACKET_In, .device = device, .endpoint = endpoint, .data_toggle = 0, .requested_length = sizeof(controller->keyboard_report) },
        .buffer_address = PHYSICAL_ADDRESS(&controller->keyboard_report),
    };

    // @Incomplete: Create the queue header and put that on the frame list.

    return true;
}

static
s32 uhci_get_port_capacity(const UHCI_Controller *controller) {
    (void) controller;
    return UHCI_PORT_CAPACITY;
}

static
HCI_Device_Speed uhci_get_port_speed(const UHCI_Controller *controller, const u8 port_idx) {
    if(port_idx >= UHCI_PORT_CAPACITY) return HCI_DEVICE_SPEED_Error;
    const UHCI_Port_Status current_status = read_port_status(controller, port_idx);
    const HCI_Device_Speed speed = (current_status & UHCI_PORT_STATUS_Low_Speed) ? HCI_DEVICE_SPEED_Low : HCI_DEVICE_SPEED_Full;
    return speed;
}

static
b8 uhci_reset_and_enable_port(UHCI_Controller *controller, const u8 port_idx) {
    if(port_idx >= UHCI_PORT_CAPACITY) return false;

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
            if(is_port_enabled(controller, port_idx)) {
                return true;
            }
            os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
        }
        return false;
    }
}

static
b8 uhci_configure_device(UHCI_Controller *controller, const u8 device_idx, const HCI_Device_Speed speed) {
    if(!validate_address(device_idx, 0) || !validate_device_speed(speed)) return false;
    controller->devices[device_idx].speed = speed;
    set_data_toggles(&controller->devices[device_idx].endpoints[0], 0);
    return true;
}

static
b8 uhci_configure_endpoint_direction(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const HCI_Direction direction, const u16 maximum_packet_size) {
    if(!validate_address(device_idx, endpoint_idx) || !validate_direction(direction) || maximum_packet_size == 0) return false;
    UHCI_Endpoint *endpoint = find_endpoint(controller, device_idx, endpoint_idx);
    endpoint->maximum_packet_size[direction] = maximum_packet_size;
    set_data_toggle(endpoint, direction, 0);
    return true;
}

static
s32 uhci_control_read(UHCI_Controller *controller, const u8 device_idx, void *header_data, u32 header_size_in_bytes, void *payload, u32 payload_size_in_bytes) {
    if(header_size_in_bytes != 8 || header_data == null) return UHCI_ERROR;
    if(payload_size_in_bytes > 0 && payload == null) return UHCI_ERROR;
    if(!validate_address(device_idx, 0)) return UHCI_ERROR;
    return control(controller, device_idx, UHCI_PACKET_In, header_data, header_size_in_bytes, payload, payload_size_in_bytes);
}

static
s32 uhci_control_write(UHCI_Controller *controller, const u8 device_idx, const void *header_data, u32 header_size_in_bytes, const void *payload, u32 payload_size_in_bytes) {
    if(header_size_in_bytes != 8 || header_data == null) return UHCI_ERROR;
    if(payload_size_in_bytes > 0 && payload == null) return UHCI_ERROR;
    if(!validate_address(device_idx, 0)) return UHCI_ERROR;
    return control(controller, device_idx, UHCI_PACKET_Out, header_data, header_size_in_bytes, payload, payload_size_in_bytes);
}

static
s32 uhci_bulk_write(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const void *data, u32 size_in_bytes) {
    if(size_in_bytes == 0 || data == null) return 0;
    if(!validate_address(device_idx, endpoint_idx)) return UHCI_ERROR;
    add_transfer_descriptors(controller, device_idx, endpoint_idx, UHCI_PACKET_Out, size_in_bytes, data);
    return submit(controller, device_idx, endpoint_idx, INVALID_TRANSFER_DESCRIPTOR_INDEX);
}

static
s32 uhci_bulk_read(UHCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, void *data, const u32 size_in_bytes) {
    if(size_in_bytes == 0 || data == null) return 0;
    if(!validate_address(device_idx, endpoint_idx)) return UHCI_ERROR;
    add_transfer_descriptors(controller, device_idx, endpoint_idx, UHCI_PACKET_In, size_in_bytes, data);
    return submit(controller, device_idx, endpoint_idx, INVALID_TRANSFER_DESCRIPTOR_INDEX);
}


// ---------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------

UHCI_Controller *uhci_allocate_controller(void) {
    if(uhci_controller_count >= UHCI_MAX_CONTROLLERS) {
        return null;
    }

    UHCI_Controller *controller = &uhci_controllers[uhci_controller_count++];
    ZERO_STRUCT(controller);
    return controller;
}

HCI_Controller hci_controller_from_uhci(UHCI_Controller *uhci) {
    return (HCI_Controller) {
        uhci,
        (HCI_Initialize_Controller) uhci_initialize_controller,
        (HCI_Initialize_Keyboard_Report) uhci_initialize_keyboard_report,
        (HCI_Get_Port_Capacity) uhci_get_port_capacity,
        (HCI_Get_Port_Speed) uhci_get_port_speed,
        (HCI_Reset_And_Enable_Port) uhci_reset_and_enable_port,
        (HCI_Configure_Device) uhci_configure_device,
        (HCI_Configure_Endpoint_Direction) uhci_configure_endpoint_direction,
        (HCI_Control_Read) uhci_control_read,
        (HCI_Control_Write) uhci_control_write,
        (HCI_Bulk_Read) uhci_bulk_read,
        (HCI_Bulk_Write) uhci_bulk_write,
    };
}

