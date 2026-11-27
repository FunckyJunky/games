// simdream.cpp: DREAM-CUBE's listening, error.blue and the control page faked for the simulator (sim/build-dream.sh).
// Every 9 s (SIM_EVERY ms) someone "says" something; 2.5 s later the cube "dreams" and the scene comes in.
// SIM_SCENE=k: scene k right away (screenshots). SIM_NONET=1: no WiFi (the WiFi picker first; picking one connects
// 2.5 s later, SIM_BADPASS=1: it fails). SIM_PAUSE=1: paused.
#include "../src/ui/ui.h"
#include "../../DreamCube/src/dream/dream.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>

DreamCfg dcfg;
const Lang LANGS[] = {{"", "AUTO"}, {"nl", "NEDERLANDS"}, {"en", "ENGLISH"}, {"de", "DEUTSCH"}};
const int N_LANGS = 4;
const char* gainLabel(int s) { static char b[12]; snprintf(b, sizeof(b), "%d dB", s * 3); return b; }
void dreamApName(char* o, size_t n) { snprintf(o, n, "%s", dcfg.apName[0] ? dcfg.apName : "DREAM-CUBE-4F2A"); }
void dreamNewCode() { strcpy(dcfg.code, "K7M2QX9D"); }
void dreamCfgLoad() { strcpy(dcfg.apPass, "48151623"); strcpy(dcfg.code, "Q7DRMX4K"); if (getenv("SIM_PAUSE")) dcfg.listen = 0; if (getenv("SIM_FPS")) dcfg.fps = 1; }
void dreamCfgSave() {}
volatile bool enetSwitching = false;
void enetSwitch() { enetSwitching = true; S.upd = UPD_LOADING; S.updPct = 37; snprintf(S.updMsg, sizeof(S.updMsg), "ERROR-CUBE 1.5.5"); }
uint32_t enetLinkAt() { return msNow() > 3000 ? msNow() - 1000 : 0; }
uint32_t enetDreamMs() { return 2140; }

// ---- the microphones
bool micBegin() { return true; }
bool micOk() { return !getenv("SIM_NOMIC"); }
void micGain(int) {}
void micRun(int) {}
void micFlush() {}
static bool talking() { uint32_t e = getenv("SIM_EVERY") ? atoi(getenv("SIM_EVERY")) : 9000; return dcfg.listen && msNow() % e < 2400; }
int micLevel(int ch) { return talking() ? 60 + (msNow() / 70 + ch * 13) % 30 : 10; }
float micDb() { return talking() ? -30 : -62; }
float micFloorDb() { return -63; }
float micGateDb() { return -54; }
bool micSpeech() { return talking(); }
uint32_t micPos() { return msNow() * 16; }
int micCopy(uint32_t, int16_t*, int) { return -1; }
bool micPiece(Piece&, uint32_t) { return false; }

