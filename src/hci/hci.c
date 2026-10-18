#include "hci.h"

b8 hci_initialize_controller(const HCI_Controller *controller, const u32 pci_address) {
    return controller->initialize_controller(controller->controller_impl, pci_address);
}

HCI_Device_Speed hci_get_port_speed(const HCI_Controller *controller, const u8 port_idx) {
    return controller->get_port_speed(controller->controller_impl, port_idx);
}

b8 hci_reset_and_enable_port(const HCI_Controller *controller, const u8 port_idx) {
    return controller->reset_and_enable_port(controller->controller_impl, port_idx);
}

b8 hci_configure_device(const HCI_Controller *controller, const u8 device_idx, const HCI_Device_Speed device_speed) {
    return controller->configure_device(controller->controller_impl, device_idx, device_speed);
}

b8 hci_configure_endpoint_direction(const HCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const HCI_Direction direction, const u16 maximum_packet_size) {
    return controller->configure_endpoint_direction(controller->controller_impl, device_idx, endpoint_idx, direction, maximum_packet_size);
}

s32 hci_control_read(const HCI_Controller *controller, const u8 device_idx, void *header_data, const u32 header_size_in_bytes, void *payload, const u32 payload_size_in_bytes) {
    return controller->control_read(controller->controller_impl, device_idx, header_data, header_size_in_bytes, payload, payload_size_in_bytes);
}

s32 hci_control_write(const HCI_Controller *controller, const u8 device_idx, const void *header_data, const u32 header_size_in_bytes, const void *payload, const u32 payload_size_in_bytes) {
    return controller->control_write(controller->controller_impl, device_idx, header_data, header_size_in_bytes, payload, payload_size_in_bytes);
}

s32 hci_bulk_write(const HCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, const void *data, const u32 size_in_bytes) {
    return controller->bulk_write(controller->controller_impl, device_idx, endpoint_idx, data, size_in_bytes);
}

s32 hci_bulk_read(const HCI_Controller *controller, const u8 device_idx, const u8 endpoint_idx, void *data, const u32 size_in_bytes) {
    return controller->bulk_read(controller->controller_impl, device_idx, endpoint_idx, data, size_in_bytes);
}
