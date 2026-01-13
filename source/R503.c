/*
 * R503.c
 *
 *  Created on: Feb 7, 2025
 *      Author: vysocan
 */

#include <stdint.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "R503.h"

#define R503_DEBUG 0

#if R503_DEBUG
#define DBG(...) {chprintf((BaseSequentialStream*)&SD1, __VA_ARGS__);}
#else
#define DBG(...)
#endif

#define DATA_SIZE 47     // Maximum data received here without pointer given by calling function.
uint8_t data[DATA_SIZE]; // Local buffer for responses like ack and long devinfo.
uint8_t send[6];         // CMD send buffer.

// Global variables
R503_t r503;

void R503Init(SerialDriver *sdp, uint32_t address, uint32_t password) {

  R503PacketInit(sdp);

  r503.address = address;
  r503.password = password;
}
/*
 *
 */
uint8_t R503Start(void) {
  uint8_t ret;

  // Read R503 Parameters
  ret = R503ReadParameters();
  if (ret != R503_OK) {
    DBG("error reading parameters from sensor (code: 0x%02X)\r\n", ret);
    return ret;
  }

  ret = R503ReadDeviceInfo();
  if (ret != R503_OK) {
    DBG("error reading device info from sensor (code: 0x%02X)\r\n", ret);
    return ret;
  }

  return R503_OK;
}
/*
 *
 */
uint8_t R503Send(uint8_t *toSend, uint8_t size, uint8_t type) {
  uint8_t ret;
  R503Packet_t out, in;

  out.type = type;
  out.length = size;
  out.payload = toSend;
  out.address = r503.address;

  ret = R503PacketSend(&out);
  DBG("R503 Send: 0x%02X\r\n", ret);

  if (ret != R503_OK) return ret;

  in.payload = data;
  ret = R503PacketReceive(&in);
  DBG("R503 Receive: 0x%02X\r\n", ret);

  return ret;
}
/*
 *
 */
uint8_t R503ReceiveData(uint8_t *toSend, uint8_t sendSize, uint8_t sendType, uint8_t *toReceive, u_int16_t *size) {
  uint8_t ret;
  R503Packet_t out, in;

  *size = 0;

  out.type = sendType;
  out.length = sendSize;
  out.payload = toSend;
  out.address = r503.address;

  ret = R503PacketSend(&out);
  DBG("R503 Send: 0x%02X\r\n", ret);

  if (ret != R503_OK) return ret;

  in.payload = data;
  ret = R503PacketReceive(&in);
  DBG("R503 Receive: 0x%02X\r\n", ret); DBG("R503 Data0: 0x%02X\r\n", data[0]);

  if (ret != R503_OK) return ret;
  if (data[0] != R503_OK) return data[0];

  do {
    in.payload = toReceive + *size;
    ret = R503PacketReceive(&in);
    DBG("R503 Receive: 0x%02X\r\n", ret);

    if (ret != R503_OK) return ret;

    *size += in.length;
    DBG("size %d, size %d, type %d \r\n", *size, in.length, in.type);
  } while (in.type == R503_PKT_DATA_START);

  return ret;
}
/*
 *
 */
uint8_t R503SendData(uint8_t *toSendCmd, uint8_t cmdSize, uint8_t sendType, uint8_t *toSendData, uint16_t dataSize) {
  uint8_t ret;
  uint16_t pos = 0;
  R503Packet_t out, in;

  out.type = sendType;
  out.length = cmdSize;
  out.payload = toSendCmd;
  out.address = r503.address;

  ret = R503PacketSend(&out);
  DBG("R503 Send: 0x%02X\r\n", ret);

  if (ret != R503_OK) return ret;

  in.payload = data;
  ret = R503PacketReceive(&in);
  DBG("R503 Receive: 0x%02X\r\n", ret); DBG("R503 Data0: 0x%02X\r\n", data[0]);

  if (ret != R503_OK) return ret;
  if (data[0] != R503_OK) return data[0];

  do {
    if (dataSize > r503.params.dataPackageSize) {
      out.type = R503_PKT_DATA_START;
      out.length = r503.params.dataPackageSize;
      dataSize -= r503.params.dataPackageSize;
    } else {
      out.type = R503_PKT_DATA_END;
      out.length = dataSize;
    }
    out.payload = toSendData + pos;
    out.address = r503.address;

    ret = R503PacketSend(&out);
    DBG("R503 Send: 0x%02X\r\n", ret);

    if (ret != R503_OK) return ret;

    pos += out.length;
    DBG("pos %d, size %d, type %d \r\n", pos, out.length, out.type);
  } while (out.type == R503_PKT_DATA_START);

  return ret;
}
/**
 * @brief Reads the parameters from the device and stores them in the provided R503Parameters object.
 *
 * @param params The R503Parameters object to store the read parameters.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 * code=00H: read complete;
 * code=01H: error when receiving package;
 * code=18H: error when write FLASH
 */
