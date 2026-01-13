/*
 * reg_defaults.h
 *
 *  Created on: Nov 7, 2025
 *      Author: vysocan
 */

#ifndef REG_DEFAULTS_H_
#define REG_DEFAULTS_H_

/*
 * Set defaults on first time
 */
void setDefault(void) {
  conf.version = VERSION;   // Change VERSION to take effect
  conf.reg[0+(REG_LEN*0)]  = 'K';       // Key
  conf.reg[1+(REG_LEN*0)]  = 'f';       // iButton
  conf.reg[2+(REG_LEN*0)]  = 0;         // Local address, must be even number
  conf.reg[3+(REG_LEN*0)]  = 0b00000000; // Default setting
  conf.reg[4+(REG_LEN*0)]  = 0b0011111; // Default setting, group='not set', enabled
  memset(&conf.reg[5+(REG_LEN*0)], 0, NODE_NAME_SIZE);
  strcpy((char *)&conf.reg[5+(REG_LEN*0)], "Fingerprint"); // Set default name
}

#endif /* REG_DEFAULTS_H_ */
