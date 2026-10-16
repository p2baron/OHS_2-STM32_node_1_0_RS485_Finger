/*
 * ohs_ssd1309.h — SSD1309 128x64 OLED driver for ChibiOS/STM32F103
 *
 * I2C1, PB8=SCL, PB9=SDA.  Default I2C address 0x3C (SA0 pin low).
 * No framebuffer — writes directly to the display one page at a time.
 *
 * If the display is absent (I2C NACK on init), dispPresent is set false
 * and all subsequent calls become no-ops.  Node runs normally without display.
 */

#ifndef OHS_SSD1309_H_
#define OHS_SSD1309_H_

#include "hal.h"
#include <string.h>
#include <stdbool.h>

#define SSD1309_ADDR    0x3D   // 7-bit; 0x3C (SA0 low) or 0x3D (SA0 high)

static bool    dispPresent = false;
static uint8_t dispAddr    = SSD1309_ADDR; // resolved at init

/* Gateway connection tracking.
 *
 * Unix seconds (RTC-derived), NOT chVTGetSystemTimeX() ticks: this board
 * builds with CH_CFG_ST_RESOLUTION=16, so systime_t is a 16-bit tick
 * counter that wraps every 65536 ticks — ~32.8s at the 2000Hz tick rate.
 * GW_TIMEOUT_SECS (120s = 240000 ticks) can't even be represented in that
 * type, so a chTimeDiffX()-based comparison against it can mathematically
 * never trip — confirmed live via SWD (lastGwContact and the derived
 * countdown both sat frozen through a disconnect test that should have
 * triggered it). Unix seconds (uint32_t, ~136 year range) have no such
 * ceiling. */
#define GW_TIMEOUT_SECS  120  // seconds without a GW message = disconnected
static volatile uint32_t lastGwContact = 0; // updated on every GW message received

static uint32_t nowUnixSec(void) {
  RTCDateTime ts;
  rtcGetTime(&RTCD1, &ts);
  return convertRTCDateTimeToUnixSecond(&ts);
}

/* Display state — written by RS485/main threads, read by display thread */
volatile uint8_t dispCountdownSecs = 0;
volatile uint8_t dispCountdownMax  = 0;
volatile uint8_t dispEnrollSlot    = 0;
volatile uint8_t dispEnrollStep    = 0;  // 0=place, 1=remove, 2=place again, 3=creating, 4=OK
volatile uint8_t dispHoldTicks     = 0;  // hold a transient screen (250ms units)
volatile uint8_t dispHoldMode      = 0;  // mode to show while holding
char             dispZoneName[17]  = {0};

/* --------------------------------------------------------------------------
 * 5x7 font, 96 printable ASCII chars (0x20 .. 0x7F).
 * Each entry: 5 column bytes.  bit0 = top row, bit6 = bottom row.
 * Rendered with one implicit zero-column gap → effective 6x8 cell.
 * -------------------------------------------------------------------------- */
