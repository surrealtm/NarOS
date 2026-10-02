#include "pci.h"
#include "output.h"
#include "port/port.h"

#define PCI_ADDRESS    0xcf8
#define PCI_DATA       0xcfc
#define BUS_COUNT      256
#define SLOT_COUNT     32
#define FUNCTION_COUNT 8

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
u16 read_configuration_word(const u8 bus, const u8 slot, const u8 function, const u8 offset) {
    const u32 address = calculate_address(bus, slot, function, offset);
    port_write_u32(PCI_ADDRESS, address);
    const u32 data = port_read_u32(PCI_DATA);
    return data >> ((offset & 0x2) * 8) & 0xffff;
}

static
void set_bar_address(const u8 bus, const u8 slot, const u8 function, const u8 bar_number, const u32 result) {
    const u32 first_part = result & 0xffff;
    const u32 second_part = (result >> 16) & 0xffff;
    write_configuration_word(bus, slot, function, bar_number + 0, first_part);
    write_configuration_word(bus, slot, function, bar_number + 2, second_part);
}

static
u32 get_bar_address(const u8 bus, const u8 slot, const u8 function, const u8 bar_number) {
    const u32 first_part = read_configuration_word(bus, slot, function, bar_number + 0);
    const u32 second_part = read_configuration_word(bus, slot, function, bar_number + 2);
    return first_part | (second_part << 16);
}

static
void initialize_mass_storage_device(const Mass_Storage_Device_Subclass subclass) {
    switch(subclass) {
        case MASS_STORAGE_DEVICE_IDE_Controller:
            os_output_print(str8("Found an IDE Controller!\n"));
            break;
    }
}

static
void initialize_serial_bus_usb_controller(const Serial_Bus_USB_Controller_Program_Interface interface) {
    switch(interface) {
        case SERIAL_BUS_USB_CONTROLLER_UHCI:
            os_output_print(str8("Found an UHCI Controller!\n"));
            break;
    }
}

static
void initialize_serial_bus_controller(const Serial_Bus_Controller_Subclass subclass, const u8 interface) {
    switch(subclass) {
        case SERIAL_BUS_CONTROLLER_USB:
            initialize_serial_bus_usb_controller(interface);
            break;
    }
}

void pci_initialize(void) {
    (void) set_bar_address;
    (void) get_bar_address;

    for(u16 bus = 0; bus < BUS_COUNT; ++bus) {
        for(u16 slot = 0; slot < SLOT_COUNT; ++slot) {
            for(u16 function = 0; function < FUNCTION_COUNT; ++function) {
                const u16 vendor = read_configuration_word(bus, slot, function, 0x0);
                const u16 device = read_configuration_word(bus, slot, function, 0x2);
                if(vendor == 0xffff) continue;

                const u16 class_and_subclass    = read_configuration_word(bus, slot, function, 0xa);
                const u8 interface_and_revision = read_configuration_word(bus, slot, function, 0x8);
                const u8 class_code = (class_and_subclass >> 8) & 0xff;
                const u8 subclass   = (class_and_subclass >> 0) & 0xff;
                const u8 interface  = (interface_and_revision >> 8) & 0xff;

                (void) device;

                switch(class_code) {
                    case CLASS_Serial_Bus_Controller:
                        initialize_serial_bus_controller(subclass, interface);
                        break;
                    case CLASS_Mass_Storage_Device:
                        initialize_mass_storage_device(subclass);
                        break;
                }
            }
        }
    }
}
