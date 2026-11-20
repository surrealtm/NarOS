#pragma once

#include "base.h"

struct Block_Device;
struct File_Mount;

/**
 * Returns whether the file mount is actually valid and backed by a device.
 */
b8 file_mount_is_valid(const struct File_Mount *file_mount);

/**
 * Registers a new file mount backed by the passed block device.
 * Returns a dummy file mount if the maximum capacity has been reached.
 */
struct File_Mount *file_mount_create(struct Block_Device *device);

