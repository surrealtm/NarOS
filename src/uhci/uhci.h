#pragma once

#include "base.h"

#define UHCI_ERROR -1
#define UHCI_FRAME_LIST_CAPACITY 1024
#define UHCI_TRANSFER_DESCRIPTOR_CAPACITY 32
#define UHCI_DEVICE_CAPACITY 128
#define UHCI_PORT_CAPACITY 2
#define UHCI_ENDPOINT_CAPACITY 16

/* --------------------------------------------------------------------------------------------------------------- */
/* Helper Types                                                                                                    */
/* --------------------------------------------------------------------------------------------------------------- */

typedef enum UHCI_Device_Speed {
    UHCI_DEVICE_SPEED_Full  = 0,
    UHCI_DEVICE_SPEED_Low   = 1,
    UHCI_DEVICE_SPEED_Error = 2,
} UHCI_Device_Speed;

typedef enum UHCI_Direction {
    UHCI_DIRECTION_In = 0,
    UHCI_DIRECTION_Out = 1,
    UHCI_DIRECTION_COUNT = 2,
} UHCI_Direction;



/* --------------------------------------------------------------------------------------------------------------- */
/* UHCI Protocol                                                                                                   */
/* --------------------------------------------------------------------------------------------------------------- */

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
    u32 pointer : 28;
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
    u32 pointer : 28;
} UHCI_Frame_List_Entry;

typedef struct UHCI_Queue_Head {
    volatile UHCI_Frame_List_Entry horizontal_pointer;
    volatile UHCI_Frame_List_Entry vertical_pointer;
} ALIGN_DECLARATION(16) UHCI_Queue_Head;


/* --------------------------------------------------------------------------------------------------------------- */
/* UHCI API                                                                                                        */
/* --------------------------------------------------------------------------------------------------------------- */

typedef struct UHCI_Endpoint {
    u8 next_data_toggle[UHCI_DIRECTION_COUNT]; // Track this separately for each direction
    u16 maximum_packet_size[UHCI_DIRECTION_COUNT];
} UHCI_Endpoint;

typedef struct UHCI_Device {
    UHCI_Endpoint endpoints[UHCI_ENDPOINT_CAPACITY];
    UHCI_Device_Speed speed;
} UHCI_Device;

typedef struct UHCI_Port {
    u32 register_address;
    b8 connected;
} UHCI_Port;

typedef struct UHCI_Controller {
    u32 frame_list[UHCI_FRAME_LIST_CAPACITY] ALIGN_DECLARATION(4096);
    u32 pci_address;
    u32 active_transfer_descriptors;
    UHCI_Transfer_Descriptor transfer_descriptors[UHCI_TRANSFER_DESCRIPTOR_CAPACITY];
    UHCI_Queue_Head queue_head;
    UHCI_Port ports[UHCI_PORT_CAPACITY];
    UHCI_Device devices[UHCI_DEVICE_CAPACITY];
} UHCI_Controller;

/**
 * Initializes the controller structure and configures the hardware controller at the given pci address.
 * Returns true if the hardware controller was successfully reset and enabled
 */
b8 uhci_initialize_controller(UHCI_Controller *controller, u32 pci_address);

/**
 * Returns the device speed for the given port. This must be used when enumerating devices through the port.
 */
UHCI_Device_Speed uhci_get_port_speed(const UHCI_Controller *controller, u8 port_idx);

/**
 * Returns whether the port at that index is connected.
 */
b8 uhci_is_port_connected(const UHCI_Controller *controller, u8 port_idx);

/**
 * Returns whether the port at that index is connected and enabled.
 */
b8 uhci_is_port_enabled(const UHCI_Controller *controller, u8 port_idx);

/**
 * Issues a reset signal on the port and then attempts to enable it.
 * Returns true if the port is now successfully enabled and connected.
 */
b8 uhci_reset_and_enable_port(UHCI_Controller *controller, u8 port_idx);

/**
 * Sets the devices transmission speed and resets the data toggles for the first endpoint.
 */
b8 uhci_configure_device(UHCI_Controller *controller, u8 device_idx, UHCI_Device_Speed speed);

/**
 * Sets the maximum packet size for a specific endpoint. This maximum packet size should be derived from
 * a device descriptor read through control interactions.
 */
b8 uhci_configure_endpoint_direction(UHCI_Controller *controller, u8 device_idx, u8 endpoint_idx, UHCI_Direction direction, u16 maximum_packet_size);

/**
 * Issues a read control transaction to the controller for the specified device id.
 * The header data is expected to be exactly 8 bytes long, the payload size is variable.
 * Returns the number of bytes read, or -1 on error.
 */
s32 uhci_control_read(UHCI_Controller *controller, u8 device_idx, void *header_data, u32 header_size_in_bytes, void *payload, u32 payload_size_in_bytes);

/**
 * Issues a write control transaction on the controller for the specified device id.
 * The header data is expected to be exactly 8 bytes long, the payload size is variable.
 * Returns the number of bytes read, or -1 on error.
 */
s32 uhci_control_write(UHCI_Controller *controller, u8 device_idx, const void *header_data, u32 header_size_in_bytes, const void *payload, u32 payload_size_in_bytes);

/**
 * Writes a bulk of data to the controller.
 * Returns the number of bytes written, or -1 on error.
 */
s32 uhci_bulk_write(UHCI_Controller *controller, u8 device_idx, u8 endpoint_idx, const void *data, u32 size_in_bytes);

/**
 * Reads a bulk of data from the controller.
 * Returns the number of bytes read, or -1 on error.
 */
s32 uhci_bulk_read(UHCI_Controller *controller, u8 device_idx, u8 endpoint_idx, void *data, u32 size_in_bytes);
