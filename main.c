/*
 * OHS node - finger print authentication
 * vysocan (c) 2025-26
 *
 * This is the firmware for OHS node with fingerprint sensor.
 * Using R503 fingerprint sensor.
 *
 */
// Node feature defines
//#define HAS_RADIO
#define HAS_RS485
#define HAS_FINGERPRINT
#define HAS_SERIAL1 // Console UART

// Sanity checks
#if defined(HAS_RADIO) && defined(HAS_RS485)
#error "Should not define both HAS_RADIO and HAS_RS485"
#endif

// This node settings
#define VERSION         100    // Version of EEPROM struct
#define SENSOR_DELAY    600    // In seconds, 600 = 10 minutes
#define ELEMENTS        1      // How many elements this node has

// Constants
#define REG_LEN         21   // Size of one conf. element
#define NODE_NAME_SIZE  16   // As defined on gateway
#define MSG_REPEAT      3    // Repeat sending
// Radio
#ifdef HAS_RADIO
#define GATEWAYID       1    // Radio Gateway
#endif
// RS485
#ifdef HAS_RS485
#define GATEWAYID       0    // RS485 Gateway
#endif
// Songs :] - RTTTL format, keep it short or raise count in main loop
#define SONG_OK           ":d=16,o=5,b=120:c,g,e,c6"
#define SONG_ERROR        ":d=16,o=5,b=120:c6,g,eb,c"
#define SONG_TICK         ":d=16,o=5,b=300:16c6"
#define SONG_ARMING       ":d=16,o=5,b=80:c,e,c"
#define SONG_ALARM        ":d=16,o=5,b=80:c,p,c,p,c,p,c"
#define SONG_AUTH1        ":d=16,o=5,b=120:c,p,c,p,c"
#define SONG_AUTH2        ":d=16,o=5,b=120:c,p,c"
#define SONG_AUTH3        ":d=16,o=5,b=120:c"
#define SONG_ARM_REJECTED ":d=16,o=4,b=80:c#,p,c#,p,c#"

// ChibiOS override
#define SERIAL_BUFFERS_SIZE 128

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ch.h"
#include "hal.h"
#include "chmboxes.h"
//#include "shell.h"
#include "chprintf.h"
#include "board.h"

// Define debug console
#ifdef HAS_SERIAL1
BaseSequentialStream *console = (BaseSequentialStream*) &SD1;
#define DBG(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG(...)
#endif

binary_semaphore_t R503Sem;

// RFM69
#ifdef HAS_RADIO
#include "rfm69.h"
#endif

// RTC related
static RTCDateTime timespec;
// Fingerprint
#ifdef HAS_FINGERPRINT
#define FINGERPRINT_CHAR_BUFFER 1
#define MAX_FINGERPRINT_SIZE 1536
uint16_t fingerSize = 0;
uint8_t finger[MAX_FINGERPRINT_SIZE];
uint8_t compressed[MAX_FINGERPRINT_SIZE + 4 + (MAX_FINGERPRINT_SIZE / 2)];
uint16_t location, confidence;
uint8_t fingerCount;
#define FINGER_PASSED           4
#define FINGER_PASSED_COOLDOWN -4
int8_t fingerPassed;
#endif

// Configuration struct
struct config_t {
  uint16_t version;
  uint8_t reg[REG_LEN * ELEMENTS]; // REG_LEN * #, number of elements on this node
} conf;

// RTTTL thread variables
static mailbox_t rtttlMailbox;
static msg_t rtttlMailboxBuffer[5]; // Storage for one message (pointer)

// OHS includes
#include "ohs_peripheral.h"
#ifdef HAS_FINGERPRINT
#include "R503.h"
#endif
#include "rle.h"
#include "reg_defaults.h"
#include "ohs_func.h"
#include "eeprom2flash.h"
#include "tone.h"
#include "date_time.h"

// Global variables
authMode_t currentMode = MODE_UNINITIALIZED;
uint8_t resp;

