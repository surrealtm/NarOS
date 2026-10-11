#include "usb.h"
#include "uhci/uhci.h"

#define INVALID_ENDPOINT U8_MAX

typedef enum USB_Descriptor_Type {
    USB_DESCRIPTOR_TYPE_Device = 0x1,
    USB_DESCRIPTOR_TYPE_Configuration = 0x2,
    USB_DESCRIPTOR_TYPE_String = 0x3,
    USB_DESCRIPTOR_TYPE_Interface = 0x4,
    USB_DESCRIPTOR_TYPE_Endpoint = 0x5,
    USB_DESCRIPTOR_TYPE_Device_Qualifier = 0x6,
    USB_DESCRIPTOR_TYPE_Other_Speed_Configuration = 0x7,
    USB_DESCRIPTOR_TYPE_Interface_Power = 0x8,
} USB_Descriptor_Type;

typedef enum USB_Request_Direction {
    USB_REQUEST_DIRECTION_Host_To_Device = 0x0,
    USB_REQUEST_DIRECTION_Device_To_Host = 0x1,
} USB_Request_Direction;

typedef enum USB_Request_Type {
    USB_REQUEST_TYPE_Standard = 0x0,
    USB_REQUEST_TYPE_Class    = 0x1,
    USB_REQUEST_TYPE_Vendor   = 0x2,
    USB_REQUEST_TYPE_Reserved = 0x3,
} USB_Request_Type;

typedef enum USB_Request_Recipient {
    USB_REQUEST_RECIPIENT_Device    = 0x0,
    USB_REQUEST_RECIPIENT_Interface = 0x1,
    USB_REQUEST_RECIPIENT_Endpoint  = 0x2,
    USB_REQUEST_RECIPIENT_Other     = 0x3,
} USB_Request_Recipient;

typedef enum USB_Request {
    USB_REQUEST_Set_Address       = 0x05,
    USB_REQUEST_Get_Descriptor    = 0x06,
    USB_REQUEST_Set_Configuration = 0x09,
} USB_Request;

typedef struct USB_Request_Header {
    u8 recipient : 5; // One of USB_Request_Recipient
    u8 request_type : 2; // One of USB_Request_Type
    u8 direction : 1; // One of USB_Request_Direction
} USB_Request_Header;

typedef struct USB_Request_Packet {
    USB_Request_Header header;
    u8  request; // One of USB_Request
    u16 value;
    u16 index;
    u16 length;
} PACKED_STRUCT USB_Request_Packet;

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

static
USB_Request_Header request_header(const USB_Request_Direction direction, const USB_Request_Type type, const USB_Request_Recipient recipient) {
    return (USB_Request_Header) { .direction = direction, .request_type = type, .recipient = recipient };
}

b8 usb_initialize_device(USB_Device *device, UHCI_Controller *controller, const u8 port_idx) {
    *device = (USB_Device) {
        .address           = port_idx + 1,
        .bulk_in_endpoint  = INVALID_ENDPOINT,
        .bulk_out_endpoint = INVALID_ENDPOINT,
    };

    USB_Device_Descriptor descriptor = { 0 };
    const u8 initial_length = 8; // The first request is limited to 8 bytes. With that information we can configure the HCI, and afterwards do arbitrary reads

    USB_Request_Packet get_descriptor = {
        .header = request_header(USB_REQUEST_DIRECTION_Device_To_Host, USB_REQUEST_TYPE_Standard, USB_REQUEST_RECIPIENT_Device),
        .request = USB_REQUEST_Get_Descriptor,
        .value = USB_DESCRIPTOR_TYPE_Device << 8,
        .index = 0,
        .length = initial_length,
    };

    if(uhci_control_read(controller, 0, &get_descriptor, sizeof(get_descriptor), &descriptor, initial_length) != initial_length) {
        return false;
    }

    const u8 device_idx = 0;
    const u8 endpoint_idx = 0;

    USB_Request_Packet set_address = {
        .header = request_header(USB_REQUEST_DIRECTION_Host_To_Device, USB_REQUEST_TYPE_Standard, USB_REQUEST_RECIPIENT_Device),
        .request = USB_REQUEST_Set_Address,
        .value = device->address,
    };

    if(uhci_control_write(controller, device_idx, &set_address, sizeof(set_address), null, 0) != 0) {
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
