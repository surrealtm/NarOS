#include "usb.h"
#include "usb/scsi.h"
#include "uhci/uhci.h"

#define INVALID_ENDPOINT U8_MAX
#define CONFIGURATION_PAYLOAD_CAPACITY 240

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

typedef enum USB_Interface_Class {
    USB_INTERFACE_CLASS_Mass_Storage = 0x08,
} USB_Interface_Class;

typedef enum USB_Interface_Mass_Storage_Subclass {
    USB_INTERFACE_MASS_STORAGE_SUBCLASS_SCSI_Transparent = 0x06,
} USB_Interface_Mass_Storage_Subclass;

typedef enum USB_Interface_Protocol {
    USB_INTERFACE_PROTOCOL_Bulk_Only = 0x50,
} USB_Interface_Protocol;

typedef enum USB_Endpoint_Direction {
    USB_ENDPOINT_DIRECTION_Out = 0x0,
    USB_ENDPOINT_DIRECTION_In = 0x1,
} USB_Endpoint_Direction;

typedef enum USB_Transfer_Type {
    USB_TRANSFER_TYPE_Control     = 0x0,
    USB_TRANSFER_TYPE_Isochronous = 0x1,
    USB_TRANSFER_TYPE_Bulk        = 0x2,
    USB_TRANSFER_TYPE_Interrupt   = 0x3,
} USB_Transfer_Type;

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

typedef struct USB_Descriptor_Header {
    u8 length;
    u8 descriptor_type;
} PACKED_STRUCT USB_Descriptor_Header;

