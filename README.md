# OHS Fingerprint Node

Fingerprint-reader node firmware for the [Open Home Security](https://openhomesecurity.blogspot.com/)
alarm system. Built for an STM32F103 ("OHS mini" board) on the system's RS485
bus, using an R503 optical fingerprint sensor for arming/disarming.

This firmware is the counterpart to
[p2baron/OHS_2-gateway](https://github.com/p2baron/OHS_2-gateway) — a fork of
the original [vysocan/OHS_2-gateway](https://github.com/vysocan/OHS_2-gateway)
with fingerprint support added. **You need both**: this node firmware talks to
that gateway fork over the existing `'F'+<sub-command>` RS485 protocol; it will
not work against the unmodified upstream gateway.

## What it does

- Scans fingerprints on an R503 sensor and tells the gateway to arm or disarm
  (each node can be configured to request "arm away" or "arm home").
- **Peer-to-peer template sync**: when a fingerprint is enrolled or deleted on
  one node (via the gateway's Fingerprints page), every other fingerprint node
  on the bus automatically receives the same template — no gateway round-trip
  needed to recognize a finger enrolled elsewhere.
- Drives an SSD1309 OLED display (optional, `HAS_DISPLAY`) showing Startup,
  Disarmed, Armed Home/Away, Offline, and Triggered (alarm) screens, with live
  gateway-connectivity status and retry countdowns.
- Drives the R503's RGB aura LED and a piezo buzzer to give state-aware visual
  and audible feedback (breathing/flash colors per mode, beeps for scan
  results and enroll/delete/flush/sync/panic events).
- Optional PN532 NFC reader support (`HAS_NFC`) as a second authentication
  method alongside fingerprints.
- Runtime health checks for both the fingerprint sensor and (if present) the
  NFC reader, surfaced on the display and to the gateway.

## Hardware

- MCU: STM32F103, ChibiOS/RT (21.11.x)
- Fingerprint sensor: R503 (UART)
- Bus: RS485 (shared with the rest of the OHS system), gateway at address 0,
  fingerprint nodes at addresses 1..N
- Node address is a compile-time constant (`NODE_ADDRESS`)

## Building

Requires [vysocan/ChibiOS_21.11.x](https://github.com/vysocan/ChibiOS_21.11.x) —
a ChibiOS 21.11.x tree with the OHS board files already added
(`os/hal/boards/OHS_STM32_F103_MINI`), not vanilla ChibiOS-RT. Then:

```bash
make CHIBIOS=/path/to/ChibiOS_21.11.x \
     UDEFS="-DNODE_ADDRESS=1 -DHAS_DISPLAY" -j$(nproc)
```

- `NODE_ADDRESS` (default 1): this node's RS485 bus address.
- `-DHAS_DISPLAY`: SSD1309 OLED on I2C1.
- `-DHAS_NFC`: optional PN532 NFC reader on I2C1.

Flash the resulting `build/ch.bin` to `0x08000000` with an ST-Link
(`st-flash write build/ch.bin 0x08000000`), then reset the target.
