/*
 * ohs_th_rs485.h
 *
 *  Created on: Jan 26, 2025
 *      Author: vysocan
 */

#ifndef OHS_TH_RS485_H_
#define OHS_TH_RS485_H_

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

  // Wait some time to avoid contention
  chThdSleepMilliseconds(rs485cfg.address * 1000);

  DBG_RS485("Conf:");

  pos = 0;
  msg.address = 0;
  msg.ack = RS485_FLAG_ACK;
  msg.ctrl = RS485_FLAG_DTA;
  msg.length = REG_LEN + 1; // Add 'R'

  while (pos < sizeof(conf.reg)) {
    msg.data[0] = 'R'; // Registration flag
    memcpy(&msg.data[1], &conf.reg[pos], REG_LEN);
    //result = rs485SendMsgWithACK(&RS485D3, &msg, MSG_REPEAT);
    result = rs485SendMsg(&RS485D3, &msg);
    DBG_RS485(" %d",result);
    pos += REG_LEN;
  }

  DBG_RS485(".");
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
  msg.address = 0;
  msg.ctrl = RS485_FLAG_DTA;
  msg.length = 7;
  msg.data[0] = conf.reg[(REG_LEN*element)];
  msg.data[1] = conf.reg[1+(REG_LEN*element)];
  msg.data[2] = conf.reg[2+(REG_LEN*element)];
  msg.data[3] = u.b[0]; msg.data[4] = u.b[1];
  msg.data[5] = u.b[2]; msg.data[6] = u.b[3];
  // Send to GW
  rs485SendMsg(&RS485D3, &msg);
  //rs485SendMsgWithACK(&RS485D3, &msg, MSG_REPEAT);
}

/*
 * RS485 thread
 */
static THD_WORKING_AREA(waRS485Thread, 1024);
static THD_FUNCTION(RS485Thread, arg) {
  chRegSetThreadName(arg);
  event_listener_t serialListener;
  eventmask_t evt;
  msg_t resp;
  RS485Msg_t rs485Msg;

  // Register
  chEvtRegister((event_source_t *)&RS485D3.event, &serialListener, EVENT_MASK(0));

  while (true) {
    evt = chEvtWaitAny(ALL_EVENTS);
    (void)evt;

    eventflags_t flags = chEvtGetAndClearFlags(&serialListener);
    DBG_RS485("%u: ", chVTGetSystemTime());
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
      //DBG_RS485("ib %d %d %d , ob %d %d %d\r\n", RS485D3.ib[0], RS485D3.ib[1],
      //          RS485D3.ib[2], RS485D3.ob[0], RS485D3.ob[1], RS485D3.ob[2]);

      if (resp == MSG_OK) {
        if (rs485Msg.ctrl == RS485_FLAG_CMD) {
        }
        if (rs485Msg.ctrl == RS485_FLAG_DTA) {
        } // data
      } // MSG_OK
    } // (flags & RS485_MSG_RECEIVED)
  }
}

#endif /* OHS_TH_RS485_H_ */
