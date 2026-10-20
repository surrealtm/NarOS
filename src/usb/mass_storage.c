#include "mass_storage.h"
#include "scsi.h"
#include "block_device/block_device.h"

static
b8 get_metadata(const struct USB_Device *device, Block_Device_Metadata *result) {
    (void) device; // @Incomplete
    result->device_name = str8("MassStorageDevice");
    return true;
}

void mass_storage_maybe_register_block_device(const struct USB_Device *device) {
    SCSI_Inquiry inquiry = { 0 };
    if (!scsi_inquire(device, &inquiry)) return;

    SCSI_Read_Capacity read_capacity;
    if (!scsi_read_capacity(device, &read_capacity)) return;

    const Block_Device_Interface interface = {
        (void *) device,
        (Block_Device_Get_Metadata) get_metadata,
    };

    block_device_allocate(interface);
}
