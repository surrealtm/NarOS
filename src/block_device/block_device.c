#include "block_device.h"

#define MAX_BLOCK_DEVICES 8

static Block_Device block_devices[MAX_BLOCK_DEVICES];
static u32 block_device_count;

Block_Device *block_device_allocate(void) {
    if(block_device_count >= MAX_BLOCK_DEVICES) {
        return null;
    }

    Block_Device *device = &block_devices[block_device_count++];
    ZERO_STRUCT(device);
    return device;
}

const Block_Device *block_device_enumerate(s32 *count) {
    *count = block_device_count;
    return block_devices;
}
