/*
 * RS503.h
 *
 *  Created on: Feb 7, 2025
 *      Author: vysocan
 */

#ifndef SOURCE_R503_H_OLD_
#define SOURCE_R503_H_OLD_

#include <stdint.h>
#include "ch.h"
#include "hal.h"
#include "R503Packet.h"

// Defaults
#define R503_PASSWORD 0x0

typedef struct {
    uint16_t statusRegister;
    uint16_t systemIdentifierCode;
    uint16_t fingerLibrarySize;
    uint16_t securityLevel;
    uint32_t deviceAddress;
    uint16_t dataPacketSize;
    uint32_t baudrate;
} R503Params_t;

typedef struct {
    char moduleType[16];
    char batchNumber[4];
    char serialNumber[8];
    uint8_t hardwareVersion[2];
    char sensorType[8];
    uint16_t sensorWidth;
    uint16_t sensorHeight;
    uint16_t templateSize;
    uint16_t databaseSize;
} R503DeviceInfo_t;

typedef struct {
    uint32_t address;
    uint32_t password;
    R503Params_t params;
    R503DeviceInfo_t info;
} R503_t;

typedef enum {
  aLEDModeBreathing = 1, // Breathing
  aLEDModeFlash,         // Quick blink
  aLEDModeON,            // On
  aLEDModeOFF,           // Off
  aLEDModeFadeIn,        // Fade in
  aLEDModeFadeOut,       // Fade out
} aLEDMode_t;

typedef enum {
  aLEDRed = 1,
  aLEDBlue,
  aLEDPurple,
  aLEDGreen,
  aLEDYellow,
  aLEDCyan,
  aLEDWhite
} aLEDColor_t;

void R503Init(SerialDriver *sdp, uint32_t address, uint32_t password);
uint8_t R503Start(void);

// R503 Device Related
uint8_t R503ReadParameters(void);
uint8_t R503ReadDeviceInfo(void);
uint8_t R503VerifyPassword(void);
uint8_t R503SetAddress(uint32_t address);
uint8_t R503SetAuraLED(uint8_t mode, uint8_t color, uint8_t speed, uint8_t repeat);
uint8_t R503HandShake(void);
uint8_t R503CheckSensor(void);
uint8_t R503SetSecurityLevel(uint8_t level);
uint8_t R503SetPacketSize(uint8_t size);
uint8_t R503WriteParameter(uint8_t paramNumber, uint8_t value);
uint8_t R503GetValidTemplateCount(uint16_t *count);
uint8_t R503CancelInstruction(void);
uint8_t R503GetRandomNumber(uint32_t *number);
uint8_t R503SoftReset(void);

// Fingerprint Related
uint8_t R503TakeImage(void);
uint8_t R503DownloadImage(uint8_t *image, uint16_t size);
uint8_t R503UploadImage(uint8_t *image, uint16_t *size);
uint8_t R503ExtractFeatures(uint8_t charBuffer);
uint8_t R503CreateTemplate(void);
uint8_t R503StoreTemplate(uint8_t charBuffer, uint16_t location);
uint8_t R503GetTemplate(uint8_t charBuffer, uint16_t location);
uint8_t R503UploadTemplate(uint8_t charBuffer, uint8_t* template, uint16_t* size);
uint8_t R503DownloadTemplate(uint8_t charBuffer, uint8_t* template, uint16_t size);
uint8_t R503DeleteTemplate(uint16_t location, uint16_t count);
uint8_t R503GetTemplateCount(uint16_t *count);
uint8_t R503EmptyLibrary(void);
uint8_t R503MatchFinger(uint16_t *confidence);
uint8_t R503SearchFinger(uint8_t charBuffer, uint16_t *location, uint16_t *confidence);
uint8_t R503ReadIndexTable(uint8_t *table, uint8_t page);

#endif /* SOURCE_R503_H_OLD_ */
