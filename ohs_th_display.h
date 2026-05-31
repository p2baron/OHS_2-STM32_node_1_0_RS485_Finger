/*
 * ohs_th_display.h — Display update thread for SSD1309 OLED
 *
 * Runs every 250 ms, redraws affected screen sections on state change.
 * State variables (dispCountdownSecs, dispZoneName) are set by the
 * RS485 thread when 'D' display-control messages arrive from the gateway.
 */

#ifndef OHS_TH_DISPLAY_H_
#define OHS_TH_DISPLAY_H_

#include "ohs_ssd1309.h"
#include "ch.h"
#include "chprintf.h"

/* Display state vars are declared in ohs_ssd1309.h (included first) */

/* -------------------------------------------------------------------------
 * Screen layout helpers
 * -------------------------------------------------------------------------
 *  Page 0: node name + time          (ssd1309Print)
 *  Page 1: separator line            (ssd1309Hline)
 *  Page 2-3: main mode text          (ssd1309PrintBig)
 *  Page 4: sub-message line 1        (ssd1309Print)
 *  Page 5: sub-message line 2        (ssd1309Print)
 *  Page 6: progress bar              (ssd1309Bar / clear)
 *  Page 7: FP hint / enrollment      (ssd1309Print)
 */

static void dispHeader(bool connected) {
  char hdr[22];
  if (!connected) {
    /* Blink the header between warning and blank to signal disconnection */
    static uint8_t blink = 0;
    blink ^= 1;
    ssd1309Print(0, blink ? "!! NO CONTACT !!" : "                ");
    return;
  }
  /* "NodeName         HH:MM" — name left (up to 16 chars), time right-aligned */
  char nodeName[NODE_NAME_SIZE + 1];
  memcpy(nodeName, &conf.reg[5], NODE_NAME_SIZE);
  nodeName[NODE_NAME_SIZE] = '\0';
  for (int8_t i = NODE_NAME_SIZE - 1; i >= 0 && nodeName[i] == ' '; i--)
    nodeName[i] = '\0';

  RTCDateTime ts;
  rtcGetTime(&RTCD1, &ts);
  uint32_t t = convertRTCDateTimeToUnixSecond(&ts);
  uint8_t h = (uint8_t)((t % 86400UL) / 3600UL);
  uint8_t m = (uint8_t)((t % 3600UL) / 60UL);

  chsnprintf(hdr, sizeof(hdr), "%-16.16s%02u:%02u", nodeName, h, m);
  ssd1309Print(0, hdr);
}

static void dispClearPages(uint8_t from, uint8_t to) {
  for (uint8_t p = from; p <= to; p++) ssd1309Print(p, "");
}

/*
 * Layout:
 *  Page 0: header (name + time)
 *  Page 1: thin separator line (1px)
 *  Page 2: blank gap
 *  Pages 3-4: main mode text (double-height)
 *  Pages 5-6: sub-message
 *  Page 7: FP hint / slot info
 */
static void dispRender(authMode_t mode) {
  char sub[22];

  dispHeader(true);
  ssd1309Hline(1);
  ssd1309Print(2, "");  // blank gap

  switch (mode) {
    case MODE_DISARMED:
      ssd1309PrintBig(3, "OHS Ready");
      dispClearPages(5, 7);
      break;

    case MODE_ARMED_AWAY:
    case MODE_ARMED_HOME:
      dispClearPages(2, 7); // blank — do not reveal armed state on screen
      break;

    case MODE_ARMING: {
      ssd1309PrintBig(3, "ARMING");
      chsnprintf(sub, sizeof(sub), "  Exit: %us", (unsigned)dispCountdownSecs);
      ssd1309Print(5, sub);
      ssd1309Print(6, "");
      uint8_t pct = dispCountdownMax ? (dispCountdownSecs * 100U / dispCountdownMax) : 0;
      ssd1309Bar(7, pct);
      break;
    }

    case MODE_AUTH_1:
    case MODE_AUTH_2:
    case MODE_AUTH_3: {
      ssd1309PrintBig(3, "  ENTRY");
      ssd1309Print(5, "  Authenticate!");
      chsnprintf(sub, sizeof(sub), "  Time: %us", (unsigned)dispCountdownSecs);
      ssd1309Print(6, sub);
      uint8_t pct = dispCountdownMax ? (dispCountdownSecs * 100U / dispCountdownMax) : 0;
      ssd1309Bar(7, pct);
      break;
    }

    case MODE_ALARM:
      ssd1309PrintBig(3, "!! ALARM !!");
      dispClearPages(5, 7);
      break;

    case MODE_ENROLLMENT: {
      static const char *enrollMsgs[6] = {
        "  Place finger...",  // 0
        "  Remove finger",   // 1
        "  Place again...",  // 2
        "  Creating...",     // 3
        "     OK!",          // 4
        "  Received",        // 5 - gateway sync
      };
      uint8_t step = dispEnrollStep < 6 ? dispEnrollStep : 5;
      const char *bigText = (step == 4) ? "  Done!" :
                            (step == 5) ? " SYNCING" : "ENROLLING";
      ssd1309PrintBig(3, bigText);
      ssd1309Print(5, enrollMsgs[step]);
      ssd1309Print(6, "");
      chsnprintf(sub, sizeof(sub), "  Slot: %u", (unsigned)dispEnrollSlot + 1);
      ssd1309Print(7, sub);
      break;
    }

    case NODE_ARM_REJECTED: {
      ssd1309PrintBig(3, "REJECTED");
      if (dispZoneName[0]) {
        chsnprintf(sub, sizeof(sub), "  Zone: %s", dispZoneName);
        ssd1309Print(5, sub);
      } else {
        ssd1309Print(5, "  Check zones");
      }
      dispClearPages(6, 7);
      break;
    }

    case MODE_UNINITIALIZED:
    default:
      ssd1309PrintBig(3, "CONNECTING");
      dispClearPages(5, 7);
      break;
  }
}

