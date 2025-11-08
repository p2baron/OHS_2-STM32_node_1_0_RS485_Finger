/*
 * rle.c
 *
 *  Created on: Nov 2, 2025
 *      Author: vysocan
 */
#include "rle.h"
/*
 * @brief Run-Length Encoding (RLE) Encoder
 *
 * @param in Pointer to input array.
 * @param inLen Input array length.
 * @param out Pointer to output array. Should bigger then input, use MAX_OUT_LEN formula.
 *
 * @return uint16_t Returns length of encoded output.
 *
 * Repeat command: High (8th) bit set (0x80). Lower 6 bits represent the repetition count (1–63).
 * Literal command: High bit unset. Lower 6 bits indicate the number of subsequent literal bytes (1–63).
 *
 * Example:
 * Original data: 0xAA, 0xAA, 0xAA, 0xBB, 0xCC, 0xCC
 * Compressed:    0x82, 0xAA, 0x01, 0xBB, 0x81, 0xCC
 *
 * When then 7th bit of command is set, then the next byte in input is used to
 *   extend the count of repeat of literal to 14bits number (<16384).
 *
 * Original data: 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, ... 95x 0xFF
 * Compressed:    0xC0, 0x64, 0xFF
 */
uint16_t rle_compress(const uint8_t *in, uint16_t inLen, uint8_t *out) {
  uint16_t outIdx = 0, inIdx = 0, start, repeat, literalLen;
  uint8_t byte;

  while (inIdx < inLen) {
    byte = in[inIdx];
    repeat = 1;
    // Detect repeating sequence
    while ((inIdx + repeat) < inLen && in[inIdx + repeat] == byte &&
        repeat < MAX_14_BIT) {
      repeat++;
    }
    // Decide
    if (repeat > 1) {
      // Insert repeats
      if (repeat < 63) {
        out[outIdx++] = 0x80 | (repeat - 1);
      } else {
        out[outIdx++] = 0xC0 | ((repeat - 1) >> 8);
        out[outIdx++] = (repeat - 1) & 0xFF;
      }
      inIdx += repeat;
      out[outIdx++] = byte;
    } else {
      // Collect literals
      start = inIdx;
      while (inIdx < inLen && (inIdx - start < MAX_14_BIT) &&
            (inIdx + 1 >= inLen || in[inIdx + 1] != in[inIdx])) {
        inIdx++;
      }
      literalLen = inIdx - start;
      if (literalLen < 63) {
        out[outIdx++] = literalLen - 1;
      } else {
        out[outIdx++] = 0x40 | ((literalLen - 1) >> 8);
        out[outIdx++] = (literalLen - 1) & 0xFF;
      }
      // 'repeat' here as temp. variable
      for (repeat = 0; repeat < literalLen; repeat++) {
        out[outIdx++] = in[start + repeat];
      }
    }
  }
  return outIdx;
}
/*
 * @brief Run-Length Encoding (RLE) Decoder
 *
 * @param in Pointer to input array.
 * @param inLen Input array length.
 * @param out Pointer to output array. Should bigger then input, use MAX_OUT_LEN formula.
 *
 * @return uint16_t Returns length of encoded output.
 */
uint16_t rle_decompress(const uint8_t *in, uint16_t inLen, uint8_t *out) {
  uint16_t outIdx = 0, inIdx = 0, i, count;
  uint8_t byte;

  while (inIdx < inLen) {
    byte = in[inIdx++];
    // Repeat count
    if (byte & 0x40) { // Extended repeat (14-bit count)
      count = ((byte & 0x3F) << 8) | in[inIdx++];
      count += 1;
    } else { // Short repeat (6-bit count)
      count = (byte & 0x3F) + 1;
    }
    // Decide type
    if (byte & 0x80) {
      // Repeat command
      byte = in[inIdx++];
      for (i = 0; i < count; i++) {
        out[outIdx++] = byte;
      }
    } else {
      // Literal command
      for (i = 0; i < count; i++) {
        out[outIdx++] = in[inIdx++];
      }
    }
  }
  return outIdx;
}
