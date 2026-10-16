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

/* Local, display-only sentinel for "lost contact with the gateway after
 * already having connected" — distinct from any real authMode_t/NODE_CMD_*
 * value, and from the 0xFF sentinel the thread loop seeds lastMode with.
 * Never sent over RS485 or compared against anything outside this file. */
#define MODE_OFFLINE_DISP ((authMode_t)0xFE)

/* -------------------------------------------------------------------------
 * Screen layout helpers
 * -------------------------------------------------------------------------
 *  Page 0: date + time               (ssd1309Print, via dispHeader)
 *  Page 1: separator line            (ssd1309Hline)
 *  Page 2-7: mode-specific content
 *
 *  MODE_DISARMED additionally uses the bottom two pages as a footer
 *  (zone/node name + connectivity icon, via dispFooter) — see the mockups
 *  this was built from. Other modes still use pages 6-7 for their own
 *  content (progress bar, FP hints) until they get redesigned too.
 */

/* 3-letter day/month names for the header. RTCDateTime.dayofweek is
 * 1..7 — assumed ISO 8601 (1=Monday); flip this table if it reads a day
 * off on real hardware. */
static const char * const dispDayName[8] =
  {"???", "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
static const char * const dispMonName[13] =
  {"???", "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

/* "MON, SEP 14          10:02" — date left, time right-aligned */
static void dispFormatDateTime(char *hdr, size_t n) {
  RTCDateTime ts;
  rtcGetTime(&RTCD1, &ts);
  uint32_t t = convertRTCDateTimeToUnixSecond(&ts);
  uint8_t h = (uint8_t)((t % 86400UL) / 3600UL);
  uint8_t m = (uint8_t)((t % 3600UL) / 60UL);

  char date[12];
  uint8_t dow = (ts.dayofweek <= 7) ? ts.dayofweek : 0;
  uint8_t mon = (ts.month >= 1 && ts.month <= 12) ? ts.month : 0;
  chsnprintf(date, sizeof(date), "%s, %s %u", dispDayName[dow], dispMonName[mon], (unsigned)ts.day);
  chsnprintf(hdr, n, "%-16.16s%02u:%02u", date, h, m);
}

static void dispHeader(bool connected) {
  if (!connected) {
    /* Blink the header between warning and blank to signal disconnection */
    static uint8_t blink = 0;
    blink ^= 1;
    ssd1309Print(0, blink ? "!! NO CONTACT !!" : "                ");
    return;
  }
  char hdr[22];
  dispFormatDateTime(hdr, sizeof(hdr));
  ssd1309Print(0, hdr);
}

/* Solid inverted header bar (dark text on lit background) — Armed Home/Away. */
static void dispHeaderInverted(void) {
  char hdr[22];
  dispFormatDateTime(hdr, sizeof(hdr));
  ssd1309PrintInverted(0, hdr);
}

/* Bottom bar: zone/node name left, connectivity icon right — pages 6-7. */
static void dispFooter(void) {
  char nodeName[NODE_NAME_SIZE + 1];
  memcpy(nodeName, &conf.reg[5], NODE_NAME_SIZE);
  nodeName[NODE_NAME_SIZE] = '\0';
  for (int8_t i = NODE_NAME_SIZE - 1; i >= 0 && nodeName[i] == ' '; i--)
    nodeName[i] = '\0';

  ssd1309HlineBottom(6);
  ssd1309Print(7, nodeName);
  /* Static for now — the mockup shows "TRY N"/no icon when disconnected,
   * which belongs to the not-yet-built Offline screen. */
  ssd1309DrawBitmap(7, 116, iconSignalSmall, 8, 1);
}

/* Footer for the Startup and Offline screens: name left, "Retry(NN)"
 * right once an attempt to reach the gateway has gone unanswered
 * (dispCountdownSecs > 0) — blank there otherwise.
 * "Retry(NN)" is always 9 chars (zero-padded) so it doesn't shift around
 * as the count drops from 10 to single digits; the name field is sized
 * to exactly fill the rest of the 21-char (126px) line — a fixed 16-char
 * name field (as the main dispFooter() uses, with no right-side text to
 * share the line with) left only 5 chars for this, which ran off the
 * right edge of the screen instead of actually being visible. */
static void dispRetryFooter(void) {
  char name[NODE_NAME_SIZE + 1];
  memcpy(name, &conf.reg[5], NODE_NAME_SIZE);
  name[NODE_NAME_SIZE] = '\0';
  for (int8_t i = NODE_NAME_SIZE - 1; i >= 0 && name[i] == ' '; i--)
    name[i] = '\0';

  char footer[22];
  if (dispCountdownSecs > 0) {
    char retry[10];
    chsnprintf(retry, sizeof(retry), "Retry(%02u)", (unsigned)dispCountdownSecs);
    chsnprintf(footer, sizeof(footer), "%-12.12s%s", name, retry);
  } else {
    chsnprintf(footer, sizeof(footer), "%-16.16s", name);
  }
  ssd1309Print(7, footer);
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
      if (initErrorFlags & INIT_ERROR_FP) {
        ssd1309PrintBig(3, "FP error!");
        ssd1309Print(5, "  Check sensor");
        dispFooter();
      } else if (initErrorFlags & INIT_ERROR_NFC) {
        ssd1309PrintBig(3, "NFC error!");
        ssd1309Print(5, "  Check sensor");
        dispFooter();
      } else {
        // Icon + "DISARMED" / "OHS READY", icon at cols 4-21 (pages 2-4),
        // text starting after it at col 26 — see the Disarmed mockup.
        // DISARMED double-height (16px, the biggest the font supports) so
        // it reads clearly bigger than the single-height OHS READY below it.
        ssd1309Print(3, "");
        ssd1309Print(4, "");
        ssd1309DrawBitmap(2, 4, iconShieldOk, 18, 3);
        ssd1309PrintBigAt(2, 26, "DISARMED");
        ssd1309PrintAt(4, 26, "OHS READY");
        ssd1309Print(5, "");
        dispFooter();
      }
      break;

    case MODE_ARMED_HOME: {
      // Inverted header (overwrites the normal one the shared preamble just
      // drew) + icon/text block + footer — see the Armed Home mockup.
      RTCDateTime ts;
      rtcGetTime(&RTCD1, &ts);
      uint32_t t = convertRTCDateTimeToUnixSecond(&ts);
      uint8_t ah = (uint8_t)((t % 86400UL) / 3600UL);
      uint8_t am = (uint8_t)((t % 3600UL) / 60UL);
      chsnprintf(sub, sizeof(sub), "SINCE %02u:%02u", ah, am);

      dispHeaderInverted();
      ssd1309Print(1, ""); // no separator line on this screen
      ssd1309Print(3, "");
      ssd1309Print(4, "");
      ssd1309Print(5, "");
      ssd1309DrawBitmap(2, 4, iconLock, 18, 3);
      ssd1309PrintAt(2, 26, "ARMED");
      ssd1309PrintBigAt(3, 26, "HOME");
      ssd1309PrintAt(5, 26, sub);
      dispFooter();
      break;
    }

    case MODE_ARMED_AWAY:
      // Inverted header, same as Armed Home, but the middle stays blank —
      // discreet, does not reveal armed state — and the footer still
      // shows, per the Armed Away mockup.
      dispHeaderInverted();
      dispClearPages(1, 5);
      dispFooter();
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

    case MODE_OFFLINE_DISP: {
      // Icon + "NO LINK"/"RECONNECTING" + retry footer — see the Offline
      // mockup. No header, same reasoning as Startup's. The actual
      // retry/ping cycle lives in ServiceThread; this just displays it.
      ssd1309Print(0, "");
      ssd1309Print(1, "");
      ssd1309Print(3, "");
      ssd1309Print(4, "");
      ssd1309DrawBitmap(2, 4, iconLinkBroken, 22, 3);
      ssd1309PrintBigAt(2, 30, "NO LINK");
      ssd1309PrintAt(4, 30, "RECONNECTING");
      ssd1309Print(5, "");
      ssd1309HlineBottom(6);
      dispRetryFooter();
      break;
    }

    case MODE_UNINITIALIZED:
    default:
      if (initErrorFlags & INIT_ERROR_FP) {
        ssd1309PrintBig(3, "FP error!");
        ssd1309Print(5, "  Sensor missing");
        dispClearPages(6, 7);
      } else if (initErrorFlags & INIT_ERROR_NFC) {
        ssd1309PrintBig(3, "NFC error!");
        ssd1309Print(5, "  Sensor missing");
        dispClearPages(6, 7);
      } else {
        // No header/date on this screen (clock isn't synced yet) — big
        // centered "OHS" title, icon + "CONNECTING" below it, footer with
        // the name and, once a registration attempt has gone unanswered, a
        // live retry countdown bottom-right. See the Startup mockup. The
        // mockup's 3-dot progress indicator is simplified away — a static
        // text line, no real animation to show.
        ssd1309Print(0, "");
        ssd1309Print(1, "");
        ssd1309PrintBigAt(2, 46, "OHS");
        ssd1309Print(4, "");
        ssd1309DrawBitmap(4, 28, iconSignalSmall, 8, 1);
        ssd1309PrintAt(4, 40, "CONNECTING");
        ssd1309Print(5, "");
        ssd1309HlineBottom(6);
        dispRetryFooter();
      }
      break;
  }
}

/* -------------------------------------------------------------------------
 * Display thread
 * ------------------------------------------------------------------------- */
/* Bumped from 384: that was marginal once DBG() calls landed at deeper
 * nesting in the update loop (chprintf + the call chain through dispRender/
 * dispHeader/ssd1309*) — intermittent crashes traced back to this thread's
 * stack running out. 20KB of SRAM is available; this is cheap insurance. */
static THD_WORKING_AREA(waDisplayThread, 768);
static THD_FUNCTION(DisplayThread, arg) {
  (void)arg;
  chRegSetThreadName("display");

  /* Run display probe + init here at LOWPRIO so it doesn't block main thread.
   * Some OLED modules are still completing their own power-on sequence by the
   * time this thread runs and NACK the first probe, so retry a few times
   * with a short delay rather than giving up on a single failed attempt. */
  for (uint8_t attempt = 0; attempt < 5 && !dispPresent; attempt++) {
    if (attempt) chThdSleepMilliseconds(100);
    ssd1309Init();
  }

  if (!dispPresent) {
    chThdExit(MSG_OK);
    return;
  }

  ssd1309Clear();

  // Deliberately not a real mode: nodeState.mode also starts at
  // MODE_UNINITIALIZED, so initializing lastMode to that same value would
  // make the first loop pass see "no change" and skip dispRender()
  // entirely, leaving the screen blank until the mode actually changes.
  authMode_t lastMode       = (authMode_t)0xFF;
  uint8_t    lastSecs       = 0;
  uint8_t    lastStep       = 0xFF;
  uint8_t    lastErrorFlags = 0;
  uint8_t    tickCount      = 0;
  uint8_t    headerTick     = 0;

  while (true) {
    chThdSleepMilliseconds(250);

    /* Gateway connection: disconnected after GW_TIMEOUT_SECS with no message */
    bool connected = (lastGwContact != 0) &&
                     ((nowUnixSec() - lastGwContact) < GW_TIMEOUT_SECS);

    /* Hold mode overrides actual mode for transient messages. Lost gateway
     * contact takes over next (but not during Startup's own "never yet
     * connected" wait — main() already owns that one's retry/display). */
    authMode_t mode;
    if (dispHoldTicks > 0) {
      dispHoldTicks--;
      mode = (authMode_t)dispHoldMode;
    } else if (!connected && nodeState.mode != MODE_UNINITIALIZED) {
      mode = MODE_OFFLINE_DISP;
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

    bool modeChanged  = (mode != lastMode);
    bool stepChanged  = (mode == MODE_ENROLLMENT) && (dispEnrollStep != lastStep);
    bool errorChanged = (initErrorFlags != lastErrorFlags);
    bool countdownTicked = (dispCountdownSecs != lastSecs) &&
                           (mode == MODE_ARMING ||
                            mode == MODE_AUTH_1 || mode == MODE_AUTH_2 || mode == MODE_AUTH_3 ||
                            mode == MODE_UNINITIALIZED || mode == MODE_OFFLINE_DISP);
    bool headerChanged = (headerTick >= 8); // ~2s (also drives disconnect blink rate)

    if (modeChanged || stepChanged || errorChanged) {
      if (mode != NODE_ARM_REJECTED) dispZoneName[0] = '\0';
      dispRender(mode);
      lastMode       = mode;
      lastSecs       = dispCountdownSecs;
      lastStep       = dispEnrollStep;
      lastErrorFlags = initErrorFlags;
      headerTick     = 0;
    } else if (countdownTicked) {
      if (mode == MODE_UNINITIALIZED || mode == MODE_OFFLINE_DISP) {
        dispRetryFooter(); // no header/bar on this screen — see dispRender()
      } else {
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
      }
      lastSecs = dispCountdownSecs;
      headerTick = 0;
    } else if (headerChanged) {
      // Startup and Offline have no header row at all (page 0 is left
      // blank) — skip it, or this blinks "!! NO CONTACT !!" there every
      // ~2s, since `connected` is false by definition on both screens.
      if (mode == MODE_UNINITIALIZED || mode == MODE_OFFLINE_DISP) {
        // nothing to refresh
      } else if (mode == MODE_ARMED_HOME || mode == MODE_ARMED_AWAY) {
        dispHeaderInverted();
      } else {
        dispHeader(connected);
      }
      headerTick = 0;
    }
  }
}

#endif /* OHS_TH_DISPLAY_H_ */
