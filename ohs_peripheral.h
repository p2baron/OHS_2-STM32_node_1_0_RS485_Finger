/*
 * ohs_peripherial.h
 *
 *  Created on: 23. 2. 2020
 *      Author: vysocan
 *
 * Peripheral configurations
 */

/*
 * SPI macros for 72Mhz
 * Peripherial Clock /2 = 36MHz for SPI1
 * SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0
 * 000: f PCLK/2    18M
 * 001: f PCLK/4    9M
 * 010: f PCLK/8    4.5M
 * 011: f PCLK /16  2.25M
 * 100: f PCLK/32   1.125M
 * 101: f PCLK/64   562k
 * 110: f PCLK /128 281k
 * 111: f PCLK /256 140k
 */
#ifndef OHS_PERIPHERAL_H_
#define OHS_PERIPHERAL_H_
#ifdef HAS_RADIO
/*
 * RFM69HW SPI setting
 */
const SPIConfig spi1cfg = {
  false,
  NULL,
  GPIOA, // CS PORT
  GPIOA_SS, // CS PIN
  SPI_CR1_BR_2,// || SPI_CR1_BR_0,
  0,
};
/*
 * RFM69 configuration
 */

rfm69Config_t rfm69cfg = {
  &SPID1,
  &spi1cfg,
  LINE_RADIO_INT,
  RF69_868MHZ,
  20,
  100
};
#endif
/*
 * Console default setting
 */
#ifdef HAS_SERIAL1
static SerialConfig serialCfg = {
  115200,
  0,
  0,
  0
};
#endif
/*
 * RS485 default setting
 */
#ifdef HAS_RS485
static RS485Config rs485cfg = {
  19200,    // speed
  7,        // address
  GPIOB,    // port
  GPIOB_DE3 // pad
};
#endif

#endif /* OHS_PERIPHERAL_H_ */
