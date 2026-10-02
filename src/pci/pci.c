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

static inline
u32 calculate_address(const u8 bus, const u8 slot, const u8 function, const u8 offset) {
    return 0x80000000 | ((u32) bus << 16) | ((u32) slot << 11) | ((u32) function << 8) | ((u32) offset & 0xfc);
}

static
void write_configuration_word(const u8 bus, const u8 slot, const u8 function, const u8 offset, const u32 data) {
    const u32 address = calculate_address(bus, slot, function, offset);
    port_write_u32(PCI_ADDRESS, address);
    port_write_u32(PCI_DATA, data);
}

static
u32 read_configuration_word(const u8 bus, const u8 slot, const u8 function, const u8 offset) {
    const u32 address = calculate_address(bus, slot, function, offset);
    port_write_u32(PCI_ADDRESS, address);
    return port_read_u32(PCI_DATA) >> ((offset & 0x2) * 8) & 0xffff;
}

void pci_set_bar_address(const u8 bus, const u8 slot, const u8 function, const u8 bar_number, const u32 value) {
    write_configuration_word(bus, slot, function, bar_number + 0, value);
}

u32 pci_get_bar_address(const u8 bus, const u8 slot, const u8 function, const u8 bar_number) {
    return read_configuration_word(bus, slot, function, bar_number);
}

static inline
u32 get_io_configuration(const u8 bus, const u8 slot, const u8 function) {
    return pci_get_bar_address(bus, slot, function, UHCI_IOBAR_NUMBER) & 0xfffc;
}

static inline
void set_io_configuration(const u8 bus, const u8 slot, const u8 function, const u32 configuration) {
    pci_set_bar_address(bus, slot, function, UHCI_IOBAR_NUMBER, configuration);
}

static inline
void take_device_ownership(const u8 bus, const u8 slot, const u8 function) {
    write_configuration_word(bus, slot, function, 0xc0, 0x2000);
}

static
b8 maybe_enable_bus_mastering(const u8 bus, const u8 slot, const u8 function) {
    const u32 bus_mastering_mask = 0x04;
    const u32 current_configuration = get_io_configuration(bus, slot, function);
    if((current_configuration & bus_mastering_mask) != 0) return true; // Already set up
    const u32 desired_configuration = current_configuration | bus_mastering_mask;
    set_io_configuration(bus, slot, function, desired_configuration);
    return (get_io_configuration(bus, slot, function) & bus_mastering_mask) != 0;
}

static
void initialize_serial_bus_usb_controller_uhci(const PCI_Device device) {
    take_device_ownership(device.bus, device.slot, device.function);
    maybe_enable_bus_mastering(device.bus, device.slot, device.function);
    uhci_initialize_device(pci_get_bar_address(device.bus, device.slot, device.function, UHCI_IOBAR_OFFSET));
}

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
        case SERIAL_BUS_USB_CONTROLLER_UHCI:
            initialize_serial_bus_usb_controller_uhci(device);
            break;
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

void pci_initialize(void) {
    for(u16 bus = 0; bus < BUS_COUNT; ++bus) {
        for(u16 slot = 0; slot < SLOT_COUNT; ++slot) {
            for(u16 function = 0; function < FUNCTION_COUNT; ++function) {
                const u16 vendor = read_configuration_word(bus, slot, function, 0x0);
                if(vendor == 0xffff) continue;

                const u16 device_id              = read_configuration_word(bus, slot, function, 0x2);
                const u16 class_and_subclass     = read_configuration_word(bus, slot, function, 0xa);
                const u16 interface_and_revision = read_configuration_word(bus, slot, function, 0x8);
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
