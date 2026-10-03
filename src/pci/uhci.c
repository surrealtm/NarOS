#include "uhci.h"
#include "base.h"
#include "ctrl.h"
#include "port/port.h"

#define WAIT_INTERVAL_NANOSECONDS 1000000 // Wait in 1 millisecond intervals
#define WAIT_MAX_RETRIES 100 // How many iterations in a wait-loop

// ---------------------------------------------------------------------------------------------------------------
// UHCI Types
// ---------------------------------------------------------------------------------------------------------------

#define UHCI_COMMAND_HALT        0
#define UHCI_COMMAND_START      (1 << 0)
#define UHCI_COMMAND_HOST_RESET (1 << 1)


// ---------------------------------------------------------------------------------------------------------------
// UHCI Interaction
// Port Mapping: https://wiki.osdev.org/Universal_Host_Controller_Interface
// ---------------------------------------------------------------------------------------------------------------

typedef b8 (*Status_Check)(const UHCI_Controller *);

static
b8 wait_for_status(const UHCI_Controller *controller, Status_Check check) {
    for(u32 i = 0; i < WAIT_MAX_RETRIES; ++i) {
        if(check(controller)) {
            return true;
        }
        os_ctrl_sleep(WAIT_INTERVAL_NANOSECONDS);
    }
    return false;
}

static inline
b8 is_halted(const UHCI_Controller *controller) {
    return (port_read_u16(controller->pci_address + 0x02) & (1 << 5)) != 0;
}

static inline
b8 is_host_reset(const UHCI_Controller *controller) {
    return (port_read_u16(controller->pci_address + 0x00) & (1 << 1)) == 0;
}

static inline
void write_frame_list_pointer(const UHCI_Controller *controller, const u32 *frame_list) {
    port_write_u32(controller->pci_address + 0x08, PHYSICAL_ADDRESS(frame_list));
}

static inline
void write_frame_number(const UHCI_Controller *controller, const u16 frame_number) {
    port_write_u16(controller->pci_address + 0x06, frame_number);
}

static inline
void write_frame_timing(const UHCI_Controller *controller, const u8 value) {
    port_write_u8(controller->pci_address + 0x0c, value);
}

static inline
void toggle_interrupts(const UHCI_Controller *controller, const b8 enabled) {
    port_write_u16(controller->pci_address + 0x4, !!(enabled));
}

static inline
void clear_signals(const UHCI_Controller *controller, const u16 mask) {
    port_write_u16(controller->pci_address + 0x02, mask);
}

static inline
void issue_command(const UHCI_Controller *controller, const u16 command) {
    port_write_u16(controller->pci_address + 0x0, command);
}

static inline
b8 check_port_connectivity(const UHCI_Controller *controller, const u32 port_idx) {
    return port_read_u16(controller->ports[port_idx]) & 0x1;
}

// ---------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------

b8 uhci_initialize_controller(UHCI_Controller *controller, const u32 pci_address) {
    controller->pci_address = pci_address;

    for(u32 i = 0; i < UHCI_PORT_CAPACITY; ++i) {
        controller->ports[i] = controller->pci_address + 0x10 + (i * 2);
    }

    // Initialize the frame list to only consist of "terminate" commands
    for(u32 i = 0; i < ARRAY_COUNT(controller->frame_list); ++i) {
        controller->frame_list[i] = 1;
    }

    issue_command(controller, UHCI_COMMAND_HALT);
    if(!wait_for_status(controller, is_halted)) {
        return false;
    }

    issue_command(controller, UHCI_COMMAND_HOST_RESET);
    if(!wait_for_status(controller, is_host_reset)) {
        return false;
    }

    clear_signals(controller, 0x1f); // Clear all signals except `HCHalted`
    toggle_interrupts(controller, false);
    write_frame_list_pointer(controller, controller->frame_list);
    write_frame_number(controller, 0x0);
    write_frame_timing(controller, 0x40);
    issue_command(controller, UHCI_COMMAND_START);

    return check_port_connectivity(controller, 0);
}
