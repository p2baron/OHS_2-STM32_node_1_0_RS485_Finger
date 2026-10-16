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

// Reconnect-retry interval while lastGwContact has timed out. Deliberately
// short for now so this is easy to test (Offline screen + retry) without
// waiting around; raise this once that's confirmed working — a disconnected
// node pinging every 10s forever is more bus traffic than this really
// needs long-term.
#define OFFLINE_RETRY_SECS 10

/*
 * Service thread
 */
static THD_WORKING_AREA(waServiceThread, 256);
static THD_FUNCTION(ServiceThread, arg) {
  chRegSetThreadName(arg);

  uint16_t counter = 1;
  uint8_t  offlineRetrySecs = 0; // counts down independently of dispCountdownSecs,
                                 // which this only ever WRITES (for the display
                                 // thread to tick down and show) — never reads
                                 // back, so the two can't double-decrement.

  while (true) {
    // 1 second sleep, we do not care much about exact time of execution.
    chThdSleepMilliseconds(1000);
    counter++;
    // reset counter every one hour
    if (counter == 3600) counter = 0;

    // Ping call every hour
    if (counter == 0) {
      ping();
    }

    // Sensor data
    if ((counter%60) == 0) {
      //sendValue(0, (float)counter);
    }

    // Actively retry while we've lost contact with the gateway, instead of
    // just waiting for the next hourly ping — ping()'s own ACK tells us
    // directly whether the gateway answered, so use that rather than
    // waiting for lastGwContact to update via the normal receive path.
    bool gwConnected = (lastGwContact != 0) &&
                       ((nowUnixSec() - lastGwContact) < GW_TIMEOUT_SECS);
    if (!gwConnected && nodeState.mode != MODE_UNINITIALIZED) {
      if (offlineRetrySecs == 0) {
        if (ping() == MSG_OK) lastGwContact = nowUnixSec();
        offlineRetrySecs = OFFLINE_RETRY_SECS;
        dispCountdownSecs = OFFLINE_RETRY_SECS;
        dispCountdownMax  = OFFLINE_RETRY_SECS;
      } else {
        offlineRetrySecs--;
      }
    } else {
      offlineRetrySecs = 0;
    }

  } // while(true)
}

#endif /* OHS_TH_SERVICE_H_ */
