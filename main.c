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
//#define HAS_NFC    // Enable via UDEFS: -DHAS_NFC (requires PN532 on I2C1, IRQ on PB5)

// Sanity checks
#if defined(HAS_RADIO) && defined(HAS_RS485)
#error "Should not define both HAS_RADIO and HAS_RS485"
#endif

// This node settings
#define VERSION         104    // Version of EEPROM struct
#define SENSOR_DELAY    600    // In seconds, 600 = 10 minutes
#define ELEMENTS        1      // How many elements this node has
#define FINGERS_SIZE    20     // Must match gateway FINGERS_SIZE

// Constants
#define REG_LEN         21   // Size of one conf. element
#define NODE_NAME_SIZE  16   // As defined on gateway
#define MSG_REPEAT      3    // Repeat sending
#define DUMMY_NO_VALUE  0xFF // Dummy value for no value
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
#ifdef HAS_NFC
binary_semaphore_t NFCSem;
#endif

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
uint16_t location = DUMMY_NO_VALUE, confidence = 0;
uint8_t fingerCount;
#define FINGER_PASSED           4
#define FINGER_PASSED_COOLDOWN -4
int8_t fingerPassed;
#endif

// Pending fingerprint sync: addresses that failed during the last enrollment sync
typedef struct {
  uint16_t location; // Template location; 0xFFFF = empty slot
  uint8_t  failMask; // Bit N set = address (N+1) needs retry
} pending_sync_t;
#define PENDING_SYNC_SLOTS 8

// Configuration struct
struct config_t {
  uint16_t version;
  uint8_t reg[REG_LEN * ELEMENTS]; // REG_LEN * #, number of elements on this node
  pending_sync_t pendingSync[PENDING_SYNC_SLOTS];
  uint16_t fpId[FINGERS_SIZE];     // gateway-assigned ID per slot; 0 = no template
} conf;

// RTTTL thread variables
static mailbox_t rtttlMailbox;
static msg_t rtttlMailboxBuffer[5]; // Storage for one message (pointer)

// OHS includes
#include "ohs_peripheral.h"
#ifdef HAS_FINGERPRINT
#include "R503.h"
#endif
#ifdef HAS_NFC
#include "ohs_nfc_pn532.h"
#endif
#ifdef HAS_DISPLAY
#include "ohs_ssd1309.h"
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
#ifdef HAS_DISPLAY
#include "ohs_th_display.h"
#endif
/*
 * Application entry point.
 */