typedef struct USB_Device_Descriptor {
    USB_Descriptor_Header header;
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

typedef struct USB_Configuration_Descriptor {
    USB_Descriptor_Header header;
    u16 total_length;
    u8 interface_count;
    u8 configuration_value;
    u8 configuration_string_index;
    u8 attributes;
    u8 maximum_power;
} PACKED_STRUCT USB_Configuration_Descriptor;

typedef struct USB_Interface_Descriptor {
    USB_Descriptor_Header header;
    u8 interface_number;
    u8 alternate_setting;
    u8 endpoint_count;
    u8 interface_class;
    u8 interface_subclass;
    u8 interface_protocol;
    u8 interface_string_index;
} PACKED_STRUCT USB_Interface_Descriptor;

typedef struct USB_Endpoint_Descriptor_Address {
    u8 endpoint_number : 4;
    u8 reserved : 3;
    u8 direction : 1;
} USB_Endpoint_Descriptor_Address;

typedef struct USB_Endpoint_Descriptor_Attributes {
    u8 transfer_type : 2;
    u8 synchronization_type : 2;
    u8 usage_type : 2;
    u8 reserved : 2;
} USB_Endpoint_Descriptor_Attributes;

typedef struct USB_Endpoint_Descriptor {
    USB_Descriptor_Header header;
    USB_Endpoint_Descriptor_Address address;
    USB_Endpoint_Descriptor_Attributes attributes;
    u16 maximum_packet_size;
    u8 interval;
} PACKED_STRUCT USB_Endpoint_Descriptor;

static
USB_Request_Header request_header(const USB_Request_Direction direction, const USB_Request_Type type, const USB_Request_Recipient recipient) {
    return (USB_Request_Header) { .direction = direction, .request_type = type, .recipient = recipient };
}

static
b8 configure_host_endpoints(USB_Device *device, UHCI_Controller *controller, const u8 port_idx) {
    const u8 device_idx = 0;
    const u8 endpoint_idx = 0;
    const u8 initial_length = 8; // The first request is limited to 8 bytes. With that information we can configure the HCI, and afterwards do arbitrary reads

    USB_Device_Descriptor descriptor = { 0 };

    USB_Request_Packet get_descriptor = {
        .header = request_header(USB_REQUEST_DIRECTION_Device_To_Host, USB_REQUEST_TYPE_Standard, USB_REQUEST_RECIPIENT_Device),
        .request = USB_REQUEST_Get_Descriptor,
        .value = USB_DESCRIPTOR_TYPE_Device << 8,
        .index = 0,
        .length = initial_length,
    };

    if(!uhci_configure_device(controller, device_idx, uhci_get_port_speed(controller, port_idx))) {
        return false;
    }

    if(uhci_control_read(controller, device_idx, &get_descriptor, sizeof(get_descriptor), &descriptor, initial_length) != initial_length) {
        return false;
    }

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

static
b8 parse_configuration_payload(USB_Device *device, const u8 *payload, const u16 payload_size_in_bytes) {
    const USB_Configuration_Descriptor *configuration_descriptor = (USB_Configuration_Descriptor *) payload;
    device->configuration_value = configuration_descriptor->configuration_value;

    b8 in_mass_storage_interface = false;

    for(u16 payload_offset_in_bytes = 0; payload_offset_in_bytes + sizeof(USB_Descriptor_Header) <= payload_size_in_bytes;) {
        const USB_Descriptor_Header *header = (USB_Descriptor_Header *) (&payload[payload_offset_in_bytes]);
        payload_offset_in_bytes += header->length;

        switch(header->descriptor_type) {
            case USB_DESCRIPTOR_TYPE_Interface: {
                const USB_Interface_Descriptor *interface = (USB_Interface_Descriptor *) header;
                in_mass_storage_interface = !in_mass_storage_interface &&
                    interface->alternate_setting == 0 &&
                    interface->interface_class == USB_INTERFACE_CLASS_Mass_Storage &&
                    interface->interface_subclass == USB_INTERFACE_MASS_STORAGE_SUBCLASS_SCSI_Transparent &&
                    interface->interface_protocol == USB_INTERFACE_PROTOCOL_Bulk_Only;

                if(in_mass_storage_interface) {
                    device->mass_storage_interface = interface->interface_number;
                }
            } break;

            case USB_DESCRIPTOR_TYPE_Endpoint: {
                const USB_Endpoint_Descriptor *endpoint = (USB_Endpoint_Descriptor *) header;
                if(!in_mass_storage_interface) break;

                const u16 packet_size_mask = 0x07ff;
                const u16 packet_size = (endpoint->maximum_packet_size & packet_size_mask);
                const u8 endpoint_number = endpoint->address.endpoint_number;
                if(endpoint->attributes.transfer_type != USB_TRANSFER_TYPE_Bulk || endpoint_number == 0 || packet_size == 0) break;

                if(endpoint->address.direction == USB_ENDPOINT_DIRECTION_In) {
                    device->bulk_in_endpoint = endpoint_number;
                    device->bulk_in_packet_size = packet_size;
                } else {
                    device->bulk_out_endpoint = endpoint_number;
                    device->bulk_out_packet_size = packet_size;
                }
            } break;
        }
    }

    return device->bulk_in_endpoint != INVALID_ENDPOINT && device->bulk_out_endpoint != INVALID_ENDPOINT;
}

static
b8 configure_device(USB_Device *device, UHCI_Controller *controller) {
    USB_Configuration_Descriptor configuration_descriptor = { 0 };
    u8 payload[CONFIGURATION_PAYLOAD_CAPACITY] = { 0 };

    //
    // Read the statically sized header of the configuration data to determine the complete size
    //
    USB_Request_Packet get_header = {
        .header  = request_header(USB_REQUEST_DIRECTION_Device_To_Host, USB_REQUEST_TYPE_Standard, USB_REQUEST_RECIPIENT_Device),
        .request = USB_REQUEST_Get_Descriptor,
        .value   = USB_DESCRIPTOR_TYPE_Configuration << 8,
        .index   = 0,
        .length  = sizeof(configuration_descriptor),
    };

    if(uhci_control_read(controller, device->address, &get_header, sizeof(get_header), &configuration_descriptor, sizeof(configuration_descriptor)) != sizeof(configuration_descriptor)) {
        return false;
    }

    if(configuration_descriptor.header.length != sizeof(configuration_descriptor) || configuration_descriptor.header.descriptor_type != USB_DESCRIPTOR_TYPE_Configuration || configuration_descriptor.total_length < sizeof(configuration_descriptor) || configuration_descriptor.total_length > sizeof(payload)) {
        return false;
    }

    //
    // Read the entire, dynamically configuration data
    //
    USB_Request_Packet get_payload = {
        .header  = request_header(USB_REQUEST_DIRECTION_Device_To_Host, USB_REQUEST_TYPE_Standard, USB_REQUEST_RECIPIENT_Device),
        .request = USB_REQUEST_Get_Descriptor,
        .value   = USB_DESCRIPTOR_TYPE_Configuration << 8,
        .index   = 0,
        .length  = configuration_descriptor.total_length,
    };

    if(uhci_control_read(controller, device->address, &get_payload, sizeof(get_payload), payload, configuration_descriptor.total_length) != configuration_descriptor.total_length) {
        return false;
    }

    //
    // Parse the endpoint configuration for this device
    //
    if(!parse_configuration_payload(device, payload, configuration_descriptor.total_length)) {
        return false;
    }

    //
    // Configure the two discovered endpoints
    //
    USB_Request_Packet set_configuration = {
        .header = request_header(USB_REQUEST_DIRECTION_Host_To_Device, USB_REQUEST_TYPE_Standard, USB_REQUEST_RECIPIENT_Device),
        .request = USB_REQUEST_Set_Configuration,
        .value = device->configuration_value,
    };

    if(uhci_control_write(controller, device->address, &set_configuration, sizeof(set_configuration), null, 0) != 0) {
        return false;
    }

    if(!uhci_configure_endpoint_direction(controller, device->address, device->bulk_in_endpoint, UHCI_DIRECTION_In, device->bulk_in_packet_size) ||
        !uhci_configure_endpoint_direction(controller, device->address, device->bulk_out_endpoint, UHCI_DIRECTION_Out, device->bulk_out_packet_size)) {
        return false;
    }

    return true;
}

b8 usb_initialize_device(USB_Device *device, UHCI_Controller *controller, const u8 port_idx) {
    *device = (USB_Device) {
        .address           = port_idx + 1,
        .bulk_in_endpoint  = INVALID_ENDPOINT,
        .bulk_out_endpoint = INVALID_ENDPOINT,
    };

    if(!configure_host_endpoints(device, controller, port_idx)) {
        return false;
    }

    if(!configure_device(device, controller)) {
        return false;
    }

    SCSI_Inquiry inquiry = { 0 };
    if(!scsi_inquire(device, controller, &inquiry)) {
        return false;
    }

    return true;
}