// Thread handling
#ifdef HAS_RADIO
#include "ohs_th_radio.h"
uint8_t msgOut[REG_LEN]; // size of REG_LEN, or larger for sensor msg if longer than REG_LEN size
#endif
#ifdef HAS_RS485
RS485Msg_t msgOut;
#include "ohs_th_rs485.h"
#endif
// other threads
#include "ohs_th_service.h"
#include "ohs_th_rtttl.h"
#include "ohs_th_blinker.h"
/*
 * Application entry point.
 */
int main(void) {
  halInit();
  chSysInit();
  // Semaphores
  chBSemObjectInit(&R503Sem, false);
  /*
   * I/O pins setup.
   */
#ifdef HAS_RADIO
  palSetPadMode(GPIOA, GPIOA_RADIO_INT, PAL_MODE_INPUT);
  palSetPadMode(GPIOA, GPIOA_SCK, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOA, GPIOA_MISO, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOA, GPIOA_MOSI, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOA, GPIOA_SS, PAL_MODE_OUTPUT_PUSHPULL);
#endif
#ifdef HAS_SERIAL1
  palSetPadMode(GPIOA, GPIOA_TX1, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOA, GPIOA_RX1, PAL_MODE_INPUT);
#endif
#ifdef HAS_FINGERPRINT
  palSetPadMode(GPIOA, GPIOA_TX2, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOA, GPIOA_RX2, PAL_MODE_INPUT);
#endif
#ifdef HAS_RS485
  palSetPadMode(GPIOB, GPIOB_TX3, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOB, GPIOB_RX3, PAL_MODE_INPUT);
  palSetPadMode(rs485cfg.deport, rs485cfg.depad, PAL_MODE_OUTPUT_PUSHPULL);
#endif
  /*
   * Definitions initialization.
   */
#ifdef HAS_SERIAL1
  sdStart(&SD1, &serialCfg);
  DBG("\r\nOHS node start\r\n");
#endif
#ifdef HAS_RS485
  rs485Start(&RS485D3, &rs485cfg);
  DBG("RS485 timeout: %d(uS)/%d(tick)\r\n", RS485D3.oneByteTimeUS, RS485D3.oneByteTimeI);
#endif
#ifdef HAS_RADIO
  // SPI
  spiStart(&SPID1, &spi1cfg);
  // RFM69
  rfm69Start(&rfm69cfg);
  rfm69SetHighPower(true); // long range version
  rfm69AutoPower(-75);
#endif
#ifdef HAS_FINGERPRINT
  R503Init(&SD2, 0xFFFFFFFF, 0);
#endif

  // mailboxes
  chMBObjectInit(&rtttlMailbox, rtttlMailboxBuffer, 5);

  // Read configuration
  readFromFlash(&conf, sizeof(conf));
  DBG("Flash EEPROM start: 0x%08x\r\n", FLASH_EE_REGION);
  if (conf.version != VERSION) {
    setDefault();
    writeToFlash(&conf, sizeof(conf));
  }
  /*
   * Create the threads.
   */
#ifdef HAS_RADIO
  chThdCreateStatic(waRadioThread, sizeof(waRadioThread), NORMALPRIO, RadioThread, (void*)"radio");
#endif
#ifdef HAS_RS485
  chThdCreateStatic(waRS485Thread, sizeof(waRS485Thread), NORMALPRIO, RS485Thread, (void*) "rs485");
#endif
  chThdCreateStatic(waServiceThread, sizeof(waServiceThread), NORMALPRIO - 1, ServiceThread, (void*) "service");
  chThdCreateStatic(waBlinkerThread, sizeof(waBlinkerThread), NORMALPRIO - 2, BlinkerThread, (void*) "blinker");
  chThdCreateStatic(waRTTTLThread, sizeof(waRTTTLThread), NORMALPRIO, RTTTLThread, (void*) "rtttl");

  // Initialize tone generation
  toneInit();
  // Initialize fingerprint sensor
  while (R503Start() != R503_OK) {
#ifdef HAS_SERIAL1
    DBG("FP Init error!\r\n");
#endif
    chThdSleepMilliseconds(2000);
  }

  // Register this node
  sendConf();

  // Initialize node state
  setNodeMode(MODE_DISARMED);

  // Initialize counters
  int8_t count = -1;
  fingerCount = 0;
  fingerPassed = 0;
  // Main loop
  while (true) {
    chThdSleepMilliseconds(200);

    // Check if mode changed
    if (nodeState.mode != currentMode) {
      currentMode = nodeState.mode;
      DBG("Mode changed to %d\r\n", nodeState.mode);
    }
    if (count > 0) {
      count--;
    } else if (count == 0) {
      // Set LED according to mode  
      switch (nodeState.mode) {
        case MODE_ALARM:
          R503SetAuraLED(aLEDModeFlash, aLEDRed, 50, 4);
          chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_ALARM, TIME_IMMEDIATE);
          break;
        case MODE_AUTH_1:
          R503SetAuraLED(aLEDModeFlash, aLEDRed, 100, 1);
          chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_AUTH1, TIME_IMMEDIATE);
          break;
        case MODE_AUTH_2:
          R503SetAuraLED(aLEDModeFlash, aLEDRed, 150, 1);
          chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_AUTH2, TIME_IMMEDIATE);
          break;
        case MODE_AUTH_3:
          R503SetAuraLED(aLEDModeFlash, aLEDRed, 200, 1);
          chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_AUTH3, TIME_IMMEDIATE);
          break;
        case MODE_ARMING:
          R503SetAuraLED(aLEDModeBreathing, aLEDBlue, 50, 1);
          chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_ARMING, TIME_IMMEDIATE);
          break;
        case MODE_ARMED_AWAY:
          R503SetAuraLED(aLEDModeBreathing, aLEDRed, 50, 2);
          break;
        case MODE_DISARMED:
          R503SetAuraLED(aLEDModeBreathing, aLEDGreen, 50, 1);
          break;
        case MODE_ARMED_HOME:
          R503SetAuraLED(aLEDModeBreathing, aLEDYellow, 50, 2);
          break;
        default:
          break;
      }
      count--;
    } else {
      // Reset count
      count = 10;
    }

    // Check if authentication is allowed
    if (!isAuthAllowed()) {
      continue;
    }

    // Increment finger passed counter
    fingerPassed++;

    // Read fingerprint
    resp = R503TakeImage();
    if ((resp == R503_OK) && (fingerPassed >= 0)) {
      resp = R503ExtractFeatures(FINGERPRINT_CHAR_BUFFER);
      if (resp != R503_OK) {
        continue;
      } else {
        resp = R503SearchFinger(FINGERPRINT_CHAR_BUFFER, &location, &confidence);
        if (resp == R503_OK) {
          chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_OK, TIME_IMMEDIATE);
          R503SetAuraLED(aLEDModeBreathing, aLEDGreen, 50, 2);
          DBG(" >> Found finger, ID: %d, confidence; %d\r\n", location, confidence);
          fingerCount++;
        } else {
          chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_ERROR, TIME_IMMEDIATE);
          R503SetAuraLED(aLEDModeBreathing, aLEDRed, 50, 2);
        }
      }
    }

    // Reset finger passed counter if no finger was found
    if (fingerCount == 0) {
      fingerPassed = -1; 
    }

    // Check if authentication passed, if armed away or home, only one finger is needed
    if ((fingerCount) && 
        ((fingerPassed > FINGER_PASSED) || (nodeState.mode == MODE_ARMED_AWAY) || (nodeState.mode == MODE_ARMED_HOME))) {
      // Send authentication message
      sendFinger(0, (fingerCount == 1) ? 0 : 1, location);
      fingerCount = 0;
      fingerPassed = FINGER_PASSED_COOLDOWN;
    }
  }
}
