/*
 * ohs_th_rs485.h
 *
 *  Created on: Jan 26, 2025
 *      Author: vysocan
 */

#ifndef OHS_TH_RS485_H_
#define OHS_TH_RS485_H_

#ifdef HAS_RS485

#define RS485_DEBUG 1

#if RS485_DEBUG
#define DBG_RS485(...) {chprintf(console, __VA_ARGS__);}
#else
#define DBG_RS485(...)
#endif
/*
 * Registration
 */
void sendConf(void){
  int8_t result;
  uint8_t pos = 0;

  // Wait some time to avoid contention
  chThdSleepMilliseconds(rs485cfg.address * 1000);

  DBG_RS485("Send Conf:");

  pos = 0;
  msgOut.address = 0;
  msgOut.ack = RS485_FLAG_ACK;
  msgOut.ctrl = RS485_FLAG_DTA;
  msgOut.length = REG_LEN + 1; // Add 'R'

  while (pos < sizeof(conf.reg)) {
    msgOut.data[0] = 'R'; // Registration flag
    memcpy(&msgOut.data[1], &conf.reg[pos], REG_LEN);
    result = rs485SendMsgWithACK(&RS485D3, &msgOut, MSG_REPEAT);
    DBG_RS485(" %d",++result);
    pos += REG_LEN;
  }

  DBG_RS485("\r\n");
}
/*
 * Ping
 */
void ping(void) {
  RS485Cmd_t cmd;
  cmd.address = GATEWAYID;
  cmd.length = 2; // PING = 2
  rs485SendCmd(&RS485D3, &cmd);
}
/*
 * Send float value to gateway
 */
void sendValue(uint8_t element, float value) {
  u.fval = value;
  msgOut.address = 0;
  msgOut.ctrl = RS485_FLAG_DTA;
  msgOut.length = 7;
  msgOut.data[0] = conf.reg[(REG_LEN*element)];
  msgOut.data[1] = conf.reg[1+(REG_LEN*element)];
  msgOut.data[2] = conf.reg[2+(REG_LEN*element)];
  msgOut.data[3] = u.b[0]; msgOut.data[4] = u.b[1];
  msgOut.data[5] = u.b[2]; msgOut.data[6] = u.b[3];
  // Send to GW
  rs485SendMsgWithACK(&RS485D3, &msgOut, MSG_REPEAT);
}
/*
 * @brief Send fingerprint authentication message to gateway
 * @param element Element index
 * @param state Arming state, 0=arm away, 1=arm home
 * @param fingerId Fingerprint ID
 * @return Result code
 */
int8_t sendFinger(uint8_t element, uint8_t state, uint16_t fingerId) {
  msgOut.address = GATEWAYID;
  msgOut.ctrl = RS485_FLAG_DTA;
  msgOut.length = 11;
  msgOut.data[0] = conf.reg[(REG_LEN*element)];     // Element ID
  msgOut.data[1] = conf.reg[1+(REG_LEN*element)];   // Element type
  msgOut.data[2] = state;                           // Arming state
  memcpy(&msgOut.data[3], "finger", 6);
  memcpy(&msgOut.data[9], &fingerId, 2);
  return (int8_t)rs485SendMsgWithACK(&RS485D3, &msgOut, MSG_REPEAT);
}
/*
 * RS485 thread
 */
