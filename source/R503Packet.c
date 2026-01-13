/*
 * R503Packet.c
 *
 *  Created on: Feb 7, 2025
 *      Author: vysocan
 */

#include "R503Packet.h"
#include "chprintf.h"

#define R503_DEBUG 0

#if R503_DEBUG
#define DBG(...) {chprintf((BaseSequentialStream*)&SD1, __VA_ARGS__);}
#else
#define DBG(...)
#endif
/*
 * Global variables
 */
R503Packet_t r503Packet;
SerialDriver *r503sdp;
uint8_t headBuffer[9];
uint8_t checksumBuffer[2];
/*
 * R503 default configuration
 */
static SerialConfig rs503_cfg = { 57600, 0, 0, 0 };
/*
 *
 */
void R503PacketInit(SerialDriver *sdp) {
  r503sdp = sdp;

  sdStart(r503sdp, &rs503_cfg);
}
/*
 * @brief Calculates the checksum for the given R503 packet.
 *
 * @param packet Pointer to the R503Packet_t structure for which the checksum is to be calculated.
 * @return None. The checksum is stored in the 'checksum' field of the packet structure.
 */
void calculateChecksum(R503Packet_t *packet) {
  packet->checksum = packet->type;
  packet->checksum += ((packet->length + 2) >> 8) + ((packet->length + 2) & 0xFF);

  for (uint16_t i = 0; i < packet->length; i++) {
    packet->checksum += packet->payload[i];
  }
}
/*
 * @brief Validates the checksum of the given R503 packet.
 *
 * @param packet Pointer to the R503Packet_t structure to be validated.
 * @return true if the checksum is valid, false otherwise.
 */
bool isChecksumValid(R503Packet_t *packet) {
  uint16_t original = packet->checksum;

  calculateChecksum(packet);
  if (original == packet->checksum) return true;

  packet->checksum = original;
  return false;
}
/*
 * @brief Sends a packet to the R503 fingerprint sensor module.
 *
 * @param packet The packet to be sent.
 * @return uint8_t Returns R503_OK if the packet is sent successfully, otherwise returns an error code.
 *        Possible error codes are
 * R503_OK 0x00 - Successful sending
 * R503_TIMEOUT 0xE9 - Timeout error
 */
uint8_t R503PacketSend(R503Packet_t *packet) {
  size_t ret;
  msg_t msg;

  uint16_t length = packet->length + 2;
  headBuffer[0] = highByte(R503_PKT_START_CODE);
  headBuffer[1] = lowByte(R503_PKT_START_CODE);
  headBuffer[2] = (uint8_t) (packet->address >> 24);
  headBuffer[3] = (uint8_t) (packet->address >> 16);
  headBuffer[4] = (uint8_t) (packet->address >> 8);
  headBuffer[5] = (uint8_t) (packet->address);
  headBuffer[6] = packet->type;
  headBuffer[7] = highByte(length);
  headBuffer[8] = lowByte(length);

  calculateChecksum(packet);

  ret = sdWriteTimeout(r503sdp, (const uint8_t* )&headBuffer, sizeof(headBuffer), TIME_MS2I(R503_SEND_TIMEOUT));
  if (ret != sizeof(headBuffer)) {
    return R503_TIMEOUT;
  }

  ret = sdWriteTimeout(r503sdp, packet->payload, packet->length, TIME_MS2I(R503_SEND_TIMEOUT));
  if (ret != packet->length) {
    return R503_TIMEOUT;
  }

  msg = sdPutTimeout(r503sdp, highByte(packet->checksum), TIME_MS2I(R503_SEND_TIMEOUT));
  if (msg == MSG_TIMEOUT) {
    return R503_TIMEOUT;
  }

  msg = sdPutTimeout(r503sdp, lowByte(packet->checksum), TIME_MS2I(R503_SEND_TIMEOUT));
  if (msg == MSG_TIMEOUT) {
    return R503_TIMEOUT;
  }

#if R503_DEBUG
  DBG(">> Sent packet: \r\n");
  DBG("- startCode: %02X %02X\r\n", highByte(R503_PKT_START_CODE), lowByte(R503_PKT_START_CODE));
  DBG("- address: %08X\r\n", packet->address);
  DBG("- type: %02X\r\n", packet->type);
  DBG("- length: %02X%02X (%d bytes inc. checksum)\r\n", highByte(length), lowByte(length), length);
  DBG("- payload: ");
  for (int i = 0; i < packet->length; i++) {
    DBG("%02X ", packet->payload[i]);
  }

  DBG("\r\n- checksum: %02X %02X\r\n", highByte(packet->checksum), lowByte(packet->checksum));
#endif
  return R503_OK;  // Successful reception
}