static const uint8_t ssd1309Font[96][5] = {
  {0x00,0x00,0x00,0x00,0x00}, // 0x20  ' '
  {0x00,0x00,0x5F,0x00,0x00}, // 0x21  !
  {0x00,0x07,0x00,0x07,0x00}, // 0x22  "
  {0x14,0x7F,0x14,0x7F,0x14}, // 0x23  #
  {0x24,0x2A,0x7F,0x2A,0x12}, // 0x24  $
  {0x23,0x13,0x08,0x64,0x62}, // 0x25  %
  {0x36,0x49,0x55,0x22,0x50}, // 0x26  &
  {0x00,0x05,0x03,0x00,0x00}, // 0x27  '
  {0x00,0x1C,0x22,0x41,0x00}, // 0x28  (
  {0x00,0x41,0x22,0x1C,0x00}, // 0x29  )
  {0x08,0x2A,0x1C,0x2A,0x08}, // 0x2A  *
  {0x08,0x08,0x3E,0x08,0x08}, // 0x2B  +
  {0x00,0x50,0x30,0x00,0x00}, // 0x2C  ,
  {0x08,0x08,0x08,0x08,0x08}, // 0x2D  -
  {0x00,0x60,0x60,0x00,0x00}, // 0x2E  .
  {0x20,0x10,0x08,0x04,0x02}, // 0x2F  /
  {0x3E,0x51,0x49,0x45,0x3E}, // 0x30  0
  {0x00,0x42,0x7F,0x40,0x00}, // 0x31  1
  {0x42,0x61,0x51,0x49,0x46}, // 0x32  2
  {0x21,0x41,0x45,0x4B,0x31}, // 0x33  3
  {0x18,0x14,0x12,0x7F,0x10}, // 0x34  4
  {0x27,0x45,0x45,0x45,0x39}, // 0x35  5
  {0x3C,0x4A,0x49,0x49,0x30}, // 0x36  6
  {0x01,0x71,0x09,0x05,0x03}, // 0x37  7
  {0x36,0x49,0x49,0x49,0x36}, // 0x38  8
  {0x06,0x49,0x49,0x29,0x1E}, // 0x39  9
  {0x00,0x36,0x36,0x00,0x00}, // 0x3A  :
  {0x00,0x56,0x36,0x00,0x00}, // 0x3B  ;
  {0x08,0x14,0x22,0x41,0x00}, // 0x3C  <
  {0x14,0x14,0x14,0x14,0x14}, // 0x3D  =
  {0x00,0x41,0x22,0x14,0x08}, // 0x3E  >
  {0x02,0x01,0x51,0x09,0x06}, // 0x3F  ?
  {0x32,0x49,0x79,0x41,0x3E}, // 0x40  @
  {0x7E,0x11,0x11,0x11,0x7E}, // 0x41  A
  {0x7F,0x49,0x49,0x49,0x36}, // 0x42  B
  {0x3E,0x41,0x41,0x41,0x22}, // 0x43  C
  {0x7F,0x41,0x41,0x22,0x1C}, // 0x44  D
  {0x7F,0x49,0x49,0x49,0x41}, // 0x45  E
  {0x7F,0x09,0x09,0x09,0x01}, // 0x46  F
  {0x3E,0x41,0x49,0x49,0x7A}, // 0x47  G
  {0x7F,0x08,0x08,0x08,0x7F}, // 0x48  H
  {0x00,0x41,0x7F,0x41,0x00}, // 0x49  I
  {0x20,0x40,0x41,0x3F,0x01}, // 0x4A  J
  {0x7F,0x08,0x14,0x22,0x41}, // 0x4B  K
  {0x7F,0x40,0x40,0x40,0x40}, // 0x4C  L
  {0x7F,0x02,0x0C,0x02,0x7F}, // 0x4D  M
  {0x7F,0x04,0x08,0x10,0x7F}, // 0x4E  N
  {0x3E,0x41,0x41,0x41,0x3E}, // 0x4F  O
  {0x7F,0x09,0x09,0x09,0x06}, // 0x50  P
  {0x3E,0x41,0x51,0x21,0x5E}, // 0x51  Q
  {0x7F,0x09,0x19,0x29,0x46}, // 0x52  R
  {0x46,0x49,0x49,0x49,0x31}, // 0x53  S
  {0x01,0x01,0x7F,0x01,0x01}, // 0x54  T
  {0x3F,0x40,0x40,0x40,0x3F}, // 0x55  U
  {0x1F,0x20,0x40,0x20,0x1F}, // 0x56  V
  {0x3F,0x40,0x38,0x40,0x3F}, // 0x57  W
  {0x63,0x14,0x08,0x14,0x63}, // 0x58  X
  {0x07,0x08,0x70,0x08,0x07}, // 0x59  Y
  {0x61,0x51,0x49,0x45,0x43}, // 0x5A  Z
  {0x00,0x7F,0x41,0x41,0x00}, // 0x5B  [
  {0x02,0x04,0x08,0x10,0x20}, // 0x5C  backslash
  {0x00,0x41,0x41,0x7F,0x00}, // 0x5D  ]
  {0x04,0x02,0x01,0x02,0x04}, // 0x5E  ^
  {0x40,0x40,0x40,0x40,0x40}, // 0x5F  _
  {0x00,0x01,0x02,0x04,0x00}, // 0x60  `
  {0x20,0x54,0x54,0x54,0x78}, // 0x61  a
  {0x7F,0x48,0x44,0x44,0x38}, // 0x62  b
  {0x38,0x44,0x44,0x44,0x20}, // 0x63  c
  {0x38,0x44,0x44,0x48,0x7F}, // 0x64  d
  {0x38,0x54,0x54,0x54,0x18}, // 0x65  e
  {0x08,0x7E,0x09,0x01,0x02}, // 0x66  f
  {0x0C,0x52,0x52,0x52,0x3E}, // 0x67  g
  {0x7F,0x08,0x04,0x04,0x78}, // 0x68  h
  {0x00,0x44,0x7D,0x40,0x00}, // 0x69  i
  {0x20,0x40,0x44,0x3D,0x00}, // 0x6A  j
  {0x7F,0x10,0x28,0x44,0x00}, // 0x6B  k
  {0x00,0x41,0x7F,0x40,0x00}, // 0x6C  l
  {0x7C,0x04,0x18,0x04,0x7C}, // 0x6D  m
  {0x7C,0x08,0x04,0x04,0x78}, // 0x6E  n
  {0x38,0x44,0x44,0x44,0x38}, // 0x6F  o
  {0x7C,0x14,0x14,0x14,0x08}, // 0x70  p
  {0x08,0x14,0x14,0x18,0x7C}, // 0x71  q
  {0x7C,0x08,0x04,0x04,0x08}, // 0x72  r
  {0x48,0x54,0x54,0x54,0x20}, // 0x73  s
  {0x04,0x3F,0x44,0x40,0x20}, // 0x74  t
  {0x3C,0x40,0x40,0x20,0x7C}, // 0x75  u
  {0x1C,0x20,0x40,0x20,0x1C}, // 0x76  v
  {0x3C,0x40,0x30,0x40,0x3C}, // 0x77  w
  {0x44,0x28,0x10,0x28,0x44}, // 0x78  x
  {0x0C,0x50,0x50,0x50,0x3C}, // 0x79  y
  {0x44,0x64,0x54,0x4C,0x44}, // 0x7A  z
  {0x00,0x08,0x36,0x41,0x00}, // 0x7B  {
  {0x00,0x00,0x7F,0x00,0x00}, // 0x7C  |
  {0x00,0x41,0x36,0x08,0x00}, // 0x7D  }
  {0x08,0x08,0x2A,0x1C,0x08}, // 0x7E  ~  (rendered as right-arrow)
  {0x7F,0x41,0x41,0x41,0x7F}, // 0x7F  DEL (solid block — used as fill)
};

