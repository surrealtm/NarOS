#include "pci.h"
#include "output.h"
#include "hci/uhci.h"
#include "hci/xhci.h"
#include "usb/usb.h"
#include "port/port.h"

#define PCI_ADDRESS    0xcf8
#define PCI_DATA       0xcfc
#define BUS_COUNT      256
#define SLOT_COUNT     32
#define FUNCTION_COUNT 8

#define PCI_COMMAND_OFFSET 0x04
#define PCI_COMMAND_IO_SPACE (1u << 0)
#define PCI_COMMAND_MEMORY_SPACE (1u << 1)
#define PCI_COMMAND_BUS_MASTER (1u << 2)
#define UHCI_IOBAR_OFFSET 0x20
#define XHCI_MEMORY_BAR_OFFSET 0x10

// ---------------------------------------------------------------------------------------------------------------
// PCI Types
// ---------------------------------------------------------------------------------------------------------------

typedef enum Class_Code {
    CLASS_Mass_Storage_Device = 0x1,
    CLASS_Serial_Bus_Controller = 0xc,
} Class_Code;

typedef enum Mass_Storage_Device_Subclass {
    MASS_STORAGE_DEVICE_IDE_Controller = 0x1,
} Mass_Storage_Device_Subclass;

typedef enum Serial_Bus_Controller_Subclass {
    SERIAL_BUS_CONTROLLER_USB = 0x3,
} Serial_Bus_Controller_Subclass;

typedef enum Serial_Bus_USB_Controller_Program_Interface {
    SERIAL_BUS_USB_CONTROLLER_UHCI = 0x00,
    SERIAL_BUS_USB_CONTROLLER_XHCI = 0x30,
} Serial_Bus_USB_Controller_Program_Interface;

// ---------------------------------------------------------------------------------------------------------------
// PCI Interaction
// ---------------------------------------------------------------------------------------------------------------

static inline
u32 calculate_address(const u8 bus, const u8 slot, const u8 function, const u8 offset) {
    return 0x80000000 | ((u32) bus << 16) | ((u32) slot << 11) | ((u32) function << 8) | ((u32) offset & 0xfc);
}

static inline
u32 get_bar_address(const PCI_Device device, const u8 bar_offset) {
    return pci_read_u32(device.bus, device.slot, device.function, bar_offset) & 0xfffffffc;
}

static inline
b8 take_device_ownership(const PCI_Device device) {
    pci_write_u16(device.bus, device.slot, device.function, 0xc0, 0x2000);
    return true;
}

static
b8 enable_bus_master(const PCI_Device device, const u16 address_space_bit) {
    const u16 required_bits = PCI_COMMAND_BUS_MASTER | address_space_bit;
    const u16 current_configuration = pci_read_u16(device.bus, device.slot, device.function, PCI_COMMAND_OFFSET);
    if((current_configuration & required_bits) == required_bits) return true;
    pci_write_u16(device.bus, device.slot, device.function, PCI_COMMAND_OFFSET, current_configuration | required_bits);
    return (pci_read_u16(device.bus, device.slot, device.function, PCI_COMMAND_OFFSET) & required_bits) == required_bits;
}

// ---------------------------------------------------------------------------------------------------------------
// Device Initialization
// ---------------------------------------------------------------------------------------------------------------

static
void initialize_usb_devices(const HCI_Controller controller, const u32 pci_address) {
    if(!hci_initialize_controller(&controller, pci_address)) return; // This should be part of the hci allocation - if this fails, don't even allocate the controller itself

    const s32 port_capacity = hci_get_port_capacity(&controller);
    for(u8 port_idx = 0; port_idx < port_capacity; ++port_idx) {
        if(hci_reset_and_enable_port(&controller, port_idx)) {
            USB_Device *device = usb_allocate_device();
            if(device) {
                usb_initialize_device(device, controller, port_idx);
            }
        }
    }
}

static
void initialize_mass_storage_device(const PCI_Device device) {
    switch((Mass_Storage_Device_Subclass) device.subclass) {
        case MASS_STORAGE_DEVICE_IDE_Controller:
            break;
    }
}

