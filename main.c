/*
 * OHS node - finger print authentication
 * vysocan 2025
 *
 *
 */
// Node definitions
#define HAS_SERIAL1
//#define HAS_RADIO
#define HAS_RS485
#define HAS_FINGERPRINT
#define DEBUG

// Sanity checks
#if defined(HAS_RADIO) && defined(HAS_RS485)
#error "Should not define both HAS_RADIO and HAS_RS485"
#endif

#ifdef DEBUG
#define DBG(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG(...)
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
// Songs :]
const char *ok    = "m=1,c=1,s=250,r=0:d=16,o=5,b=120:c,g,e,c6";
const char *error = "m=1,c=1,s=250,r=0:d=16,o=5,b=120:c6,g,eb,c";
#define SONG_ARMING     "m=1,c=4,s=250,r=0:d=4,o=5,b=140:p"
#define SONG_AUTH0      "m=1,c=1,s=250,r=0:d=4,o=5,b=140:c,p,p,p,p,p,p,p,p"
#define SONG_AUTH1      "m=1,c=1,s=200,r=0:d=4,o=5,b=140:c,p,p,p,p,p,p"
#define SONG_AUTH2      "m=1,c=1,s=150,r=0:d=4,o=5,b=140:c,p,p,p,p"
#define SONG_AUTH3      "m=1,c=1,s=100,r=0:d=4,o=5,b=140:c,p,p"
#define SONG_ARMED_AWAY "m=1,c=4,s=250,r=0:d=4,o=5,b=140:p"
#define SONG_DISARMED   "m=1,c=4,s=250,r=0:d=4,o=5,b=140:p"
#define SONG_ARMED_HOME "m=1,c=4,s=250,r=0:d=4,o=5,b=140:p"
// Songs array
static const char *songs[8] = {
SONG_ARMING,
SONG_AUTH0,
SONG_AUTH1,
SONG_AUTH2,
SONG_AUTH3,
SONG_ARMED_AWAY,
SONG_DISARMED,
SONG_ARMED_HOME };
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

binary_semaphore_t R503Sem;

// Define debug console
#ifdef HAS_SERIAL1
BaseSequentialStream *console = (BaseSequentialStream*) &SD1;
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
uint8_t commpressed[MAX_FINGERPRINT_SIZE + 4 + (MAX_FINGERPRINT_SIZE / 2)];
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
  chMBObjectInit(&rtttlMailbox, rtttlMailboxBuffer, 1);

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
    chprintf(console, "FP Init error!\r\n");
#endif
    chThdSleepMilliseconds(2000);
  }

  // Register this node
  sendConf();

  // Initialize node state
  setNodeMode(MODE_DISARMED);
  uint8_t ret;
  uint16_t location, confidence;
  authMode_t last = MODE_UNINITIALIZED;
  while (true) {
    chThdSleepMilliseconds(200);
    // Check if mode changed
    if (nodeState.mode != last) {
      last = nodeState.mode;
      chprintf(console, "Mode changed to %d\r\n", nodeState.mode);
    }

    // Check if authentication is allowed
    if (!isAuthAllowed()) {
      continue;
    }

    // Read fingerprint
    ret = R503TakeImage();
    if (ret == R503_OK) {
      ret = R503ExtractFeatures(FINGERPRINT_CHAR_BUFFER);
      if (ret != R503_OK) {
        continue;
      } else {
        ret = R503SearchFinger(FINGERPRINT_CHAR_BUFFER, &location, &confidence);
        if (ret == R503_OK) {
          //playNote("c", 200, 5, 100);
          //playRTTTL(ok);
          // send song to RTTTL thread
          chMBPostTimeout(&rtttlMailbox, (msg_t)ok, TIME_IMMEDIATE);

          R503SetAuraLED(aLEDModeBreathing, aLEDGreen, 100, 1);

          chprintf(console, " >> Found finger, ");
          chprintf(console, "ID: %d, ", location);
          chprintf(console, "Confidence: %d\r\n", confidence);

          // Send authentication message
          msgOut.address = GATEWAYID;
          msgOut.ctrl = RS485_FLAG_DTA;
          msgOut.length = 11;
          msgOut.data[0] = conf.reg[0]; // Element ID
          msgOut.data[1] = conf.reg[1]; // Element type
          msgOut.data[2] = 0;           // Arming state
          memcpy(&msgOut.data[3], "finger", 6);
          memcpy(&msgOut.data[9], &location, 2);
          rs485SendMsgWithACK(&RS485D3, &msgOut, MSG_REPEAT);
          chprintf(console, " >> Sent: ");
          for (uint8_t i = 0; i < 8; i++) {
            chprintf(console, " %02X", msgOut.data[i + 3]);
          }
          chprintf(console, "\r\n");
        } else {
          // send song to RTTTL thread
          chMBPostTimeout(&rtttlMailbox, (msg_t)error, TIME_IMMEDIATE);
          R503SetAuraLED(aLEDModeBreathing, aLEDRed, 100, 1);
        }
      }
    }
    //chprintf(console," >> ret: 0x%02X\r\n", ret);
  }
}
