// sys.h: what the interface (ui.cpp, apps*.cpp) needs from the board and the network.
// On the board: board.cpp, net.cpp, sound.cpp. In the PC simulator: sim/simsys.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "config.h"

uint32_t msNow();

// ------------------------------------------------------------------ settings (saved in flash)
struct Cfg {
  // first, never moved: a newer firmware keeps the settings an older one saved (new fields only go at the end)
  uint32_t magic = 0xC0BE0002;
  // look
  uint8_t color = 0;        // SCHEMES[]
  uint8_t glow = 1;         // 0 off, 1 low, 2 high
  uint8_t scan = 1;         // scanlines
  uint8_t glitch = 1;       // 0 off, 1 low, 2 medium, 3 high
  uint8_t boot = 0;         // 0 full, 1 short, 2 off
  uint8_t crt = 1;          // power on/off animation
  // screen
  uint8_t bright = 70;      // 5..100 %
  uint8_t offIdx = 3;       // SCREEN OFF AFTER: OFF_SECS[]
  uint8_t dim = 1;          // dim 10 s before going off
  uint8_t aod = 0;          // when "off": a dim clock instead of black
  uint8_t tapWake = 1;      // a tap wakes the screen
  uint8_t moveWake = 0;     // picking it up wakes the screen
  uint8_t rot = 0;          // 0..3 = 0/90/180/270, 4 = turn with the cube
  // sound
  uint8_t vol = 60;         // 0..100 %
  uint8_t clicks = 1;       // key clicks
  uint8_t sounds = 1;       // beeps, crackles, floppy
  uint8_t chatBeep = 1;     // beep for new chat messages
  // time
  uint8_t ntp = 1;          // set the clock from the internet
  uint8_t tzIdx = 0;        // TIMEZONES[]
  uint8_t h24 = 1;
  uint8_t secs = 1;         // seconds on the clock
  // power
  uint8_t chgIdx = 2;       // CHARGE_MA[]
  uint8_t autoOffIdx = 0;   // AUTO_OFF_MIN[]: power off after this long asleep on battery
  uint8_t saver = 0;        // battery saver: slower CPU, no effects
  // error.blue
  char name[17] = "";
  char chan[9] = "LOBBY";
  uint8_t online = 1;       // show the cube on error.blue's who's-online list
  uint8_t link = 1;         // live link for error.blue/cube
  char code[7] = "";        // link code
  uint8_t tls = 1;          // 1 check certificates, 0 skip check
  // updates
  uint8_t autoUpd = 1;
  uint8_t updIdx = 1;       // UPD_HOURS[]
  // other
  uint8_t kbd = 0;          // on-screen keyboard: 0 EN, 1 UA
  uint8_t alarmOn = 0, alarmH = 7, alarmM = 30, alarmDays = 0x7F;   // bit 0 = Monday
  uint8_t wifiOn = 1;
  int8_t axes[3] = {1, 2, 3};   // motion sensor -> screen: which sensor axis (1..3, minus = turned round) is x, y, z
  // 3D and effects
  uint8_t view = 1;         // the menu: 0 DOS list, 1 3D carousel
  uint8_t lens = 1;         // 0 flat glass, 1 curved CRT, 2 fisheye
  uint8_t fringe = 1;       // colour fringe toward the edges
  uint8_t anim = 2;         // 0 off, 1 simple, 2 full (the cube turns between screens)
  uint8_t parallax = 1;     // the 3D menu moves with how you hold the cube
  uint8_t intro = 1;        // the 3D ERROR intro at start (BOOT FULL)
  // new settings go here, at the end
};
extern Cfg cfg;
void cfgSave();             // write to flash (the board waits a moment, so many quick changes are one write)
void cfgReset();            // factory reset: every setting back to the default, WiFi forgotten

extern const uint16_t OFF_SECS[]; extern const int N_OFF;
extern const uint16_t CHARGE_MA[]; extern const int N_CHARGE;
extern const uint16_t AUTO_OFF_MIN[]; extern const int N_AUTO_OFF;
extern const uint8_t UPD_HOURS[]; extern const int N_UPD;
struct Tz { const char* name; const char* posix; };
extern const Tz TIMEZONES[]; extern const int N_TZ;

