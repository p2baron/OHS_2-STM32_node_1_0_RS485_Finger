/*
 * ohs_th_service.h
 *
 *  Created on: 23. 2. 2020
 *      Author: vysocan
 */

#ifndef OHS_TH_SERVICE_H_
#define OHS_TH_SERVICE_H_

#ifndef SERVICE_DEBUG
#define SERVICE_DEBUG 0
#endif

#if SERVICE_DEBUG
#define DBG_SERVICE(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG_SERVICE(...)
#endif

// Float conversion
union u_tag {
  uint8_t b[4];
  float   fval;
} u;
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
 * Send float value of one element to gateway
 */
void sendValue(uint8_t element, float value){
  u.fval = value;
  msg[0] = conf.reg[(REG_LEN*element)];
  msg[1] = conf.reg[1+(REG_LEN*element)];
  msg[2] = conf.reg[2+(REG_LEN*element)];
  memcpy(&msg[3], &u.b[0], 4);
  // Send to GW
  rfm69SendWithRetry(GATEWAYID, msg, 7, RADIO_REPEAT);
  rfm69Sleep();
}
/*
 * Service thread
 */
static THD_WORKING_AREA(waServiceThread, 256);
static THD_FUNCTION(ServiceThread, arg) {
  chRegSetThreadName(arg);

  uint16_t counter = 1;

  while (true) {
    // 1 second sleep, we do not care much about time of execution.
    chThdSleepMilliseconds(1000);
    counter++;
    // reset counter every one hour
    if (counter == 3600) counter = 0;

    // Ping
    if (counter == 0) {
      msg[0] = 'C';
      msg[1] = 2; // Ping
      rfm69SendWithRetry(GATEWAYID, &msg[0], 2, RADIO_REPEAT);
    }

    // Sensor data
    if ((counter%60) == 0) {
      sendValue(0, (float)counter);
    }

  } // while(true)
}

#endif /* OHS_TH_SERVICE_H_ */