/* Expand 4-bit nibble to 8 bits by doubling each bit (for double-height) */
static const uint8_t ssd1309Expand[16] = {
  0x00,0x03,0x0C,0x0F,0x30,0x33,0x3C,0x3F,
  0xC0,0xC3,0xCC,0xCF,0xF0,0xF3,0xFC,0xFF
};

/* -------------------------------------------------------------------------
 * Low-level helpers
 * ------------------------------------------------------------------------- */

/* Raw register-level I2C write, bypassing ChibiOS's DMA-backed
 * i2cMasterTransmitTimeout(): that call hangs indefinitely on this board's
 * STM32F103 I2Cv1 LLD (confirmed via SWD — the same transactions complete
 * instantly when driven directly through CR1/DR/SR1/SR2), while the
 * interrupt/DMA-driven path never completes. Polling-based, so it blocks
 * the calling thread for the duration of the transfer instead of yielding. */
volatile uint32_t i2cRawTxFailCount = 0; // diagnostic: non-zero means _i2cRawTx has failed at least once since boot

static bool _i2cRawTx(uint8_t addr7, const uint8_t *data, uint8_t len) {
  uint32_t guard;

  I2C1->CR1 |= I2C_CR1_START;
  guard = 100000;
  while (!(I2C1->SR1 & I2C_SR1_SB) && --guard) ;
  if (!guard) { i2cRawTxFailCount++; DBG("_i2cRawTx: SB timeout, SR1=%04X SR2=%04X\r\n", (unsigned)I2C1->SR1, (unsigned)I2C1->SR2); I2C1->CR1 |= I2C_CR1_STOP; return false; }

  I2C1->DR = (uint8_t)(addr7 << 1);
  guard = 100000;
  while (!(I2C1->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF)) && --guard) ;
  if (!guard || (I2C1->SR1 & I2C_SR1_AF)) {
    i2cRawTxFailCount++;
    DBG("_i2cRawTx: addr 0x%02X fail, guard=%lu SR1=%04X\r\n", addr7, (unsigned long)guard, (unsigned)I2C1->SR1);
    I2C1->SR1 &= ~I2C_SR1_AF;
    I2C1->CR1 |= I2C_CR1_STOP;
    return false;
  }
  (void)I2C1->SR2; // reading SR2 after SR1/ADDR clears the ADDR flag

  for (uint8_t i = 0; i < len; i++) {
    guard = 100000;
    while (!(I2C1->SR1 & I2C_SR1_TXE) && --guard) ;
    if (!guard) { i2cRawTxFailCount++; DBG("_i2cRawTx: TXE timeout at byte %d/%d, SR1=%04X\r\n", (int)i, (int)len, (unsigned)I2C1->SR1); I2C1->CR1 |= I2C_CR1_STOP; return false; }
    I2C1->DR = data[i];
  }
  guard = 100000;
  while (!(I2C1->SR1 & I2C_SR1_BTF) && --guard) ;
  I2C1->CR1 |= I2C_CR1_STOP;
  return true;
}

