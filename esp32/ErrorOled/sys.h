// What the DOS screens (ui.cpp) need from the board: WiFi, the network, Bluetooth, settings.
// ErrorOled.ino implements it on the ESP32-C3; the PC simulator has a fake one.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "model.h"

enum NetState : uint8_t { NET_OFF, NET_CONNECTING, NET_UP, NET_FAIL, NET_PORTAL };
enum NetJob : uint8_t { JOB_ONLINE = 1, JOB_CHAT, JOB_CHAT_BG, JOB_SAY, JOB_BEACON };

struct WifiNet { char ssid[33]; int8_t rssi; uint8_t open; };

enum BtType : uint8_t { BT_OTHER = 0, BT_KEYBOARD = 1, BT_MOUSE = 2, BT_COMBO = 3 };
struct BtDev { char name[30]; char addr[18]; int8_t rssi; uint8_t type; uint8_t bonded; };

class Sys {
 public:
  virtual ~Sys() {}
  virtual uint32_t ms() = 0;
  virtual bool clock(int& h, int& m, int& s) = 0;     // false until the time is known (NTP)
  virtual Settings& cfg() = 0;
  virtual void saveCfg() = 0;
  virtual void led(bool on) = 0;
  virtual void serialOut(const char* line) = 0;       // copy of DOS output for the USB serial monitor

  // WiFi
  virtual uint8_t netState() = 0;                     // NetState
  virtual const char* netSsid() = 0;
  virtual const char* netIp() = 0;
  virtual int netRssi() = 0;
  virtual void wifiScan() = 0;
  virtual int wifiResults(WifiNet* out, int max) = 0; // -1 while scanning
  virtual void wifiJoin(const char* ssid, const char* pass) = 0;
  virtual void portalStart() = 0;                     // phone setup: own WiFi "ERROR-OLED" + web page
  virtual void portalStop() = 0;

  // error.blue (answers come back through uiNetResult)
  virtual bool netBusy(uint8_t job) = 0;
  virtual void fetchOnline() = 0;
  virtual void fetchChat(const char* code, uint32_t after, bool background) = 0;
  virtual void sendChat(const char* code, const char* text) = 0;
  virtual void beacon() = 0;                          // "this box is online"
  virtual const char* netError() = 0;                 // last problem, "" if none

  // Bluetooth keyboard / mouse
  virtual void btScan() = 0;
  virtual bool btScanning() = 0;
  virtual int btResults(BtDev* out, int max) = 0;
  virtual void btPair(const char* addr) = 0;
  virtual void btForget() = 0;
  virtual int btConnected(BtDev* out, int max) = 0;
  virtual uint32_t btPasskey() = 0;                   // != 0: show it, the keyboard wants it typed
  virtual const char* btStatus() = 0;                 // "", "CONNECTING", "PAIRING", "FAILED"...
  virtual int btBonds() = 0;

  // the chip
  virtual uint32_t freeHeap() = 0;
  virtual uint32_t totalHeap() = 0;
  virtual const char* chipInfo() = 0;
  virtual void reboot() = 0;
};
