/*
 * ohs_th_radio.h
 *
 *  Created on: 31. 3. 2020
 *      Author: adam
 */

#ifndef OHS_TH_RADIO_H_
#define OHS_TH_RADIO_H_

#ifndef RADIO_DEBUG
#define RADIO_DEBUG 1
#endif

#if RADIO_DEBUG
#define DBG_RADIO(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG_RADIO(...)
#endif
/*
 * Registration
 */
void sendConf(){
  int8_t result;
  uint8_t count = 0;

  // Wait some time to avoid contention
  chThdSleepMilliseconds(rfm69cfg.nodeID * 1000);

  #ifdef SERIAL_PORT
    SERIAL_PORT.print(F("Conf:"));
  #endif

  while (count < sizeof(conf.reg)) {
    msg[0] = 'R'; // Registration flag
    memcpy(&msg[1], &conf.reg[count], REG_LEN);
    result = rfm69SendWithRetry(GATEWAYID, &msg[0], REG_LEN + 1, RADIO_REPEAT);
    //result = rfm69Send(1, &msg[0], REG_LEN + 1, false);
    #ifdef SERIAL_PORT
      SERIAL_PORT.print(F(" ")); SERIAL_PORT.print(result);
    #endif
    count += REG_LEN;
  }

  #ifdef SERIAL_PORT
    SERIAL_PORT.println(F("."));
  #endif
}
/*
 * RFM69 thread
 */
static THD_WORKING_AREA(waRadioThread, 1024);
static THD_FUNCTION(RadioThread, arg) {
  chRegSetThreadName(arg);
  msg_t resp;
  uint8_t tmp;

  while (true) {
    // Wait for packet
    resp = chBSemWaitTimeout(&rfm69DataReceived, TIME_INFINITE);

    // Process packet
    if ((resp == MSG_OK) && (rfm69GetData() == RF69_RSLT_OK)) {
      DBG_RADIO("Radio from: %u, RSSI: %d, Data: ", rfm69Data.senderId, rfm69Data.rssi);
      for(uint8_t i = 0; i < rfm69Data.length; i++) { DBG_RADIO("%x, ", rfm69Data.data[i]); }
      DBG_RADIO("\r\n");

      // Do some logic on received packet
      switch(rfm69Data.data[0]) {
        // Commands
        case 'C':
          DBG_RADIO("Radio command #%u\r\n", rfm69Data.data[1]);
          // Commands from gateway
          switch (rfm69Data.data[1]) {
            case 1: // Request for registration
              sendConf();
              break;
            default: break;
          }
          break;
        // Registration
        case 'R':
          DBG_RADIO("Registration # ");
          tmp = 0;
          while (((conf.reg[tmp] != rfm69Data.data[1]) || (conf.reg[tmp+1] != rfm69Data.data[2]) ||
                  (conf.reg[tmp+2] != rfm69Data.data[3])) && (tmp < sizeof(conf.reg))) {
            tmp += REG_LEN; // size of one conf. element
          }
          if (tmp < sizeof(conf.reg)) {
            DBG_RADIO("%u", tmp/REG_LEN); // Show # of updated element
            // Replace data
            memcpy(&conf.reg[tmp], (uint8_t*)&rfm69Data.data[1], REG_LEN);
            // Save it to EEPROM
            conf.version = VERSION;
            // Update EEPROM
            //***EEPROM.put(0, conf);
          }
          DBG_RADIO("\r\n");
          break;
      } // switch case
    } // received
  }
}

#endif /* OHS_TH_RADIO_H_ */
