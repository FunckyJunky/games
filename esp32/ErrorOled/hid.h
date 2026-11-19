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