// ---- what's said, what comes back
static const char* SAID[] = {
  "We took the dog to the beach and it started to rain",
  "Happy birthday! Let's eat the cake and open the presents",
  "Gisteren zagen we een draak bij de vulkaan, echt waar",
  "The rocket goes to Mars tonight with two aliens on board",
  "Ik heb zo'n zin in pizza en een koud drankje",
  "Do you think the money for the new house will be enough?",
};
static const char* SCENES[] = {
  R"({"title":"Dog in the rain","place":"beach","time":"day","weather":"rain","mood":"sad","fx":"none","cam":"orbit","things":[{"m":"dog","a":"run","c":"#b07840"},{"m":"umbrella","at":"left","c":"#e53950"},{"m":"person","a":"walk","at":"right"}]})",
  R"({"title":"Happy birthday","place":"room","time":"day","weather":"clear","mood":"happy","fx":"confetti","cam":"orbit","things":[{"m":"cake","at":"center"},{"m":"gift","n":3,"at":"left","c":"#3b8bff"},{"m":"child","n":2,"a":"dance","at":"right"},{"m":"balloon","n":3,"at":"back","c":"#ff3b8b"}]})",
  R"({"title":"Draak bij de vulkaan","place":"lava","time":"sunset","weather":"clear","mood":"exciting","fx":"fire","cam":"orbit","things":[{"m":"dragon","a":"fly"},{"m":"volcano"},{"m":"person","n":2,"a":"shake","at":"front"}]})",
  R"({"title":"Off to Mars","place":"space","time":"night","weather":"clear","mood":"exciting","fx":"stars","cam":"orbit","things":[{"m":"rocket","a":"fly"},{"m":"planet","c":"#d9542b"},{"m":"alien","n":2,"a":"wave","at":"right"}]})",
  R"({"title":"Zin in pizza","place":"room","time":"night","weather":"clear","mood":"happy","fx":"sparkles","cam":"close","things":[{"m":"pizza","s":1.5},{"m":"drink","at":"right","c":"#e8a020"},{"m":"table","at":"back"}]})",
  R"({"title":"Money for a house","place":"void","time":"night","weather":"clear","mood":"mystery","fx":"money","cam":"orbit","things":[{"m":"house","c":"#eadbc0"},{"m":"money","at":"left"},{"m":"question","at":"right"}]})",
};
static const int NS = 6;
struct H { std::string json, title, when; };
static std::vector<H> hist;
static int pos = 0, want = -1, made = 0, slot = -1;
static std::string incoming; static bool incFresh = false;
static char heard[200] = ""; static uint32_t heardAt = 0; static bool thinking = false;
static std::string said;
void dreamBegin() {}
static void simTick() {
  uint32_t e = getenv("SIM_EVERY") ? atoi(getenv("SIM_EVERY")) : 9000;
  static bool first = true;
  extern uint32_t simJoinAt;
  if (simJoinAt && msNow() - simJoinAt > 2500) { simJoinAt = 0; if (getenv("SIM_BADPASS")) S.net = NET_FAIL; else { S.net = NET_UP; strcpy(S.ip, "192.168.1.42"); } }
  if (first) { first = false; dreamCfgLoad(); if (getenv("SIM_NONET")) { S.net = NET_FAIL; strcpy(S.ssid, ""); }
    if (getenv("SIM_SCENE")) { int k = atoi(getenv("SIM_SCENE")) % NS; incoming = SCENES[k]; incFresh = true; snprintf(heard, sizeof(heard), "%s", SAID[k]); heardAt = msNow(); } }
  if (!dcfg.listen || S.net != NET_UP) return;
  int s = msNow() / e; uint32_t in = msNow() % e;
  if (s != slot && in > 2400) { slot = s; snprintf(heard, sizeof(heard), "%s", SAID[s % NS]); heardAt = msNow(); thinking = true; }
  if (thinking && in > 4600 && slot == s) { thinking = false; incoming = SCENES[s % NS]; incFresh = true; }
}
void dreamLoop() { simTick(); }
void dreamListen(bool on) { dcfg.listen = on; uiDirty(); }
bool dreamListening() { return dcfg.listen; }
int dreamStatus() {
  if (!micOk()) return ST_NOMIC;
  if (S.net != NET_UP) return ST_OFFLINE;
  if (!dcfg.listen) return ST_PAUSED;
  if (thinking) return ST_THINK;
  return talking() ? ST_HEAR : ST_LISTEN;
}
const char* dreamStatusText(int st) {
  static const char* T[] = {"READY", "LISTENING", "HEARING", "SENDING", "DREAMING...", "NO INTERNET", "DAILY LIMIT", "ERROR", "NO MICROPHONES", "PAUSED"};
  return T[st];
}
const char* dreamErr() { return getenv("SIM_ERR") ? getenv("SIM_ERR") : ""; }
void dreamSay(const char* t) { snprintf(heard, sizeof(heard), "%s", t); heardAt = msNow(); incoming = SCENES[(made + 3) % NS]; incFresh = true; }
const char* dreamHeard() { return heard; }
uint32_t dreamHeardAt() { return heardAt; }
int dreamWordsWaiting() { return 0; }
bool dreamThinking() { return thinking; }
uint32_t dreamScenes() { return made; }
int dreamCount() { return hist.size(); }
int dreamPos() { return pos; }
void dreamShowAt(int p) { if (p >= 0 && p < (int)hist.size()) want = p; }
bool dreamTitleAt(int p, char* out, size_t n, char* when, size_t wn) {
  if (p < 0 || p >= (int)hist.size()) return false;
  snprintf(out, n, "%s", hist[p].title.c_str()); if (when) snprintf(when, wn, "%s", hist[p].when.c_str());
  return true;
}
bool dreamPoll(SceneDef& d, bool& fresh) {
  if (!incoming.empty()) {
    std::string j = incoming; incoming.clear();
    if (!sceneParse(j.c_str(), d)) return false;
    struct tm t; sysLocalTime(t); char w[8]; snprintf(w, sizeof(w), "%02d:%02d", t.tm_hour, t.tm_min);
    hist.insert(hist.begin(), {j, d.title, w}); if (hist.size() > 40) hist.pop_back();
    pos = 0; want = -1; fresh = incFresh; if (fresh) made++;
    return true;
  }
  if (want >= 0 && want != pos) { pos = want; want = -1; fresh = false; return sceneParse(hist[pos].json.c_str(), d); }
  want = -1;
  return false;
}
void dreamCmd(int, const char*, const char*) {}
void dreamState(char* o, size_t n) { snprintf(o, n, "{}"); }
