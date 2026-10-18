#pragma once

typedef enum HCI_Device_Speed {
    HCI_DEVICE_SPEED_Full  = 0,
    HCI_DEVICE_SPEED_Low   = 1,
    HCI_DEVICE_SPEED_Error = 2,
} HCI_Device_Speed;

typedef b8(*HCI_Initialize_Controller)(void *controller_impl, const u32 pci_adddress);
typedef b8(*HCI_Reset_And_Enable_Port)(void *controller_impl, const u8 port_idx);

typedef struct HCI_Controller {
    void *controller_impl;
    HCI_Initialize_Controller initialize_controller;
    HCI_Reset_And_Enable_Port reset_and_enable_port;
} HCI_Controller;
