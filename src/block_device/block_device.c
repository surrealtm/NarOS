#include "block_device.h"

#define MAX_BLOCK_DEVICES 8

typedef struct Block_Device {
    Block_Device_Interface interface;
} Block_Device;

static Block_Device block_devices[MAX_BLOCK_DEVICES];
static u32 block_device_count;

Block_Device *block_device_allocate(const Block_Device_Interface interface) {
    if(block_device_count >= MAX_BLOCK_DEVICES) {
        return null;
    }

    Block_Device *device = &block_devices[block_device_count++];
    ZERO_STRUCT(device);
    device->interface = interface;
    return device;
}
