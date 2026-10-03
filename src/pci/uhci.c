#include "uhci.h"
#include "base.h"
#include "ctrl.h"
#include "port/port.h"

#define FRAME_LIST_CAPACITY 1024

static volatile u32 frame_list[FRAME_LIST_CAPACITY] ALIGN_DECLARATION(4096);

static
void wait_for_present_flag(const u32 source_register, const u32 flag) {
    while(!(port_read_u16(source_register) & flag)) {
        os_ctrl_sleep(1000000);
    }
}

static
void wait_for_missing_flag(const u32 source_register, const u32 flag) {
    while(port_read_u16(source_register) & flag) {
        os_ctrl_sleep(1000000);
    }
}

static inline
b8 check_port_connectivity(const u32 port) {
    return port_read_u16(port) & 0x1;
}

b8 uhci_initialize_device(const u32 io_base) {
    // Initialize the frame list to only consist of "terminate" commands
    for(u32 i = 0; i < ARRAY_COUNT(frame_list); ++i) {
        frame_list[i] = 1;
    }

    const u32 usb_command_register  = io_base + 0x00;
    const u32 usb_status_register   = io_base + 0x02;
    const u32 usb_interrupt_enable  = io_base + 0x04;
    const u32 frame_number_register = io_base + 0x06;
    const u32 frame_list_base_address_register = io_base + 0x08;
    const u32 start_of_frame_modify = io_base + 0x0c;
    const u32 port1 = io_base + 0x10;

    const b8 first_check = check_port_connectivity(port1);

    // Stop the UHCI
    port_write_u16(usb_command_register, 0x0);
    wait_for_present_flag(usb_status_register, 1 << 5);

    // Reset the controller
    port_write_u16(usb_command_register, 1 << 1);
    wait_for_missing_flag(usb_command_register, 1 << 1);

    // Clear the controller status
    port_write_u16(usb_status_register, 0x1f);
    port_write_u16(usb_interrupt_enable, 0x0);

    // Prepare the frame list
    port_write_u32(frame_list_base_address_register, (u32) frame_list);
    port_write_u16(frame_number_register, 0);
    port_write_u8(start_of_frame_modify, 0x40);

    // Start the controller
    port_write_u16(usb_command_register, 0xc1);

    const b8 second_check = check_port_connectivity(port1);
    return !first_check && second_check;
}
