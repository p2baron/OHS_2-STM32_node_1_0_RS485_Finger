/*
 * rle.h
 *
 *  Created on: Nov 2, 2025
 *      Author: vysocan
 */

#ifndef SOURCE_RLE_H_
#define SOURCE_RLE_H_

#include <stdint.h>

// Helper to allocate worst compression length
#define MAX_OUT_LEN(in) ((in) + ((in) / 2) + 4)

// Defines
#define MAX_14_BIT (0x4000)

// RLE functions
uint16_t rle_compress(const uint8_t *in, uint16_t inLen, uint8_t *out);
uint16_t rle_decompress(const uint8_t *in, uint16_t inLen, uint8_t *out);

#endif /* SOURCE_RLE_H_ */
