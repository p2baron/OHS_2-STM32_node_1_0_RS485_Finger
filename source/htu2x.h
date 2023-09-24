/*
 * HTU2xD_SHT2x_Si70xx.h
 *
 *  Created on: Sep 19, 2023
 *      Author: vysocan
 */

#ifndef SOURCE_HTU2X_H_
#define SOURCE_HTU2X_H_

#include "hal.h"

/* list of I2C addresses */
#define HTU2XD_SHT2X_SI702X_SI700X_ADDRESS_X40  0x40    //HTU2xD, SHT2x, Si702x, Si700x  address
#define SI701X_ADDRESS_X41                      0x41    //SI701x address

/* list of command registers */
#define HTU2XD_SHT2X_SI70XX_USER_WRITE_REG      0xE6    //write to user register
#define HTU2XD_SHT2X_SI70XX_USER_READ_REG       0xE7    //read from user register
#define SI70XX_HEATER_WRITE_REG                 0x51    //write to heater register, for Si7xx only
#define SI70XX_HEATER_READ_REG                  0x11    //read from heater register, for Si7xx only
#define HTU2XD_SHT2X_SI70XX_SOFT_RESET_REG      0xFE    //write soft reset
#define HTU2XD_SHT2X_SI70XX_SERIAL1_READ1_REG   0xFA    //read 1-st 2-bytes of serial number
#define HTU2XD_SHT2X_SI70XX_SERIAL1_READ2_REG   0x0F    //read 2-nd 2-bytes of serial number
#define HTU2XD_SHT2X_SI70XX_SERIAL2_READ1_REG   0xFC    //read 3-rd 2-bytes of serial number
#define HTU2XD_SHT2X_SI70XX_SERIAL2_READ2_REG   0xC9    //read 4-th 2-bytes of serial number
#define HTU2XD_SHT2X_SI70XX_FW_READ1_REG        0x84    //read firmware revision, 1-st part of the command
#define HTU2XD_SHT2X_SI70XX_FW_READ2_REG        0xB8    //read firmware revision, 2-nd part of the command
//HTU2XD_SHT2X_SI70XX_TEMP_OPERATION_MODE_REG           //start temperature measurement, see below
//HTU2XD_SHT2X_SI70XX_HUMD_OPERATION_MODE_REG           //start humidity measurement, see below

/* list of user registers controls */
#define HTU2XD_SHT2X_USER_CTRL_OTP_ON           0x02    //disable OTP reload for HTU2xD/SHT2x only, bit[1] in user register (default after power-on or reset)
#define HTU2XD_SHT2X_SI70XX_USER_CTRL_HEATER_ON 0x04    //heater ON, bit[2] in user register (OFF by default after power-on or reset)
#define HTU2XD_SHT2X_SI70XX_USER_CTRL_VDD_LOW   0x40    //operating voltage LOW, bit[6] in user register

#define HTU2XD_SHT2X_SI70XX_FW_READ_CTRL_V1     0xFF    //sensor firmware v1.0
#define HTU2XD_SHT2X_SI70XX_FW_READ_CTRL_V2     0x20    //sensor firmware v2.0
//HTU2XD_SHT2X_SI70XX_USER_CTRL_RES                     //RH/T measurement resolution, bit [7:0] in user register, see below

#define SI7000_SERIAL2_READ_CTRL_CHIP_ID        0x00    //Si70xx engineering device ID, 1-st byte in second memory access
#define SI7006_SERIAL2_READ_CTRL_CHIP_ID        0x06    //Si7006 device ID, 1-st byte in second memory access
#define SI7013_SERIAL2_READ_CTRL_CHIP_ID        0x0D    //Si7013 device ID, 1-st byte in second memory access
#define SI7020_SERIAL2_READ_CTRL_CHIP_ID        0x14    //Si7020 device ID, 1-st byte in second memory access
#define SI7021_SERIAL2_READ_CTRL_CHIP_ID        0x15    //Si7021 device ID, 1-st byte in second memory access
#define HTU2XD_SHT2X_SERIAL2_READ_CTRL_CHIP_ID  0x32    //HTU2xD/SHT2x device ID, 1-st byte in second memory access
#define SI70FF_SERIAL2_READ_CTRL_CHIP_ID        0xFF    //Si70xx engineering device ID, 1-st byte in second memory access

/* speed & delay */
#define HTU2XD_SHT2X_SI70XX_I2C_SPEED_HZ        100000  //sensor I2C speed 100KHz..400KHz, in Hz
#define HTU2XD_SHT2X_SI70XX_I2C_STRETCH_USEC    1000    //I2C stretch time, in usec

#define HTU2XD_SHT2X_POWER_ON_DELAY             15      //wait for HTU2xD/SHT2x to initialize after power-on, in milliseconds
#define SI70XX_POWER_ON_DELAY                   80      //wait for Si70xx to initialize to full range after power-on, in milliseconds
#define HTU2XD_SHT2X_SI70XX_SOFT_RESET_DELAY    15      //wait for HTU2xD/SHT2x to initialize after reset, in milliseconds

