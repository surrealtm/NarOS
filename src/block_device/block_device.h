#pragma once

#include "base.h"

struct Block_Device;

typedef struct Block_Device_Metadata {
    Str8 device_name;
} Block_Device_Metadata;

typedef b8(*Block_Device_Get_Metadata)(struct Block_Device *device, Block_Device_Metadata *result);

typedef struct Block_Device {
    void *interface_impl;
    Block_Device_Get_Metadata get_metadata;
} Block_Device;

/**
 * Returns a pointer to the persistent storage, or null if the capacity of block devices was reached.
 */
Block_Device *block_device_allocate(void);

/**
 * Returns a pointer to array of block devices registered.
 * The size of the array is returned in `count`.
 */
const Block_Device *block_device_enumerate(s32 *count);
