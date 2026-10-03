#include "uhci.h"
#include "base.h"
#include "ctrl.h"
#include "port/port.h"

#define WAIT_INTERVAL_NANOSECONDS 1000000 // Wait in 1 millisecond intervals
#define WAIT_MAX_RETRIES 100 // How many iterations in a wait-loop

static
b8 wait_for_present_flag(const u32 source_register, const u32 flag) {
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        if((port_read_u16(source_register) & flag) != 0) {
            return true;
        }
        os_ctrl_sleep(1000000);
    }
    return false;
}

static
b8 wait_for_missing_flag(const u32 source_register, const u32 flag) {
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        if((port_read_u16(source_register) & flag) == 0) {
            return true;
        }
        os_ctrl_sleep(1000000);
    }
    return false;
}

static inline
b8 check_port_connectivity(const u32 port) {
    return port_read_u16(port) & 0x1;
}

b8 uhci_initialize_controller(UHCI_Controller *controller, const u32 pci_address) {
    controller->pci_address = pci_address;

    // Initialize the frame list to only consist of "terminate" commands
    for(u32 i = 0; i < ARRAY_COUNT(controller->frame_list); ++i) {
        controller->frame_list[i] = 1;
    }

    const u32 usb_command_register  = controller->pci_address + 0x00;
    const u32 usb_status_register   = controller->pci_address + 0x02;
    const u32 usb_interrupt_enable  = controller->pci_address + 0x04;
    const u32 frame_number_register = controller->pci_address + 0x06;
    const u32 frame_list_base_address_register = controller->pci_address + 0x08;
    const u32 start_of_frame_modify = controller->pci_address + 0x0c;
    const u32 port1                 = controller->pci_address + 0x10;

    // Stop the UHCI
    port_write_u16(usb_command_register, 0x0);
    if(!wait_for_present_flag(usb_status_register, 1 << 5)) {
        return false;
    }

    // Reset the controller
    port_write_u16(usb_command_register, 1 << 1);
    if(!wait_for_missing_flag(usb_command_register, 1 << 1)) {
        return false;
    }

    // Clear the controller status
    port_write_u16(usb_status_register, 0x1f);
    port_write_u16(usb_interrupt_enable, 0x0);

    // Prepare the frame list
    port_write_u32(frame_list_base_address_register, PHYSICAL_ADDRESS(controller->frame_list));
    port_write_u16(frame_number_register, 0);
    port_write_u8(start_of_frame_modify, 0x40);

    // Start the controller
    port_write_u16(usb_command_register, 0xc1);

    return check_port_connectivity(port1);
}
