#include "block_device.h"

#define MAX_BLOCK_DEVICES 8

static Block_Device block_devices[MAX_BLOCK_DEVICES];
static u32 block_device_count;

Block_Device *block_device_allocate(const Block_Device_Interface interface) {
    if(block_device_count >= MAX_BLOCK_DEVICES) {
        return null;
    }

    const Block_Device_Key key = { 'C' + block_device_count };

    Block_Device *device = &block_devices[block_device_count];
    ZERO_STRUCT(device);
    device->key = key;
    device->interface = interface;
    ++block_device_count;
    return device;
}

const Block_Device *block_device_enumerate(s32 *count) {
    *count = block_device_count;
    return block_devices;
}

b8 block_device_get_metadata(const Block_Device *device, Block_Device_Metadata *result) {
    return device->interface.get_metadata(device->interface.interface_impl, result);
}

b8 block_device_read(const Block_Device *device, const u32 logical_block_address, void *destination) {
    return device->interface.read(device->interface.interface_impl, logical_block_address, destination);
}

b8 block_device_write(const Block_Device *device, const u32 logical_block_address, const void *source) {
    return device->interface.write(device->interface.interface_impl, logical_block_address, source);
}
