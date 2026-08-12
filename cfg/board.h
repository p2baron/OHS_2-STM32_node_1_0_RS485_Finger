/*
 * cfg/board.h — project-level board override layer
 *
 * Sits first in INCDIR (ahead of the ChibiOS board directory) and pulls in
 * the real OHS_STM32_F103_MINI board.h via #include_next, then overrides
 * the USB bus-connect/disconnect macros to drive PB7 (D+ pullup).
 *
 * This keeps the USB wiring change in the firmware repo — no ChibiOS fork
 * needed.
 */
#ifndef CFG_BOARD_OVERRIDE_H_
#define CFG_BOARD_OVERRIDE_H_

#include_next "board.h"   /* pull in the real ChibiOS board.h */

#ifdef HAS_USB_SERIAL
/* PB7 drives the D+ pullup resistor; high = host sees device as connected. */
#ifndef GPIOB_USB_PULLUP
#define GPIOB_USB_PULLUP 7U
#endif
#undef  usb_lld_connect_bus
#undef  usb_lld_disconnect_bus
#define usb_lld_connect_bus(usbp)    palSetPad(GPIOB, GPIOB_USB_PULLUP)
#define usb_lld_disconnect_bus(usbp) palClearPad(GPIOB, GPIOB_USB_PULLUP)
#endif /* HAS_USB_SERIAL */

#endif /* CFG_BOARD_OVERRIDE_H_ */
