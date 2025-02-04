/*
 * tone.h
 *
 *  Created on: Jan 27, 2025
 *      Author: vysocan
 */

#ifndef TONE_H
#define TONE_H

#include "ch.h"
#include "hal.h"

// Note frequency constants (Hz)
#define NOTE_A3  220.00
#define NOTE_B3  246.94
#define NOTE_C4  261.63
#define NOTE_D4  293.66
#define NOTE_E4  329.63
#define NOTE_F4  349.23
#define NOTE_G4  392.00
#define NOTE_A4  440.00
#define NOTE_B4  493.88
#define NOTE_C5  523.25

void toneInit(void);
void tone(uint32_t frequency, uint32_t duration);
void noTone(void);

#endif // TONE_H

