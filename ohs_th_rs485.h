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
 * @brief Send NFC card UID authentication message to gateway.
 * Uses 'N'+'R' prefix — distinct from fingerprint 'F' messages so gateway
 * can route by auth source without overlap.
 */
#ifdef HAS_NFC
int8_t sendNFCCard(uint8_t element, uint8_t state, const uint8_t *uid, uint8_t uidLen) {
  msgOut.address = GATEWAYID;
  msgOut.ctrl = RS485_FLAG_DTA;
  msgOut.length = (uint8_t)(7 + uidLen); /* reg[0]+reg[1]+state+"nfc"+uidLen+uid */
  msgOut.data[0] = conf.reg[(REG_LEN*element)];
  msgOut.data[1] = conf.reg[1+(REG_LEN*element)];
  msgOut.data[2] = state;
  memcpy(&msgOut.data[3], "nfc", 3);
  msgOut.data[6] = uidLen;
  memcpy(&msgOut.data[7], uid, uidLen);
  return (int8_t)rs485SendMsgWithACK(&RS485D3, &msgOut, MSG_REPEAT);
}
#endif

/*
 * Low-level RS485 data send — required by sendDataMultipart() in ohs_multipart.h.
 * Reuses the global msgOut to keep stack usage minimal.
 */
static int8_t sendDataDirect(uint8_t address, const uint8_t *data, uint8_t length) {
  msgOut.address = address;
  msgOut.ctrl    = RS485_FLAG_DTA;
  msgOut.length  = length;
  memcpy(&msgOut.data[0], data, length);
  return (rs485SendMsgWithACK(&RS485D3, &msgOut, MSG_REPEAT) == MSG_OK) ? 1 : -1;
}

#include "ohs_multipart.h"

/*
 * Maximum RS485 address to probe when pushing a new template to peers.
 * Adjust to match the number of nodes on the bus.
 */
#define FINGER_SYNC_MAX_ADDR 4

static void addPendingSync(uint16_t loc, uint8_t addr) {
  uint8_t free = PENDING_SYNC_SLOTS;
  for (uint8_t i = 0; i < PENDING_SYNC_SLOTS; i++) {
    if (conf.pendingSync[i].location == loc) {
      conf.pendingSync[i].failMask |= (uint8_t)(1u << (addr - 1));
      writeToFlash(&conf, sizeof(conf));
      return;
    }
    if (conf.pendingSync[i].location == 0xFFFF && free == PENDING_SYNC_SLOTS) free = i;
  }
  if (free < PENDING_SYNC_SLOTS) {
    conf.pendingSync[free].location = loc;
    conf.pendingSync[free].failMask = (uint8_t)(1u << (addr - 1));
    writeToFlash(&conf, sizeof(conf));
  }
}

static void removePendingSyncAddr(uint16_t loc, uint8_t addr) {
  for (uint8_t i = 0; i < PENDING_SYNC_SLOTS; i++) {
    if (conf.pendingSync[i].location == loc) {
      conf.pendingSync[i].failMask &= (uint8_t)(~(1u << (addr - 1)));
      if (conf.pendingSync[i].failMask == 0)
        conf.pendingSync[i].location = 0xFFFF;
      writeToFlash(&conf, sizeof(conf));
      return;
    }
  }
}

static void clearPendingSync(uint16_t loc) {
  for (uint8_t i = 0; i < PENDING_SYNC_SLOTS; i++) {
    if (conf.pendingSync[i].location == loc) {
      conf.pendingSync[i].location = 0xFFFF;
      conf.pendingSync[i].failMask = 0;
      writeToFlash(&conf, sizeof(conf));
      return;
    }
  }
}

static void syncDeleteToNodes(uint16_t loc) {
  RS485Msg_t drainMsg;
  uint8_t delData[3] = {'F', 'D', (uint8_t)loc};
  for (uint8_t addr = 1; addr <= FINGER_SYNC_MAX_ADDR; addr++) {
    if (addr == rs485cfg.address) continue;
    DBG_RS485("FP sync delete: -> addr %u\r\n", addr);
    if (RS485D3.trcState == TRC_RECEIVED)
      rs485GetMsg(&RS485D3, &drainMsg);
    sendDataDirect(addr, delData, sizeof(delData));
  }
}

