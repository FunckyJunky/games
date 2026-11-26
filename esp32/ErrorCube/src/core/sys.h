// sys.h: what the interface (ui.cpp, apps*.cpp) needs from the board and the network.
// On the board: board.cpp, net.cpp, sound.cpp. In the PC simulator: sim/simsys.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "config.h"

uint32_t msNow();

// what a button (or two together) can do: SETUP > BUTTONS. New ones only at the end (saved settings keep their number)
enum { BA_NONE, BA_PREV, BA_NEXT, BA_OK, BA_BACK, BA_HOME, BA_QUICK, BA_SCREEN, BA_POWER, BA_VOLUP, BA_VOLDN,
       BA_LIGHTUP, BA_LIGHTDN, BA_HOLO, BA_HCENTRE, BA_OFFLINE, BA_SHOT, BA_TORCH, BA_GLITCH, BA_CLOCK, BA_FILES,
       BA_SETUP, BA_PLAY, BA_NEXTTRK, BA_PREVTRK, BA_MUSIC, BA_COUNT };

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
  // HOLO 3D (1.2): the tilt makes the interface deep, like a hologram (holo.cpp)
  uint8_t holo = 1;         // on / off
  uint8_t holoLayers = 1;   // depth layers: parts of the interface move by their depth
  uint8_t holoDepth = 7;    // px a layer moves at full tilt (0..20)
  uint8_t holoFocus = 2;    // which layer stays still: 0 BACK, 1 PANELS, 2 TEXT, 3 BARS, 4 FRONT
  uint8_t holoPop = 50;     // how far dialogs and messages float in front (0..100)
  uint8_t holoShadow = 1;   // 0 off, 1 soft, 2 hard
  uint8_t holoWalls = 3;    // the sides of raised parts: 0 off .. 5 thick
  uint8_t holoBack = 4;     // the far wall: 0 off, 1 dots, 2 grid, 3 stars, 4 room
  uint8_t holoLens = 26;    // px the glass's centre moves at full tilt (0..60)
  uint8_t holoPersp = 35;   // the picture leans with the tilt (0..100)
  uint8_t holoGlare = 1;    // light reflection on the glass: 0 off, 1 soft, 2 bright
  uint8_t holoRainbow = 2;  // the colour fringe moves with the tilt: px (0..4)
  uint8_t holoRange = 25;   // degrees of tilt for the full effect (10..60)
  uint8_t holoDead = 1;     // degrees that do nothing (0..10)
  uint8_t holoSmooth = 45;  // smoothing (0..100)
  uint8_t holoCentre = 1;   // the "straight" way to hold it follows you: 0 off, 1 slow, 2 medium, 3 fast
  uint8_t holoInvX = 0, holoInvY = 0;
  uint8_t holoSensor = 0;   // 0 gyro + gravity (sees every turn), 1 gravity only (no turning left/right when upright)
  // BUTTONS (1.2): [-, +, PWR] x [press, double press, hold]
  uint8_t btnAct[3][3] = {{BA_PREV, BA_NONE, BA_BACK}, {BA_NEXT, BA_NONE, BA_OK}, {BA_SCREEN, BA_NONE, BA_POWER}};
  uint8_t btnCombo[4] = {BA_QUICK, BA_HCENTRE, BA_NONE, BA_SHOT};   // - & + press, - & + hold, hold - + PWR, hold + + PWR
  uint8_t btnHold = 12;     // x50 ms: when - / + count as held (300..2000 ms)
  uint8_t btnDouble = 0;    // x10 ms: the gap for a double press (0 = no double press: a press acts at once)
  uint8_t btnRepeat = 0;    // x10 ms: a held - / + repeats its press (when its hold does nothing); 0 = off
  uint8_t pwrHold = 0;      // PWR counts as held after 0 1 s, 1 1.5 s, 2 2 s, 3 2.5 s
  uint8_t pwrOff = 1;       // PWR held this long switches the cube off by force: 0 4 s, 1 6 s, 2 8 s, 3 10 s
  uint8_t btnSwap = 0;      // - and + swapped
  uint8_t btnWake = 0;      // 0 any button wakes the screen, 1 only PWR
  // OFFLINE (1.2): like airplane mode: WiFi off, no internet, programs that work without it
  uint8_t offline = 0;
  // the clock when the screen is off (1.2): low power
  uint8_t aodStyle = 0;     // 0 BIG, 1 SMALL, 2 ANALOG, 3 DOS
  uint8_t aodLight = 6;     // % (1..30)
  uint8_t aodDate = 1, aodBatt = 1, aodNote = 1;
  uint8_t aodWifi = 1;      // while the screen is off: 0 WiFi as normal, 1 save power, 2 WiFi off
  // WALLPAPER (1.2): holoBack is the style (0 glass .. 4 room, 5 a picture)
  char wallPic[72] = "";    // the picture (JPEG / BMP on the SD card or the cube's drive)
  uint8_t wallLight = 40;   // how bright the picture is behind the text, %
  // UPDATES (1.2): a message when a new firmware is ready (UPDATE NOW / LATER)
  uint8_t updAsk = 1;
  // PICO-8 (1.4): tilt = the arrows, - = O, + = X
  uint8_t p8Dead = 12;      // degrees of tilt before an arrow counts (4..30)
  uint8_t p8InvX = 0, p8InvY = 0;
  uint8_t p8Swap = 0;       // - is X, + is O
  uint8_t p8CalOk = 0;      // calibrated: p8Cal is how you hold it to play (else: how you hold it when a cart starts)
  int16_t p8Cal[3] = {0, 0, 1000};   // gravity, x1000 (screen directions)
  // LOW GRAPHICS (1.4.1): the plainest interface: no 3D, glass, glow, scanlines, glitches, animations, wallpaper
  uint8_t lowGfx = 0;
  // the ALARM (1.4.1): a song instead of beeps, at full volume; moving the cube snoozes it
  char alarmSong[96] = "";  // MP3 / WAV ("" = beeps)
  uint8_t alarmLoud = 1;    // full volume while it rings (whatever VOLUME is)
  uint8_t alarmMove = 1;    // moving / picking up the cube = snooze
  uint8_t alarmSnooze = 5;  // minutes
  // MUSIC (1.5): which picture of the sound (tap it to change)
  uint8_t musVis = 0;
  // new settings go here, at the end
};
extern Cfg cfg;
static inline bool lowFx() { return cfg.saver || cfg.lowGfx; }   // no effects: BATTERY SAVER or LOW GRAPHICS
void cfgSave();             // write to flash (the board waits a moment, so many quick changes are one write)
void cfgFlushNow();         // write it now (before something that could go wrong)
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
  char linkErr[64] = "";                         // why the live link to error.blue/cube failed last
  char crash[240] = ""; uint8_t crashAt = 0;     // the last start was a crash: what (and the core dump's PC + backtrace)
  uint8_t cfgRestored = 0;
  volatile int8_t dlState = 0; volatile int dlPct = 0;   // netDownload                       // the settings came back from C:\cube.cfg / A:\ERRORCUBE\cube.cfg
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
  uint32_t fileAt = 0; char fileName[48] = "";  // a file came in from the control page
  uint32_t openAt = 0; char openPath[200] = "";  // the control page asks to open a file (in its player)
  // an MP4 the phone converts for the cube (its own page http://IP/cv): which file, and how far the phone is
  char convPath[200] = ""; uint32_t convAt = 0; int convPct = 0; char convMsg[40] = "";
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
void sysApplyButtons();                // PWR hold / force-off times to the power chip
void sysLowPower(bool on);             // the screen is off: slower CPU, WiFi saving power (cfg.aodWifi)
enum { IMG_STRETCH, IMG_FIT, IMG_COVER };   // fill the box / the whole picture with bars / fill it, cut the sides
bool jpegDecode(const uint8_t* d, size_t n, uint16_t* out, int ow, int oh, int mode, int* iw = nullptr, int* ih = nullptr);

