#include "usb.h"
#include "uhci/uhci.h"

#define INVALID_ENDPOINT U8_MAX

typedef enum USB_Descriptor {
    USB_DESCRIPTOR_Device        = 0x01,
    USB_DESCRIPTOR_Configuration = 0x02,
} USB_Descriptor;

typedef enum USB_Request_Type {
    USB_REQUEST_TYPE_Host_To_Device = 0x00,
    USB_REQUEST_TYPE_Device_To_Host = 0x80,
} USB_Request_Type;

typedef enum USB_Request {
    USB_REQUEST_Set_Address       = 0x05,
    USB_REQUEST_Get_Descriptor    = 0x06,
    USB_REQUEST_Set_Configuration = 0x09,
} USB_Request;

typedef struct USB_Setup_Packet {
    u8  request_type;
    u8  request;
    u16 value;
    u16 index;
    u16 length;
} PACKED_STRUCT USB_Setup_Packet;

typedef struct USB_Device_Descriptor {
    u8  length;
    u8  descriptor_type;
    u16 usb_version;
    u8  device_class;
    u8  device_subclass;
    u8  device_protocol;
    u8  maximum_packet_size_0;
    u16 vendor_id;
    u16 product_id;
    u16 device_version;
    u8  manufacturer_string_index;
    u8  product_string_index;
    u8  serial_number_string_index;
    u8  configuration_count;
} PACKED_STRUCT USB_Device_Descriptor;

STATIC_ASSERT(sizeof(USB_Device_Descriptor) == 18);

typedef struct USB_Device {
    u8 address;
    u8 configuration_value;
    u8 bulk_in_endpoint;
    u8 bulk_out_endpoint;
    u8 bulk_in_packet_size;
    u8 bulk_out_packet_size;
} USB_Device;

static
b8 enumerate_port(UHCI_Controller *controller, const u8 port_idx, const u8 desired_address, USB_Device *device) {
    *device = (USB_Device) {
        .address           = desired_address,
        .bulk_in_endpoint  = INVALID_ENDPOINT,
        .bulk_out_endpoint = INVALID_ENDPOINT,
    };

    USB_Device_Descriptor descriptor = { 0 };
    USB_Setup_Packet get_descriptor = {
        .request_type = USB_REQUEST_TYPE_Device_To_Host,
        .request = USB_REQUEST_Get_Descriptor,
        .value = USB_DESCRIPTOR_Device << 8,
        .index = 0,
        .length = 8,
    };

    if(uhci_control_read(controller, 0, &get_descriptor, sizeof(get_descriptor), &descriptor, sizeof(USB_Device_Descriptor)) != sizeof(USB_Device_Descriptor)) {
        return false;
    }

    const u8 endpoint_idx = 0;

    USB_Setup_Packet set_address = {
        .request_type = USB_REQUEST_TYPE_Host_To_Device,
        .request = USB_REQUEST_Set_Address,
        .value = device->address,
    };

    if(!uhci_control_write(controller, 0, &set_address, sizeof(set_address), null, 0)) {
        return false;
    }

    if(!uhci_configure_device(controller, device->address, uhci_get_port_speed(controller, port_idx))) {
        return false;
    }

    if(!uhci_configure_endpoint_direction(controller, device->address, endpoint_idx, UHCI_DIRECTION_In, descriptor.maximum_packet_size_0) ||
        !uhci_configure_endpoint_direction(controller, device->address, endpoint_idx, UHCI_DIRECTION_Out, descriptor.maximum_packet_size_0)) {
        return false;
    }

    return true;
}

b8 usb_initialize_device(UHCI_Controller *controller) {
    for(u8 port = 0; port < UHCI_PORT_CAPACITY; ++port) {
        USB_Device device;
        enumerate_port(controller, port, port + 1, &device);
    }
    return false;
}
