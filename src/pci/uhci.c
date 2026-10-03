#include "uhci.h"
#include "base.h"
#include "ctrl.h"
#include "port/port.h"

#define WAIT_TIME_NANOSECONDS 1000000 // Wait in 1 millisecond intervals
#define WAIT_MAX_RETRIES 100 // How many iterations in a wait-loop
#define TRANSFER_DESCRIPTOR_PAYLOAD_SIZE 32
#define REQUIRED_TRANSFER_DESCRIPTORS(size_in_bytes) ((size_in_bytes / (size_in_bytes) + 1) / TRANSFER_DESCRIPTOR_PAYLOAD_SIZE)

// ---------------------------------------------------------------------------------------------------------------
// UHCI Types
// ---------------------------------------------------------------------------------------------------------------

typedef enum UHCI_Command {
    UHCI_COMMAND_Halt       = 0x0,
    UHCI_COMMAND_Start      = 1 << 0,
    UHCI_COMMAND_Host_Reset = 1 << 1,
} UHCI_Command;

typedef enum UHCI_Packet_Type {
    UHCI_PACKET_Setup = 0x2d,
    UHCI_PACKET_Out   = 0xe1,
    UHCI_PACKET_In    = 0xe9,
} UHCI_Packet_Type;

typedef enum UHCI_Memory_Structure_Type {
    UHCI_MEMORY_STRUCTURE_Transfer_Descriptor = 0x0,
    UHCI_MEMORY_STRUCTURE_Queue_Head          = 0x1,
} UHCI_Memory_Structure_Type;

typedef struct UHCI_Transfer_Descriptor_Link {
    u32 terminate : 1;
    UHCI_Memory_Structure_Type memory_structure_type : 1;
    u32 depth_first : 1;
    u32 reserved : 1;
    u32 pointer : 28;
} UHCI_Transfer_Descriptor_Link;

typedef struct UHCI_Transfer_Descriptor_Status {
    u32 length : 11;
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
    u32 error_counter : 2;
    u32 short_packet_detect : 1;
    u32 reserved1 : 2;
} UHCI_Transfer_Descriptor_Status;

typedef struct UHCI_Transfer_Descriptor_Packet_Header {
    UHCI_Packet_Type packet_type : 8;
    u32 device : 7;
    u32 endpoint : 4;
    u32 data_toggle : 1;
    u32 reserved : 1;
    u32 maximum_length : 11;
} UHCI_Transfer_Descriptor_Packet_Header;

typedef struct UHCI_Transfer_Descriptor {
    UHCI_Transfer_Descriptor_Link link;
    UHCI_Transfer_Descriptor_Status status;
    UHCI_Transfer_Descriptor_Packet_Header packet_header;
    u32 buffer_address;
    u8  reserved[16];
} PACKED_STRUCT UHCI_Transfer_Descriptor;

typedef struct UHCI_Frame_List_Entry {
    u32 terminate : 1;
    UHCI_Memory_Structure_Type memory_structure_type : 1;
    u32 reserved : 2;
    u32 pointer : 28;
} UHCI_Frame_List_Entry;

typedef struct UHCI_Queue_Head {
    UHCI_Frame_List_Entry vertical_pointer;
    UHCI_Frame_List_Entry horizontal_pointer;
} PACKED_STRUCT UHCI_Queue_Head;

// ---------------------------------------------------------------------------------------------------------------
// UHCI Interaction
// Port Mapping: https://wiki.osdev.org/Universal_Host_Controller_Interface
// ---------------------------------------------------------------------------------------------------------------

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
b8 check_port_connectivity(const UHCI_Controller *controller, const u32 port_idx) {
    return port_read_u16(controller->ports[port_idx]) & 0x1;
}

static inline
b8 transfer_descriptor_has_error(const UHCI_Transfer_Descriptor *descriptor) {
    return descriptor->status.bit_error || descriptor->status.timeout_crc || descriptor->status.babble_detected || descriptor->status.data_buffer_error || descriptor->status.stalled;
}

static
void clear_frame_list(UHCI_Controller *controller) {
    for(u32 i = 0; i < ARRAY_COUNT(controller->frame_list); ++i) {
        controller->frame_list[i] = 1;
    }
}

