#pragma once

#include "base.h"

struct UHCI_Controller;
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

STATIC_ASSERT(sizeof(SCSI_Inquiry) == 36);

b8 scsi_inquire(const struct USB_Device *device, struct UHCI_Controller *controller, SCSI_Inquiry *result);
