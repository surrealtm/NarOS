#pragma once

struct USB_Device;

void mass_storage_maybe_register_block_device(const struct USB_Device *device);