static THD_WORKING_AREA(waRS485Thread, 512);
static THD_FUNCTION(RS485Thread, arg) {
  chRegSetThreadName(arg);
  event_listener_t serialListener;
  eventmask_t evt;
  msg_t resp;
  RS485Msg_t rs485Msg;
  uint8_t temp;
  uint16_t size = 0;

  // Register
  chEvtRegister((event_source_t *)&RS485D3.event, &serialListener, EVENT_MASK(0));

  while (true) {
    evt = chEvtWaitAny(ALL_EVENTS);
    (void)evt;

    eventflags_t flags = chEvtGetAndClearFlags(&serialListener);
    DBG_RS485("RS485 flag: %u, state: %u, length: %u\r\n", flags, RS485D3.trcState, RS485D3.ibHead);
    //resp = chBSemWait(&RS485D3.received);
    if ((flags & RS485_MSG_RECEIVED) ||
        (flags & RS485_MSG_RECEIVED_WA)){
      resp = rs485GetMsg(&RS485D3, &rs485Msg);

      DBG_RS485("RS485: %d, ", resp);
      DBG_RS485("from: %u, ", rs485Msg.address);
      if (rs485Msg.ctrl) {
        DBG_RS485("command: %u.", rs485Msg.length);
      } else {
        DBG_RS485("data (len: %u): ", rs485Msg.length);
        for(uint8_t i = 0; i < rs485Msg.length; i++) {DBG_RS485("%x, ", rs485Msg.data[i]);}
      }
      DBG_RS485("\r\n");

      if (resp == MSG_OK) {
        // Commands
        if (rs485Msg.ctrl == RS485_FLAG_CMD) {
          switch (rs485Msg.length) {
            case NODE_CMD_REGISTRATION: // Request for registration
              sendConf();
              break;
            case NODE_CMD_ARMING ... NODE_CMD_ARMED_HOME : // Mode commands
              setNodeMode((authMode_t)(rs485Msg.length));
              break;
            case NODE_CMD_ARM_REJECTED: // Arm rejected, just play sound
              chMBPostTimeout(&rtttlMailbox, (msg_t) SONG_ARM_REJECTED, TIME_IMMEDIATE);
              break;
            default: break;
          }
        }
        // Data
        if (rs485Msg.ctrl == RS485_FLAG_DTA) {
          switch (rs485Msg.data[0]) {
            case 'R': // Registration
              temp = 0;
              while (((conf.reg[temp] != rs485Msg.data[1]) || (conf.reg[temp+1] != rs485Msg.data[2]) ||
                      (conf.reg[temp+2] != rs485Msg.data[3])) && (temp < sizeof(conf.reg))) {
                temp += REG_LEN; // size of one conf. element
              }
              if (temp < sizeof(conf.reg)) {
                memcpy(&conf.reg[temp], &rs485Msg.data[1], REG_LEN);
                // Save it to EEPROM
                conf.version = VERSION;
                writeToFlash(&conf, sizeof(conf));
                // send song to RTTTL thread
                chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_TICK, TIME_IMMEDIATE);
                DBG_RS485("RS485: Reg. updated at pos %u\r\n", temp/REG_LEN); // Show # of updated element
              }
              break;
            case 'T': // Time beacon
              memcpy(&timeConv.ch[0], &rs485Msg.data[1], 4);
              convertUnixSecondToRTCDateTime(&timespec, timeConv.val);
              rtcSetTime(&RTCD1, &timespec);
              DBG_RS485("RS485: Time updated to %u\r\n", timeConv.val);
              rtcGetTime(&RTCD1, &timespec);
              DBG_RS485("RTC: Time updated to %u\r\n", convertRTCDateTimeToUnixSecond(&timespec));
              break;
            case 'F': // Fingerprint
              DBG_RS485("RS485: Fingerprint command received\r\n");
              switch (rs485Msg.data[1]) {
                case 'E': // Enroll
                  enrollFinger((uint16_t)rs485Msg.data[2]);
                  break;
                case 'G': // Get template
                  temp = downloadTemplate((uint16_t)rs485Msg.data[2], &finger[0], &fingerSize);
                  if (temp == R503_OK) {
                    DBG_RS485("RS485: Fingerprint template downloaded, size %u\r\n", fingerSize);
                	}
                	// Compress template
                	size = rle_compress(&finger[0], fingerSize, &compressed[0]);
                	if (size > fingerSize) {
                    DBG_RS485("RS485: Fingerprint template compressed, size %u\r\n", size);
                	} else {
                    DBG_RS485("RS485: Fingerprint template not compressed, size %u\r\n", size);
                	}
                	// To be implemented: send template back to gateway
                  break;
                case 'S': // Save template
                	// To be implemented: receive template from gateway and save to location
                	temp = uploadTemplate((uint16_t)rs485Msg.data[2], &finger[0], fingerSize);
                  break;
                default:
                  break;
              }
              break;
            default:
              break;
          }
        } // data
      } // MSG_OK
    } // (flags & RS485_MSG_RECEIVED)
  }
}

#endif /* HAS_RS485 */
#endif /* OHS_TH_RS485_H_ */