/**
 * @brief Receives a packet from the R503 fingerprint sensor module.
 *
 * @param packet The packet to be populated with the received data.
 * @return uint8_t Returns R503_OK if the packet is received successfully, otherwise returns an error code.
 *         Possible error codes are
 * R503_INVALID_START_CODE 0xE6 - Invalid start code
 * R503_CHECKSUM_MISMATCH 0xE3 - Checksum mismatch
 * R503_OK 0x00 - Successful sending
 * R503_TIMEOUT 0xE9 - Timeout error
 */
uint8_t R503PacketReceive(R503Packet_t *packet) {
  size_t ret;

  ret = sdReadTimeout(r503sdp, (uint8_t* )&headBuffer, sizeof(headBuffer), TIME_MS2I(R503_RECEIVE_TIMEOUT));
  if (ret != sizeof(headBuffer)) {
    return R503_TIMEOUT;
  }

#if R503_DEBUG
  // Print packet details
  DBG(">> Received packet:\r\n");
  DBG("- startCode: %02X %02X\r\n", headBuffer[0], headBuffer[1]);
  DBG("- address: %02X%02X%02X%02X\r\n", headBuffer[2], headBuffer[3], headBuffer[4], headBuffer[5]);
  DBG("- type: %02X\r\n", headBuffer[6]);
  DBG("- length: %02X%02X (%d bytes inc. checksum)\r\n", headBuffer[7], headBuffer[8], (headBuffer[7] << 8) | headBuffer[8]);
#endif

  // Verify start code
  if ((headBuffer[0] != highByte(R503_PKT_START_CODE)) || (headBuffer[1] != lowByte(R503_PKT_START_CODE))) {
    return R503_INVALID_START_CODE;
  }

  // Populate packet fields
  packet->address = (headBuffer[2] << 24) | (headBuffer[3] << 16) | (headBuffer[4] << 8) | headBuffer[5];
  packet->type = headBuffer[6];
  packet->length = ((headBuffer[7] << 8) | headBuffer[8]) - 2;

  ret = sdReadTimeout(r503sdp, packet->payload, packet->length, TIME_MS2I(R503_RECEIVE_TIMEOUT));
  if (ret != packet->length) {
    return R503_TIMEOUT;
  }

#if R503_DEBUG
  DBG("- payload: ");
  for (int i = 0; i < packet->length; i++) {
    DBG("%02X ", packet->payload[i]);
  }
#endif

  // Read checksum
  ret = sdReadTimeout(r503sdp, (uint8_t* )&checksumBuffer, sizeof(checksumBuffer), TIME_MS2I(R503_RECEIVE_TIMEOUT));
  if (ret != sizeof(checksumBuffer)) {
    return R503_TIMEOUT;
  }

  packet->checksum = (checksumBuffer[0] << 8) | checksumBuffer[1];

#if R503_DEBUG
  DBG("\r\n");
  DBG("- checksum: %02X %02X\r\n", checksumBuffer[0], checksumBuffer[1]);
  #endif

  // Verify checksum
  if (!isChecksumValid(packet)) {
#if R503_DEBUG
    DBG("checksum mismatch: %02X %02X\r\n", checksumBuffer[0], checksumBuffer[1]);
    #endif
    return R503_CHECKSUM_MISMATCH;  // Error code: checksum mismatch
  }

  return R503_OK;  // Successful reception
}
