#pragma once

#include "base.h"

struct Block_Device;

typedef struct Block_Device_Key {
    char identifier;
} Block_Device_Key;

typedef b8(*Block_Device_Read)(void *interface_impl, u32 logical_block_address, void *destination);
typedef b8(*Block_Device_Write)(void *interface_impl, u32 logical_block_address, const void *source);

typedef struct Block_Device_Interface {
    void *interface_impl;
    Block_Device_Read read;
    Block_Device_Write write;
} Block_Device_Interface;

typedef struct Block_Device {
    Block_Device_Key key;
    Block_Device_Interface interface;

    char device_name[32];
    u32 block_size_in_bytes;
    u32 block_count;
} Block_Device;

/**
 * Returns true if the block device is valid.
 */
b8 block_device_is_valid(const Block_Device *device);

/**
 * Returns true if the two keys are equal.
 */
b8 block_device_keys_equal(Block_Device_Key lhs, Block_Device_Key rhs);

/**
 * Returns a pointer to the persistent storage, or a dummy device if the capacity of block devices was reached.
 * Use @p block_device_is_valid to determine the success.
 */
Block_Device *block_device_allocate(Block_Device_Interface interface, Str8 device_name, u32 block_size_in_bytes, u32 block_count);

/**
 * Returns a pointer to array of block devices registered.
 * The size of the array is returned in `count`.
 */
const Block_Device *block_device_enumerate(s32 *count);

/**
 * Returns the block device for the given key, or a dummy device if no such block device exists.
 * Use @p block_device_is_valid to determine the success.
 */
const Block_Device *block_device_find(Block_Device_Key key);

/**
 * Attempts to read one block from the device.
 * The destination buffer must have the capacity for one block size (obtained through the block-device's metadata).
 * Returns whether the block-transfer was successful.
 */
b8 block_device_read(const Block_Device *device, u32 logical_block_address, void *destination);

/**
 * Attempts to write one block to the device.
 * The source buffer must contain enough valid data for one block size (obtained through the block-device's metadata).
 * Returns whether the block-transfer was successful.
 */
b8 block_device_write(const Block_Device *device, u32 logical_block_address, const void *source);
