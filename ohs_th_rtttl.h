/*
 * ohs_th_rtttl.h
 *
 *  Created on: Nov 7, 2025
 *      Author: vysocan
 */

#ifndef OHS_TH_RTTTL_H_
#define OHS_TH_RTTTL_H_

#include "ch.h"
#include "hal.h"
#include "tone.h"

// Debugging macros for RTTTL thread
#ifndef RTTTL_DEBUG
#define RTTTL_DEBUG 0
#endif

#if RTTTL_DEBUG
#define DBG_RTTTL(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG_RTTTL(...)
#endif

/*
 * RTTTL thread
 */
static THD_WORKING_AREA(waRTTTLThread, 384);
static THD_FUNCTION(RTTTLThread, arg) {
  chRegSetThreadName(arg);
  msg_t msg = 0;  // Initialize to NULL
  const char *song = 0;

  while (true) {
    // Check for a message in the RTTTL mailbox
    if (chMBFetchTimeout(&rtttlMailbox, &msg, TIME_IMMEDIATE) == MSG_OK) {
      song = (const char *)msg;
    }

    if (song != NULL) {
      DBG_RTTTL("RTTTL: time: %d\r\n", chVTGetSystemTime());
//      if (chBSemWaitTimeout (&R503Sem, TIME_IMMEDIATE) == MSG_OK) {
      playRTTTL(song);
      DBG_RTTTL("RTTTL: Played song: %p, time: %d\r\n", song, chVTGetSystemTime());
      song = NULL;
//        chBSemSignal (&R503Sem);
//      }
    } else {
      // Block waiting for a song if none available
      if (chMBFetchTimeout(&rtttlMailbox, &msg, TIME_INFINITE) == MSG_OK) {
        song = (const char *)msg;
      }
    }
  }
}

#endif /* OHS_TH_RTTTL_H_ */
