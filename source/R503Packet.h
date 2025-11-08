/*
 * R503Packet.h
 *
 *  Created on: Feb 7, 2025
 *      Author: vysocan
 */

#ifndef SOURCE_R503PACKET_H_
#define SOURCE_R503PACKET_H_

#ifndef R503PACKET_H
#define R503PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include "ch.h"
#include "hal.h"

// Defaults
#define R503_PASSWORD 0x0
#define R503_RECEIVE_TIMEOUT 3000
#define R503_SEND_TIMEOUT 3000
#define R503_RESET_TIMEOUT 3000

// Confirmation Codes
#define R503_OK 0x00
#define R503_ERROR_RECEIVING_PACKET 0x01
#define R503_NO_FINGER 0x02
#define R503_ERROR_TAKING_IMAGE 0x03
#define R503_IMAGE_MESSY 0x06
#define R503_FEATURE_FAIL 0x07
#define R503_NO_MATCH 0x08
#define R503_NO_MATCH_IN_LIBRARY 0x09
#define R503_WRONG_PASSWORD 0x13
#define R503_NO_IMAGE 0x15
#define R503_BAD_LOCATION 0x0B
#define R503_ERROR_WRITING_FLASH 0x18
#define R503_SENSOR_ABNORMAL 0x29
#define R503_ERROR_TRANSFER_DATA = 0x0E

// Error Codes
#define R503_ADDRESS_MISMATCH 0xE1
#define R503_NOT_ENOUGH_MEMORY 0xE2
#define R503_CHECKSUM_MISMATCH 0xE3
#define R503_PACKET_MISMATCH 0xE5
#define R503_INVALID_START_CODE 0xE6
#define R503_INVALID_BAUDRATE 0xE8
#define R503_TIMEOUT 0xE9

#define R503_PKT_START_CODE 0xEF01
#define R503_PKT_COMMAND 0x01
#define R503_PKT_DATA_START 0x02
#define R503_PKT_ACK 0x07
#define R503_PKT_DATA_END 0x08

// Helper macros
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))

typedef struct {
    uint32_t address;
    uint8_t type;
    uint16_t length;
    uint8_t *payload;
    uint16_t checksum;
    uint8_t *payloadOverflow;
} R503Packet_t ;

void R503PacketInit(SerialDriver *sdp);
uint8_t R503PacketSend(R503Packet_t* packet);
uint8_t R503PacketReceive(R503Packet_t* packet);

#endif

#endif /* SOURCE_R503PACKET_H_ */
