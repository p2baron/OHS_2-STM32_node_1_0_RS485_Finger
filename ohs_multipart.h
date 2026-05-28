/*
 * ohs_multipart.h - Multipart message protocol for node-to-node large data transfer.
 *
 * Wire format (in the data[] field of each RS485Msg_t chunk):
 *   data[0] = 'M'          - multipart marker
 *   data[1] = chunk_index  - 0-based position of this chunk
 *   data[2] = chunk_total  - total number of chunks
 *   data[3..N] = payload   - up to MP_CHUNK_PAYLOAD bytes
 *
 * Reassembled payload format for fingerprint sync:
 *   [0]    = 'F'           - fingerprint operation
 *   [1]    = 'P'           - push/sync
 *   [2..3] = location      - uint16_t little-endian, target slot in R503
 *   [4..]  = template data - raw fingerprint template bytes
 *
 * The receive side writes directly into an external buffer (no embedded
 * reassembly array) to keep RAM usage minimal on STM32F103.
 *
 * sendDataDirect() must be defined by the including file before this header.
 */

#ifndef OHS_MULTIPART_H_
#define OHS_MULTIPART_H_

#define MP_MARKER         'M'
#define MP_HEADER_SIZE    3     // 'M' + chunk_index + chunk_total
#define MP_CHUNK_PAYLOAD  58    // Must match gateway: min(RS485_MSG_SIZE, RF69_MAX_DATA_LEN) - MP_HEADER_SIZE
#define MP_MAX_CHUNKS     32    // Limited by uint32_t bitmask
#define MP_TIMEOUT_MS     3000

typedef struct {
  uint8_t   chunksTotal;
  uint32_t  chunksReceived;
  uint8_t   senderAddress;
  systime_t startTime;
  uint16_t  receivedLength;
  bool      active;
} mpRxState_t;

static mpRxState_t mpRx;

static inline void mpRxReset(mpRxState_t *rx) {
  memset(rx, 0, sizeof(*rx));
}

static inline void mpRxCheckTimeout(mpRxState_t *rx) {
  if (rx->active &&
      chTimeI2MS(chVTTimeElapsedSinceX(rx->startTime)) > MP_TIMEOUT_MS) {
    mpRxReset(rx);
  }
}

/*
 * Process one received multipart chunk.
 * Payload is written into destBuf at offset chunkIndex * MP_CHUNK_PAYLOAD.
 *
 * Returns:  1  all chunks received, destBuf[0..rx->receivedLength-1] is complete
 *           0  chunk accepted, more chunks expected
 *          -1  error (bad params, overflow, sender mismatch)
 */
static int8_t mpRxProcess(mpRxState_t *rx, uint8_t sender,
                          const uint8_t *data, uint8_t length,
                          uint8_t *destBuf, uint16_t destBufSize) {
  uint8_t  chunkIndex, chunkTotal, payloadLen;
  uint16_t offset;
  uint32_t expectedMask;

  if (length <= MP_HEADER_SIZE) return -1;

  chunkIndex = data[1];
  chunkTotal = data[2];
  payloadLen = length - MP_HEADER_SIZE;

  if (chunkTotal == 0 || chunkTotal > MP_MAX_CHUNKS || chunkIndex >= chunkTotal) {
    mpRxReset(rx);
    return -1;
  }

  // Discard and restart on sender or total mismatch
  if (rx->active && (rx->senderAddress != sender || rx->chunksTotal != chunkTotal))
    mpRxReset(rx);

  if (!rx->active) {
    rx->active         = true;
    rx->chunksTotal    = chunkTotal;
    rx->chunksReceived = 0;
    rx->senderAddress  = sender;
    rx->receivedLength = 0;
    rx->startTime      = chVTGetSystemTimeX();
  }

  if (rx->chunksReceived & (1U << chunkIndex)) return 0; // duplicate, ignore

  offset = (uint16_t)chunkIndex * MP_CHUNK_PAYLOAD;
  if (offset + payloadLen > destBufSize) {
    mpRxReset(rx);
    return -1;
  }

  memcpy(&destBuf[offset], &data[MP_HEADER_SIZE], payloadLen);
  rx->chunksReceived |= (1U << chunkIndex);
  rx->receivedLength += payloadLen;

  expectedMask = (chunkTotal == 32) ? 0xFFFFFFFFU : ((1U << chunkTotal) - 1);
  return (rx->chunksReceived == expectedMask) ? 1 : 0;
}

/*
 * Split data into MP_CHUNK_PAYLOAD-sized chunks and send each via sendDataDirect().
 *
 * Returns:  1  all chunks sent
 *          -1  error (length too large or sendDataDirect failure)
 */
static int8_t sendDataMultipart(uint8_t address, const uint8_t *data, uint16_t length) {
  uint8_t  chunksTotal, payloadLen, i;
  uint8_t  chunkBuf[MP_HEADER_SIZE + MP_CHUNK_PAYLOAD];
  uint16_t offset;
  int8_t   resp;

  if (length == 0 || length > (uint16_t)MP_MAX_CHUNKS * MP_CHUNK_PAYLOAD)
    return -1;

  chunksTotal = (uint8_t)((length + MP_CHUNK_PAYLOAD - 1) / MP_CHUNK_PAYLOAD);

  chunkBuf[0] = MP_MARKER;
  chunkBuf[2] = chunksTotal;
  offset = 0;

  for (i = 0; i < chunksTotal; i++) {
    payloadLen = ((length - offset) > MP_CHUNK_PAYLOAD)
                 ? MP_CHUNK_PAYLOAD
                 : (uint8_t)(length - offset);

    chunkBuf[1] = i;
    memcpy(&chunkBuf[MP_HEADER_SIZE], &data[offset], payloadLen);

    resp = sendDataDirect(address, chunkBuf, MP_HEADER_SIZE + payloadLen);
    if (resp != 1) return -1;

    offset += payloadLen;
    if (i < chunksTotal - 1)
      chThdSleepMilliseconds(10); // brief gap between chunks
  }
  return 1;
}

#endif /* OHS_MULTIPART_H_ */
