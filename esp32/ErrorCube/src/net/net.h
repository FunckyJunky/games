// net.h: WiFi (saved networks, phone setup), the cube's own web page, internet time; and cloud.cpp: error.blue.
// Everything runs in one task on core 0 (netBegin), plus an HTTPS worker for requests to error.blue.
#pragma once
#include <stdint.h>
#include <stddef.h>

void netBegin();
void netStep();                 // WiFi state, web page, phone setup (net task)
bool netUp();
void netSleepMode(int m);      // the screen is off: 0 WiFi as normal, 1 power saving, 2 off (cfg.aodWifi)

// cloud.cpp
void cloudBegin();
void cloudStep();               // the live link, timers for who/chat/updates (net task)
void cloudMarkGood();           // the new firmware runs fine: keep it