/* Some boards see a much higher transient failure rate on this bus than
 * others (observed: dozens of failures within seconds on one node, vs
 * effectively none on another, same firmware) — likely signal-integrity
 * differences (pull-ups, wiring) rather than anything software can fix
 * outright. Retry a few times so a frequently-updated page (e.g. the
 * header, redrawn every ~2s) doesn't get stuck showing stale content
 * after one failed write with nothing to ever retry it. */
static bool _i2cRawTxRetry(uint8_t addr7, const uint8_t *data, uint8_t len) {
  for (uint8_t attempt = 0; attempt < 4; attempt++) {
    if (_i2cRawTx(addr7, data, len)) return true;
  }
  return false;
}

static void _ssd1309TxCmd(const uint8_t *cmds, uint8_t len) {
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, cmds, len);
  i2cReleaseBus(&I2CD1);
}

/* Set page-addressing cursor to (page, col) */
static void _ssd1309SetPos(uint8_t page, uint8_t col) {
  uint8_t cmds[4] = {
    0x00,
    (uint8_t)(0xB0 | (page & 0x07)),
    (uint8_t)(0x00 | (col & 0x0F)),
    (uint8_t)(0x10 | (col >> 4)),
  };
  _ssd1309TxCmd(cmds, 4);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/*
 * Initialise display.  Sets dispPresent = true on success, false if absent.
 * Non-blocking: a missing display will NACK and dispPresent stays false.
 */
static void ssd1309Init(void) {
  static const uint8_t initSeq[] = {
    0x00,    // following bytes are commands
    0xAE,    // display off
    0xD5, 0x80, // clock / osc freq
    0xA8, 0x3F, // multiplex ratio (64 rows)
    0xD3, 0x00, // display offset = 0
    0x40,    // start line = 0
    0x8D, 0x14, // charge pump ON (for modules with built-in booster)
    0x20, 0x02, // page addressing mode
    0xA1,    // segment remap (col 127 = SEG0)
    0xC8,    // COM scan direction reversed
    0xDA, 0x12, // COM pin config
    0x81, 0xCF, // contrast
    0xD9, 0xF1, // precharge
    0xDB, 0x40, // VCOMH deselect
    0xA4,    // output follows RAM
    0xA6,    // normal (non-inverted) display
    0x2E,    // deactivate scroll
    0xAF,    // display on
  };

  /* main.c releases I2C1 (i2cStop) at boot if the bus never came up clean —
   * a stuck-busy peripheral floods the NVIC with high-priority error IRQs
   * that disrupt the buzzer/sensor, so it's better left off. Mirror that
   * decision here via driver state rather than re-reading SR2_BUSY, since
   * the peripheral may now be unclocked. */
  if (I2CD1.state != I2C_READY) {
    DBG("SSD1309: I2C1 not ready, skipping\r\n");
    return;
  }
  static const uint8_t addrs[2] = {0x3C, 0x3D};
  uint8_t probe[1] = {0x00};
  dispPresent = false;
  DBG("SSD1309: probing...\r\n");
  for (uint8_t ai = 0; ai < 2; ai++) {
    i2cAcquireBus(&I2CD1);
    bool ok = _i2cRawTx(addrs[ai], probe, 1);
    i2cReleaseBus(&I2CD1);
    if (ok) {
      DBG("SSD1309: ACK at 0x%02X\r\n", addrs[ai]);
      dispAddr    = addrs[ai];
      dispPresent = true;
      break;
    }
    i2cStop(&I2CD1);
    i2cStart(&I2CD1, &i2c1cfg);
  }
  if (!dispPresent) {
    DBG("SSD1309: no device found\r\n");
    return;
  }
  DBG("SSD1309: using address 0x%02X\r\n", dispAddr);
  _ssd1309TxCmd(initSeq, sizeof(initSeq));
}

/* Clear entire display — one 129-byte transaction per page (8 total) */
static void ssd1309Clear(void) {
  if (!dispPresent) return;
  static uint8_t buf[129]; // static: BSS not stack
  buf[0] = 0x40;
  memset(&buf[1], 0x00, 128);
  for (uint8_t page = 0; page < 8; page++) {
    _ssd1309SetPos(page, 0);
    i2cAcquireBus(&I2CD1);
    _i2cRawTxRetry(dispAddr, buf, 129);
    i2cReleaseBus(&I2CD1);
  }
}

/*
 * Print a text string at the given page (row 0-7) starting at column 0.
 * Up to 21 chars per line (21 × 6 = 126 px).  Pads remainder with spaces.
 */
static void ssd1309Print(uint8_t page, const char *s) {
  if (!dispPresent) return;
  static uint8_t txBuf[129]; // static: BSS not stack — full 128 px width
                              // (not just the 126 the 21-char text area
                              // needs): the last 2 columns must still get
                              // cleared, or stale lit pixels from some
                              // other full-width draw (Hline, Bar, Clear)
                              // show through there on the next redraw.
  txBuf[0] = 0x40;
  uint8_t col = 0;
  while (*s && col < 126) { // 21 chars × 6 px = 126 px (display is 128 px)
    uint8_t c = (uint8_t)*s++;
    if (c < 0x20 || c > 0x7F) c = 0x20;
    const uint8_t *g = ssd1309Font[c - 0x20];
    txBuf[1 + col]     = g[0];
    txBuf[1 + col + 1] = g[1];
    txBuf[1 + col + 2] = g[2];
    txBuf[1 + col + 3] = g[3];
    txBuf[1 + col + 4] = g[4];
    txBuf[1 + col + 5] = 0x00; // inter-char gap
    col += 6;
  }
  memset(&txBuf[1 + col], 0x00, 128 - col); // pad to the full display width
  _ssd1309SetPos(page, 0);
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, txBuf, 129);
  i2cReleaseBus(&I2CD1);
}

