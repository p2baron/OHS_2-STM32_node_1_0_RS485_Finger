/*
 * eeprom2flash.h
 *
 *  Created on: Feb 7, 2025
 *      Author: vysocan
 */

#ifndef SOURCE_EEPROM2FLASH_H_
#define SOURCE_EEPROM2FLASH_H_

#include <stdint.h>
#include "ch.h"
#include "hal.h"

/*
 * Our Definitions
 */
#define FLASH_TOT_SIZE  0x10000  // 64kB
// Could be (FLASH_BANK1_END - FLASH_BASE) but ChibiOS would need proper chip definition.
#if defined(STM32F10X_HD) || defined(STM32F10X_XL)
  #define FLASH_PAGE_SIZE 0x800  // 2kB for high/XL density
#else
  #define FLASH_PAGE_SIZE 0x400  // 1kB for low/medium density
#endif
#define FLASH_EE_REGION (FLASH_BASE + FLASH_TOT_SIZE - FLASH_PAGE_SIZE)
#define flash_wait_nb() while (FLASH->SR & FLASH_SR_BSY)
/*
 * Functions
 */
void readFromFlash(void *data, uint16_t len);
void writeToFlash(void *data, uint16_t len);

#endif /* SOURCE_EEPROM2FLASH_H_ */
