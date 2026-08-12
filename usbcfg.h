#pragma once
#include "hal.h"

#define USBD1_DATA_REQUEST_EP       1U
#define USBD1_DATA_AVAILABLE_EP     1U
#define USBD1_INTERRUPT_REQUEST_EP  2U

extern SerialUSBDriver SDU1;
extern const USBConfig usbcfg;
extern const SerialUSBConfig serusbcfg;
