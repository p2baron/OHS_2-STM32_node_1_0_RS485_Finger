/*
 * ohs_func.h
 *
 *  Created on: Jan 26, 2025
 *      Author: vysocan
 */

#ifndef OHS_FUNC_H_
#define OHS_FUNC_H_

#define FUNC_DEBUG 1

#if FUNC_DEBUG
#define DBG_FUNC(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG_FUNC(...)
#endif

// Float conversion
union u_tag {
  uint8_t b[4];
  float   fval;
} u;
// time_t conversion
union time_tag {
  char   ch[4];
  time_t val;
} timeConv;
/*
 * Add float value to message
 */
void addFloatVal(uint8_t element, uint8_t *out, float value){
  out[0] = conf.reg[1+(REG_LEN*element)];
  out[1] = conf.reg[2+(REG_LEN*element)];
  u.fval = value;
  memcpy(&out[2], &u.b[0], 4);
}
/*
 *
 */
void enrollFinger(void) {
  uint8_t ret;

  uint8_t featureCount = 6;
  uint16_t location = 1;

  for (int i = 1; i <= featureCount; i++)   {
    R503SetAuraLED(aLEDBreathing, aLEDBlue, 50, 255);
    DBG_FUNC(">> Place finger on sensor...\r\n");
    while (true) {
      chThdSleepMilliseconds(1000);
      ret = R503TakeImage();
      DBG_FUNC(" ret %d,", ret);

      if (ret == R503_NO_FINGER) {
        continue; // try again
      } else if (ret == R503_OK) {
		DBG_FUNC(" >> Image %d of %d taken \r\n", i, featureCount);
        R503SetAuraLED(aLEDBreathing, aLEDWhite, 255, 255);
        // Go for feature extraction
      } else {
		DBG_FUNC("[X] Could not take image (code: 0x%02X)\r\n", ret);
        R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
        chThdSleepMilliseconds(1000);
        continue; // try again
      }

      ret = R503ExtractFeatures(i);
      DBG_FUNC(" ef-ret %d,", ret);

      if (ret != R503_OK) {
        DBG_FUNC("[X] Failed to extract features, trying again (code: 0x%02X)\r\n", ret);
        R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
        chThdSleepMilliseconds(1000);
        continue;
      }
      R503SetAuraLED(aLEDBreathing, aLEDGreen, 255, 255);
	  DBG_FUNC(" >> Features %d of %d extracted\r\n", i, featureCount);
      chThdSleepMilliseconds(250);
      break;
    }

	DBG_FUNC("Lift your finger from the sensor!\r\n");
    while (R503TakeImage() != R503_NO_FINGER) {
      chThdSleepMilliseconds(100);
    }
  }

  DBG_FUNC(" >> Creating template...\r\n");
  R503SetAuraLED(aLEDBreathing, aLEDPurple, 100, 255);
  chThdSleepMilliseconds(100);
  ret = R503CreateTemplate();

  if (ret != R503_OK) {
    DBG_FUNC("[X] Failed to create a template (code: 0x%02X)\r\n", ret);
    R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
    return;
  } else {
    DBG_FUNC(" >> Template created\r\n");
  }

  DBG_FUNC(" >> Storing template...\r\n");
  ret = R503StoreTemplate(1, location);
  if (ret != R503_OK) {
    DBG_FUNC("[X] Failed to store the template (code: 0x%02X)\r\n", ret);
    R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
    return;
  }
  chThdSleepMilliseconds(250);

  R503SetAuraLED(aLEDBreathing, aLEDGreen, 255, 1);
  DBG_FUNC(" >> Template stored at location: %d\r\n", location);
}
/*
 * @brief Search for finger in library
 *
 * @param void
 */