uint8_t R503ReadParameters(void) {
  uint8_t ret;
  uint8_t cmd = 0x0F;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND); // 17
  if (ret != R503_OK) return ret;

  r503.params.statusRegister = data[1] << 8 | data[2];
  r503.params.systemIdentifierCode = data[3] << 8 | data[4];
  r503.params.fingerLibrarySize = data[5] << 8 | data[6];
  r503.params.securityLevel = data[7] << 8 | data[8];
  r503.params.deviceAddress = data[9] << 24 | data[10] << 16 | data[11] << 8 | data[12];
  r503.params.dataPackageSize = 32 << (data[13] << 8 | data[14]);
  r503.params.baudrate = 9600 * (data[15] << 8 | data[16]);

  return data[0];

}
/**
 * @brief Reads device information from the R503 fingerprint module and stores it in the provided R503DeviceInfo struct.
 *
 * @param info The R503DeviceInfo struct to store the device information in.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 * code=0x00: success;
 * code=0x01: error when receiving package
 */
uint8_t R503ReadDeviceInfo(void) {
  uint8_t ret;
  uint8_t cmd = 0x3C;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND); // 47
  if (ret != R503_OK) return ret;

  memcpy(r503.info.moduleType, &data[1], 16);
  memcpy(r503.info.batchNumber, &data[17], 4);
  memcpy(r503.info.serialNumber, &data[21], 8);
  r503.info.hardwareVersion[0] = data[29];
  r503.info.hardwareVersion[1] = data[30];
  memcpy(r503.info.sensorType, &data[31], 8);
  r503.info.sensorWidth = data[39] << 8 | data[40];
  r503.info.sensorHeight = data[41] << 8 | data[42];
  r503.info.templateSize = data[43] << 8 | data[44];
  r503.info.databaseSize = data[45] << 8 | data[46];

  return data[0];
}
/**
 * @brief Verifies the password for the fingerprint sensor.
 *
 * @return uint8_t Returns R503_OK if the password is verified successfully, otherwise returns an error code.
 * code = 00H: Correct password;
 * code = 01H: error when receiving package;
 * code = 13H: Wrong password;
 */