/*
 * Like ssd1309Print(), but dark text on a filled (lit) background — the
 * whole page is lit except the glyph pixels themselves, i.e. every byte
 * is the bitwise complement of what ssd1309Print() would send. Used for
 * the inverted header bar on Armed Home/Away.
 */
static void ssd1309PrintInverted(uint8_t page, const char *s) {
  if (!dispPresent) return;
  static uint8_t txBuf[129]; // static: BSS not stack — full 128 px width,
                              // unlike ssd1309Print()'s 126: a lit background
                              // needs to reach both edges or the last 2
                              // columns show as an unlit notch.
  txBuf[0] = 0x40;
  uint8_t col = 0;
  while (*s && col < 126) {
    uint8_t c = (uint8_t)*s++;
    if (c < 0x20 || c > 0x7F) c = 0x20;
    const uint8_t *g = ssd1309Font[c - 0x20];
    txBuf[1 + col]     = (uint8_t)~g[0];
    txBuf[1 + col + 1] = (uint8_t)~g[1];
    txBuf[1 + col + 2] = (uint8_t)~g[2];
    txBuf[1 + col + 3] = (uint8_t)~g[3];
    txBuf[1 + col + 4] = (uint8_t)~g[4];
    txBuf[1 + col + 5] = 0xFF; // inter-char gap, lit (background)
    col += 6;
  }
  memset(&txBuf[1 + col], 0xFF, 128 - col); // pad to the full display width, lit
  _ssd1309SetPos(page, 0);
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, txBuf, 129);
  i2cReleaseBus(&I2CD1);
}

