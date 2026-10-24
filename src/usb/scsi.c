#include "scsi.h"
#include "block_device/block_device.h"
#include "hci/hci.h"
#include "usb/usb.h"

#define COMMAND_BLOCK_WRAPPER_SIGNATURE  0x43425355
#define COMMAND_STATUS_WRAPPER_SIGNATURE 0x53425355

#define EXTRACT_BYTE(value, byte_idx) (((u8 *) &(value))[byte_idx])

// ---------------------------------------------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------------------------------------------

typedef enum SCSI_Command {
    SCSI_COMMAND_Test_Unit_Ready   = 0x00,
    SCSI_COMMAND_Request_Sense     = 0x03,
    SCSI_COMMAND_Inquiry           = 0x12,
    SCSI_COMMAND_Read_Capacity     = 0x25,
    SCSI_COMMAND_Read              = 0x28,
    SCSI_COMMAND_Write             = 0x2a,
    SCSI_COMMAND_Synchronize_Cache = 0x35,
} SCSI_Command;

typedef enum SCSI_Direction {
    SCSI_DIRECTION_To_Device = 0x0,
    SCSI_DIRECTION_To_Host   = 0x80,
} SCSI_Direction;

typedef enum SCSI_Status {
    SCSI_STATUS_Success     = 0x0,
    SCSI_STATUS_Failed      = 0x1,
    SCSI_STATUS_Phase_Error = 0x2,
} SCSI_Status;

typedef struct Command_Block_Wrapper {
    u32 signature;
    u32 tag;
    u32 transfer_length;
    u8 direction;
    u8 logical_unit_number;
    u8 command_length;
    u8 command_data[16];
} PACKED_STRUCT Command_Block_Wrapper;

typedef struct Command_Status_Wrapper {
    u32 signature;
    u32 tag;
    u32 data_residue;
    u8 status;
} PACKED_STRUCT Command_Status_Wrapper;

typedef struct SCSI_Command_Inquiry {
    const u8 operation_code;
    const u8 evpd : 1;
    const u8 obsolete : 1;
    const u8 reserved : 6;
    const u8 page_code;
    const u16 allocation_length;
    const u8 control;
} PACKED_STRUCT SCSI_Command_Inquiry;

typedef struct SCSI_Command_Read_Capacity {
    const u8 operation_code;
    const u8 obsolete : 1;
    const u8 reserved0 : 7;
    const u32 logical_block_address;
    const u16 reserved1;
    const u8 pmi : 1;
    const u8 reserved2 : 7;
    const u8 control;
} PACKED_STRUCT SCSI_Command_Read_Capacity;

typedef struct SCSI_Command_Read {
    const u8 operation_code;
    const u8 obsolete : 1;
    const u8 fua_nv : 1;
    const u8 reserved0 : 1;
    const u8 fua : 1;
    const u8 dpo : 1;
    const u8 rdprotect : 3;
    const u32 logical_block_address;
    const u8 group_number : 5;
    const u8 reserved1 : 3;
    const u16 transfer_length;
    const u8 control;
} PACKED_STRUCT SCSI_Command_Read;

typedef struct SCSI_Command_Write {
    const u8 operation_code;
    const u8 obsolete : 1;
    const u8 fua_nv : 1;
    const u8 reserved0 : 1;
    const u8 fua : 1;
    const u8 dpo : 1;
    const u8 wrprotect : 3;
    const u32 logical_block_address;
    const u8 group_number : 5;
    const u8 reserved1 : 3;
    const u16 transfer_length;
    const u8 control;
} PACKED_STRUCT SCSI_Command_Write;

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

static u32 scsi_tag = 1;

// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

static b8 scsi_read_capacity(const USB_Device *device, SCSI_Read_Capacity *result);

static
s32 determine_block_sector_size(const USB_Device *device) {
    SCSI_Read_Capacity capacity;
    if(!scsi_read_capacity(device, &capacity)) return -1;
    return capacity.block_size_in_bytes;
}

// ---------------------------------------------------------------------------------------------------------------
// SCSI Commands
// ---------------------------------------------------------------------------------------------------------------

