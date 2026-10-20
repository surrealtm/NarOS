#pragma once

#include "base.h"
#include "hci.h"

struct UHCI_Controller;

/**
 * Returns an uninitialized UHCI controller structure, or null if the capacity for controllers was reached
 */
struct UHCI_Controller *uhci_allocate_controller(void);

/**
 * Creates an HCI_Controller structure for the given UHCI_Controller.
 * The UHCI_Controller must remain alive for the lifetime of the HCI_Controller.
 */
HCI_Controller hci_controller_from_uhci(struct UHCI_Controller *uhci);