void searchFinger(void) {
  uint8_t ret;

  DBG_FUNC(" >> Place your finger on the sensor...\r\n");
  R503SetAuraLED(aLEDBreathing, aLEDBlue, 50, 255);

  while (true) {
	DBG_FUNC(" >> Waiting for finger...\r\nTime: %u", chVTGetSystemTime());
    ret = R503TakeImage();
    DBG_FUNC(" ret %d, time: %u", ret, chVTGetSystemTime());

    if (ret == R503_NO_FINGER) {
      chThdSleepMilliseconds(250);
      continue;
    } else if (ret == R503_OK) {
      DBG_FUNC(" >> Image taken \r\n");
      R503SetAuraLED(aLEDBreathing, aLEDYellow, 150, 255);
      break;
    } else {
      DBG_FUNC("[X] Could not take image (code: 0x%02X)\r\n", ret);
      R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
      chThdSleepMilliseconds(1000);
      continue;
    }
  }

  // Extract features
  ret = R503ExtractFeatures(1);
  if (ret != R503_OK) {
    DBG_FUNC("[X] Could not extract features (code: 0x%02X)\r\n", ret);
    R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
    return;
  }

  uint16_t location, confidence;
  ret = R503SearchFinger(1, &location, &confidence);
  if (ret == R503_NO_MATCH_IN_LIBRARY) {
    DBG_FUNC(" >> No matching finger found\r\n");
    R503SetAuraLED(aLEDBreathing, aLEDRed, 255, 1);
  } else if (ret == R503_OK) {
    DBG_FUNC(" >> Found finger\r\n");
    DBG_FUNC("    Finger ID: %d\r\n", location);
    DBG_FUNC("    Confidence: %d\r\n", confidence);
    R503SetAuraLED(aLEDBreathing, aLEDGreen, 255, 1);
  } else {
    DBG_FUNC("[X] Could not search library (code: 0x%02X)\r\n", ret);
    R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
  }
}
/*
 *
 */
void downloadTemplate(void) {
  uint8_t ret;
  uint16_t location = 1, size;

  DBG_FUNC(" >> Retrieving...\r\n");
  ret = R503GetTemplate(1, location);
  if (ret != R503_OK) {
    DBG_FUNC("[X] Failed to retrieve the template (code: 0x%02X)\r\n", ret);
    R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
    return;
  }

  memset(&finger[0], 0x55, sizeof(finger));

  DBG_FUNC(" >> Downloading...\r\n");
  ret = R503UploadTemplate(1, &finger[0], &size);
  if (ret != R503_OK) {
    DBG_FUNC("[X] Failed to download the template (code: 0x%02X)\r\n", ret);
    R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
    return;
  }

  //chThdSleepMilliseconds(250);

  for (uint16_t var = 0; var < size; ++var) {
    DBG_FUNC("%02X ", finger[var]);
  }
  DBG_FUNC("\r\n");

  DBG_FUNC("compressing\r\n");
  uint16_t commpressed = rle_compress(&finger[0], size, &comm[0]);
  DBG_FUNC("compress size %d\r\n", commpressed);
  for (uint16_t var = 0; var < commpressed; ++var) {
    DBG_FUNC("%02X ", comm[var]);
  }
  DBG_FUNC("\r\n");

  DBG_FUNC("de-compressing\r\n");

  uint16_t decommpressed = rle_decompress(&comm[0], commpressed, &finger[0]);

  DBG_FUNC("decompress size %d\r\n", decommpressed);
  for (uint16_t var = 0; var < decommpressed; ++var) {
	DBG_FUNC("%02X ", finger[var]);
  }
  DBG_FUNC("\r\n");

//  chThdSleepMilliseconds(250);

//  DBG_FUNC(" >> Erasing...\r\n");
//  ret = R503EmptyLibrary();
//  if (ret != R503_OK) {
//    DBG_FUNC("[X] Failed to erase lib (code: 0x%02X)\r\n", ret);
//    R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
//    return;
//  }
//  chThdSleepMilliseconds(250);
//
//  DBG_FUNC(" >> Uploading...\r\n");
//  ret = R503DownloadTemplate(1, &finger[0], size);
//  if (ret != R503_OK) {
//    DBG_FUNC("[X] Failed to upload (code: 0x%02X)\r\n", ret);
//    R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
//    return;
//  }
//  chThdSleepMilliseconds(250);
//
//  DBG_FUNC(" >> Storing...\r\n");
//  ret = R503StoreTemplate(1, location);
//  if (ret != R503_OK) {
//    DBG_FUNC("[X] Failed to store (code: 0x%02X)\r\n", ret);
//    R503SetAuraLED(aLEDFlash, aLEDRed, 50, 3);
//    return;
//  }
//  chThdSleepMilliseconds(250);
}

#endif /* OHS_FUNC_H_ */