static void retryPendingSyncs(void) {
  bool hasAny = false;
  for (uint8_t i = 0; i < PENDING_SYNC_SLOTS; i++) {
    if (conf.pendingSync[i].location != 0xFFFF) { hasAny = true; break; }
  }
  if (!hasAny) return;

  chThdSleepMilliseconds(3000); // Let R503 initialize before accessing it
  uint8_t pendingCount = 0;
  for (uint8_t i = 0; i < PENDING_SYNC_SLOTS; i++)
    if (conf.pendingSync[i].location != 0xFFFF) pendingCount++;
  DBG_RS485("FP sync: retrying %u pending entries on boot\r\n", pendingCount);

  for (uint8_t i = 0; i < PENDING_SYNC_SLOTS; i++) {
    if (conf.pendingSync[i].location == 0xFFFF || conf.pendingSync[i].failMask == 0) continue;
    uint16_t loc  = conf.pendingSync[i].location;
    uint8_t  mask = conf.pendingSync[i].failMask;

    chBSemWait(&R503Sem);
    bool dlOK = (downloadTemplate(loc, &finger[0], &fingerSize) == R503_OK);
    chBSemSignal(&R503Sem);

    if (!dlOK) {
      DBG_RS485("FP sync retry: loc %u download failed\r\n", loc);
      continue;
    }

    compressed[0] = 'F'; compressed[1] = 'C';
    memcpy(&compressed[2], &loc, 2);
    uint16_t compLen = rle_compress(&finger[0], fingerSize, &compressed[4]);
    uint16_t syncLen = (uint16_t)(compLen + 4);

    for (uint8_t addr = 1; addr <= FINGER_SYNC_MAX_ADDR; addr++) {
      if (!(mask & (uint8_t)(1u << (addr - 1)))) continue;
      if (addr == rs485cfg.address) continue;
      DBG_RS485("FP sync retry: -> addr %u (loc %u)\r\n", addr, loc);
      RS485Msg_t drainMsg;
      int8_t result = -1;
      for (uint8_t attempt = 0; attempt < 3 && result != 1; attempt++) {
        if (attempt > 0) chThdSleepMilliseconds(200);
        if (RS485D3.trcState == TRC_RECEIVED)
          rs485GetMsg(&RS485D3, &drainMsg);
        result = sendDataMultipart(addr, compressed, syncLen);
      }
      if (result == 1) {
        DBG_RS485("FP sync retry: addr %u OK\r\n", addr);
        removePendingSyncAddr(loc, addr);
      } else {
        DBG_RS485("FP sync retry: addr %u still failed\r\n", addr);
      }
    }
  }
}