// buttons (buttons.cpp): the board reports - / + up and down, and the power chip's PWR presses
void btnRaw(int id, bool down);        // id 0 = -, 1 = +
void btnPwr(bool held);
extern bool (*btnHook)(int id, bool down);   // a game takes - (0), + (1), PWR (2) raw; true = taken
void btnPoll();                        // every loop: double press and hold timers
void btnDo(int action);
const char* btnName(int action);
extern char btnLast[48];               // what the buttons did last (BUTTONS > TEST)

// sound (all made up on the spot, like on error.blue)
enum { SND_KEY, SND_MOVE, SND_BEEP, SND_THUD, SND_CRACKLE, SND_FLOPPY, SND_OK, SND_ERR, SND_MSG, SND_ALARM, SND_ON, SND_OFF };
void snd(int kind);
void tone(int hz, int ms, int vol = 100);
void sndForceVolume(int v);                            // the alarm: this volume (0..100) whatever VOLUME is; -1 = back to normal
void sndStop();
// media sound (music, video): stereo 16-bit at any rate, mixed with the beeps at 44.1 kHz
int sndMediaRoom();                                   // frames (44.1 kHz) that still fit
void sndMediaPut(const int16_t* lr, int frames, int rate);
void sndMediaClear();
uint32_t sndMediaPlayed();                            // frames played since the last clear (44.1 kHz): the video's clock
int sndMediaQueued();
void sndMediaPause(bool p);
void sndMediaWave(int16_t* out, int n);               // the last n samples played (mono), to draw the sound