/*
 * Print a text string at the given page, starting at an arbitrary pixel
 * column. Unlike ssd1309Print(), writes only the glyph columns the string
 * actually needs — it does not clear anything before `col` or after the
 * string — so it can sit alongside other content already drawn on the same
 * page (e.g. an icon to its left). Caller is responsible for clearing the
 * page first if stale content could otherwise show through.
 */
static void ssd1309PrintAt(uint8_t page, uint8_t col, const char *s) {
  if (!dispPresent) return;
  static uint8_t txBuf[127]; // static: BSS not stack
  txBuf[0] = 0x40;
  uint8_t n = 0;
  while (*s && (uint16_t)col + n + 6 <= 128) {
    uint8_t c = (uint8_t)*s++;
    if (c < 0x20 || c > 0x7F) c = 0x20;
    const uint8_t *g = ssd1309Font[c - 0x20];
    txBuf[1 + n]     = g[0];
    txBuf[1 + n + 1] = g[1];
    txBuf[1 + n + 2] = g[2];
    txBuf[1 + n + 3] = g[3];
    txBuf[1 + n + 4] = g[4];
    txBuf[1 + n + 5] = 0x00; // inter-char gap
    n += 6;
  }
  if (n == 0) return;
  _ssd1309SetPos(page, col);
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, txBuf, (uint8_t)(n + 1));
  i2cReleaseBus(&I2CD1);
}

/*
 * Draw a monochrome bitmap at (page, col). `bmp` is `pageSpan` page-rows of
 * `cols` bytes each, row-major (all columns for the first page, then the
 * next), one byte per column per page — the same column-major, bit0=top
 * layout as the font. Like ssd1309PrintAt(), touches only its own
 * footprint; caller clears the page(s) first if needed.
 */
static void ssd1309DrawBitmap(uint8_t page, uint8_t col, const uint8_t *bmp,
                               uint8_t cols, uint8_t pageSpan) {
  if (!dispPresent) return;
  static uint8_t buf[129]; // static: BSS not stack
  buf[0] = 0x40;
  for (uint8_t p = 0; p < pageSpan; p++) {
    memcpy(&buf[1], &bmp[(uint16_t)p * cols], cols);
    _ssd1309SetPos((uint8_t)(page + p), col);
    i2cAcquireBus(&I2CD1);
    _i2cRawTxRetry(dispAddr, buf, (uint8_t)(cols + 1));
    i2cReleaseBus(&I2CD1);
  }
}

/* Small 8x8 "signal / connected" glyph — ascending bars, 1 page tall. */
static const uint8_t iconSignalSmall[8] = {
  0xC0, 0x00, 0xF0, 0x00, 0xFC, 0x00, 0xFF, 0x00
};

/* 18x20 shield-with-checkmark glyph (status-ready icon), 3 pages tall
 * (rows 16-19 of the 3rd page are unused/blank — 20 isn't a multiple of 8).
 * One byte per column per page, bit0 = top row of that page. Converted
 * from a row-major bitmap and verified by decoding it back to ASCII before
 * committing — see the lessons memory. */
static const uint8_t iconShieldOk[54] = {
  0xF0, 0xF0, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x03, 0x03, 0x06, 0x06, 0x8C, 0x8C, 0x18, 0x18, 0xF0, 0xF0,
  0x0F, 0x3F, 0x70, 0xC0, 0x84, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x80, 0xC0, 0x70, 0x3F, 0x0F,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x02, 0x06, 0x0C, 0x0C, 0x06, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00
};

