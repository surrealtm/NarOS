#pragma once

#include "base.h"

struct Block_Device;

typedef struct Block_Device_Key {
    char identifier;
} Block_Device_Key;

typedef struct Block_Device_Metadata {
    char device_name[128];
    u32 block_size_in_bytes;
    u32 block_count;
} Block_Device_Metadata;

typedef b8(*Block_Device_Get_Metadata)(void *interface_impl, Block_Device_Metadata *result);
typedef b8(*Block_Device_Read)(void *interface_impl, u32 logical_block_address, void *destination);
typedef b8(*Block_Device_Write)(void *interface_impl, u32 logical_block_address, const void *source);

typedef struct Block_Device_Interface {
    void *interface_impl;
    Block_Device_Get_Metadata get_metadata;
    Block_Device_Read read;
    Block_Device_Write write;
} Block_Device_Interface;

typedef struct Block_Device {
    Block_Device_Key key;
    Block_Device_Interface interface;
} Block_Device;

/**
 * Returns a pointer to the persistent storage, or null if the capacity of block devices was reached.
 */
Block_Device *block_device_allocate(Block_Device_Interface interface);

/**
 * Returns a pointer to array of block devices registered.
 * The size of the array is returned in `count`.
 */
const Block_Device *block_device_enumerate(s32 *count);

/**
 * Returns true if the metadata could be gathered from the device.
 * The metadata is written into `result`.
 */
b8 block_device_get_metadata(const Block_Device *device, Block_Device_Metadata *result);

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
