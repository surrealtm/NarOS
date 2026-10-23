#include "mass_storage.h"
#include "base.h"
#include "scsi.h"
#include "block_device/block_device.h"

static
b8 get_metadata(const struct USB_Device *device, Block_Device_Metadata *result) {
    SCSI_Inquiry inquiry = { 0 };
    if(!scsi_inquire(device, &inquiry)) return false;

    *result = (Block_Device_Metadata) { 0 };
    move_memory(result->device_name, inquiry.product, min(ARRAY_COUNT(result->device_name), ARRAY_COUNT(inquiry.product)));

    return true;
}

void mass_storage_register_block_device(const struct USB_Device *usb) {
    SCSI_Inquiry inquiry = { 0 };
    if (!scsi_inquire(usb, &inquiry)) return;

    SCSI_Read_Capacity read_capacity;
    if (!scsi_read_capacity(usb, &read_capacity)) return;

    Block_Device *block = block_device_allocate();
    *block = (Block_Device) {
        (void *) usb,
        (Block_Device_Get_Metadata) get_metadata,
    };
}
