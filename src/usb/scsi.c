#include "scsi.h"
#include "uhci/uhci.h"
#include "usb/usb.h"

#define BLOCK_SECTOR_SIZE 512

#define COMMAND_BLOCK_WRAPPER_SIGNATURE  0x43425355
#define COMMAND_STATUS_WRAPPER_SIGNATURE 0x53425355

#define EXTRACT_BYTE(value, byte_idx) (((u8 *) &(value))[byte_idx])

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
} SCSI_Command_Read_Capacity;

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

static u32 scsi_tag = 1;

static
b8 scsi_execute(UHCI_Controller *controller, const USB_Device *device, const void *command_data, const u8 command_length, void *data, const u32 data_length, const SCSI_Direction direction) {
    Command_Block_Wrapper command_block = {
        .signature = COMMAND_BLOCK_WRAPPER_SIGNATURE,
        .tag = scsi_tag++,
        .transfer_length = data_length,
        .direction = direction,
        .logical_unit_number = 0,
        .command_length = command_length,
    };

    move_memory(command_block.command_data, command_data, command_length);

    if(uhci_bulk_write(controller, device->address, device->bulk_out_endpoint, &command_block, sizeof(command_block)) != sizeof(command_block)) {
        return false;
    }

    if(data_length > 0) {
        const s32 transferred = direction == SCSI_DIRECTION_To_Host ? uhci_bulk_read(controller, device->address, device->bulk_in_endpoint, data, data_length) : uhci_bulk_write(controller, device->address, device->bulk_out_endpoint, data, data_length);

        if((u32) transferred != data_length) {
            return false;
        }
    }

    Command_Status_Wrapper command_status = { 0 };
    if(uhci_bulk_read(controller, device->address, device->bulk_in_endpoint, &command_status, sizeof(command_status)) != sizeof(command_status)) {
        return false;
    }

    return command_status.signature == COMMAND_STATUS_WRAPPER_SIGNATURE &&
        command_status.tag == command_block.tag &&
        command_status.data_residue == 0 &&
        command_status.status == SCSI_STATUS_Success;
}

b8 scsi_inquire(const USB_Device *device, UHCI_Controller *controller, SCSI_Inquiry *result) {
    const SCSI_Command_Inquiry inquiry = {
        .operation_code = SCSI_COMMAND_Inquiry,
        .allocation_length = u16_to_big_endian(sizeof(*result)),
    };

    return scsi_execute(controller, device, &inquiry, sizeof(inquiry), result, sizeof(*result), SCSI_DIRECTION_To_Host);
}

b8 scsi_read_capacity(const USB_Device *device, UHCI_Controller *controller, SCSI_Read_Capacity *result) {
    const u32 logical_block_address = 0x1;

    const SCSI_Command_Read_Capacity read_capacity = {
        .operation_code = SCSI_COMMAND_Read_Capacity,
        .logical_block_address = u32_to_big_endian(logical_block_address),
    };

    if(!scsi_execute(controller, device, &read_capacity, sizeof(read_capacity), result, sizeof(*result), SCSI_DIRECTION_To_Host)) {
        return false;
    }

    result->logical_block_address = u32_from_big_endian(result->logical_block_address);
    result->block_size_in_bytes = u32_from_big_endian(result->block_size_in_bytes);
    return true;
}

b8 scsi_read(const USB_Device *device, UHCI_Controller *controller, u32 logical_block_address, SCSI_Read *result) {
    const SCSI_Command_Read read = {
        .operation_code = SCSI_COMMAND_Read,
        .logical_block_address = u32_to_big_endian(logical_block_address),
        .transfer_length = u16_to_big_endian(result->requested_number_of_blocks),
    };

    return scsi_execute(controller, device, &read, sizeof(read), result->buffer, result->requested_number_of_blocks * BLOCK_SECTOR_SIZE, SCSI_DIRECTION_To_Host);
}