int main(void) {
  halInit();
  chSysInit();
  
  // Semaphores
  chBSemObjectInit(&R503Sem, false);
#ifdef HAS_NFC
  chBSemObjectInit(&NFCSem, true);  /* taken=true: wait blocks until IRQ fires */
#endif
  
  // LED
  palSetPadMode(GPIOC, GPIOC_LED, PAL_MODE_OUTPUT_PUSHPULL);

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
#ifdef HAS_NFC
  palSetPadMode(GPIOB, GPIOB_SCL1, PAL_MODE_STM32_ALTERNATE_OPENDRAIN);
  palSetPadMode(GPIOB, GPIOB_SDA1, PAL_MODE_STM32_ALTERNATE_OPENDRAIN);
  palSetPadMode(GPIOB, GPIOB_PIN5, PAL_MODE_INPUT_PULLUP);
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
#ifdef HAS_NFC
  i2cStart(&I2CD1, &i2c1cfg);
  palSetLineCallback(PAL_LINE(GPIOB, GPIOB_PIN5), nfcIrqCallback, NULL);
  palEnableLineEvent(PAL_LINE(GPIOB, GPIOB_PIN5), PAL_EVENT_MODE_FALLING_EDGE);
#endif
#ifdef HAS_DISPLAY
  /* I2C bus recovery before starting the peripheral.
   * Uses a busy-wait loop (no scheduler needed) to generate 9 SCL pulses,
   * releasing any SDA stuck-low condition from a previous session.
   * Required to work around STM32F103 I2C BUSY-flag silicon bug. */
  { volatile uint32_t d;
    palSetPadMode(GPIOB, GPIOB_SCL1, PAL_MODE_OUTPUT_OPENDRAIN);
    palSetPadMode(GPIOB, GPIOB_SDA1, PAL_MODE_OUTPUT_OPENDRAIN);
    palSetPad(GPIOB, GPIOB_SDA1); // SDA high
    for (uint8_t bi = 0; bi < 9; bi++) {
      palClearPad(GPIOB, GPIOB_SCL1); for (d = 720; d; d--); // ~10µs@72MHz
      palSetPad(GPIOB,  GPIOB_SCL1); for (d = 720; d; d--);
    }
    palClearPad(GPIOB, GPIOB_SDA1); for (d = 720; d; d--); // STOP: SDA rises while SCL high
    palSetPad(GPIOB,  GPIOB_SDA1); for (d = 720; d; d--);
    palSetPadMode(GPIOB, GPIOB_SCL1, PAL_MODE_STM32_ALTERNATE_OPENDRAIN);
    palSetPadMode(GPIOB, GPIOB_SDA1, PAL_MODE_STM32_ALTERNATE_OPENDRAIN);
  }
  i2cStart(&I2CD1, &i2c1cfg);
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
#ifdef HAS_DISPLAY
  chThdCreateStatic(waDisplayThread, sizeof(waDisplayThread), LOWPRIO, DisplayThread, NULL);
#endif

  // Initialize tone generation
  toneInit();
  // Initialize fingerprint sensor
  while (R503Start() != R503_OK) {
#ifdef HAS_SERIAL1
    DBG("FP Init error!\r\n");
#endif
    chThdSleepMilliseconds(2000);
  }
#ifdef HAS_NFC
  while (pn532Init(&I2CD1) != PN532_OK) {
    DBG("NFC Init error!\r\n");
    chThdSleepMilliseconds(2000);
  }
  DBG("NFC OK\r\n");
#endif

  // Register this node
  sendConf();

  // Initialize node state
  setNodeMode(MODE_DISARMED);

  // Initialize counters
  int8_t count = -1;
  uint32_t lastBeepSec = 0; // RTC second when synchronized beep last fired
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

    /* Synchronized beep — fires at even RTC seconds so both nodes beep together.
     * Both nodes share the same clock via gateway 'T' beacons. */
    {
      RTCDateTime _ts;
      rtcGetTime(&RTCD1, &_ts);
      uint32_t nowSec = convertRTCDateTimeToUnixSecond(&_ts);
      if ((nowSec % 2 == 0) && (nowSec != lastBeepSec)) {
        lastBeepSec = nowSec;
        switch (nodeState.mode) {
          case MODE_ALARM:  chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_ALARM,  TIME_IMMEDIATE); break;
          case MODE_ARMING: chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_ARMING, TIME_IMMEDIATE); break;
          case MODE_AUTH_1: chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_AUTH1,  TIME_IMMEDIATE); break;
          case MODE_AUTH_2: chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_AUTH2,  TIME_IMMEDIATE); break;
          case MODE_AUTH_3: chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_AUTH3,  TIME_IMMEDIATE); break;
          default: break;
        }
      }
    }

    if (count > 0) {
      count--;
    } else if (count == 0) {
      // Set LED and song according to mode
      chBSemWait(&R503Sem);
      switch (nodeState.mode) {
        case MODE_ALARM:
          R503SetAuraLED(aLEDModeFlash, aLEDRed, 50, 4);
          break;
        case MODE_AUTH_1:
          R503SetAuraLED(aLEDModeFlash, aLEDRed, 100, 1);
          break;
        case MODE_AUTH_2:
          R503SetAuraLED(aLEDModeFlash, aLEDRed, 150, 1);
          break;
        case MODE_AUTH_3:
          R503SetAuraLED(aLEDModeFlash, aLEDRed, 200, 1);
          break;
        case MODE_ARMING:
          R503SetAuraLED(aLEDModeBreathing, aLEDBlue, 50, 1);
          break;
        case MODE_ARMED_AWAY:
          R503SetAuraLED(aLEDModeBreathing, aLEDRed, 150, 0); // medium breathing red (infinite)
          break;
        case MODE_DISARMED: {
          // Solid white if connected, blink white if gateway not responding
          bool gwOk = (lastGwContact != 0) &&
                      (chTimeDiffX(lastGwContact, chVTGetSystemTimeX()) < GW_TIMEOUT_TICKS);
          if (gwOk)
            R503SetAuraLED(aLEDModeON, aLEDWhite, 255, 0);   // permanent white
          else
            R503SetAuraLED(aLEDModeFlash, aLEDWhite, 150, 0); // blink white = no contact
          break;
        }
        case MODE_ARMED_HOME:
          R503SetAuraLED(aLEDModeBreathing, aLEDPurple, 50, 0); // slow breathing purple (infinite)
          break;
        case MODE_UNINITIALIZED:
          R503SetAuraLED(aLEDModeFlash, aLEDWhite, 150, 0); // medium blink white (not connected)
          break;
        default:
          break;
      }
      chBSemSignal(&R503Sem);
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

    // Read fingerprint — hold R503Sem to prevent concurrent access from RS485 thread
    chBSemWait(&R503Sem);
    resp = R503TakeImage();
    if ((resp == R503_OK) && (fingerPassed >= 0)) {
      resp = R503ExtractFeatures(FINGERPRINT_CHAR_BUFFER);
      if (resp != R503_OK) {
        chBSemSignal(&R503Sem);
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
    chBSemSignal(&R503Sem);

#ifdef HAS_NFC
    {
      uint8_t nfcUid[7]; uint8_t nfcLen = 0;
      bool nfcGotCard = (pn532StartDetect(&I2CD1) == PN532_OK) &&
                        (chBSemWaitTimeout(&NFCSem, TIME_MS2I(300)) == MSG_OK) &&
                        (pn532ReadUID(&I2CD1, nfcUid, &nfcLen) == PN532_OK);

      if (nfcGotCard) {
        DBG("NFC: card UID len=%u\r\n", nfcLen);
        sendNFCCard(0, 0, nfcUid, nfcLen);
        chThdSleepMilliseconds(500); /* debounce — prevent duplicate sends */
      }
    }
#endif

    // Reset finger passed counter if no finger was found
    if (fingerCount == 0) {
      fingerPassed = -1;
    }

    // Check if authentication passed, if armed away or home, only one finger is needed
    if ((fingerCount) &&
        ((fingerPassed > FINGER_PASSED) || (nodeState.mode == MODE_ARMED_AWAY) || (nodeState.mode == MODE_ARMED_HOME))) {
      sendFinger(0, (fingerCount == 1) ? 0 : 1, location);
      fingerCount = 0;
      fingerPassed = FINGER_PASSED_COOLDOWN;
      location = DUMMY_NO_VALUE;
    }
  }
}
