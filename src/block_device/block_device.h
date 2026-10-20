#pragma once

#include "base.h"

struct Block_Device;

typedef struct Block_Device_Metadata {
    const Str8 device_name;
} Block_Device_Metadata;

typedef b8(*Block_Device_Get_Metadata)(struct Block_Device *device);

typedef struct Block_Device_Interface {
    Block_Device_Get_Metadata get_metadata;
} Block_Device_Interface;

struct Block_Device *block_device_register(Block_Device_Interface interface);
