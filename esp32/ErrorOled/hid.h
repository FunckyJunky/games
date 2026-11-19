// Bluetooth keyboard + mouse for ERROR-OLED (Bluetooth Low Energy "HID" devices).
// The ESP32-C3 only has Bluetooth LE: keyboards/mice that say "Bluetooth 5.0", "BLE" or
// "Bluetooth Low Energy" work; old "Bluetooth 3.0" (Classic) ones and 2.4 GHz USB dongles don't.
#pragma once
#include <stdint.h>
#include "sys.h"

void hidBegin(const char* deviceName);
void hidLoop(uint32_t nowMs, uint8_t layout);   // in loop(): keys/mouse to the screens, reconnects
void hidScan();
bool hidScanning();
int hidResults(BtDev* out, int max);
void hidPair(const char* addr);
void hidForget();
int hidConnected(BtDev* out, int max);
uint32_t hidPasskey();
const char* hidStatus();
int hidBonds();

// scan settings (set on the setup web page, kept in flash)
struct BtOpts {
  uint8_t secs;           // how long a scan runs: 5..60 s
  uint8_t active;         // 1 = ask devices for their name too (active scan), 0 = only listen
  uint8_t show;           // 0 = keyboards/mice only, 1 = + any device with a name, 2 = everything
  int8_t minRssi;         // hide devices weaker than this (dBm), -100 = show all
  uint8_t autoReconnect;  // look for paired keyboards/mice and connect by themselves
};
const BtOpts& hidOpts();
void hidSetOpts(const BtOpts& o);
bool hidStarted();
void hidPairAddr(const char* addr, uint8_t atype);   // pair with an address typed by hand (atype 0 public, 1 random)
uint32_t hidScanStartedMs();
