/*
 * ohs_th_blinker.h
 *
 *  Created on: Nov 28, 2025
 *      Author: vysocan
 */

#ifndef OHS_TH_BLINKER_H_
#define OHS_TH_BLINKER_H_

/*
 * Blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waBlinkerThread, 128);
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

#endif /* OHS_TH_BLINKER_H_ */
