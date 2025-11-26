/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

// Node definitions
#define HAS_SERIAL1
//#define HAS_RADIO
#define HAS_RS485
#define HAS_FINGERPRINT
#define DEBUG

#ifdef DEBUG
#define DBG(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG(...)
#endif

// This node settings
#define VERSION         101    // Version of EEPROM struct
#define SENSOR_DELAY    600    // In seconds, 600 = 10 minutes
#define ELEMENTS 1             // How many elements this node has

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
const char *melody = "Impossible:d=16,o=6,b=95:32d,32d#,32d,32d#,32d,32d#,32d,32d#,32d,32d,32d#,32e,32f,32f#,32g,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,a#,g,2d,32p,a#,g,2c#,32p,a#,g,2c,a#5,8c,2p,32p,a#5,g5,2f#,32p,a#5,g5,2f,32p,a#5,g5,2e,d#,8d";
const char *song = "Imperial:d=4,o=5,b=120:32e,32e,32e,32c,32e,32g";
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
BaseSequentialStream* console = (BaseSequentialStream*)&SD1;
#endif


//#include "usbcfg.h"

// RFM69
#ifdef HAS_RADIO
#include "rfm69.h"
#endif

// Global variables
volatile uint8_t mode = 0; // Authentication mode
#ifdef HAS_FINGERPRINT
#define MAX_FINGERPRINT_SIZE 1536
uint8_t finger[MAX_FINGERPRINT_SIZE];
uint8_t comm[1024*3];
#endif

// Configuration struct
struct config_t {
  uint16_t version;
  uint8_t  reg[REG_LEN * ELEMENTS]; // REG_LEN * #, number of elements on this node
} conf;

// RTTTL thread variables
static mailbox_t rtttlMailbox;
static msg_t rtttlMailboxBuffer[1]; // Storage for one message (pointer)

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

// Thread handling
#ifdef HAS_RADIO
#include "ohs_th_radio.h"
uint8_t msg[REG_LEN]; // size of REG_LEN, or larger for sensor msg if longer than REG_LEN size
#endif
#ifdef HAS_RS485
RS485Msg_t msg;
#include "ohs_th_rs485.h"
#endif
// other threads
#include "ohs_th_service.h"
#include "ohs_th_rtttl.h"

/*
 * Blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waBlinkerThread, 256);
static __attribute__((noreturn)) THD_FUNCTION(BlinkerThread, arg) {
  chRegSetThreadName(arg);

  systime_t time = 500;

  while (true) {
    palClearPad(GPIOC, GPIOC_LED);
    chThdSleepMilliseconds(time);
    palSetPad(GPIOC, GPIOC_LED);
    chThdSleepMilliseconds(time);
  }
}
/*
 * Application entry point.
 */
int main(void) {
  halInit();
  chSysInit();
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
  sdStart(&SD1,  &serialCfg);
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
  if (conf.version != VERSION ) {
    setDefault();
    writeToFlash(&conf, sizeof(conf));
  }

  // Register this node
  sendConf();

  /*
   * Create the threads.
   */
#ifdef HAS_RADIO
  chThdCreateStatic(waRadioThread, sizeof(waRadioThread), NORMALPRIO, RadioThread, (void*)"radio");
#endif
#ifdef HAS_RS485
  chThdCreateStatic(waRS485Thread, sizeof(waRS485Thread), NORMALPRIO, RS485Thread, (void*)"rs485");
#endif
  chThdCreateStatic(waServiceThread, sizeof(waServiceThread), NORMALPRIO-1, ServiceThread, (void*)"service");
  chThdCreateStatic(waBlinkerThread, sizeof(waBlinkerThread), NORMALPRIO, BlinkerThread, (void*)"blinker");
  chThdCreateStatic(waRTTTLThread, sizeof(waRTTTLThread), NORMALPRIO, RTTTLThread, (void*)"rtttl");

  // Normal main() thread activity, spawning shells.
  toneInit();

  while (R503Start() != R503_OK) {
	#ifdef HAS_SERIAL1
	  chprintf(console, "FP Init error!\r\n");
	#endif
	chThdSleepMilliseconds(2000);
  }

  //enrollFinger();
  chThdSleepMilliseconds(200);
  searchFinger();
  chThdSleepMilliseconds(200);
  downloadTemplate();

  chMBPostTimeout(&rtttlMailbox, (msg_t)song, TIME_INFINITE);

  uint64_t test = 0;
  int8_t count = 0;
  while (true) {
    chThdSleepMilliseconds(2000);
    if (count == 5) count = 0;


    chThdSleepMilliseconds(2000);
    R503SetAuraLED(aLEDBreathing, count, 200, 1);
  }
}