/* 18x22 padlock glyph (armed-home icon), 3 pages tall (rows 22-23 of the
 * 3rd page unused). Converted from a row-major bitmap and verified by
 * decoding it back to ASCII before committing. */
static const uint8_t iconLock[54] = {
  0x00, 0x00, 0x00, 0xF8, 0xFC, 0x0E, 0x07, 0x03, 0x03, 0x03, 0x03, 0x07, 0x0E, 0xFC, 0xF8, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, 0x03, 0x33, 0xF3, 0xF3, 0x33, 0x03, 0x03, 0x03, 0x03, 0x03, 0xFF, 0xFF,
  0x3F, 0x3F, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3F, 0x3F
};

/* 22x22 broken-link glyph (offline icon), 3 pages tall (rows 22-23 of the
 * 3rd page unused). Converted from a row-major bitmap and verified by
 * decoding it back to ASCII before committing. */
static const uint8_t iconLinkBroken[66] = {
  0x00, 0x00, 0x00, 0x38, 0x38, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x18, 0x1C, 0x1E, 0x0F, 0x07, 0x07, 0x07, 0x8F, 0xFE, 0xFC, 0xF8,
  0xC0, 0xE0, 0xF0, 0x78, 0x3C, 0x1E, 0x0F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x3C, 0x1E, 0x0F, 0x07, 0x03, 0x01, 0x00,
  0x07, 0x0F, 0x1F, 0x3C, 0x38, 0x38, 0x38, 0x3C, 0x1E, 0x0E, 0x06, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x07, 0x07, 0x00, 0x00, 0x00
};

/*
 * Draw a 1-pixel-high horizontal separator line at the top of the given page.
 * bit 0 = top row, so 0x01 gives a single thin line.
 */
static void ssd1309Hline(uint8_t page) {
  if (!dispPresent) return;
  static uint8_t buf[129];
  buf[0] = 0x40;
  memset(&buf[1], 0x01, 128); // single pixel row at top of page
  _ssd1309SetPos(page, 0);
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, buf, 129);
  i2cReleaseBus(&I2CD1);
}

/*
 * Draw a 1-pixel-high horizontal separator line one row above the bottom
 * of the given page, leaving a 1px gap before whatever starts on the next
 * page — matching the 1px gap ssd1309Hline() leaves below itself (the 5x7
 * font's unused 8th row) before the text below it.
 */
static void ssd1309HlineBottom(uint8_t page) {
  if (!dispPresent) return;
  static uint8_t buf[129];
  buf[0] = 0x40;
  memset(&buf[1], 0x40, 128); // single pixel row, one above the bottom of the page
  _ssd1309SetPos(page, 0);
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, buf, 129);
  i2cReleaseBus(&I2CD1);
}

/*
 * Print double-height text at pages (page) and (page+1).
 * Each char is 12 px wide × 16 px tall — fits 10 chars on 128 px line.
 */
static void ssd1309PrintBig(uint8_t page, const char *s) {
  if (!dispPresent) return;
  static uint8_t topBuf[129]; // static: BSS not stack — full 128 px width,
                              // not just the 120 the 10-char text area
                              // needs (same reasoning as ssd1309Print()).
  static uint8_t botBuf[129];
  topBuf[0] = 0x40;
  botBuf[0] = 0x40;
  uint8_t col = 0;
  while (*s && col < 120) { // 10 chars × 12 px = 120 px (fits in 128 px)
    uint8_t c = (uint8_t)*s++;
    if (c < 0x20 || c > 0x7F) c = 0x20;
    const uint8_t *g = ssd1309Font[c - 0x20];
    for (uint8_t sc = 0; sc < 5; sc++) {
      uint8_t src = g[sc];
      uint8_t top = ssd1309Expand[src & 0x0F];
      uint8_t bot = ssd1309Expand[(src >> 4) & 0x0F];
      // Each source column → 2 destination columns
      topBuf[1 + col]     = top;
      topBuf[1 + col + 1] = top;
      botBuf[1 + col]     = bot;
      botBuf[1 + col + 1] = bot;
      col += 2;
    }
    // Inter-char gap: 2 zero columns
    topBuf[1 + col]     = 0;
    topBuf[1 + col + 1] = 0;
    botBuf[1 + col]     = 0;
    botBuf[1 + col + 1] = 0;
    col += 2;
  }
  memset(&topBuf[1 + col], 0, 128 - col);
  memset(&botBuf[1 + col], 0, 128 - col);

  _ssd1309SetPos(page,     0);
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, topBuf, 129);
  i2cReleaseBus(&I2CD1);
  _ssd1309SetPos(page + 1, 0);
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, botBuf, 129);
  i2cReleaseBus(&I2CD1);
}

