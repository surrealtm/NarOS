#pragma once

#include "base.h"
#include "hci.h"

struct XHCI_Controller;

struct XHCI_Controller *xhci_allocate_controller(void);
HCI_Controller hci_controller_from_xhci(struct XHCI_Controller *xhci);
