/*
 * ohs_th_radio.h
 *
 *  Created on: 31. 3. 2020
 *      Author: adam
 */

#ifndef OHS_TH_RADIO_H_
#define OHS_TH_RADIO_H_

#ifdef HAS_RADIO

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
void sendConf(void){
  int8_t result;
  uint8_t count = 0;

  // Wait some time to avoid contention
  chThdSleepMilliseconds(rfm69cfg.nodeID * 1000);

  DBG_RADIO("Conf:");

  while (count < sizeof(conf.reg)) {
    msgOut[0] = 'R'; // Registration flag
    memcpy(&msgOut[1], &conf.reg[count], REG_LEN);
    result = rfm69SendWithRetry(GATEWAYID, &msgOut[0], REG_LEN + 1, MSG_REPEAT);
    DBG_RADIO(" %d",result);

    count += REG_LEN;
  }

  DBG_RADIO(".");
}
/*
 * Ping
 */
void ping(void){
  msgOut[0] = 'C';
  msgOut[1] = 2; // Ping
  rfm69SendWithRetry(GATEWAYID, &msgOut[0], 2, MSG_REPEAT);
}
/*
 * Send float value of one element to gateway
 */
void sendValue(uint8_t element, float value){
  u.fval = value;
  msgOut[0] = conf.reg[(REG_LEN*element)];
  msgOut[1] = conf.reg[1+(REG_LEN*element)];
  msgOut[2] = conf.reg[2+(REG_LEN*element)];
  memcpy(&msgOut[3], &u.b[0], 4);
  // Send to GW
  rfm69SendWithRetry(GATEWAYID, msgOut, 7, MSG_REPEAT);
  rfm69Sleep();
}
/*
 * Send fingerprint authentication message to gateway
 */
int8_t sendFinger(uint8_t element, uint8_t state, uint16_t fingerId) {
  int8_t resp;
  msgOut[0] = conf.reg[(REG_LEN*element)];     // Element ID
  msgOut[1] = conf.reg[1+(REG_LEN*element)];   // Element type
  msgOut[2] = state;                           // Arming state
  memcpy(&msgOut[3], "finger", 6);
  memcpy(&msgOut[9], &fingerId, 2);
  resp = rfm69SendWithRetry(GATEWAYID, msgOut, 11, MSG_REPEAT);
  rfm69Sleep();
  return resp;
}
/*
 * RFM69 thread
 */
static THD_WORKING_AREA(waRadioThread, 1024);
static THD_FUNCTION(RadioThread, arg) {
  chRegSetThreadName(arg);
  msg_t resp;
  uint8_t temp;

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
          temp = 0;
          while (((conf.reg[temp] != rfm69Data.data[1]) || (conf.reg[temp+1] != rfm69Data.data[2]) ||
                  (conf.reg[temp+2] != rfm69Data.data[3])) && (temp < sizeof(conf.reg))) {
            temp += REG_LEN; // size of one conf. element
          }
          if (temp < sizeof(conf.reg)) {
            // Replace data
            memcpy(&conf.reg[temp], (uint8_t*)&rfm69Data.data[1], REG_LEN);
            // Save it to EEPROM
            conf.version = VERSION;
            writeToFlash(&conf, sizeof(conf));
            // send song to RTTTL thread
            chMBPostTimeout(&rtttlMailbox, (msg_t)tick, TIME_IMMEDIATE);
            DBG_RADIO("Radio: Reg. updated at pos %u\r\n", temp/REG_LEN); // Show # of updated element
          }
          DBG_RADIO("\r\n");
          break;
      } // switch case
    } // received
  }
}

#endif /* HAS_RADIO */
#endif /* OHS_TH_RADIO_H_ */
