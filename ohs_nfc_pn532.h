/*
 * ohs_nfc_pn532.h
 *
 * Minimal PN532 NFC driver over I2C for ChibiOS/STM32F103.
 * Supports card UID detection (ISO14443A, 106kbps).
 * Requires HAS_NFC to be defined to compile in.
 *
 * Wiring: I2C1 (PB8=SCL, PB9=SDA), IRQ on PB5.
 * PN532 I2C address: 0x24 (default, ADR1=1 ADR0=0).
 */

#ifndef OHS_NFC_PN532_H_
#define OHS_NFC_PN532_H_

#ifdef HAS_NFC

#define PN532_I2C_ADDR    0x24

#define PN532_OK          0
#define PN532_NOTARGET    1
#define PN532_ERROR      -1

/* Called from EXTI ISR on PB5 falling edge — signals NFCSem */
static void nfcIrqCallback(void *arg) {
  (void)arg;
  chSysLockFromISR();
  chBSemSignalI(&NFCSem);
  chSysUnlockFromISR();
}

/*
 * Build a standard PN532 info frame into buf.
 * data[] = command byte + parameters (TFI=0xD4 is added internally).
 * Returns frame length.
 */
static uint8_t pn532BuildFrame(uint8_t *buf, const uint8_t *data, uint8_t dataLen) {
  uint8_t i = 0;
  uint8_t len = (uint8_t)(dataLen + 1); /* TFI + data */
  uint8_t dcs = 0xD4;                   /* starts with TFI */
  buf[i++] = 0x00; buf[i++] = 0x00; buf[i++] = 0xFF;
  buf[i++] = len;
  buf[i++] = (uint8_t)(~len + 1);       /* LCS */
  buf[i++] = 0xD4;                       /* TFI (host→PN532) */
  for (uint8_t j = 0; j < dataLen; j++) {
    buf[i++] = data[j];
    dcs += data[j];
  }
  buf[i++] = (uint8_t)(~dcs + 1);       /* DCS */
  buf[i++] = 0x00;                       /* postamble */
  return i;
}

/*
 * Send a command and read/verify the ACK frame.
 * ACK is read synchronously (fixed 5ms wait — PN532 ACKs in <2ms).
 * Returns PN532_OK or PN532_ERROR.
 */
static int8_t pn532SendCmd(I2CDriver *i2cp, const uint8_t *cmd, uint8_t cmdLen) {
  uint8_t txbuf[16], rxbuf[8];
  uint8_t flen = pn532BuildFrame(txbuf, cmd, cmdLen);

  i2cAcquireBus(i2cp);
  msg_t s = i2cMasterTransmitTimeout(i2cp, PN532_I2C_ADDR, txbuf, flen, NULL, 0, TIME_MS2I(50));
  i2cReleaseBus(i2cp);
  if (s != MSG_OK) return PN532_ERROR;

  chThdSleepMilliseconds(5); /* wait for PN532 to prepare ACK */

  i2cAcquireBus(i2cp);
  s = i2cMasterReceiveTimeout(i2cp, PN532_I2C_ADDR, rxbuf, 7, TIME_MS2I(20));
  i2cReleaseBus(i2cp);
  if (s != MSG_OK || rxbuf[0] != 0x01) return PN532_ERROR;

  /* ACK frame: [status=0x01][0x00][0x00][0xFF][0x00][0xFF][0x00] */
  if (rxbuf[2] != 0x00 || rxbuf[3] != 0xFF || rxbuf[4] != 0x00 || rxbuf[5] != 0xFF)
    return PN532_ERROR;

  return PN532_OK;
}

/*
 * Read a response frame from PN532.
 * Called after PN532 asserts IRQ (or after a known wait in init).
 * Fills data[] with bytes starting from PD0 (response code) through PDn.
 * Returns PN532_OK or PN532_ERROR.
 *
 * I2C buffer layout (status byte prepended by PN532 I2C layer):
 *   [0] status (0x01=ready)
 *   [1] preamble (0x00)
 *   [2] start code (0x00)
 *   [3] start code (0xFF)
 *   [4] LEN (TFI + payload count)
 *   [5] LCS
 *   [6] TFI (0xD5 = PN532→host)
 *   [7..] PD0..PDn (response code + data)
 */