#define SHT2X_HUMD_12BIT_RES_DELAY              29      //12-bit RH-resolution measurement delay, HTU2xD 14..16msec | SHT2x 22..29msec | Si70xx 10+7..12+10.8msec
#define SHT2X_HUMD_11BIT_RES_DELAY              15      //11-bit RH-resolution measurement delay, HTU2xD 7..8msec | SHT2x 12..15msec | Si70xx 7+1.5..10.8+2.4msec
#define SI70XX_HUMD_10BIT_RES_DELAY             13      //10-bit RH-resolution measurement delay, HTU2xD 4..5msec | SHT2x 7..9msec | Si70xx 3.7+4..5.4+6.2msec
#define SI70XX_HUMD_08BIT_RES_DELAY             7       //8-bit  RH-resolution measurement delay, HTU2xD 2..3msec | SHT2x 3..4msec | Si70xx 2.6+2.4..3.1+3.8msec

#define SHT2X_TEMP_14BIT_RES_DELAY              85      //14-bit T-resolution measurement delay, HTU2xD 44..50msec | SHT2x 66..85msec | Si70xx 7..10.8msec
#define SHT2X_TEMP_13BIT_RES_DELAY              43      //13-bit T-resolution measurement delay, HTU2xD 22..25msec | SHT2x 33.43msec | Si70xx 4..6.2msec
#define SHT2X_TEMP_12BIT_RES_DELAY              22      //12-bit T-resolution measurement delay, HTU2xD 11..13msec | SHT2x 17..22msec | Si70xx 2.4..3.8msec
#define SHT2X_TEMP_11BIT_RES_DELAY              11      //11-bit T-resolution measurement delay, HTU2xD 6..7msec | SHT2x 9..11msec | Si70xx 1.5..2.4msec

/* misc */
#define HTU2XD_SHT2X_TEMP_COEF_0C_80C           -0.15   //temperature coefficient for RH compensation at range 0C..80C, for HTU2xD/SHT2x only
#define HTU2XD_SHT2X_SI70XX_CRC8_POLYNOMINAL    0x13100 //16-bit CRC8 polynomial for CRC8 -> x^8 + x^5 + x^4 + 1
#define HTU2XD_SHT2X_SI70XX_ERROR               0xFF    //returns 255, if CRC8 or communication error is occurred


/* list of user registers controls, continue */
typedef enum {
  HUMD_12BIT_TEMP_14BIT = 0x00,                         //resolution RH 12-bit / T 14-bit, bit[7:0] in user register (default after power-on or reset)
  HUMD_08BIT_TEMP_12BIT = 0x01,                         //resolution RH 8-bit  / T 12-bit, bit[7:0] in user register
  HUMD_10BIT_TEMP_13BIT = 0x80,                         //resolution RH 10-bit / T 13-bit, bit[7:0] in user register
  HUMD_11BIT_TEMP_11BIT = 0x81                          //resolution RH 11-bit / T 11-bit, bit[7:0] in user register
} HTU2XD_SHT2X_SI70XX_USER_CTRL_RES;


/* list of command registers, continue */
typedef enum {
  START_HUMD_HOLD_I2C   = 0xE5,                         //start humidity measurement & blocking/stretching I2C bus during measurement
  START_HUMD_NOHOLD_I2C = 0xF5                          //start humidity measurement & without blocking/stretching I2C bus during measurement
} HTU2XD_SHT2X_SI70XX_HUMD_OPERATION_MODE_REG;


/* list of command registers, continue */
typedef enum {
  START_TEMP_HOLD_I2C   = 0xE3,                         //start temperature measurement & blocking I2C bus during measurement
  START_TEMP_NOHOLD_I2C = 0xF3,                         //start temperature measurement & without blocking I2C bus during measurement
  READ_TEMP_AFTER_RH    = 0xE0                          //read temperature value from previous humidity measurement, for Si70xx only
} HTU2XD_SHT2X_SI70XX_TEMP_OPERATION_MODE_REG;


/* custom list of supported sensors */
typedef enum {
  HTU2xD_SENSOR = 0x00,
  SHT2x_SENSOR  = 0x01,
  SI700x_SENSOR = 0x02,
  SI701x_SENSOR = 0x03,
  SI702x_SENSOR = 0x04
} HTU2XD_SHT2X_SI70XX_I2C_SENSOR;

uint8_t htu2xBegin(I2CDriver *i2cdp, HTU2XD_SHT2X_SI70XX_I2C_SENSOR sensorType,
                   HTU2XD_SHT2X_SI70XX_USER_CTRL_RES resolution);
float    htu2xReadHumidity(HTU2XD_SHT2X_SI70XX_HUMD_OPERATION_MODE_REG sensorOperationMode);
float    htu2xGetCompensatedHumidity(float temperature);
float    htu2xReadTemperature(HTU2XD_SHT2X_SI70XX_TEMP_OPERATION_MODE_REG sensorOperationMode);
//
void     htu2xSetType(HTU2XD_SHT2X_SI70XX_I2C_SENSOR);
void     htu2xSetResolution(HTU2XD_SHT2X_SI70XX_USER_CTRL_RES sensorResolution);
uint8_t  htu2xVoltageStatus(void);
void     htu2xSetHeater(bool heaterOn, uint8_t powerLevel);
uint16_t htu2xReadDeviceID(void);
uint8_t  htu2xReadFirmwareVersion(void);
void     htu2xSoftReset(void);

#endif /* SOURCE_HTU2X_H_ */