static
void initialize_serial_bus_usb_controller(const PCI_Device device) {
    switch((Serial_Bus_USB_Controller_Program_Interface) device.interface) {
        case SERIAL_BUS_USB_CONTROLLER_UHCI: {
            const u32 pci_address = get_bar_address(device, UHCI_IOBAR_OFFSET);
            struct UHCI_Controller *uhci = uhci_allocate_controller();
            if(uhci) {
                if(take_device_ownership(device) && enable_bus_master(device, PCI_COMMAND_IO_SPACE)) {
                    initialize_usb_devices(hci_controller_from_uhci(uhci), pci_address);
                }
            }
        } break;

        case SERIAL_BUS_USB_CONTROLLER_XHCI: {
            const u32 bar = pci_read_u32(device.bus, device.slot, device.function, XHCI_MEMORY_BAR_OFFSET);
            if((bar & 1u) || !(bar & ~0xfu) || ((bar & 6u) == 4u && pci_read_u32(device.bus, device.slot, device.function, XHCI_MEMORY_BAR_OFFSET + 4) != 0)) {
                break;
            }

            const u32 pci_address = bar & ~0xfu;
            XHCI_Controller *xhci = xhci_allocate_controller();
            if(!xhci) {
                break;
            }
            if(!enable_bus_master(device, PCI_COMMAND_MEMORY_SPACE)) {
                break;
            }

            initialize_usb_devices(hci_controller_from_xhci(xhci), pci_address);
        } break;
    }
}

static
void initialize_serial_bus_controller(const PCI_Device device) {
    switch((Serial_Bus_Controller_Subclass) device.subclass) {
        case SERIAL_BUS_CONTROLLER_USB:
            initialize_serial_bus_usb_controller(device);
            break;
    }
}

static
void initialize_device(const PCI_Device device) {
    switch((Class_Code) device.class_code) {
        case CLASS_Serial_Bus_Controller:
            initialize_serial_bus_controller(device);
            break;

        case CLASS_Mass_Storage_Device:
            initialize_mass_storage_device(device);
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------

void pci_write_u16(const u8 bus, const u8 slot, const u8 function, const u8 offset, const u16 data) {
    const u32 address = calculate_address(bus, slot, function, offset);
    port_write_u32(PCI_ADDRESS, address);
    port_write_u16(PCI_DATA + (offset & 0x2), data);
}

void pci_write_u32(const u8 bus, const u8 slot, const u8 function, const u8 offset, const u32 data) {
    const u32 address = calculate_address(bus, slot, function, offset);
    port_write_u32(PCI_ADDRESS, address);
    port_write_u32(PCI_DATA, data);
}

u16 pci_read_u16(const u8 bus, const u8 slot, const u8 function, const u8 offset) {
    const u32 address = calculate_address(bus, slot, function, offset);
    port_write_u32(PCI_ADDRESS, address);
    return port_read_u16(PCI_DATA + (offset & 0x2));
}

u32 pci_read_u32(const u8 bus, const u8 slot, const u8 function, const u8 offset) {
    const u32 address = calculate_address(bus, slot, function, offset);
    port_write_u32(PCI_ADDRESS, address);
    return port_read_u32(PCI_DATA);
}

void pci_initialize(void) {
    for(u16 bus = 0; bus < BUS_COUNT; ++bus) {
        for(u16 slot = 0; slot < SLOT_COUNT; ++slot) {
            for(u16 function = 0; function < FUNCTION_COUNT; ++function) {
                const u16 vendor = pci_read_u16(bus, slot, function, 0x0);
                if(vendor == 0xffff) continue;

                const u16 device_id              = pci_read_u16(bus, slot, function, 0x2);
                const u16 class_and_subclass     = pci_read_u16(bus, slot, function, 0xa);
                const u16 interface_and_revision = pci_read_u16(bus, slot, function, 0x8);
                const u8 class_code = (class_and_subclass >> 8) & 0xff;
                const u8 subclass   = (class_and_subclass >> 0) & 0xff;
                const u8 interface  = (interface_and_revision >> 8) & 0xff;
                const u8 revision   = (interface_and_revision >> 0) & 0xff;

                const PCI_Device device = (PCI_Device) { bus, slot, function, class_code, subclass, interface, revision, vendor, device_id };
                initialize_device(device);
            }
        }
    }
}
