#pragma once

#include "base.h"

typedef enum HCI_Device_Speed {
    HCI_DEVICE_SPEED_Full  = 0,
    HCI_DEVICE_SPEED_Low   = 1,
    HCI_DEVICE_SPEED_High  = 2,
    HCI_DEVICE_SPEED_Super = 3,
    HCI_DEVICE_SPEED_Super_Plus = 4,
    HCI_DEVICE_SPEED_Error = 5,
} HCI_Device_Speed;

typedef enum HCI_Direction {
    HCI_DIRECTION_In = 0,
    HCI_DIRECTION_Out = 1,
    HCI_DIRECTION_COUNT = 2,
} HCI_Direction;

typedef b8(*HCI_Initialize_Controller)(void *controller_impl,  u32 pci_adddress);
typedef s32(*HCI_Get_Port_Capacity)(void *controller_impl);
typedef HCI_Device_Speed(*HCI_Get_Port_Speed)(void *controller_impl,  u8 port_idx);
typedef b8(*HCI_Reset_And_Enable_Port)(void *controller_impl,  u8 port_idx);
typedef b8(*HCI_Configure_Device)(void *controller_impl,  u8 device_idx,  HCI_Device_Speed device_speed);
typedef b8(*HCI_Configure_Endpoint_Direction)(void *controller_impl,  u8 device_idx,  u8 endpoint_idx,  HCI_Direction direction,  u16 maximum_packet_size);
typedef s32(*HCI_Control_Read)(void *controller_impl,  u8 device_idx, void *header_data,  u32 header_size_in_bytes, void *payload, u32 payload_size_in_bytes);
typedef s32(*HCI_Control_Write)(void *controller_impl,  u8 device_idx, const void *header_data,  u32 header_size_in_bytes, const void *payload,  u32 payload_size_in_bytes);
typedef s32(*HCI_Bulk_Write)(void *controller_impl,  u8 device_idx,  u8 endpoint_idx, const void *data,  u32 size_in_bytes);
typedef s32(*HCI_Bulk_Read)(void *controller_impl,  u8 device_idx,  u8 endpoint_idx, void *data,  u32 size_in_bytes);

/**
 * Abstraction layer over the different Host Controller Interfaces.
 * These are the operations supported by all implemented HCIs that are required to implement the USB protocol.
 * Do not call the function pointers directly, use the standalone function wrappers provided below.
 */
typedef struct HCI_Controller {
    void *controller_impl;
    HCI_Initialize_Controller initialize_controller;
    HCI_Get_Port_Capacity get_port_capacity;
    HCI_Get_Port_Speed get_port_speed;
    HCI_Reset_And_Enable_Port reset_and_enable_port;
    HCI_Configure_Device configure_device;
    HCI_Configure_Endpoint_Direction configure_endpoint_direction;
    HCI_Control_Read control_read;
    HCI_Control_Write control_write;
    HCI_Bulk_Read bulk_read;
    HCI_Bulk_Write bulk_write;
} HCI_Controller;

/**
 * Initializes the controller structure and configures the hardware controller at the given pci address.
 * Returns true if the hardware controller was successfully reset and enabled
 */
b8 hci_initialize_controller(const HCI_Controller *controller, u32 pci_address);

/**
 * Returns how many ports this specific controller supports, which depends on the underlying protocol
 * and the hardware device.
 */
s32 hci_get_port_capacity(const HCI_Controller *controller);

/**
 * Returns the device speed for the given port. This must be used when enumerating devices through the port.
 */
HCI_Device_Speed hci_get_port_speed(const HCI_Controller *controller, u8 port_idx);

/**
 * Issues a reset signal on the port and then attempts to enable it.
 * Returns true if the port is now successfully enabled and connected.
 */
b8 hci_reset_and_enable_port(const HCI_Controller *controller, u8 port_idx);

/**
 * Sets the devices transmission speed and resets the data toggles for the first endpoint.
 */
b8 hci_configure_device(const HCI_Controller *controller, u8 device_idx, HCI_Device_Speed device_speed);

/**
 * Sets the maximum packet size for a specific endpoint. This maximum packet size should be derived from
 * a device descriptor read through control interactions.
 */
b8 hci_configure_endpoint_direction(const HCI_Controller *controller, u8 device_idx, u8 endpoint_idx, HCI_Direction direction, u16 maximum_packet_size);

/**
 * Issues a read control transaction to the controller for the specified device id.
 * The header data is expected to be exactly 8 bytes long, the payload size is variable.
 * Returns the number of bytes read, or -1 on error.
 */
s32 hci_control_read(const HCI_Controller *controller, u8 device_idx, void *header_data, u32 header_size_in_bytes, void *payload, u32 payload_size_in_bytes);

/**
 * Issues a write control transaction on the controller for the specified device id.
 * The header data is expected to be exactly 8 bytes long, the payload size is variable.
 * Returns the number of bytes read, or -1 on error.
 */
s32 hci_control_write(const HCI_Controller *controller, u8 device_idx, const void *header_data, u32 header_size_in_bytes, const void *payload, u32 payload_size_in_bytes);

/**
 * Writes a bulk of data to the controller.
 * Returns the number of bytes written, or -1 on error.
 */
s32 hci_bulk_write(const HCI_Controller *controller, u8 device_idx, u8 endpoint_idx, const void *data, u32 size_in_bytes);

/**
 * Reads a bulk of data from the controller.
 * Returns the number of bytes read, or -1 on error.
 */
s32 hci_bulk_read(const HCI_Controller *controller, u8 device_idx, u8 endpoint_idx, void *data, u32 size_in_bytes);
