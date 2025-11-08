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
static THD_WORKING_AREA(waRTTTLThread, 256);
static THD_FUNCTION(RTTTLThread, arg) {
  (void)arg;
  msg_t msg;

  while (true) {
    // Wait indefinitely for a message in the RTTTL mailbox
    if (chMBFetchTimeout(&rtttlMailbox, &msg, TIME_INFINITE) == MSG_OK) {
		playRTTTL((const char *)msg);
    }
  }
}

#endif /* OHS_TH_RTTTL_H_ */
