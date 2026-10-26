#pragma once

#include "base.h"

struct USB_Device;

void scsi_register_block_device(const struct USB_Device *device);