// ------------------------------------------------------------------ what the board and the network report
enum { NET_OFF, NET_JOINING, NET_UP, NET_FAIL, NET_NOSAVED };
enum { UPD_IDLE, UPD_CHECKING, UPD_NEW, UPD_LOADING, UPD_DONE, UPD_FAIL, UPD_LATEST };
struct SysState {
  // hardware found at start-up
  bool touch, pmu, rtc, imu, codec, sd;
  char sdInfo[24];
  // power
  int batPct = -1, batMv = 0, vbusMv = 0;
  bool charging = false, usb = false, battery = false;
  float pmuTemp = 0, chipTemp = 0;
  // network
  uint8_t net = NET_OFF;
  char ssid[33] = "", ip[16] = "", mac[18] = "", host[24] = "";
  int rssi = -100;
  bool portal = false;      // phone setup WiFi is on
  char portalName[24] = "";
  bool link = false;        // live link to error.blue
  int panels = 0;           // control pages watching
  bool timeOk = false;
  char netErr[48] = "";
  // updates
  uint8_t upd = UPD_IDLE;
  char updVer[16] = "", updNotes[120] = "", updMsg[48] = "";
  int updPct = 0;
  uint32_t updCheckedAt = 0;
  // chat
  int chatUnread = 0;
  char lastFrom[17] = "", lastText[80] = "";
  uint32_t msgAt = 0;       // a message from the control page arrived
  char msgText[120] = "";
  // media and web apps from the control page
  uint32_t vidAt = 0, vidSeq = 0, audAt = 0;   // last video frame / sound slice
  int audLevel = 0;                            // 0..100, how loud the sound coming in is
  char wantApp[24] = "";                       // the control page asked to open this web app
  uint32_t wantAppAt = 0;
  uint32_t wantHomeAt = 0;
  uint32_t cfgAt = 0;                          // settings changed from the control page: apply them
  uint32_t wakeAt = 0, sleepAt = 0;            // the control page wants the screen on / off
};
extern SysState S;

// ------------------------------------------------------------------ board
void sysBrightness(int pct);           // 0 = panel off
void sysPowerOff();
void sysRestart();
void sysSetTime(int y, int mo, int d, int h, int mi, int s);
bool sysLocalTime(struct tm& t);       // false = the clock was never set
bool imuRead(float a[3], float g[3]);  // g in m/s^2 (a) and deg/s (g), in screen directions: x right, y down, z out
uint32_t freeHeap();
uint32_t freePsram();
uint32_t uptimeSecs();
const char* resetReason();
void sysChargeCurrent(int ma);
void sysApplyRotation();

// sound (all made up on the spot, like on error.blue)
enum { SND_KEY, SND_MOVE, SND_BEEP, SND_THUD, SND_CRACKLE, SND_FLOPPY, SND_OK, SND_ERR, SND_MSG, SND_ALARM, SND_ON, SND_OFF };
void snd(int kind);
void tone(int hz, int ms, int vol = 100);
void sndStop();

// files: "/sd/..." = the TF card, "/c/..." = the cube's own flash drive
struct FileEnt { char name[64]; bool dir; uint32_t size; };
int fsList(const char* dir, FileEnt* out, int max);           // -1 = no such drive / folder
bool fsRead(const char* path, uint8_t* buf, size_t max, size_t& n);
bool fsWrite(const char* path, const uint8_t* buf, size_t n);
bool fsRemove(const char* path);
bool fsInfo(const char* drive, uint64_t& total, uint64_t& used);

// ------------------------------------------------------------------ network
struct WifiNet { char ssid[33]; int rssi; bool open; bool saved; };
void netWifi(bool on);
void netScan();
int netScanResults(WifiNet* out, int max);    // -1 = still scanning
void netJoin(const char* ssid, const char* pass);
int netSaved(char (*ssids)[33], int max);
void netForget(const char* ssid);
void netPortal(bool on);                      // phone setup WiFi
void netLinkNewCode();
void netLinkState();                          // tell the control page about a change now
void netUpdateCheck();
void netUpdateInstall();
bool netInstallFile(const char* path);        // firmware from a file (TF card)
void netSetFocus(int what);                   // what's on screen: polls that more often
enum { FOCUS_NONE, FOCUS_WHO, FOCUS_CHAT };

struct WhoGame { char title[28]; char dos[9]; int n; char players[200]; };
struct WhoList { uint32_t at; int total; int ng; WhoGame g[14]; bool ok; };
bool netWho(WhoList& out);                    // the newest list (false = none yet)
struct ChatMsg { uint32_t id; char name[17]; char text[204]; bool me, sys; };
struct ChatLog { ChatMsg m[40]; int n; char who[160]; bool ok; uint32_t at; };
bool netChat(ChatLog& out);
void netChatSay(const char* text);
void netChatReset();                          // the channel changed

// ------------------------------------------------------------------ video from the control page (net decodes it)
extern uint16_t* vidBuf;                      // the newest frame, 480 x 480 RGB565 (PSRAM), nullptr = none yet

// ------------------------------------------------------------------ web apps: error.blue/cube/apps/apps.json
struct AppInfo { char id[24], name[24], info[48], file[64], v[8]; uint32_t color; };
int netApps(AppInfo* out, int max);           // the list; -1 = still loading, -2 = can't get it (no WiFi)
void netAppsRefresh();
int netAppGet(const char* id);                // 2 = ready on the cube, 1 = downloading, 0 = started, -1 = failed
char* netAppSource(const char* id, size_t* n);   // the downloaded JavaScript (malloc'd: free it)
void netAppSend(const char* id, const char* json);   // an app talks to the control page
bool netAppMsg(char* out, size_t n);          // a message for the running app from the control page
void netFetch(int id, const char* path);      // an app asks error.blue for something (GET)
bool netFetchDone(int& id, int& status, char*& body);   // body malloc'd (free it)
void netReportProg(const char* name);         // tell the control page what's open
