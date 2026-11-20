// WiFi, PHONE SETUP (own WiFi + web page) and talking to error.blue over HTTPS.
// The HTTPS requests run in their own task, so the screens never freeze while waiting.
#pragma once
#include <Arduino.h>
#include "sys.h"

void netBegin(Settings* cfg, void (*save)());
void netLoop();                              // in loop(): WiFi state, phone setup, answers to the screens
uint8_t netStateNow();
const char* netSsidNow();
const char* netIpNow();
int netRssiNow();
const char* netErr();
void netScan();
int netScanResults(WifiNet* out, int max);
void netJoin(const char* ssid, const char* pass);
void netPortal(bool on);
uint32_t netReserved();                 // bytes kept aside for the next HTTPS handshake
bool netJobBusy(uint8_t kind);
void netRequest(uint8_t kind, const char* method, const String& path, const String& body);
String jsonEscape(const char* s);
String urlEncode(const char* s);