/* -------------------------------------------------------------------------
 * Display thread
 * ------------------------------------------------------------------------- */
static THD_WORKING_AREA(waDisplayThread, 384);
static THD_FUNCTION(DisplayThread, arg) {
  (void)arg;
  chRegSetThreadName("display");

  /* Run display probe + init here at LOWPRIO so it doesn't block main thread */
  ssd1309Init();

  if (!dispPresent) {
    chThdExit(MSG_OK);
    return;
  }

  ssd1309Clear();
  ssd1309Print(0, "");
  ssd1309Hline(1);
  ssd1309Print(2, "");
  ssd1309PrintBig(3, "CONNECTING");

  authMode_t lastMode    = MODE_UNINITIALIZED;
  uint8_t    lastSecs    = 0;
  uint8_t    lastStep    = 0xFF;
  uint8_t    tickCount   = 0;
  uint8_t    headerTick  = 0;

  while (true) {
    chThdSleepMilliseconds(250);

    /* Gateway connection: disconnected after GW_TIMEOUT_TICKS with no message */
    bool connected = (lastGwContact != 0) &&
                     (chTimeDiffX(lastGwContact, chVTGetSystemTimeX()) < GW_TIMEOUT_TICKS);

    /* Hold mode overrides actual mode for transient messages */
    authMode_t mode;
    if (dispHoldTicks > 0) {
      dispHoldTicks--;
      mode = (authMode_t)dispHoldMode;
    } else {
      mode = nodeState.mode;
    }

    /* Countdown: decrement once per second */
    tickCount++;
    if (tickCount >= 4) {
      tickCount = 0;
      if (dispCountdownSecs > 0) dispCountdownSecs--;
    }

    headerTick++;

    bool modeChanged = (mode != lastMode);
    bool stepChanged = (mode == MODE_ENROLLMENT) && (dispEnrollStep != lastStep);
    bool countdownTicked = (dispCountdownSecs != lastSecs) &&
                           (mode == MODE_ARMING ||
                            mode == MODE_AUTH_1 || mode == MODE_AUTH_2 || mode == MODE_AUTH_3);
    bool headerChanged = (headerTick >= 8); // ~2s (also drives disconnect blink rate)

    if (modeChanged || stepChanged) {
      if (mode != NODE_ARM_REJECTED) dispZoneName[0] = '\0';
      dispRender(mode);
      lastMode = mode;
      lastSecs = dispCountdownSecs;
      lastStep = dispEnrollStep;
      headerTick = 0;
    } else if (countdownTicked) {
      dispHeader(connected);
      char sub[22];
      if (mode == MODE_ARMING) {
        chsnprintf(sub, sizeof(sub), "  Exit: %us", (unsigned)dispCountdownSecs);
        ssd1309Print(5, sub);
      } else {
        chsnprintf(sub, sizeof(sub), "  Time: %us", (unsigned)dispCountdownSecs);
        ssd1309Print(6, sub);
      }
      uint8_t pct = dispCountdownMax ? (dispCountdownSecs * 100U / dispCountdownMax) : 0;
      ssd1309Bar(7, pct);
      lastSecs = dispCountdownSecs;
      headerTick = 0;
    } else if (headerChanged) {
      dispHeader(connected);
      headerTick = 0;
    }
  }
}

#endif /* OHS_TH_DISPLAY_H_ */