static
b8 submit_queue(UHCI_Controller *controller, const volatile UHCI_Transfer_Descriptor *descriptor_table) {
    const UHCI_Queue_Head queue_head = (UHCI_Queue_Head) { (UHCI_Frame_List_Entry) { .terminate = 1 }, (UHCI_Frame_List_Entry) { .terminate = 0, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor, .pointer = PHYSICAL_ADDRESS(descriptor_table) } };

    // Make this queue live on the controller so that the descriptors should be executed
    for(u32 i = 0; i < ARRAY_COUNT(controller->frame_list); ++i) {
        controller->frame_list[i] = PHYSICAL_ADDRESS(&queue_head);
    }

    // Wait until the controller has executed all descriptors in our queue
    b8 successful = true;
    while(successful && queue_head.vertical_pointer.terminate == false) {
        const UHCI_Transfer_Descriptor *current_descriptor = VIRTUAL_ADDRESS(queue_head.vertical_pointer.pointer);
        if(current_descriptor->status.active == false && transfer_descriptor_has_error(current_descriptor)) {
            successful = false;
            break;
        }

        os_ctrl_sleep(WAIT_TIME_NANOSECONDS);
    }

    clear_frame_list(controller);
    return successful;
}

// ---------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------

b8 uhci_initialize_controller(UHCI_Controller *controller, const u32 pci_address) {
    controller->pci_address = pci_address;
    controller->current_data_toggle = 0;

    for(u32 i = 0; i < UHCI_PORT_CAPACITY; ++i) {
        controller->ports[i] = controller->pci_address + 0x10 + (i * 2);
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

    return check_port_connectivity(controller, 0);
}

b8 uhci_bulk_write(UHCI_Controller *controller, const u8 device, const u8 endpoint, const void *data, u32 size_in_bytes) {
    const u32 descriptor_list_capacity = REQUIRED_TRANSFER_DESCRIPTORS(512);
    UHCI_Transfer_Descriptor descriptor_list[descriptor_list_capacity];

    const u32 required_descriptor_count = size_in_bytes / (TRANSFER_DESCRIPTOR_PAYLOAD_SIZE + 1) + 1;
    assert(required_descriptor_count < ARRAY_COUNT(descriptor_list), "The maximum capacity of a UHCI bulk write was reached.");

    for(u32 descriptor_idx = 0; descriptor_idx < required_descriptor_count; ++descriptor_idx) {
        const u32 offset_in_bytes = descriptor_idx * TRANSFER_DESCRIPTOR_PAYLOAD_SIZE;
        const b8 is_last_descriptor = (descriptor_idx + 1 == required_descriptor_count);
        const u32 next_descriptor_address = PHYSICAL_ADDRESS(&descriptor_list[descriptor_idx + 1]);
        const UHCI_Transfer_Descriptor_Link link = (UHCI_Transfer_Descriptor_Link) { .terminate = is_last_descriptor, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor, .pointer = next_descriptor_address };
        const UHCI_Transfer_Descriptor_Status status = (UHCI_Transfer_Descriptor_Status) { .length = min(size_in_bytes - offset_in_bytes, TRANSFER_DESCRIPTOR_PAYLOAD_SIZE), .active = 1 };
        const UHCI_Transfer_Descriptor_Packet_Header packet_header = (UHCI_Transfer_Descriptor_Packet_Header) { .packet_type = UHCI_PACKET_Out, .device = device, .endpoint = endpoint, .data_toggle = controller->current_data_toggle, .maximum_length = TRANSFER_DESCRIPTOR_PAYLOAD_SIZE };
        descriptor_list[descriptor_idx] = (UHCI_Transfer_Descriptor) { .link = link, .status = status, .packet_header = packet_header, .buffer_address = PHYSICAL_ADDRESS(data + offset_in_bytes) };
        controller->current_data_toggle = !controller->current_data_toggle;
    }

    return submit_queue(controller, descriptor_list);
}

b8 uhci_control(UHCI_Controller *controller, void *header_data, const u32 header_size_in_bytes, void *payload, const u32 payload_size_in_bytes) {
    const u32 descriptor_list_capacity = REQUIRED_TRANSFER_DESCRIPTORS(32);
    UHCI_Transfer_Descriptor descriptor_list[descriptor_list_capacity];
    u32 descriptor_idx = 0;

    // SETUP Transfer Descriptor
    {
        const UHCI_Transfer_Descriptor_Link link = (UHCI_Transfer_Descriptor_Link) { .terminate = false, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor, .pointer = PHYSICAL_ADDRESS(&descriptor_list[1]) };
        const UHCI_Transfer_Descriptor_Status status = (UHCI_Transfer_Descriptor_Status) { .length = header_size_in_bytes, .active = 1 };
        const UHCI_Transfer_Descriptor_Packet_Header packet_header = (UHCI_Transfer_Descriptor_Packet_Header) { .packet_type = UHCI_PACKET_Setup, .device = 0, .endpoint = 0, .data_toggle = controller->current_data_toggle, .maximum_length = header_size_in_bytes };
        descriptor_list[descriptor_idx++] = (UHCI_Transfer_Descriptor) { .link = link, .status = status, .packet_header = packet_header, .buffer_address = PHYSICAL_ADDRESS(header_data) };
        controller->current_data_toggle = !controller->current_data_toggle;
    }

    // IN Transfer Descriptor
    const u32 required_payload_descriptor_count = REQUIRED_TRANSFER_DESCRIPTORS(payload_size_in_bytes);
    for(u32 payload_descriptor_idx = 0; payload_descriptor_idx < required_payload_descriptor_count; ++payload_descriptor_idx) {
        const u32 offset_in_bytes = descriptor_idx * TRANSFER_DESCRIPTOR_PAYLOAD_SIZE;
        const u32 next_descriptor_address = PHYSICAL_ADDRESS(&descriptor_list[descriptor_idx + 1]);
        const UHCI_Transfer_Descriptor_Link link = (UHCI_Transfer_Descriptor_Link) { .terminate = false, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor, .pointer = next_descriptor_address };
        const UHCI_Transfer_Descriptor_Status status = (UHCI_Transfer_Descriptor_Status) { .length = min(payload_size_in_bytes - offset_in_bytes, TRANSFER_DESCRIPTOR_PAYLOAD_SIZE), .active = 1 };
        const UHCI_Transfer_Descriptor_Packet_Header packet_header = (UHCI_Transfer_Descriptor_Packet_Header) { .packet_type = UHCI_PACKET_In, .device = 0, .endpoint = 0, .data_toggle = controller->current_data_toggle, .maximum_length = TRANSFER_DESCRIPTOR_PAYLOAD_SIZE };
        descriptor_list[descriptor_idx++] = (UHCI_Transfer_Descriptor) { .link = link, .status = status, .packet_header = packet_header, .buffer_address = PHYSICAL_ADDRESS(payload + offset_in_bytes) };
        controller->current_data_toggle = !controller->current_data_toggle;
    }

    // OUT Transfer Descriptor
    {
        const UHCI_Transfer_Descriptor_Link link = (UHCI_Transfer_Descriptor_Link) { .terminate = true, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor };
        const UHCI_Transfer_Descriptor_Status status = (UHCI_Transfer_Descriptor_Status) { .length = 0, .active = 1 };
        const UHCI_Transfer_Descriptor_Packet_Header packet_header = (UHCI_Transfer_Descriptor_Packet_Header) { .packet_type = UHCI_PACKET_Out, .device = 0, .endpoint = 0, .data_toggle = controller->current_data_toggle, .maximum_length = 0 };
        descriptor_list[descriptor_idx++] = (UHCI_Transfer_Descriptor) { .link = link, .status = status, .packet_header = packet_header };
        controller->current_data_toggle = !controller->current_data_toggle;
    }

    return submit_queue(controller, descriptor_list);
}

b8 uhci_bulk_read(UHCI_Controller *controller, const u8 device, const u8 endpoint, void *data, const u32 size_in_bytes) {
    const u32 descriptor_list_capacity = REQUIRED_TRANSFER_DESCRIPTORS(512);
    UHCI_Transfer_Descriptor descriptor_list[descriptor_list_capacity];

    const u32 required_descriptor_count = size_in_bytes / (TRANSFER_DESCRIPTOR_PAYLOAD_SIZE + 1) + 1;
    assert(required_descriptor_count < ARRAY_COUNT(descriptor_list), "The maximum capacity of a UHCI bulk write was reached.");

    for(u32 descriptor_idx = 0; descriptor_idx < required_descriptor_count; ++descriptor_idx) {
        const u32 offset_in_bytes = descriptor_idx * TRANSFER_DESCRIPTOR_PAYLOAD_SIZE;
        const b8 is_last_descriptor = (descriptor_idx + 1 == required_descriptor_count);
        const u32 next_descriptor_address = PHYSICAL_ADDRESS(&descriptor_list[descriptor_idx + 1]);
        const UHCI_Transfer_Descriptor_Link link = (UHCI_Transfer_Descriptor_Link) { .terminate = is_last_descriptor, .memory_structure_type = UHCI_MEMORY_STRUCTURE_Transfer_Descriptor, .pointer = next_descriptor_address };
        const UHCI_Transfer_Descriptor_Status status = (UHCI_Transfer_Descriptor_Status) { .length = min(size_in_bytes - offset_in_bytes, TRANSFER_DESCRIPTOR_PAYLOAD_SIZE), .active = 1 };
        const UHCI_Transfer_Descriptor_Packet_Header packet_header = (UHCI_Transfer_Descriptor_Packet_Header) { .packet_type = UHCI_PACKET_In, .device = device, .endpoint = endpoint, .data_toggle = controller->current_data_toggle, .maximum_length = TRANSFER_DESCRIPTOR_PAYLOAD_SIZE };
        descriptor_list[descriptor_idx] = (UHCI_Transfer_Descriptor) { .link = link, .status = status, .packet_header = packet_header, .buffer_address = PHYSICAL_ADDRESS(data + offset_in_bytes) };
        controller->current_data_toggle = !controller->current_data_toggle;
    }

    return submit_queue(controller, descriptor_list);
}

