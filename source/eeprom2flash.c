/*
 * eeprom2flash.c
 *
 *  Created on: Feb 7, 2025
 *      Author: vysocan
 */
#include <stdint.h>
#include <eeprom2flash.h>
/*
 *
 */
void readFromFlash(void *data, uint16_t len) {

  // Convert to halfwords (= 16 bits, shorts).
  len++;
  len = len >> 1;

  // Read data
  for (uint16_t i=0;i<len;i++){
    ((uint16_t *)data)[i] = ((uint16_t *)FLASH_EE_REGION)[i];
  }
}
/*
 *
 */
void writeToFlash(void *data, uint16_t len) {
  uint16_t i;

  // Convert to halfwords (= 16 bits, shorts).
  len++;
  len = len >> 1;

  // Check data
  for (i=0;i<len;i++){
    if (((uint16_t *)FLASH_EE_REGION)[i] != ((uint16_t *)data)[i]) break;
  }
  // The data there is already OK.
  if (i == len) {
    return;
  }

  if (FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = 0x45670123;
    FLASH->KEYR = 0xcdef89ab;
  }

  for (i=0;i<len;i++) {
    if (((uint16_t *)FLASH_EE_REGION)[i] != 0xffff) break;
  }

  if (i != len) {
    // We need erase
    if ( FLASH->SR & FLASH_SR_EOP) {
      FLASH->SR |= FLASH_SR_EOP;
    }
    flash_wait_nb();
    FLASH->CR = FLASH_CR_PER;
    FLASH->AR = FLASH_EE_REGION;
    FLASH->CR |= FLASH_CR_STRT;
    flash_wait_nb();
    FLASH->CR &= ~FLASH_CR_PER;
  }

  FLASH->CR |= FLASH_CR_PG;

  for (i=0;i<len;i++) {
    ((uint16_t *)FLASH_EE_REGION)[i] = ((uint16_t *)data)[i];
  }

  FLASH->CR &= ~FLASH_CR_PG;
  FLASH->CR |= FLASH_CR_LOCK;
}

