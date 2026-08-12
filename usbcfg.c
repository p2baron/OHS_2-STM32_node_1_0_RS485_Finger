#include "hal.h"
#include "usbcfg.h"

#ifdef HAS_USB_SERIAL

SerialUSBDriver SDU1;

static const uint8_t vcom_device_descriptor_data[18] = {
  USB_DESC_DEVICE(0x0110,
                  0x02,           /* bDeviceClass (CDC)       */
                  0x00,
                  0x00,
                  0x40,           /* bMaxPacketSize0 (64)     */
                  0x0483,         /* idVendor (ST)            */
                  0x5740,         /* idProduct                */
                  0x0200,
                  1,              /* iManufacturer            */
                  2,              /* iProduct                 */
                  3,              /* iSerialNumber            */
                  1)
};
static const USBDescriptor vcom_device_descriptor = {
  sizeof vcom_device_descriptor_data,
  vcom_device_descriptor_data
};

/* 9+9+5+5+4+5+7+9+7+7 = 67 bytes */
static const uint8_t vcom_configuration_descriptor_data[67] = {
  USB_DESC_CONFIGURATION(67, 0x02, 0x01, 0, 0xC0, 50),
  /* Interface 0: CDC Control */
  USB_DESC_INTERFACE(0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0),
  /* CDC Header Functional */
  USB_DESC_BYTE(5), USB_DESC_BYTE(0x24), USB_DESC_BYTE(0x00),
  USB_DESC_WORD(0x0110),
  /* CDC Call Management Functional */
  USB_DESC_BYTE(5), USB_DESC_BYTE(0x24), USB_DESC_BYTE(0x01),
  USB_DESC_BYTE(0x00), USB_DESC_BYTE(0x01),
  /* CDC ACM Functional */
  USB_DESC_BYTE(4), USB_DESC_BYTE(0x24), USB_DESC_BYTE(0x02),
  USB_DESC_BYTE(0x02),
  /* CDC Union Functional */
  USB_DESC_BYTE(5), USB_DESC_BYTE(0x24), USB_DESC_BYTE(0x06),
  USB_DESC_BYTE(0x00), USB_DESC_BYTE(0x01),
  /* EP2 IN interrupt */
  USB_DESC_ENDPOINT(USBD1_INTERRUPT_REQUEST_EP | 0x80, 0x03, 0x0008, 0xFF),
  /* Interface 1: CDC Data */
  USB_DESC_INTERFACE(0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00),
  /* EP1 OUT bulk */
  USB_DESC_ENDPOINT(USBD1_DATA_AVAILABLE_EP,        0x02, 0x0040, 0x00),
  /* EP1 IN bulk */
  USB_DESC_ENDPOINT(USBD1_DATA_REQUEST_EP | 0x80,   0x02, 0x0040, 0x00)
};
static const USBDescriptor vcom_configuration_descriptor = {
  sizeof vcom_configuration_descriptor_data,
  vcom_configuration_descriptor_data
};

static const uint8_t vcom_string0[] = {
  USB_DESC_BYTE(4), USB_DESC_BYTE(USB_DESCRIPTOR_STRING),
  USB_DESC_WORD(0x0409)
};
static const uint8_t vcom_string1[] = {
  USB_DESC_BYTE(14), USB_DESC_BYTE(USB_DESCRIPTOR_STRING),
  'O', 0, 'H', 0, 'S', 0, ' ', 0, 'N', 0, 'o', 0
};
static const uint8_t vcom_string2[] = {
  USB_DESC_BYTE(24), USB_DESC_BYTE(USB_DESCRIPTOR_STRING),
  'F', 0, 'i', 0, 'n', 0, 'g', 0, 'e', 0, 'r', 0,
  'N', 0, 'o', 0, 'd', 0, 'e', 0, ' ', 0
};
static const uint8_t vcom_string3[] = {
  USB_DESC_BYTE(8), USB_DESC_BYTE(USB_DESCRIPTOR_STRING),
  '0', 0, '0', 0, '1', 0
};
static const USBDescriptor vcom_strings[] = {
  {sizeof vcom_string0, vcom_string0},
  {sizeof vcom_string1, vcom_string1},
  {sizeof vcom_string2, vcom_string2},
  {sizeof vcom_string3, vcom_string3}
};

static const USBDescriptor *get_descriptor(USBDriver *usbp, uint8_t dtype,
                                           uint8_t dindex, uint16_t lang) {
  (void)usbp; (void)lang;
  switch (dtype) {
  case USB_DESCRIPTOR_DEVICE:      return &vcom_device_descriptor;
  case USB_DESCRIPTOR_CONFIGURATION: return &vcom_configuration_descriptor;
  case USB_DESCRIPTOR_STRING:
    if (dindex < 4) return &vcom_strings[dindex];
  }
  return NULL;
}

static USBInEndpointState  ep1instate;
static USBOutEndpointState ep1outstate;
static const USBEndpointConfig ep1config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  sduDataTransmitted,
  sduDataReceived,
  0x0040, 0x0040,
  &ep1instate, &ep1outstate,
  1, NULL
};

static USBInEndpointState ep2instate;
static const USBEndpointConfig ep2config = {
  USB_EP_MODE_TYPE_INTR,
  NULL,
  sduInterruptTransmitted,
  NULL,
  0x0010, 0x0000,
  &ep2instate, NULL,
  1, NULL
};

static void usb_event(USBDriver *usbp, usbevent_t event) {
  switch (event) {
  case USB_EVENT_CONFIGURED:
    chSysLockFromISR();
    if (usbp->state == USB_ACTIVE) {
      usbInitEndpointI(usbp, USBD1_DATA_REQUEST_EP, &ep1config);
      usbInitEndpointI(usbp, USBD1_INTERRUPT_REQUEST_EP, &ep2config);
      sduConfigureHookI(&SDU1);
    } else if (usbp->state == USB_SELECTED) {
      usbDisableEndpointsI(usbp);
    }
    chSysUnlockFromISR();
    return;
  case USB_EVENT_RESET:
  case USB_EVENT_UNCONFIGURED:
  case USB_EVENT_SUSPEND:
    chSysLockFromISR();
    sduSuspendHookI(&SDU1);
    chSysUnlockFromISR();
    return;
  case USB_EVENT_WAKEUP:
    chSysLockFromISR();
    sduWakeupHookI(&SDU1);
    chSysUnlockFromISR();
    return;
  default:
    return;
  }
}

static bool requests_hook(USBDriver *usbp) {
  return sduRequestsHook(usbp);
}

static void sof_handler(USBDriver *usbp) {
  (void)usbp;
  osalSysLockFromISR();
  sduSOFHookI(&SDU1);
  osalSysUnlockFromISR();
}

const USBConfig usbcfg = {
  usb_event,
  get_descriptor,
  requests_hook,
  sof_handler
};

const SerialUSBConfig serusbcfg = {
  &USBD1,
  USBD1_DATA_REQUEST_EP,
  USBD1_DATA_AVAILABLE_EP,
  USBD1_INTERRUPT_REQUEST_EP
};

#endif /* HAS_USB_SERIAL */
