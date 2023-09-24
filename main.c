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

// This node settings
#define VERSION         101    // Version of EEPROM struct
#define SENSOR_DELAY    600    // In seconds, 600 = 10 minutes

// Constants
#define GATEWAYID       1    // Radio Gateway
#define REG_LEN         21   // Size of one conf. element
#define NODE_NAME_SIZE  16   // As defined on gateway
// Radio
#define RADIO_REPEAT    1    // Repeat sending

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "ch.h"
#include "hal.h"

//#include "shell.h"
#include "chprintf.h"

// Define debug console
BaseSequentialStream* console = (BaseSequentialStream*)&SD1;

//#include "usbcfg.h"

// RFM69
#include "rfm69.h"

#include "htu2x.h"
#ifdef HTU2XD_SHT2X_SI70XX
  #define ELEMENTS 5
#else
  #define ELEMENTS 3
#endif

// Global variables
uint8_t msg[31]; // size of REG_LEN, or larger for sensor msg if longer than REG_LEN size
RTCDateTime timespec;
RTCAlarm alarmspec;




// Configuration struct
struct config_t {
  uint16_t version;
  uint8_t  reg[REG_LEN * ELEMENTS]; // REG_LEN * #, number of elements on this node
} conf;

// OHS includes
#include "ohs_peripheral.h"
// Thread handling
#include "ohs_th_radio.h"
#include "ohs_th_service.h"


/*===========================================================================*/
/* Generic code.                                                             */
/*===========================================================================*/

/*
 * Blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread1, 512);
static __attribute__((noreturn)) THD_FUNCTION(Thread1, arg) {

  (void)arg;
  chRegSetThreadName("blinker");
  while (true) {
    systime_t time = 500;
    palClearPad(GPIOC, GPIOC_LED);
    chThdSleepMilliseconds(time);
    palSetPad(GPIOC, GPIOC_LED);
    chThdSleepMilliseconds(time);
  }
}



/*
 * Set defaults on first time
 */
void setDefault() {
  conf.version = VERSION;   // Change VERSION to force EEPROM re-load
  conf.reg[0+(REG_LEN*0)] = 'S';       // Sensor
  conf.reg[1+(REG_LEN*0)] = 'V';       // Voltage
  conf.reg[2+(REG_LEN*0)] = 0;         // Local address
  conf.reg[3+(REG_LEN*0)] = 0b00000000; // Default setting
  conf.reg[4+(REG_LEN*0)] = 0b00011111; // Default setting, group=16, disabled
  memset(&conf.reg[5+(REG_LEN*0)], 0, NODE_NAME_SIZE);
  conf.reg[0+(REG_LEN*1)] = 'S';       // Sensor
  conf.reg[1+(REG_LEN*1)] = 'X';       // TX power level
  conf.reg[2+(REG_LEN*1)] = 0;         // Local address
  conf.reg[3+(REG_LEN*1)] = 0b00000000; // Default setting
  conf.reg[4+(REG_LEN*1)] = 0b00011111; // Default setting, group=16, disabled
  memset(&conf.reg[5+(REG_LEN*1)], 0, NODE_NAME_SIZE);
  conf.reg[0+(REG_LEN*2)] = 'S';       // Sensor
  conf.reg[1+(REG_LEN*2)] = 'D';       // Digital pin, 1 = charging
  conf.reg[2+(REG_LEN*2)] = 0;         // Local address
  conf.reg[3+(REG_LEN*2)] = 0b00000000; // Default setting
  conf.reg[4+(REG_LEN*2)] = 0b00011111; // Default setting, group=16, disabled
  memset(&conf.reg[5+(REG_LEN*2)], 0, NODE_NAME_SIZE);
  #ifdef HTU2XD_SHT2X_SI70XX
    conf.reg[0+(REG_LEN*3)] = 'S';       // Sensor
    conf.reg[1+(REG_LEN*3)] = 'T';       // Temperature
    conf.reg[2+(REG_LEN*3)] = 0;         // Local address
    conf.reg[3+(REG_LEN*3)] = 0b00000000; // Default setting
    conf.reg[4+(REG_LEN*3)] = 0b00011111; // Default setting, group=16, disabled
    memset(&conf.reg[5+(REG_LEN*3)], 0, NODE_NAME_SIZE);
    conf.reg[0+(REG_LEN*4)] = 'S';       // Sensor
    conf.reg[1+(REG_LEN*4)] = 'H';       // Humidity
    conf.reg[2+(REG_LEN*4)] = 0;         // Local address
    conf.reg[3+(REG_LEN*4)] = 0b00000000; // Default setting
    conf.reg[4+(REG_LEN*4)] = 0b00011111; // Default setting, group=16, disabled
    memset(&conf.reg[5+(REG_LEN*4)], 0, NODE_NAME_SIZE);
  #endif
}
/*
 * Application entry point.
 */
