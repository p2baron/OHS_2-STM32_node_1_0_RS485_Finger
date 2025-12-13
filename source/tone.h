/*
 * tone.h
 *
 *  Created on: Jan 27, 2025
 *      Author: vysocan
 */

#ifndef TONE_H
#define TONE_H

#include <stddef.h>
#include "ch.h"
#include "hal.h"

void toneInit(void);
void tone(uint16_t frequency, uint16_t duration);
void noTone(void);
void playNote(const char *note, uint16_t defaultDuration, uint8_t defaultOctave, uint16_t bpm);
void playRTTTL(const char *rtttl);
bool isTonePlaying(void);

#endif // TONE_H

