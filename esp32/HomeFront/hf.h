// What the network side (core 0) and the screen side (core 1) share. Lock G.lock to touch st/text/frame.
#pragma once
#include <Arduino.h>
#include "freertos/semphr.h"
#include "screen.h"

struct HfState {                   // the web app's state, as error.blue sends it
  uint32_t rev = 0;
  bool app = false;                // a web app is open (COMM)
  bool screen = true;
  uint8_t bright = 80;
  bool scan = false;
  char name[24] = "";
  char tb[20] = "";                // which text picture
  CommSet comm;
};

struct Shared {
  SemaphoreHandle_t lock;
  // device
  char id[16] = "";
  // WiFi (written by the network task)
  volatile bool staUp = false, everStaUp = false, apOn = false, joining = false, joinLow = false;
  volatile int apClients = 0;
  volatile uint32_t connectedAt = 0, apSince = 0;
  char wifi[34] = "", ip[20] = "", ap[24] = "", joinSsid[34] = "", setupMsg[48] = "";
  // error.blue
  volatile bool wsUp = false;
  volatile uint32_t textAt = 0;    // when a new text picture arrived (LED flash)
  // the web app (lock!)
  bool haveState = false;
  HfState st;
  TextPic* text = nullptr;
  uint8_t* frame = nullptr;        // 4-bit packed, up to 320x172
  int fw = 0, fh = 0;
  uint32_t frameAt = 0;
  // requests from the screen side / the setup page to the network task
  volatile bool wantSetup = false, wantRelink = false;
  volatile bool flipChanged = false;
  volatile int renderFps = 0;
};
extern Shared G;

// settings (Preferences "hf")
struct Net { char s[33]; char p[65]; bool low; };
#define MAX_NETS 8
int netsLoad(Net* out);
void netsSave(const Net* n, int count);
void netsAdd(const char* ssid, const char* pass);
void netsForget(const char* ssid);
String prefGet(const char* key, const char* def = "");
void prefSet(const char* key, const String& v);
bool prefFlip();

// network.cpp
void netBegin();
void webLoop();                    // call often from loop(): setup page + captive DNS
// link.cpp
void linkLoadSaved();              // the last web app from flash
void linkStep();                   // keep the WebSocket to error.blue going (network task)
void linkDrop();
