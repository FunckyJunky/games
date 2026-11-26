// simear.cpp: EAR-CUBE's microphones, scribe and error.blue faked for the simulator (sim/build-ear.sh).
// LIVE / RECORD start and stop like on the cube; lines "come back" every few seconds from a script, and RECORD
// writes them into a real file under the simulator's TF card (so TEXTS and READ show it).
#include "../src/ui/ui.h"
#include "../../EarCube/src/ear/ear.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#include <stdlib.h>

extern std::string root;
static std::string real(const char* p) { if (!strncmp(p, "/sd", 3)) return root + "/sd" + (p + 3); if (!strncmp(p, "/c", 2)) return root + "/c" + (p + 2); return root + p; }

void earCfgLoad() {}
void earCfgSave() {}
volatile bool enetSwitching = false;
void enetSwitch() { enetSwitching = true; S.upd = UPD_LOADING; S.updPct = 37; snprintf(S.updMsg, sizeof(S.updMsg), "ERROR-CUBE 1.5.4"); }
uint32_t enetSttMs() { return 1840; }

// ---- the microphones: a voice that comes and goes
static int mode = EAR_IDLE, runMode = 0;
bool micBegin() { return true; }
bool micOk() { return !getenv("SIM_NOMIC"); }
void micGain(int) {}
void micRun(int m) { runMode = m; }
void micFlush() { runMode = 0; }
static bool talking() { return mode != EAR_IDLE && (msNow() / 1000) % 5 < 3; }
int micLevel(int ch) { float t = msNow() / 1000.0f; int base = talking() ? 55 : 12; return base + (int)(25 * fabsf(sinf(t * (3.1f + ch)))); }
float micDb() { return talking() ? -31 : -63; }
float micFloorDb() { return -64; }
float micGateDb() { return -55; }
bool micSpeech() { return talking(); }
uint32_t micPos() { return msNow() * 16; }
int micCopy(uint32_t, int16_t*, int) { return -1; }
bool micPiece(Piece&, uint32_t) { return false; }

