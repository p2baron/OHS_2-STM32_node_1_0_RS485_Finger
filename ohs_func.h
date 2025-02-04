/*
 * ohs_func.h
 *
 *  Created on: Jan 26, 2025
 *      Author: vysocan
 */

#ifndef OHS_FUNC_H_
#define OHS_FUNC_H_

// Float conversion
union u_tag {
  uint8_t b[4];
  float   fval;
} u;
/*
 * Set defaults on first time
 */
void setDefault(void) {
  conf.version = VERSION;   // Change VERSION to force EEPROM re-load
  conf.reg[0+(REG_LEN*0)] = 'S';       // Sensor
  conf.reg[1+(REG_LEN*0)] = 'V';       // Voltage
  conf.reg[2+(REG_LEN*0)] = 0;         // Local address
  conf.reg[3+(REG_LEN*0)] = 0b00000000; // Default setting
  conf.reg[4+(REG_LEN*0)] = 0b00011111; // Default setting, group=16, disabled
  memset(&conf.reg[5+(REG_LEN*0)], 0, NODE_NAME_SIZE);
  conf.reg[0+(REG_LEN*1)] = 'S';       // Sensor
  conf.reg[1+(REG_LEN*1)] = 'X';       // TX power level
  conf.reg[2+(REG_LEN*1)] = 0;         // Local address
  conf.reg[3+(REG_LEN*1)] = 0b00000000; // Default setting
  conf.reg[4+(REG_LEN*1)] = 0b00011111; // Default setting, group=16, disabled
  memset(&conf.reg[5+(REG_LEN*1)], 0, NODE_NAME_SIZE);
  conf.reg[0+(REG_LEN*2)] = 'S';       // Sensor
  conf.reg[1+(REG_LEN*2)] = 'D';       // Digital pin, 1 = charging
  conf.reg[2+(REG_LEN*2)] = 0;         // Local address
  conf.reg[3+(REG_LEN*2)] = 0b00000000; // Default setting
  conf.reg[4+(REG_LEN*2)] = 0b00011111; // Default setting, group=16, disabled
  memset(&conf.reg[5+(REG_LEN*2)], 0, NODE_NAME_SIZE);
  #ifdef HTU2XD_SHT2X_SI70XX
    conf.reg[0+(REG_LEN*3)] = 'S';       // Sensor
    conf.reg[1+(REG_LEN*3)] = 'T';       // Temperature
    conf.reg[2+(REG_LEN*3)] = 0;         // Local address
    conf.reg[3+(REG_LEN*3)] = 0b00000000; // Default setting
    conf.reg[4+(REG_LEN*3)] = 0b00011111; // Default setting, group=16, disabled
    memset(&conf.reg[5+(REG_LEN*3)], 0, NODE_NAME_SIZE);
    conf.reg[0+(REG_LEN*4)] = 'S';       // Sensor
    conf.reg[1+(REG_LEN*4)] = 'H';       // Humidity
    conf.reg[2+(REG_LEN*4)] = 0;         // Local address
    conf.reg[3+(REG_LEN*4)] = 0b00000000; // Default setting
    conf.reg[4+(REG_LEN*4)] = 0b00011111; // Default setting, group=16, disabled
    memset(&conf.reg[5+(REG_LEN*4)], 0, NODE_NAME_SIZE);
  #endif
}

#endif /* OHS_FUNC_H_ */