static void syncFingerprintToNodes(uint16_t location) {
  uint8_t addr;
  RS485Msg_t drainMsg;

  if (downloadTemplate(location, &finger[0], &fingerSize) != R503_OK) {
    DBG_RS485("FP sync: template download failed\r\n");
    return;
  }

  compressed[0] = 'F';
  compressed[1] = 'C'; // RLE-compressed payload
  memcpy(&compressed[2], &location, 2);
  uint16_t compLen = rle_compress(&finger[0], fingerSize, &compressed[4]);
  uint16_t syncLen = (uint16_t)(compLen + 4);

  DBG_RS485("FP sync: %u bytes (raw %u), scanning addrs 1..%u\r\n",
            syncLen, (uint16_t)(fingerSize + 4), FINGER_SYNC_MAX_ADDR);

  for (addr = 1; addr <= FINGER_SYNC_MAX_ADDR; addr++) {
    if (addr == rs485cfg.address) continue;
    DBG_RS485("FP sync: -> addr %u\r\n", addr);
    int8_t result = -1;
    for (uint8_t attempt = 0; attempt < 3 && result != 1; attempt++) {
      if (attempt > 0)
        chThdSleepMilliseconds(200);
      if (RS485D3.trcState == TRC_RECEIVED)
        rs485GetMsg(&RS485D3, &drainMsg);
      result = sendDataMultipart(addr, compressed, syncLen);
    }
    if (result == 1) {
      DBG_RS485("FP sync: addr %u OK\r\n", addr);
      removePendingSyncAddr(location, addr);
    } else {
      DBG_RS485("FP sync: addr %u failed\r\n", addr);
      addPendingSync(location, addr);
    }
  }
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
  uint8_t temp;
  uint16_t size = 0;

  // Register
  chEvtRegister((event_source_t *)&RS485D3.event, &serialListener, EVENT_MASK(0));

  retryPendingSyncs();

  while (true) {
    // Timed wait allows periodic mpRx timeout checks without a separate thread
    evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(1000));
    (void)evt;

    mpRxCheckTimeout(&mpRx);

    eventflags_t flags = chEvtGetAndClearFlags(&serialListener);
    if (flags == 0) continue; // timeout tick, no RS485 event

    DBG_RS485("RS485 flag: %u, state: %u, length: %u\r\n", flags, RS485D3.trcState, RS485D3.ibHead);

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
        // Update gateway contact timestamp on any message from gateway
        if (rs485Msg.address == GATEWAYID) lastGwContact = chVTGetSystemTimeX();
        // Commands
        if (rs485Msg.ctrl == RS485_FLAG_CMD) {
          switch (rs485Msg.length) {
            case NODE_CMD_REGISTRATION: // Request for registration
              sendConf();
              break;
            case NODE_CMD_ARMING ... NODE_CMD_ARMED_HOME : // Mode commands
              setNodeMode((authMode_t)(rs485Msg.length));
#ifdef HAS_DISPLAY
              // Reset countdown max when entering idle states (fresh next delay)
              if (rs485Msg.length == NODE_CMD_DISARMED || rs485Msg.length == NODE_CMD_ARMED_AWAY
                  || rs485Msg.length == NODE_CMD_ARMED_HOME)
                dispCountdownMax = 0;
#endif
              break;
            case NODE_CMD_ARM_REJECTED: // Arm rejected, just play sound
              chMBPostTimeout(&rtttlMailbox, (msg_t) SONG_ARM_REJECTED, TIME_IMMEDIATE);
#ifdef HAS_DISPLAY
              dispHoldMode  = NODE_ARM_REJECTED;
              dispHoldTicks = 24; // 6 seconds — zone name arrives slightly after the command
#endif
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
            case 'F': // Fingerprint command from gateway
              DBG_RS485("RS485: Fingerprint command received\r\n");
              switch (rs485Msg.data[1]) {
                case 'E': // Enroll, then upload template to gateway
                  temp = rs485Msg.data[2]; // save location — rs485Msg may be overwritten below
#ifdef HAS_DISPLAY
                  dispEnrollSlot = temp;
#endif
                  DBG_RS485("RS485: Enroll cmd slot %u\r\n", temp);
                  if (enrollFinger((uint16_t)temp) == R503_OK) {
                    DBG_RS485("RS485: enrollFinger OK\r\n");
                    chBSemWait(&R503Sem);
                    R503SetAuraLED(aLEDModeON, aLEDGreen, 50, 0);
                    // Drain any RS485 message that arrived while the thread was
                    // blocked inside enrollFinger(). Check driver state directly —
                    // event flags may already be consumed by chEvtWaitAnyTimeout.
                    // Without this, rs485SendMsgWithACK cannot send (TRC_RECEIVED).
                    chEvtGetAndClearFlags(&serialListener); // discard stale flags
                    if (RS485D3.trcState == TRC_RECEIVED)
                      rs485GetMsg(&RS485D3, &rs485Msg);
                    // Download and upload template to gateway (addr 0)
                    if (downloadTemplate((uint16_t)temp, &finger[0], &fingerSize) == R503_OK) {
                      compressed[0] = 'F'; compressed[1] = 'U';
                      compressed[2] = (uint8_t)temp; compressed[3] = 0;
                      size = rle_compress(&finger[0], fingerSize, &compressed[4]);
                      if (size >= fingerSize) { // RLE expanded — send raw
                        memcpy(&compressed[4], &finger[0], fingerSize);
                        size = fingerSize;
                      }
                      sendDataMultipart(GATEWAYID, compressed, (uint16_t)(size + 4));
                      DBG_RS485("RS485: FP enrolled slot %u, uploaded %u bytes to GW\r\n", temp, size);
#ifdef HAS_DISPLAY
                      dispEnrollStep = 4; // "OK!"
                      dispHoldMode   = MODE_ENROLLMENT;
                      dispHoldTicks  = 12; // 3 seconds
#endif
                    }
                    chBSemSignal(&R503Sem);
                    setLastNodeMode(); // re-enable main loop R503 access
                  }
                  break;
                case 'D': { // Delete
                  uint8_t delLoc = rs485Msg.data[2];
                  chBSemWait(&R503Sem);
                  R503DeleteTemplate((uint16_t)delLoc, 1);
                  chBSemSignal(&R503Sem);
                  clearPendingSync((uint16_t)delLoc);
                  if (delLoc < FINGERS_SIZE) { conf.fpId[delLoc] = 0; writeToFlash(&conf, sizeof(conf)); }
                  chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_TICK, TIME_IMMEDIATE);
                  // Forward to peers only if the command came from the gateway (not a peer forwarding it)
                  if (rs485Msg.address == 0)
                    syncDeleteToNodes((uint16_t)delLoc);
                  break;
                }
                case 'A': // Flush all templates from R503 sensor
                  chBSemWait(&R503Sem);
                  R503EmptyLibrary();
                  chBSemSignal(&R503Sem);
                  for (uint8_t si = 0; si < PENDING_SYNC_SLOTS; si++) {
                    conf.pendingSync[si].location = 0xFFFF;
                    conf.pendingSync[si].failMask = 0;
                  }
                  memset(conf.fpId, 0, sizeof(conf.fpId));
                  writeToFlash(&conf, sizeof(conf));
                  DBG_RS485("RS485: FP flush done\r\n");
                  chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_TICK, TIME_IMMEDIATE);
                  break;
                case 'G': // Get specific template, send back to requester
                  chBSemWait(&R503Sem);
                  temp = downloadTemplate((uint16_t)rs485Msg.data[2], &finger[0], &fingerSize);
                  chBSemSignal(&R503Sem);
                  if (temp == R503_OK) {
                    compressed[0] = 'F'; compressed[1] = 'G';
                    compressed[2] = rs485Msg.data[2]; compressed[3] = 0;
                    size = rle_compress(&finger[0], fingerSize, &compressed[4]);
                    if (size >= fingerSize) {
                      memcpy(&compressed[4], &finger[0], fingerSize);
                      size = fingerSize;
                    }
                    sendDataMultipart(rs485Msg.address, compressed, (uint16_t)(size + 4));
                    DBG_RS485("RS485: FP get slot %u sent %u bytes\r\n", rs485Msg.data[2], size);
                  }
                  break;
                case 'Q': // Query — send ID table to gateway
                  msgOut.address = GATEWAYID;
                  msgOut.ctrl = RS485_FLAG_DTA;
                  msgOut.data[0] = 'F'; msgOut.data[1] = 'I';
                  memcpy(&msgOut.data[2], &conf.fpId[0], FINGERS_SIZE * sizeof(uint16_t));
                  msgOut.length = 2 + FINGERS_SIZE * sizeof(uint16_t);
                  rs485SendMsgWithACK(&RS485D3, &msgOut, MSG_REPEAT);
                  DBG_RS485("RS485: FP ID table sent to GW\r\n");
                  break;
                default:
                  break;
              }
              break;
            case MP_MARKER: { // Multipart chunk from a peer node
              int8_t mpResp = mpRxProcess(&mpRx, rs485Msg.address,
                                           rs485Msg.data, rs485Msg.length,
                                           compressed, sizeof(compressed));
              DBG_RS485("MP: chunk from %u, resp=%d\r\n", rs485Msg.address, mpResp);
              if (mpResp == 1) {
                DBG_RS485("MP: complete, %u bytes, type='%c%c'\r\n",
                          mpRx.receivedLength, compressed[0], compressed[1]);
                if (compressed[0] == 'F' && compressed[1] == 'C') {
                  // Fingerprint push from gateway (6-byte header: 'F','C',slot_lo,slot_hi,id_lo,id_hi)
                  uint16_t loc, fpid;
                  memcpy(&loc,  &compressed[2], 2);
                  memcpy(&fpid, &compressed[4], 2);
                  uint16_t payloadLen = (uint16_t)(mpRx.receivedLength - 6);
                  fingerSize = rle_decompress(&compressed[6], payloadLen, &finger[0]);
                  setNodeMode(MODE_ENROLLMENT); // block main loop from touching R503
                  if (uploadTemplate(loc, &finger[0], fingerSize) == R503_OK) {
                    if (loc < FINGERS_SIZE) {
                      conf.fpId[loc] = fpid;
                      writeToFlash(&conf, sizeof(conf));
                    }
                    DBG_RS485("FP sync: stored at loc %u id %u, size %u\r\n", loc, fpid, fingerSize);
                    chMBPostTimeout(&rtttlMailbox, (msg_t)SONG_TICK, TIME_IMMEDIATE);
#ifdef HAS_DISPLAY
                    dispEnrollSlot = (uint8_t)loc;
                    dispEnrollStep = 5; // "SYNCING / Received" (gateway push, not finger scan)
                    dispHoldMode   = MODE_ENROLLMENT;
                    dispHoldTicks  = 8; // 2 seconds
#endif
                  } else {
                    DBG_RS485("FP sync: upload failed\r\n");
                  }
                  setLastNodeMode(); // re-enable main loop R503 access
                }
                mpRxReset(&mpRx);
              } else if (mpResp < 0) {
                DBG_RS485("MP: chunk error\r\n");
              }
            } break;
            case 'N': // NFC gateway commands — reserved for future enrollment/delete
              break;
#ifdef HAS_DISPLAY
            case 'D': // Display control from gateway
              switch (rs485Msg.data[1]) {
                case 'E': // Exit delay (arming countdown)
                  dispCountdownSecs = rs485Msg.data[2];
                  dispCountdownMax  = rs485Msg.data[2];
                  break;
                case 'I': // Entry delay — send total remaining; only grow Max for bar scaling
                  dispCountdownSecs = rs485Msg.data[2];
                  if (rs485Msg.data[2] > dispCountdownMax) dispCountdownMax = rs485Msg.data[2];
                  break;
                case 'Z': // Zone name blocking arm
                  strncpy(dispZoneName, (char*)&rs485Msg.data[2], 16);
                  dispZoneName[16] = '\0';
                  break;
              }
              break;
#endif
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