/*
 * Double-height text starting at an arbitrary pixel column — same
 * no-clear-outside-its-own-footprint contract as ssd1309PrintAt().
 */
static void ssd1309PrintBigAt(uint8_t page, uint8_t col, const char *s) {
  if (!dispPresent) return;
  static uint8_t topBuf[121];
  static uint8_t botBuf[121];
  topBuf[0] = 0x40;
  botBuf[0] = 0x40;
  uint8_t n = 0;
  while (*s && (uint16_t)col + n + 12 <= 128) {
    uint8_t c = (uint8_t)*s++;
    if (c < 0x20 || c > 0x7F) c = 0x20;
    const uint8_t *g = ssd1309Font[c - 0x20];
    for (uint8_t sc = 0; sc < 5; sc++) {
      uint8_t src = g[sc];
      uint8_t top = ssd1309Expand[src & 0x0F];
      uint8_t bot = ssd1309Expand[(src >> 4) & 0x0F];
      topBuf[1 + n]     = top;
      topBuf[1 + n + 1] = top;
      botBuf[1 + n]     = bot;
      botBuf[1 + n + 1] = bot;
      n += 2;
    }
    topBuf[1 + n]     = 0;
    topBuf[1 + n + 1] = 0;
    botBuf[1 + n]     = 0;
    botBuf[1 + n + 1] = 0;
    n += 2;
  }
  if (n == 0) return;
  _ssd1309SetPos(page, col);
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, topBuf, (uint8_t)(n + 1));
  i2cReleaseBus(&I2CD1);
  _ssd1309SetPos((uint8_t)(page + 1), col);
  i2cAcquireBus(&I2CD1);
  _i2cRawTxRetry(dispAddr, botBuf, (uint8_t)(n + 1));
  i2cReleaseBus(&I2CD1);
}

/*
 * Draw a progress bar on the given page (0-100 %).
 * Format:  [================    ]
 * Pixels:  2 border + 122 fill area + 2 border = 126 px wide.
 */
static void ssd1309Bar(uint8_t page, uint8_t percent) {
  if (!dispPresent) return;
  if (percent > 100) percent = 100;
  /* Bar is exactly 126 px wide (matches ssd1309Print width) to avoid artifacts
   * when switching from bar to text — the text clear covers columns 0-125. */
  uint8_t filled = (uint8_t)((uint16_t)percent * 120 / 100);
  uint8_t buf[9];
  buf[0] = 0x40;
  _ssd1309SetPos(page, 0);
  uint8_t px = 0;
  while (px < 126) {
    uint8_t n = 0;
    while (n < 8 && px < 126) {
      if (px == 0 || px == 125) {
        buf[1 + n] = 0xFF; // border
      } else if (px >= 3 && px < 3 + filled) {
        buf[1 + n] = 0x3C; // fill (middle 4 bits = bar height)
      } else if (px >= 3 && px < 123) {
        buf[1 + n] = 0x24; // empty (top+bottom border only)
      } else {
        buf[1 + n] = 0xFF; // border
      }
      n++; px++;
    }
    i2cAcquireBus(&I2CD1);
    _i2cRawTxRetry(dispAddr, buf, n + 1);
    i2cReleaseBus(&I2CD1);
  }
}

#endif /* OHS_SSD1309_H_ */
