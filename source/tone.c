/*
 * tone.c
 *
 *  Created on: Jan 27, 2025
 *      Author: vysocan
 */

#include "tone.h"

// Define the pin to be used for tone generation
#define TONE_PIN PAL_LINE(GPIOB, GPIOB_PIN_TONE) // Replace with your desired pin (example: PA0)

// PWM configuration
static PWMConfig pwmcfg = {
  1000000,                    // PWM clock frequency (1 MHz)
  20000,                      // PWM period (20 ms = 20000 ticks @ 1 MHz)
  NULL,                       // No callback function
  {
      {PWM_OUTPUT_ACTIVE_HIGH, NULL},    // Channel 1 configuration (PB6)
      {PWM_OUTPUT_DISABLED, NULL},       // Channel 2 configuration
      {PWM_OUTPUT_DISABLED, NULL},       // Channel 3 configuration
      {PWM_OUTPUT_DISABLED, NULL}        // Channel 4 configuration
  },
  0,
  0
};

// PWM driver pointer
static PWMDriver *pwm_driver;

void toneInit(void) {
  // Set the pin mode to PWM
  palSetPadMode(GPIOB, GPIOB_PIN_TONE, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  // Initialize PWM driver
  pwm_driver = &PWMD4;
  pwmStart(pwm_driver, &pwmcfg);
}

void tone(uint32_t frequency, uint32_t duration) {
  if (frequency == 0) {
      noTone();
      return;
  }

  uint32_t period = 1000000 / frequency;
  pwmcfg.period = period;

  pwmChangePeriod(pwm_driver, period);
  pwmEnableChannel(pwm_driver, 0, PWM_PERCENTAGE_TO_WIDTH(pwm_driver, 5000));

//  if (duration > 0) {
//      chThdSleepMilliseconds(duration);
//      noTone();
//  }
}

void noTone(void) {
  pwmDisableChannel(pwm_driver, 0);
}