// music and video (media.cpp): MP3 / WAV, MJPEG AVI with sound; they play in the background
enum { MED_STOP, MED_PLAY, MED_PAUSE, MED_END, MED_ERR };
bool musPlay(const char* path);
void musPause(bool p);
void musStop();
void musSeek(uint32_t ms);
int musState();
uint32_t musPosMs();
uint32_t musLenMs();
const char* musFile();
const char* musTitle();
const char* musArtist();
int musKbps();
bool vidPlay(const char* path, char* err);           // err: why not
void vidPause(bool p);
void vidStop();
void vidSeek(uint32_t ms);
int vidState();
uint32_t vidPosMs();
uint32_t vidLenMs();
int vidInfo(int* w, int* h, int* fps);               // the frames' size, frames a second; returns the audio rate (0 = silent)
extern uint16_t* vidFrame;                            // the newest picture (480 x 480), vidFrameSeq counts them
extern volatile uint32_t vidFrameSeq;
void mediaStart();                                    // starts the media task (the first time music or video plays)
void mediaPump();                                     // the media task: read, decode (board: its own task; sim: the loop)
bool mediaPumpWork();                                 // same, true = it did something (call again soon)

// files: "/sd/..." = the TF card, "/c/..." = the cube's own flash drive
struct FileEnt { char name[64]; bool dir; uint32_t size; };
int fsList(const char* dir, FileEnt* out, int max);           // -1 = no such drive / folder
bool fsRead(const char* path, uint8_t* buf, size_t max, size_t& n);
bool fsWrite(const char* path, const uint8_t* buf, size_t n);
bool fsRemove(const char* path);
bool fsInfo(const char* drive, uint64_t& total, uint64_t& used);
bool fsAppend(const char* path, const uint8_t* buf, size_t n, size_t at);   // at = 0: a new file; else must be its length
bool fsMkdir(const char* path);
bool fsRename(const char* from, const char* to);
bool fsRmdir(const char* path);                               // empty folders only
bool fsCopy(const char* from, const char* to, int* pct = nullptr);
long fsSize(const char* path);                                // -1 = none
void* fsOpen(const char* path, const char* mode);     // streaming (FILE* on the board)
size_t fsReadF(void* f, void* buf, size_t n);
bool fsSeekF(void* f, long pos);                      // -1 = the end
long fsTellF(void* f);
void fsClose(void* f);
bool sdMount();                       // the SD card in now (a card put in after the start); updates S.sd, S.sdInfo
void sdEject();                       // safe to take out

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
void netOffline(bool on);                     // OFFLINE: WiFi off and no internet jobs (cfg.offline)
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
void netDownload(const char* path, const char* file);   // error.blue path -> a file (S.dlState: 1 loading, 2 done, -1 failed)
bool netFetchDone(int& id, int& status, char*& body);   // body malloc'd (free it)
void netReportProg(const char* name);         // tell the control page what's open
