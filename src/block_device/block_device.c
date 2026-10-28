#include "block_device.h"

#define MAX_BLOCK_DEVICES 8

static
b8 dummy_read(void *interface_impl, const u32 logical_block_address, void *destination) {
    (void) interface_impl;
    (void) logical_block_address;
    (void) destination;
    return false;
}

static
b8 dummy_write(void *interface_impl, const u32 logical_block_address, const void *source) {
    (void) interface_impl;
    (void) logical_block_address;
    (void) source;
    return false;
}

static Block_Device dummy_device = {
    .interface = {
        null,
        dummy_read,
        dummy_write,
    },
};

static Block_Device block_devices[MAX_BLOCK_DEVICES];
static u32 block_device_count;

b8 block_device_is_valid(const Block_Device *device) {
    return device->key.identifier != 0;
}

b8 block_device_keys_equal(const Block_Device_Key lhs, const Block_Device_Key rhs) {
    return compare_memory(&lhs, &rhs, sizeof(lhs)) == 0;
}

Block_Device *block_device_allocate(const Block_Device_Interface interface, const Str8 device_name, const u32 block_size_in_bytes, const u32 block_count) {
    if(block_device_count >= MAX_BLOCK_DEVICES) {
        return &dummy_device;
    }

    const Block_Device_Key key = { 'C' + block_device_count };

    Block_Device *device = &block_devices[block_device_count];
    ZERO_STRUCT(device);
    device->key = key;
    device->interface = interface;
    copy_memory(device->device_name, device_name.data, min((s32) ARRAY_COUNT(device->device_name), device_name.count));
    device->block_size_in_bytes = block_size_in_bytes;
    device->block_count = block_count;
    ++block_device_count;
    return device;
}

const Block_Device *block_device_enumerate(s32 *count) {
    *count = block_device_count;
    return block_devices;
}

const Block_Device *block_device_find(const Block_Device_Key key) {
    s32 count = 0;
    const Block_Device *devices = block_device_enumerate(&count);

    for (s32 i = 0; i < count; ++i) {
        if (block_device_keys_equal(devices[i].key, key)) {
            return &devices[i];
        }
    }

    return &dummy_device;
}

b8 block_device_read(const Block_Device *device, const u32 logical_block_address, void *destination) {
    return device->interface.read(device->interface.interface_impl, logical_block_address, destination);
}

b8 block_device_write(const Block_Device *device, const u32 logical_block_address, const void *source) {
    return device->interface.write(device->interface.interface_impl, logical_block_address, source);
}
