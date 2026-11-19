// ERROR-OLED's screens: a tiny ERROR-DOS for a 72x40 (or 128x64) OLED.
//   WHO.EXE    who is online on error.blue, in which game (the info screen you see after start)
//   CHAT.EXE   read and write ERROR-CHAT
//   C:\>       DOS prompt: DIR, HELP, SAY, NAME, CODE, WIFI ...
//   WIFI.EXE, BT.EXE (Bluetooth keyboard + mouse), SETUP.EXE, LOG.TXT
// Works with a Bluetooth keyboard and mouse, the BOOT button, or the USB serial monitor.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "keys.h"
#include "sys.h"

typedef struct u8g2_struct u8g2_t;

void uiBegin(u8g2_t* g, Sys* sys);
void uiTick();                                    // call every loop(): timers, network, drawing
void uiKey(const KeyEv& k);
void uiMouse(int dx, int dy, int wheel, uint8_t buttons);
void uiButton(BtnEv e);
void uiSerialLine(const char* line);              // a line typed in the serial monitor
void uiNetResult(uint8_t job, int status, const char* body, size_t len);
void uiToast(const char* text);                   // a one-line pop-up (e.g. "KEYBOARD ON")
void uiSetLang(bool ukrainian);                   // keyboard types Cyrillic: show "UA"
void uiApplyDisplay();                            // contrast + flip from the settings
