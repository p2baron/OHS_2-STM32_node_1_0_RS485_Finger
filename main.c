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
#define HAS_USART1
//#define HAS_RADIO
#define HAS_RS485

// This node settings
#define VERSION         100    // Version of EEPROM struct
#define SENSOR_DELAY    600    // In seconds, 600 = 10 minutes
#define ELEMENTS 3             // How many elements this node has

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ch.h"
#include "hal.h"

//#include "shell.h"
#include "chprintf.h"

// Define debug console
#ifdef HAS_USART1
BaseSequentialStream* console = (BaseSequentialStream*)&SD1;
#endif

//#include "usbcfg.h"

// RFM69
#ifdef HAS_RADIO
#include "rfm69.h"
#endif

// Global variables
uint8_t pos = 0;

// Configuration struct
struct config_t {
  uint16_t version;
  uint8_t  reg[REG_LEN * ELEMENTS]; // REG_LEN * #, number of elements on this node
} conf;

// OHS includes
#include "ohs_peripheral.h"
#include "ohs_func.h"
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
#include "ohs_th_service.h"

/*
 * Blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread1, 256);
static __attribute__((noreturn)) THD_FUNCTION(Thread1, arg) {
  (void)arg;

  chRegSetThreadName("blinker");
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
#ifdef HAS_USART1
  palSetPadMode(GPIOA, GPIOA_TX1, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOA, GPIOA_RX1, PAL_MODE_INPUT);
#endif
#ifdef HAS_RS485
  palSetPadMode(GPIOB, GPIOB_TX3, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  palSetPadMode(GPIOB, GPIOB_RX3, PAL_MODE_INPUT);
  palSetPadMode(GPIOB, GPIOB_PIN13, PAL_MODE_OUTPUT_PUSHPULL);
#endif

  // Debug port
  sdStart(&SD1,  &serialCfg);
  chprintf(console, "\r\nOHS node start\r\n");
  // RS485
#ifdef HAS_RS485
  rs485Start(&RS485D3, &rs485cfg);
  chprintf(console, "RS485 timeout: %d(uS)/%d(tick)\r\n", RS485D3.oneByteTimeUS, RS485D3.oneByteTimeI);
#endif

#ifdef HAS_RADIO
  // SPI
  spiStart(&SPID1, &spi1cfg);
  // RFM69
  rfm69Start(&rfm69cfg);
  rfm69SetHighPower(true); // long range version
  rfm69AutoPower(-75);
#endif
#ifdef HAS_RS485
#endif
  // Register this node
  setDefault();
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
  chThdCreateStatic(waServiceThread, sizeof(waServiceThread), NORMALPRIO, ServiceThread, (void*)"service");
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /*
   * Normal main() thread activity, spawning shells.
   */
  toneInit();
  tone(NOTE_A3,1000);
  while (true) {
    chThdSleepMilliseconds(2000);

  }
}
