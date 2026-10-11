#include "scsi.h"
#include "uhci/uhci.h"
#include "usb/usb.h"

#define COMMAND_BLOCK_WRAPPER_SIGNATURE  0x43425355
#define COMMAND_STATUS_WRAPPER_SIGNATURE 0x53425355

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

static u32 scsi_tag = 1;

static
b8 scsi_execute(UHCI_Controller *controller, const USB_Device *device, const u8 command_data[16], const u8 command_length, void *data, const u32 data_length, const SCSI_Direction direction) {
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
    const u8 command_data[6] = {
        SCSI_COMMAND_Inquiry,
        0,
        0,
        0,
        sizeof(*result),
        0,
    };

    return scsi_execute(controller, device, command_data, sizeof(command_data), result, sizeof(*result), SCSI_DIRECTION_To_Host);
}
