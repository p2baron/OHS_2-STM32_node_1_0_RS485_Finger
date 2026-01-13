/*
 * tone.c
 *
 *  Created on: Nov 7, 2025
 *      Author: vysocan
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tone.h"
#include "R503.h"

// PWM configuration
static PWMConfig pwmcfg = { 1000000,    // PWM clock frequency (1 MHz)
    20000,                              // PWM period (20 ms = 20000 ticks @ 1 MHz)
    NULL,                               // No callback function
    { { PWM_OUTPUT_ACTIVE_HIGH, NULL }, // Channel 1 configuration (PB6)
        { PWM_OUTPUT_DISABLED, NULL },  // Channel 2 configuration
        { PWM_OUTPUT_DISABLED, NULL },  // Channel 3 configuration
        { PWM_OUTPUT_DISABLED, NULL }   // Channel 4 configuration
    }, 0, 0, 0 };
// PWM driver pointer
static PWMDriver *pwm_driver;
//
static uint8_t isPlaying = 0;
/*
 * Function to initialize tone generation
 *
 */
void toneInit(void) {
  // Set the pin mode to PWM
  palSetPadMode(GPIOB, GPIOB_PIN_TONE, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  // Initialize PWM driver
  pwm_driver = &PWMD4;
  pwmStart(pwm_driver, &pwmcfg);
}
/*
 * Function to play tone at specified frequency (Hz) for specified duration (ms)
 * @param frequency Frequency in Hz
 * @param duration Duration in milliseconds (0 for indefinite)
 *
 */
void tone(uint16_t frequency, uint16_t duration) {
  uint32_t period = 1000000 / frequency;
  pwmcfg.period = period;

  pwmChangePeriod(pwm_driver, period);
  pwmEnableChannel(pwm_driver, 0, PWM_PERCENTAGE_TO_WIDTH(pwm_driver, 5000));

  if (duration > 0) {
    chThdSleepMilliseconds(duration);
    noTone();
  }
}
/*
 * Function to stop playing tone
 * @
 */
void noTone(void) {
  pwmDisableChannel(pwm_driver, 0);
}
/*
 * Function to play a single note
 * @param note Note string (e.g., "c#", "d4", "p", etc.)
 * @param duration Default duration if not specified in note
 * @param octave Default octave if not specified in note
 * @param bpm Beats per minute for duration calculation
 *
 */
void playNote(const char *note, uint16_t duration, uint8_t octave, uint16_t bpm) {
  uint16_t frequency = 0;

  // Parse duration: Overwrite 'duration' if a specific one is found
  if (isdigit((uint8_t)*note)) {
    duration = 0;
    while (isdigit((uint8_t)*note)) {
      duration = duration * 10 + (*note - '0');
      note++;
    }
  }

  // Parse note
  switch (*note) {
    case 'c': frequency = 261; break;
    case 'd': frequency = 294; break;
    case 'e': frequency = 329; break;
    case 'f': frequency = 349; break;
    case 'g': frequency = 392; break;
    case 'a': frequency = 440; break;
    case 'b': frequency = 493; break;
    case 'p': frequency = 0;   break;
    default:  return;
  }
  note++;

  // Parse accidental
  if (*note == '#') {
    frequency = (uint16_t)(frequency * 1.05946f);
    note++;
  } else if (*note == 'b') {
    frequency = (uint16_t)(frequency / 1.05946f);
    note++;
  }

  // Parse octave
  if (isdigit((uint8_t)*note)) {
    octave = *note - '0';
    note++;
  }

  // Adjust frequency based on octave
  int shift = (int)octave - 4;
  if (shift > 0) {
    frequency <<= shift;
  } else if (shift < 0) {
    frequency >>= -shift;
  }

  // Parse dotted note
  if (*note == '.') {
    duration = (uint16_t)((duration * 3U) / 2U);
    note++;
  }

  // Calculate note duration in milliseconds
  // (60,000 ms/min * 4 quarter-notes) / (BPM * note_value)
  uint16_t noteDuration = (uint16_t)(240000UL / (bpm * duration));

  if (frequency > 0) {
    tone(frequency, noteDuration);
  } else {
    noTone();
    chThdSleepMilliseconds(noteDuration);
  }
}

/*
 * Function to play RTTTL melody
 * @param rtttl RTTTL string
 *
 */
void playRTTTL(const char *rtttl) {
  // Check for NULL RTTTL string
  if (rtttl == NULL) {
    return;
  }

  // RTTTL default parameters
  uint16_t defaultDuration = 4;
  uint8_t defaultOctave = 6;
  uint16_t bpm = 63;
  // LED parameters
  uint8_t LEDMode = aLEDModeFlash;
  uint8_t LEDColor = aLEDRed;
  uint8_t LEDspeed = 50;
  uint8_t LEDrepeat = 3;
  // Pointer to traverse the RTTTL string
  const char *p = rtttl;
  // Let's mark that we are playing
  isPlaying = 1;

  // Parse LED header
  while (*p && *p != ':') {
    if (*p == 'm') {
      p += 2; // Skip "m=", mode
      LEDMode = atoi(p);
    } else if (*p == 'c') {
      p += 2; // Skip "c=", color
      LEDColor = atoi(p);
    } else if (*p == 's') {
      p += 2; // Skip "s=", speed
      LEDspeed = atoi(p);
    } else if (*p == 'r') {
      p += 2; // Skip "r=", repeat
      LEDrepeat = atoi(p);
    }
    while (*p && *p != ',' && *p != ':')
      p++; // Stop at ',' or ':'
    if (*p == ',') p++;
  }
  if (*p == ':') p++; // Move past the second colon
  //R503SetAuraLED (LEDMode, LEDColor, LEDspeed, LEDrepeat);

  // Parse RTTL defaults
  while (*p && *p != ':') {
    if (*p == 'd') {
      p += 2; // Skip "d="
      defaultDuration = atoi(p);
    } else if (*p == 'o') {
      p += 2; // Skip "o="
      defaultOctave = atoi(p);
    } else if (*p == 'b') {
      p += 2; // Skip "b="
      bpm = atoi(p);
    }
    while (*p && *p != ',' && *p != ':')
      p++; // Stop at ',' or ':'
    if (*p == ',') p++;
  }
  if (*p == ':') p++; // Move past the second colon

  // Parse and play notes
  while (*p) {
    playNote(p, defaultDuration, defaultOctave, bpm);
    while (*p && *p != ',')
      p++;
    if (*p == ',') p++;
  }

  // Let's mark that we are done playing
  isPlaying = 0;
}
/*
 * Function to check if tone is playing
 * @return 1 if tone is playing, 0 otherwise
 *
 */
bool isTonePlaying(void) {
  return isPlaying ? true : false;
}
