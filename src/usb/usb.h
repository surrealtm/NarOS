#pragma once

#include "base.h"

struct UHCI_Controller;

b8 usb_initialize_device(struct UHCI_Controller *uhci);
