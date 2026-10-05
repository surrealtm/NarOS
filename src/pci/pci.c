#include "pci.h"
#include "uhci.h"
#include "output.h"
#include "port/port.h"

#define PCI_ADDRESS    0xcf8
#define PCI_DATA       0xcfc
#define BUS_COUNT      256
#define SLOT_COUNT     32
#define FUNCTION_COUNT 8

#define UHCI_IOBAR_NUMBER 0x4
#define UHCI_IOBAR_OFFSET 0x20

// ---------------------------------------------------------------------------------------------------------------
// Connected Devices
// ---------------------------------------------------------------------------------------------------------------

#define MAX_UHCI_CONTROLLERS 1

typedef struct Connected_Devices {
    UHCI_Controller uhci[MAX_UHCI_CONTROLLERS];
    u32 uhci_count;
} Connected_Devices;

static Connected_Devices connected_devices = { 0 };

static
UHCI_Controller *allocate_uhci_controller(void) {
    if(connected_devices.uhci_count == MAX_UHCI_CONTROLLERS) {
        return null;
    }

    UHCI_Controller *pointer = &connected_devices.uhci[connected_devices.uhci_count++];
    set_memory(pointer, 0, sizeof(UHCI_Controller));
    return pointer;
}

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
    SERIAL_BUS_USB_CONTROLLER_UHCI = 0x0,
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
void take_device_ownership(const PCI_Device device) {
    pci_write_u16(device.bus, device.slot, device.function, 0xc0, 0x2000);
}

static
b8 enable_bus_master(const PCI_Device device) {
    const u16 bus_master_mask = (1 << 2) | (1 << 0); // Enable bus master and IO decoding
    const u16 current_configuration = pci_read_u16(device.bus, device.slot, device.function, UHCI_IOBAR_NUMBER);
    if((current_configuration & bus_master_mask) == bus_master_mask) return true; // Already set up
    const u16 desired_configuration = current_configuration | bus_master_mask;
    pci_write_u16(device.bus, device.slot, device.function, UHCI_IOBAR_NUMBER, desired_configuration);
    return (pci_read_u16(device.bus, device.slot, device.function, UHCI_IOBAR_NUMBER) & bus_master_mask) == bus_master_mask;
}

// ---------------------------------------------------------------------------------------------------------------
// Device Initialization
// ---------------------------------------------------------------------------------------------------------------

static
void initialize_mass_storage_device(const PCI_Device device) {
    switch(device.subclass) {
        case MASS_STORAGE_DEVICE_IDE_Controller:
            break;
    }
}

static
void initialize_serial_bus_usb_controller(const PCI_Device device) {
    switch(device.interface) {
        case SERIAL_BUS_USB_CONTROLLER_UHCI: {
            UHCI_Controller *controller = allocate_uhci_controller();
            if(controller) {
                take_device_ownership(device);
                const b8 success = enable_bus_master(device) && uhci_initialize_controller(controller, get_bar_address(device, UHCI_IOBAR_OFFSET));
                if(success) {
                    os_output_print(str8("Successfully initialized an UHCI device.\n"));
                    if(uhci_is_port_connected(controller, 0)) os_output_print(str8("Port 0 is connected\n"));
                    if(uhci_is_port_connected(controller, 1)) os_output_print(str8("Port 1 is connected\n"));
                } else {
                    os_output_print(str8("Failed to initialize an UHCI device.\n"));
                }
            }
        } break;
    }
}

static
void initialize_serial_bus_controller(const PCI_Device device) {
    switch(device.subclass) {
        case SERIAL_BUS_CONTROLLER_USB:
            initialize_serial_bus_usb_controller(device);
            break;
    }
}

static
void initialize_device(const PCI_Device device) {
    switch(device.class_code) {
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
