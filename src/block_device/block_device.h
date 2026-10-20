#pragma once

#include "base.h"

struct Block_Device;

typedef struct Block_Device_Metadata {
    Str8 device_name;
} Block_Device_Metadata;

typedef b8(*Block_Device_Get_Metadata)(struct Block_Device *device, Block_Device_Metadata *result);

typedef struct Block_Device_Interface {
    void *interface_impl;
    Block_Device_Get_Metadata get_metadata;
} Block_Device_Interface;

struct Block_Device *block_device_allocate(Block_Device_Interface interface);