// ---- the scribe
static const char* SCRIPT[][2] = {
  {"nl", "Goedemorgen allemaal, zullen we beginnen met de planning voor deze week?"},
  {"nl", "Ja, prima. Ik heb het rapport gisteren afgemaakt en naar Jeroen gestuurd."},
  {"en", "Quick question: is the demo still on Thursday at two?"},
  {"nl", "Volgens mij wel, maar ik check het even in de agenda."},
  {"de", "Wir brauchen noch die Zahlen aus Brück, die kommen morgen."},
  {"nl", "Oké, dan zet ik dat als actiepunt erbij. Wie neemt de presentatie?"},
};
static std::vector<EarLine> lines;
static uint32_t seq = 0, cleared = 0, startAt = 0, nextAt = 0;
static int si = 0; static uint32_t words = 0;
static char cur[96] = "", last[96] = "";
static void stamp(char* at) { struct tm t; sysLocalTime(t); snprintf(at, 12, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec); }
static void addLine(const char* text) {
  EarLine l; l.n = ++seq; stamp(l.at); snprintf(l.text, sizeof(l.text), "%s", text);
  lines.push_back(l); if (lines.size() > 64) lines.erase(lines.begin());
  for (const char* p = text; *p; p++) if (*p == ' ') words++; words++;
  if (cur[0]) { FILE* f = fopen(real(cur).c_str(), "ab"); if (f) { fprintf(f, "[%s] %s\n", l.at, text); fclose(f); } }
  uiDirty();
}
static void simTick() {
  static uint32_t lastT = 0;
  if (msNow() == lastT) return;
  lastT = msNow();
  if (mode != EAR_IDLE && msNow() >= nextAt) { addLine(SCRIPT[si % 6][1]); si++; nextAt = msNow() + 3200; }
}
void scribeBegin() {}
void earWant(int m) {
  if (m == mode) return;
  if (m == EAR_IDLE) { mode = EAR_IDLE; cur[0] = 0; uiDirty(); return; }
  mode = m; startAt = msNow(); words = 0; nextAt = msNow() + 2600;
  if (m == EAR_REC) {
    struct tm t; sysLocalTime(t);
    snprintf(cur, sizeof(cur), "/sd/EAR/%04d-%02d-%02d_%02d-%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
    fsMkdir("/sd/EAR");
    FILE* f = fopen(real(cur).c_str(), "wb");
    if (f) { fprintf(f, "EAR-CUBE transcript\nStarted: %04d-%02d-%02d %02d:%02d\nLanguage: auto\n\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min); fclose(f); }
    snprintf(last, sizeof(last), "%s", cur);
  }
  uiDirty();
}
int earMode() { simTick(); return mode; }
const char* earStartError() { return ""; }
uint32_t earSecs() { return mode == EAR_IDLE ? 0 : (msNow() - startAt) / 1000 + (getenv("SIM_LONG") ? 2537 : 0); }
const char* earFile() { return cur; }
const char* earLastFile() { return last; }
int earWaiting() { return getenv("SIM_WAIT") ? atoi(getenv("SIM_WAIT")) : (mode != EAR_IDLE && (msNow() / 1000) % 5 == 3 ? 1 : 0); }
int earDoneCount() { return (int)seq; }
uint32_t earWords() { return words; }
bool earSending() { return mode != EAR_IDLE && (msNow() / 1000) % 5 == 3; }
const char* earErr() { return getenv("SIM_ERR") ? getenv("SIM_ERR") : ""; }
int earStatus() {
  simTick();
  if (!micOk()) return ST_NOMIC;
  if (getenv("SIM_OFFLINE")) return ST_OFFLINE;
  if (mode != EAR_IDLE) return micSpeech() ? ST_HEAR : earSending() ? ST_SEND : ST_LISTEN;
  return ST_READY;
}
const char* earStatusText(int st) {
  static const char* T[8] = {"READY", "LISTENING", "HEARING SPEECH", "WRITING", "NO INTERNET", "DAILY LIMIT", "ERROR", "NO MICROPHONE"};
  return T[st & 7];
}
uint32_t earLineSeq() { simTick(); return seq; }
int earLines(EarLine* out, int max, uint32_t after) {
  std::vector<EarLine> v;
  for (auto& l : lines) if (l.n > after && l.n > cleared) v.push_back(l);
  int k = 0, from = (int)v.size() > max ? (int)v.size() - max : 0;
  for (int i = from; i < (int)v.size(); i++) out[k++] = v[i];
  return k;
}
void earClearLines() { cleared = seq; uiDirty(); }
bool earSaveLive(char* path, size_t n) { snprintf(path, n, "/sd/EAR/LIVE-2026-11-26_23-41.txt"); return seq > cleared; }
const char* earDir() { return "/sd/EAR"; }
int earFiles(EarFile* out, int max) {
  static FileEnt e[100];
  int n = fsList("/sd/EAR", e, 100), k = 0;
  std::vector<EarFile> all;
  for (int i = 0; i < n; i++) {
    size_t l = strlen(e[i].name);
    if (e[i].dir || l < 5 || strcmp(e[i].name + l - 4, ".txt")) continue;
    EarFile f; snprintf(f.path, sizeof(f.path), "/sd/EAR/%s", e[i].name); earNiceName(e[i].name, f.name, sizeof(f.name));
    f.size = e[i].size; f.live = !strncmp(e[i].name, "LIVE", 4);
    std::string w = f.path; w.replace(w.size() - 4, 4, ".wav"); f.hasWav = fsSize(w.c_str()) > 0;
    all.push_back(f);
  }
  std::sort(all.begin(), all.end(), [](const EarFile& a, const EarFile& b) {
    bool ac = !strcmp(a.path, cur), bc = !strcmp(b.path, cur); if (ac != bc) return ac;
    const char* x = strrchr(a.path, '/') + 1; const char* y = strrchr(b.path, '/') + 1;
    if (!strncmp(x, "LIVE-", 5)) x += 5; if (!strncmp(y, "LIVE-", 5)) y += 5;
    return strcmp(x, y) > 0; });
  for (auto& f : all) { if (k >= max) break; out[k++] = f; }
  return k;
}
bool scribeNext(SttJob&) { return false; }
void scribeDone(uint32_t, int, const char*, const char*) {}
bool scribeWants() { return false; }