int main(void) {
  uint32_t tv_sec;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * SPI2 I/O pins setup.
   */
  //palSetPadMode(GPIOA, GPIOA_RADIO_INT, PAL_MODE_INPUT);                 /* Radio INT    */
  //palSetPadMode(GPIOA, GPIOA_SCK, PAL_MODE_STM32_ALTERNATE_PUSHPULL);    /* New SCK.     */
  //palSetPadMode(GPIOA, GPIOA_MISO, PAL_MODE_STM32_ALTERNATE_PUSHPULL);   /* New MISO.    */
  //palSetPadMode(GPIOA, GPIOA_MOSI, PAL_MODE_STM32_ALTERNATE_PUSHPULL);   /* New MOSI.    */
  //palSetPadMode(GPIOA, GPIOA_SS, PAL_MODE_OUTPUT_PUSHPULL);              /* New CS.      */

  //palSetPadMode(GPIOA, GPIOA_PIN9, PAL_MODE_STM32_ALTERNATE_PUSHPULL);      /* USART1 TX.       */
  //palSetPadMode(GPIOA, GPIOA_PIN10, PAL_MODE_INPUT);

  palSetPadMode(GPIOB, GPIOB_SCL1, PAL_MODE_STM32_ALTERNATE_OPENDRAIN);
  palSetPadMode(GPIOB, GPIOB_SDA1, PAL_MODE_STM32_ALTERNATE_OPENDRAIN);

  // Debug port
  sdStart(&SD1,  &serialCfg);
  chprintf(console, "\r\nOHS node start\r\n");

  // I2C
  i2cStart(&I2CD1, &i2cfg1);
  // Check connected
  while (htu2xBegin(&I2CD1, HTU2xD_SENSOR, HUMD_12BIT_TEMP_14BIT)) {
    chprintf(console, "HTU2xD/SHT2x failed\r\n");
  }
  chprintf(console, "HTU2xD/SHT2x OK\r\n");

  // SPI
  spiStart(&SPID1, &spi1cfg);
  // RFM69
  rfm69Start(&rfm69cfg);
  rfm69SetHighPower(true); // long range version
  rfm69AutoPower(-75);
  // Register
  setDefault();
  sendConf();

  /*
   * Create the threads.
   */
  chThdCreateStatic(waRadioThread, sizeof(waRadioThread), NORMALPRIO, RadioThread, (void*)"radio");
  chThdCreateStatic(waServiceThread, sizeof(waServiceThread), NORMALPRIO, ServiceThread, (void*)"service");
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);


  /* compile ability test */
  rtcGetTime(&RTCD1, &timespec);

  /*
   * Normal main() thread activity, spawning shells.
   */
  while (true) {
    //chThdSleepMilliseconds(30000);
    chprintf(console, "HTU2xD/SHT2x %02f : %02f\r\n", htu2xReadTemperature(START_TEMP_HOLD_I2C), htu2xReadHumidity(START_HUMD_HOLD_I2C));
    chThdSleepMilliseconds(1000);
    /* set alarm in near future */
    rtcSTM32GetSecMsec(&RTCD1, &tv_sec, NULL);
    alarmspec.tv_sec = tv_sec + 20;
    rtcSetAlarm(&RTCD1, 0, &alarmspec);
    /* going to anabiosis*/
    chSysLock();
    PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF;
    PWR->CR |= PWR_CR_PDDS | PWR_CR_LPDS;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __WFI();

  }
}
