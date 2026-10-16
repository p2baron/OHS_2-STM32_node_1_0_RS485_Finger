 /*
 * ohs_th_service.h
 *
 *  Created on: 23. 2. 2020
 *      Author: vysocan
 */

#ifndef OHS_TH_SERVICE_H_
#define OHS_TH_SERVICE_H_

#ifndef SERVICE_DEBUG
#define SERVICE_DEBUG 0
#endif

#if SERVICE_DEBUG
#define DBG_SERVICE(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG_SERVICE(...)
#endif

// Ping cadence while apparently connected — keeps lastGwContact fresh
// through the node's OWN activity instead of depending entirely on the
// gateway happening to send something during a quiet period. Without
// this, a node that goes untouched for GW_TIMEOUT_SECS looks falsely
// "offline" (confirmed live: a perfectly healthy node sat retrying every
// 10s for several minutes with nothing actually wrong) and that false
// state hijacks the display for every mode, not just Disarmed — e.g. an
// Arming countdown that arrives while in that state never shows, even
// though the underlying RS485 command still gets through fine.
#define KEEPALIVE_SECS 60
// Faster retry cadence once genuinely disconnected.
#define OFFLINE_RETRY_SECS 10

/*
 * Service thread
 *
 * Only ever SETS pingPending and the display countdown here — the actual
 * ping() call happens from RS485Thread's own idle-tick (see ohs_th_rs485.h),
 * never directly from this thread: calling it here would block this thread
 * waiting for an ACK while RS485Thread is busy with something else, with no
 * coordination between the two — exactly the kind of collision that drops
 * a real incoming message if it lands mid-ping.
 */
static THD_WORKING_AREA(waServiceThread, 256);
static THD_FUNCTION(ServiceThread, arg) {
  chRegSetThreadName(arg);

  uint16_t pingCountdown = KEEPALIVE_SECS; // seconds until next requested ping

  while (true) {
    // 1 second sleep, we do not care much about exact time of execution.
    chThdSleepMilliseconds(1000);

    bool gwConnected = (lastGwContact != 0) &&
                       ((nowUnixSec() - lastGwContact) < GW_TIMEOUT_SECS);

    if (pingCountdown > 0) pingCountdown--;
    if (pingCountdown == 0 && nodeState.mode != MODE_UNINITIALIZED) {
      pingPending = true;
      pingCountdown = gwConnected ? KEEPALIVE_SECS : OFFLINE_RETRY_SECS;
      if (!gwConnected) {
        dispCountdownSecs = OFFLINE_RETRY_SECS;
        dispCountdownMax  = OFFLINE_RETRY_SECS;
      }
    }

  } // while(true)
}

#endif /* OHS_TH_SERVICE_H_ */