static int8_t pn532ReadResponse(I2CDriver *i2cp, uint8_t *data, uint8_t *dataLen) {
  uint8_t rxbuf[32];

  i2cAcquireBus(i2cp);
  msg_t s = i2cMasterReceiveTimeout(i2cp, PN532_I2C_ADDR, rxbuf, sizeof(rxbuf), TIME_MS2I(50));
  i2cReleaseBus(i2cp);

  if (s != MSG_OK || rxbuf[0] != 0x01) return PN532_ERROR;
  if (rxbuf[1] != 0x00 || rxbuf[2] != 0x00 || rxbuf[3] != 0xFF || rxbuf[6] != 0xD5)
    return PN532_ERROR;

  uint8_t len = rxbuf[4]; /* TFI + payload byte count */
  if (len < 2) return PN532_ERROR;
  *dataLen = (uint8_t)(len - 1); /* exclude TFI, keep response code + data */
  memcpy(data, &rxbuf[7], *dataLen);
  return PN532_OK;
}

/*
 * Initialize PN532:
 *   1. SAMConfiguration — normal mode, use IRQ pin
 *   2. RFConfiguration — limit passive retries to 2 for fast "no tag" response
 */
int8_t pn532Init(I2CDriver *i2cp) {
  uint8_t cmd[5], rsp[16]; uint8_t rspLen;

  cmd[0] = 0x14; cmd[1] = 0x01; cmd[2] = 0x14; cmd[3] = 0x01;
  if (pn532SendCmd(i2cp, cmd, 4) != PN532_OK) return PN532_ERROR;
  chThdSleepMilliseconds(10);
  if (pn532ReadResponse(i2cp, rsp, &rspLen) != PN532_OK) return PN532_ERROR;
  if (rsp[0] != 0x15) return PN532_ERROR; /* SAMConfiguration response code */

  cmd[0] = 0x32; cmd[1] = 0x05; cmd[2] = 0xFF; cmd[3] = 0x01; cmd[4] = 0x02;
  if (pn532SendCmd(i2cp, cmd, 5) != PN532_OK) return PN532_ERROR;
  chThdSleepMilliseconds(10);
  pn532ReadResponse(i2cp, rsp, &rspLen); /* ignore RFConfiguration response */

  chBSemReset(&NFCSem, true); /* taken=true: next wait blocks until response IRQ */
  return PN532_OK;
}

/*
 * Start passive card detection (non-blocking).
 * Sends InListPassiveTarget for ISO14443A at 106kbps.
 * PN532 will assert IRQ on PB5 when detection completes (card found or max retries).
 * After this returns, wait on NFCSem then call pn532ReadUID().
 */
int8_t pn532StartDetect(I2CDriver *i2cp) {
  uint8_t cmd[3] = {0x4A, 0x01, 0x00}; /* InListPassiveTarget, maxTg=1, 106kbps ISO14443A */
  int8_t ret = pn532SendCmd(i2cp, cmd, 3);
  chBSemReset(&NFCSem, true); /* taken=true: discard ACK IRQ, next wait blocks until response IRQ */
  return ret;
}

/*
 * Read the UID from a completed InListPassiveTarget response.
 * Call after pn532StartDetect() + chBSemWaitTimeout() returns MSG_OK.
 * Returns PN532_OK (uid/uidLen filled), PN532_NOTARGET, or PN532_ERROR.
 */
int8_t pn532ReadUID(I2CDriver *i2cp, uint8_t *uid, uint8_t *uidLen) {
  uint8_t rsp[20]; uint8_t rspLen;

  if (pn532ReadResponse(i2cp, rsp, &rspLen) != PN532_OK) return PN532_ERROR;
  if (rspLen < 2 || rsp[0] != 0x4B) return PN532_ERROR; /* validate response code */
  if (rsp[1] == 0x00) return PN532_NOTARGET;             /* NbTg = 0, no card */
  if (rspLen < 7) return PN532_ERROR;

  /* rsp: [0x4B][NbTg][Tg][ATQA0][ATQA1][SAK][NfcIdLength][NfcId...] */
  *uidLen = rsp[6];
  if (*uidLen > 7) *uidLen = 7;
  memcpy(uid, &rsp[7], *uidLen);
  return PN532_OK;
}

#endif /* HAS_NFC */
#endif /* OHS_NFC_PN532_H_ */