uint8_t R503VerifyPassword(void) {
  uint8_t ret;

  send[0] = 0x13;
  send[1] = (uint8_t) (r503.password >> 24);
  send[2] = (uint8_t) (r503.password >> 16);
  send[3] = (uint8_t) (r503.password >> 8);
  send[4] = (uint8_t) (r503.password & 0xFF);

  ret = R503Send(&send[0], 5, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}

/**
 * @brief Sets the address of the fingerprint sensor.
 *
 * @param address The new address to set.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 * code=00H: address setting complete;
 * code=01H: error when receiving package;
 * code=18H: error when write FLASH
 */
uint8_t R503SetAddress(uint32_t address) {
  uint8_t ret;

  send[0] = 0x15;
  send[1] = (uint8_t) (r503.address >> 24);
  send[2] = (uint8_t) (r503.address >> 16);
  send[3] = (uint8_t) (r503.address >> 8);
  send[4] = (uint8_t) (r503.address & 0xFF);

  ret = R503Send(&send[0], 5, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  ret = data[0];
  if (ret != R503_OK) return ret;

  r503.address = address;
  return ret;
}
/**
 * Sets the aura LED of the R503 fingerprint sensor.
 *
 * @param mode The control mode of the LED.
 * @param color The color of the LED.
 * @param speed The speed of the LED.
 * @param repeat The repeat times of the LED.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 * code=0x00: success;
 * code=0x01:error when receiving package;
 */
uint8_t R503SetAuraLED(uint8_t control, uint8_t color, uint8_t speed, uint8_t repeat) {
  uint8_t ret;

  // Check parameters
  if (control < aLEDModeBreathing || control > aLEDModeFadeOut) {
    control = aLEDModeBreathing;
  }
  if (color < aLEDRed || color > aLEDWhite) {
    color = aLEDRed;
  }

  send[0] = 0x35;
  send[1] = control;
  send[2] = speed;
  send[3] = color;
  send[4] = repeat;

  ret = R503Send(&send[0], 5, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Sends a handshake command to the R503 fingerprint sensor.
 *
 * @return uint8_t Returns R503_OK if ready to receive commands, otherwise returns an error code.
 * code=0x00: the device is normal and can receive instructions;
 * code=other: the device is abnormal
 */
uint8_t R503HandShake(void) {
  uint8_t cmd = 0x40, ret;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * Sends a command to check the status of the fingerprint sensor.
 *
 * @return uint8_t Returns R503_OK if the sensor "normal" or R503_SENSOR_ABNORMAL if the sensor is abnormal.
 * code=0x00: the sensor is normal;
 * code=0x29: the sensor is abnormal
 */
uint8_t R503CheckSensor(void) {
  uint8_t cmd = 0x36, ret;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Sets the security level for the fingerprint sensor.
 *
 * @param level The security level to set. Valid values are between 1 and 5.
 *              1: False Acceptance Rate is lowest, False Rejection Rate is highest.
 *              5: False Acceptance Rate is highest, False Rejection Rate is lowest.
 *
 * @return uint8_t Returns R503_OK if the request was successful, or an error code otherwise.
 * code=00H: parameter setting complete;
 * code=01H: error when receiving package;
 * code=1aH: wrong register number;
 * code=18H: error when write FLASH
 */
uint8_t R503SetSecurityLevel(uint8_t level) {
  uint8_t ret;

  send[0] = 0x0E;
  send[1] = 0x05;
  send[2] = level;

  ret = R503Send(&send[0], 3, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Sets the packet size for the fingerprint sensor.
 *
 * @param size The desired packet size (0 = 32, 1 = 64, 2 = 128, or 3 = 256 bytes).
 *
 * @return uint8_t Returns R503_OK if the request was successful, or an error code otherwise.
 * code=00H: parameter setting complete;
 * code=01H: error when receiving package;
 * code=1aH: wrong register number;
 * code=18H: error when write FLASH
 */
uint8_t R503SetPacketSize(uint8_t size) {
  uint8_t ret;

  send[0] = 0x0E;
  send[1] = 0x06; // 6 = packet size
  if (size < 4) {
    send[2] = size;
  } else {
    send[2] = 2;
#if R503_DEBUG
      DBG("invalid packet size: %d\n", size);
    #endif
  }

  ret = R503Send(&send[0], 3, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Gets the number of valid templates stored in the sensor.
 *
 * @param count The number of valid templates stored in the sensor.
 * @return uint8_t Returns R503_OK if the request was successful, or an error code otherwise.
 * code=0x00: read success;
 * code=0x01: error when receiving package
 */
uint8_t R503GetValidTemplateCount(uint16_t *count) {
  uint8_t cmd = 0x1D, ret;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret == R503_OK) {
    *count = data[1] << 8 | data[2];
  }

  return data[0];
}
/**
 * @brief Cancels the current instruction being executed by the R503 fingerprint sensor.
 *
 * @return uint8_t Returns R503_OK if the reset was successful, or an error code otherwise.
 * code=0x00: cancel setting successful
 * code=other: cancel setting failed
 */
uint8_t R503CancelInstruction(void) {
  uint8_t cmd = 0x30, ret;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Gets a random number from the fingerprint module.
 *
 * @param number The random number obtained from the module.
 * @return uint8_t Returns R503_OK if the reset was successful, or an error code otherwise.
 * code=00H: generation success;
 * code=01H: error when receiving package
 */
uint8_t R503GetRandomNumber(uint32_t *number) {
  uint8_t cmd = 0x14, ret;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret == R503_OK) {
    *number = data[1] << 24 | data[2] << 16 | data[3] << 8 | data[4];
  }

  return data[0];
}
/**
 * @brief Sends a reset command to the R503 fingerprint sensor module.
 *
 * @return uint8_t Returns R503_OK if the reset was successful, or an error code otherwise.
 */
uint8_t R503SoftReset(void) {
  uint8_t cmd = 0x3D, ret;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Takes an image and stores it in the buffer (R503)
 *
 * @return R503_OK on success
 * code=00H: finger collection success;
 * code=01H: error when receiving package;
 * code=02H: can’t detect finger;
 * code=03H: fail to collect finger
 */
uint8_t R503TakeImage(void) {
  uint8_t cmd = 0x01, ret;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
///**
// * @brief Downloads an image from the R503 fingerprint sensor to the MCU.
// *
// * @param image Pointer to the image data to be uploaded.
// * @param size The size of the image data.
// *
// * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
// */
//uint8_t R503DownloadImage(uint8_t* image, uint16_t size) {
//  uint8_t cmd = 0x0A;
//
//  return R503SendP(&cmd, image, 1, R503_PKT_COMMAND);
//}
/**
 * @brief Uploads an image to the fingerprint sensor.
 *
 * @param image Pointer to the image data to be downloaded.
 * @param size Size of the image data in bytes.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 */
//uint8_t R503UploadImage(uint8_t* image) {
//  uint8_t cmd = 0x0B;
//
//  return R503SendP(&cmd, image, 1, R503_PKT_COMMAND);
//}
//uint8_t R503UploadTemplate(uint8_t charBuffer, uint8_t* template) {
//  uint8_t send[2];
//
//  send[0] = 0x0B;
//  send[1] = charBuffer;
//
//  return R503ReceiveData(&send[0], 2, R503_PKT_COMMAND, template);
//}
/**
 * @brief Extracts features from the given character buffer.
 *
 * @param charBuffer The character buffer to extract features from (range from 1 to 6)
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 * code=00H: generate character file complete;
 * code=01H: error when receiving package;
 * code=06H: fail to generate character file due to the over-disorderly fingerprint image;
 * code=07H: fail to generate character file due to lackness of character point or over-smallness of fingerprint image;
 * code=15H: fail to generate the image for the lackness of valid primary image;
 */
uint8_t R503ExtractFeatures(uint8_t charBuffer) {
  uint8_t ret;

  send[0] = 0x02;
  send[1] = charBuffer;

  ret = R503Send(&send[0], 2, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Create a fingerprint template.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 * code=00H: operation success;
 * code=01H: error when receiving package;
 * code=0aH: fail to combine the character files. That’s, the character files don’t belong to one finger
 */
uint8_t R503CreateTemplate(void) {
  uint8_t cmd = 0x05, ret;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Stores a template in the specified location.
 *
 * @param charBuffer The character buffer to store the template in.
 * @param location The location to store the template in.
 *
 * @return uint8_t Returns 0 on success, or an error code on failure.
 * code=00H: storage success;
 * code=01H: error when receiving package;
 * code=0bH: addressing ModelID is beyond the finger library;
 * code=18H: error when writing Flash.
 */
uint8_t R503StoreTemplate(uint8_t charBuffer, uint16_t location) {
  uint8_t ret;

  send[0] = 0x06;
  send[1] = charBuffer;
  send[2] = (uint8_t) (location >> 8);
  send[3] = (uint8_t) (location & 0xFF);

  ret = R503Send(&send[0], 4, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Gets a template from the specified location and stores it in the specified character buffer.
 *
 * @param charBuffer The character buffer to store the template in.
 * @param location The location of the template to retrieve.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 * code=00H: load success;
 * code=01H: error when receiving package;
 * code=0cH: error when reading template from library or the readout template is invalid;
 * code=0BH: addressing ModelID is beyond the finger library;
 */
uint8_t R503GetTemplate(uint8_t charBuffer, uint16_t location) {
  uint8_t ret;

  send[0] = 0x07;
  send[1] = charBuffer;
  send[2] = (uint8_t) (location >> 8);
  send[3] = (uint8_t) (location & 0xFF);

  ret = R503Send(&send[0], 4, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Uploads a template from the R503 fingerprint sensor to the MCU.
 *
 * @param charBuffer The character buffer to upload the template from.
 * @param template Pointer to the image data to be uploaded.
 * @param size The size of the template data.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 */
uint8_t R503UploadTemplate(uint8_t charBuffer, uint8_t *template, uint16_t *size) {

  send[0] = 0x08;
  send[1] = charBuffer;

  return R503ReceiveData(&send[0], 2, R503_PKT_COMMAND, template, size);
}
/**
 * @brief Downloads a template to the specified character buffer on R503
 *
 * @param charBuffer The character buffer to upload the template to.
 * @param template The template data to upload.
 * @param size The size of the template data.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 */
uint8_t R503DownloadTemplate(uint8_t charBuffer, uint8_t *template, uint16_t size) {

  send[0] = 0x09;
  send[1] = charBuffer;

  return R503SendData(&send[0], 2, R503_PKT_COMMAND, template, size);
}
/**
 * @brief Deletes a template from the specified location.
 *
 * @param location The location of the template to be deleted.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 * code=00H: delete success;
 * code=01H: error when receiving package;
 * code=10H: faile to delete templates;
 * code=18H: error when write FLASH;
 */
uint8_t R503DeleteTemplate(uint16_t location, uint16_t count) {
  uint8_t ret;

  send[0] = 0x0C;
  send[1] = (uint8_t) (location >> 8);
  send[2] = (uint8_t) (location & 0xFF);
  send[3] = (uint8_t) (count >> 8);
  send[4] = (uint8_t) (count & 0xFF);

  ret = R503Send(&send[0], 5, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
/**
 * @brief Sends a command to empty the library of fingerprints.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 * code=00H: empty success;
 * code=01H: error when receiving package;
 * code=11H: fail to clear finger library;
 * code=18H: error when write FLASH
 */
uint8_t R503EmptyLibrary(void) {
  uint8_t cmd = 0x0D, ret;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  return data[0];
}
///**
// * @brief Downloads the template data from the specified character buffer from R503
// *
// * @param charBuffer The character buffer to download the template from.
// * @param templateData Pointer to the buffer where the template data will be stored.
// * @param size Reference to the variable where the size of the downloaded template data will be stored.
// *
// * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
// */
//uint8_t R503DownloadTemplate(uint8_t charBuffer, uint8_t* templateData, uint16_t* size) {
//  uint8_t ret;
//  R503Packet_t out, in;
//
//  out.type = R503_PKT_DATA_END;
//  out.length = 1;
//  out.payload = &charBuffer;
//  out.address = r503.address;
//
//  return R503SendData(&out);
//}

///**
// * @brief Uploads a template to the specified character buffer on R503
// *
// * @param charBuffer The character buffer to upload the template to.
// * @param templateData The template data to upload.
// * @param size The size of the template data.
// *
// * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
// */
//uint8_t R503UploadTemplate(uint8_t charBuffer, uint8_t *templateData, uint16_t size) {
//    uint16_t tempBufferSize = fpsTemplateSize + 256;
//    uint8_t tempBuffer[tempBufferSize];
//    memset(tempBuffer, 0xFF, tempBufferSize); // Fill the buffer with 0xFF
//    if(size <= tempBufferSize) {
//        memcpy(tempBuffer, templateData, size); // Copy the template data to the buffer, leaving the rest as 0xFF if the template data is smaller than the buffer size
//    }
//    else {
//        memcpy(tempBuffer, templateData, tempBufferSize);
//    }
//
//    GET_PACKET(1, 0x09, charBuffer);
//    if (confirmationCode != R503_OK)
//        return confirmationCode;
//
//    return sendData(tempBuffer, tempBufferSize); // Send the buffer to the sensor
//}
/**
 * @brief Gets the number of templates stored in the device.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 */
uint8_t R503GetTemplateCount(uint16_t *count) {
  uint8_t ret;
  uint8_t cmd = 0x1D;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  *count = data[1] << 8 | data[2];

  return ret;
}
/**
 * @brief Matches the fingerprint and returns the confidence level.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 */
uint8_t R503MatchFinger(uint16_t *confidence) {
  uint8_t ret;
  uint8_t cmd = 0x03;

  ret = R503Send(&cmd, 1, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  *confidence = data[1] << 8 | data[2];

  return ret;
}
/**
 * @brief Searches for a finger in the fingerprint library.
 *
 * @param charBuffer The character buffer to search for the finger.
 *
 * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
 */
uint8_t R503SearchFinger(uint8_t charBuffer, uint16_t *location, uint16_t *confidence) {
  uint8_t ret;

  send[0] = 0x04;
  send[1] = charBuffer;
  send[2] = 0; // Start location
  send[3] = 0; // Num
  send[4] = r503.params.fingerLibrarySize >> 8;
  send[5] = r503.params.fingerLibrarySize & 0xFF;

  ret = R503Send(&send[0], 6, R503_PKT_COMMAND);
  if (ret != R503_OK) return ret;

  *location = data[1] << 8 | data[2];
  *confidence = data[3] << 8 | data[4];

  return data[0];
}
///**
// * @brief Reads the index table of a specified page and stores it in the provided buffer.
// *
// * @param table Pointer to the buffer where the index table will be stored.
// * @param page The page number of the index table to be read.
// *
// * @return uint8_t Returns R503_OK if successful, otherwise returns an error code.
// */
//uint8_t R503Lib::readIndexTable(uint8_t *table, uint8_t page) {
//    GET_PACKET(33, 0x1F, page);
//    memcpy(table, &data[1], 32);
//
//    return confirmationCode;
//}