static
b8 scsi_execute(const USB_Device *device, const void *command_data, const u8 command_length, void *data, const u32 data_length, const SCSI_Direction direction) {
    Command_Block_Wrapper command_block = {
        .signature = COMMAND_BLOCK_WRAPPER_SIGNATURE,
        .tag = scsi_tag++,
        .transfer_length = data_length,
        .direction = direction,
        .logical_unit_number = 0,
        .command_length = command_length,
    };

    move_memory(command_block.command_data, command_data, command_length);

    if(hci_bulk_write(&device->hci, device->address, device->mass_storage.bulk_out_endpoint, &command_block, sizeof(command_block)) != sizeof(command_block)) {
        return false;
    }

    if(data_length > 0) {
        const s32 transferred = direction == SCSI_DIRECTION_To_Host ? hci_bulk_read(&device->hci, device->address, device->mass_storage.bulk_in_endpoint, data, data_length) : hci_bulk_write(&device->hci, device->address, device->mass_storage.bulk_out_endpoint, data, data_length);

        if((u32) transferred != data_length) {
            return false;
        }
    }

    Command_Status_Wrapper command_status = { 0 };
    if(hci_bulk_read(&device->hci, device->address, device->mass_storage.bulk_in_endpoint, &command_status, sizeof(command_status)) != sizeof(command_status)) {
        return false;
    }

    return command_status.signature == COMMAND_STATUS_WRAPPER_SIGNATURE &&
        command_status.tag == command_block.tag &&
        command_status.data_residue == 0 &&
        command_status.status == SCSI_STATUS_Success;
}

static
b8 scsi_inquire(const USB_Device *device, SCSI_Inquiry *result) {
    if (!device->mass_storage.supported) return false;

    const SCSI_Command_Inquiry inquiry = {
        .operation_code = SCSI_COMMAND_Inquiry,
        .allocation_length = u16_to_big_endian(sizeof(*result)),
    };

    return scsi_execute(device, &inquiry, sizeof(inquiry), result, sizeof(*result), SCSI_DIRECTION_To_Host);
}

static
b8 scsi_read_capacity(const USB_Device *device, SCSI_Read_Capacity *result) {
    if (!device->mass_storage.supported) return false;

    const SCSI_Command_Read_Capacity read_capacity = {
        .operation_code = SCSI_COMMAND_Read_Capacity,
    };

    if(!scsi_execute(device, &read_capacity, sizeof(read_capacity), result, sizeof(*result), SCSI_DIRECTION_To_Host)) {
        return false;
    }

    result->logical_block_address = u32_from_big_endian(result->logical_block_address);
    result->block_size_in_bytes = u32_from_big_endian(result->block_size_in_bytes);
    return true;
}

static
b8 scsi_read(const USB_Device *device, const u32 logical_block_address, const u16 block_count, void *data) {
    if (!device->mass_storage.supported) return false;

    const SCSI_Command_Read read = {
        .operation_code = SCSI_COMMAND_Read,
        .logical_block_address = u32_to_big_endian(logical_block_address),
        .transfer_length = u16_to_big_endian(block_count),
    };

    const u32 block_sector_size = determine_block_sector_size(device);
    return scsi_execute(device, &read, sizeof(read), data, block_count * block_sector_size, SCSI_DIRECTION_To_Host);
}

static
b8 scsi_write(const USB_Device *device, const u32 logical_block_address, const u16 number_of_blocks, const void *data) {
    if (!device->mass_storage.supported) return false;

    const SCSI_Command_Write write = {
        .operation_code = SCSI_COMMAND_Write,
        .logical_block_address = u32_to_big_endian(logical_block_address),
        .transfer_length = u16_to_big_endian(number_of_blocks),
    };

    const u32 block_sector_size = determine_block_sector_size(device);
    return scsi_execute(device, &write, sizeof(write), (void *) data, number_of_blocks * block_sector_size, SCSI_DIRECTION_To_Device);
}

// ---------------------------------------------------------------------------------------------------------------
// Block Device API
// ---------------------------------------------------------------------------------------------------------------

static
b8 get_metadata(const struct USB_Device *device, Block_Device_Metadata *result) {
    // @Temporary: Only necessary until block device api has read & write
    (void) scsi_write;
    (void) scsi_read;

    SCSI_Inquiry inquiry = { 0 };
    if(!scsi_inquire(device, &inquiry)) return false;

    *result = (Block_Device_Metadata) { 0 };
    move_memory(result->device_name, inquiry.product, min(ARRAY_COUNT(result->device_name), ARRAY_COUNT(inquiry.product)));

    SCSI_Read_Capacity read_capacity;
    if (!scsi_read_capacity(device, &read_capacity)) return false;

    result->block_size_in_bytes = read_capacity.block_size_in_bytes;
    result->block_count = read_capacity.logical_block_address + 1;

    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------

void scsi_register_block_device(const struct USB_Device *device) {
    SCSI_Inquiry inquiry = { 0 };
    if (!scsi_inquire(device, &inquiry)) return;

    SCSI_Read_Capacity read_capacity;
    if (!scsi_read_capacity(device, &read_capacity)) return;

    Block_Device_Interface interface = {
        (void *) device,
        (Block_Device_Get_Metadata) get_metadata,
    };

    block_device_allocate(interface);
}
