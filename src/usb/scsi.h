#pragma once

#include "base.h"

struct USB_Device;

typedef struct SCSI_Inquiry {
    const u8 peripheral_device_type : 5;
    const u8 peripheral_qualifier : 3;
    const u8 device_type_modifier : 7;
    const u8 rmb : 1;
    const u8 ansi_approved_version : 3;
    const u8 ecma_version : 3;
    const u8 iso_version : 2;
    const u8 response_data_format : 4;
    const u8 reserved_0 : 2;
    const u8 trm_iop: 1;
    const u8 aenc : 1;
    const u8 additional_length;
    const u8 reserved_1[2];
    const u8 sft_re : 1;
    const u8 cmd_queue : 1;
    const u8 reserved_2 : 1;
    const u8 linked : 1;
    const u8 sync : 1;
    const u8 wbus16 : 1;
    const u8 wbus32 : 1;
    const u8 relative_address : 1;
    const char vendor[8];
    const char product[16];
    const char revision[4];
} PACKED_STRUCT SCSI_Inquiry;

typedef struct SCSI_Read_Capacity {
    u32 logical_block_address;
    u32 block_size_in_bytes;
} PACKED_STRUCT SCSI_Read_Capacity;

/**
 * Fills in the result structure with metadata about the USB mass storage device.
 * Returns false on hardware failure.
 */
b8 scsi_inquire(const struct USB_Device *device, SCSI_Inquiry *result);

/**
 * Fills in the read capacity into the result structure.
 * Returns false on hardware failure.
 */
b8 scsi_read_capacity(const struct USB_Device *device, SCSI_Read_Capacity *result);

/**
 * Reads the specified number of block segments into the data pointer, starting at the specified block address.
 * The data pointer is expected to hold `block_count * block_size_in_bytes` number of bytes.
 * Returns false if not all bytes could be read.
 */
b8 scsi_read(const struct USB_Device *device, u32 logical_block_address, u16 block_count, void *data);

/**
 * Writes the specified number of block segments from the data pointer onto the mass storage device, starting at the specified
 * block address.
 * The data pointer is expected to hold `block_count * block_size_in_bytes` number of bytes.
 * Returns false if not all bytes could be written.
 */
b8 scsi_write(const struct USB_Device *device, u32 logical_block_address, u16 block_count, const void *data);
