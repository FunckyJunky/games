// scribe.cpp: LIVE and RECORD, see ear.h.
//  - a piece of speech from mic.cpp -> a WAV (16 kHz mono, made louder when quiet) -> the queue for error.blue
//  - LIVE: pieces wait in memory; without internet they're dropped (it's live). The text shows on the screen
//    and the web page; SAVE LIVE TEXT also writes it to LIVE-date.txt.
//  - RECORD: every piece goes to the TF card first (A:\EAR\WAIT\n.wav + n.inf), so nothing is lost without
//    internet or when the power goes: they're sent in order when it's back, even after a restart. The text goes
//    into A:\ear\<date>_<time>.txt with the time it was said. KEEP AUDIO also writes all the sound to a WAV.
//    No TF card: C:\ear (the cube's own 9 MB drive), pieces in memory first.
//  - ERROR-CHAT (1.1): every line also goes to a channel on error.blue (ecfg.chatChan, "EAR1"), a finished
//    transcript is sent there whole, and the channel's buttons come back here (earRemote).
// One task (core 0) takes the pieces and the start / stop wishes; the HTTPS worker (enet.cpp) asks for the
// next piece (scribeNext) and hands back the text (scribeDone).
#include <Arduino.h>
#include <stddef.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <sys/stat.h>
#include <dirent.h>
#include <deque>
#include <string>
#include <vector>
#include <algorithm>
#include "../ui/ui.h"
#include "ear.h"

static SemaphoreHandle_t qmx, lmx;
#define QLOCK() xSemaphoreTake(qmx, portMAX_DELAY)
#define QUNLOCK() xSemaphoreGive(qmx)
#define LLOCK() xSemaphoreTake(lmx, portMAX_DELAY)
#define LUNLOCK() xSemaphoreGive(lmx)

// ERROR-CHAT (further down): a ring of lines for the channel, oldest first; transcripts to send whole
#define NCHAT 96
#define NUP 64
static ChatLine* chat = nullptr;
static char (*ups)[96] = nullptr;

// ------------------------------------------------------------------ settings
void earCfgLoad() {
  Preferences p; p.begin("ear", true);
  EarCfg c;
  size_t n = p.getBytesLength("cfg");
  bool upgrade = false;
  if (n >= 8 && n <= sizeof(EarCfg) * 2) {
    uint8_t* b = (uint8_t*)malloc(n);
    uint32_t mg = 0;
    if (b && p.getBytes("cfg", b, n) == n) memcpy(&mg, b, 4);
    if (mg == c.magic) memcpy(&c, b, n < sizeof(c) ? n : sizeof(c));
    else if (mg == 0xEA700001) {
      // saved by 1.0 / 1.1: 1.0's settings end at liveLines; the byte after it was padding in 1.0, and 1.1 read it
      // as ERROR-CHAT "off". Keep 1.0's settings, ERROR-CHAT on, the channel if 1.1 saved a good one.
      size_t keep = offsetof(EarCfg, chatOn);
      memcpy(&c, b, n < keep ? n : keep);
      if (n >= offsetof(EarCfg, chatChan) + sizeof(c.chatChan)) {
        char ch[9]; memcpy(ch, b + offsetof(EarCfg, chatChan), 9); ch[8] = 0;
        if (chanOk(ch)) memcpy(c.chatChan, ch, 9);
      }
      c.magic = EarCfg().magic; c.chatOn = 1;
      upgrade = true;
    }
    free(b);
  }
  p.end();
  bool fresh = n == 0;
  ecfg = c;
  ecfg.apName[sizeof(ecfg.apName) - 1] = 0; ecfg.apPass[sizeof(ecfg.apPass) - 1] = 0;
  // the first start: the hotspot gets a password of 8 digits (the transcripts are private); SETUP shows it,
  // the QR code has it, and it can be changed or taken off
  if (fresh) {
    snprintf(ecfg.apPass, sizeof(ecfg.apPass), "%08u", (unsigned)(esp_random() % 100000000u));
    earCfgSave();
  } else if (upgrade) earCfgSave();
}
void earCfgSave() { Preferences p; p.begin("ear", false); p.putBytes("cfg", &ecfg, sizeof(ecfg)); p.end(); }

// ------------------------------------------------------------------ the text that came back
#define NLINES 64
static EarLine* lines = nullptr;
static uint32_t lineSeq = 0, clearedAt = 0;
static void lineAdd(const char* at, const char* text) {
  LLOCK();
  uint32_t n = ++lineSeq;
  EarLine& l = lines[n % NLINES];
  l.n = n; snprintf(l.at, sizeof(l.at), "%s", at); snprintf(l.text, sizeof(l.text), "%s", text);
  LUNLOCK();
  uiDirty();
}
uint32_t earLineSeq() { return lineSeq; }
int earLines(EarLine* out, int max, uint32_t after) {
  LLOCK();
  uint32_t first = lineSeq > NLINES ? lineSeq - NLINES + 1 : 1;
  if (after + 1 > first) first = after + 1;
  if (clearedAt + 1 > first) first = clearedAt + 1;      // CLEAR: only what came after
  if (lineSeq >= first && lineSeq - first + 1 > (uint32_t)max) first = lineSeq - max + 1;
  int k = 0;
  for (uint32_t n = first; n <= lineSeq && k < max; n++) out[k++] = lines[n % NLINES];
  LUNLOCK();
  return k;
}
void earClearLines() { LLOCK(); for (int i = 0; i < NLINES; i++) lines[i].n = 0; clearedAt = lineSeq; LUNLOCK(); uiDirty(); }

// ------------------------------------------------------------------ the queue for error.blue
struct Item {
  uint32_t id;
  char file[96];           // the transcript it belongs to ("" = LIVE, nothing written)
  char at[12];             // when it was said: "22:55:07" (or "+12:34" without a clock)
  uint8_t* wav; size_t len;
  char disk[72];           // its WAV on the card / drive ("" = in memory only)
  bool live;
  uint32_t made;
};
static std::deque<Item*> q;
static size_t ramBytes = 0;
static uint32_t nextId = 1;
static volatile uint32_t sendingId = 0;
static volatile uint32_t retryAt = 0;
static int fails = 0;
static int lastCode = 0;
static char lastErr[64] = "";
static const size_t RAM_MAX = 3 * 1024 * 1024;

static void freeItem(Item* it, bool delFiles) {
  if (it->wav) { ramBytes -= it->len; heap_caps_free(it->wav); }
  if (delFiles && it->disk[0]) {
    remove(it->disk);
    std::string inf = it->disk; inf.replace(inf.size() - 4, 4, ".inf"); remove(inf.c_str());
  }
  delete it;
}
static const char* waitDir() { return S.sd ? "/sd/ear/wait" : "/c/ear/wait"; }
// to the card: n.wav (the sound) + n.inf (which transcript, when it was said)
static bool toDisk(Item* it) {
  char p[72]; snprintf(p, sizeof(p), "%s/%08u.wav", waitDir(), (unsigned)it->id);
  fsMkdir(waitDir());
  FILE* f = fopen(p, "wb");
  if (!f) return false;
  bool ok = fwrite(it->wav, 1, it->len, f) == it->len;
  fclose(f);
  char inf[72]; snprintf(inf, sizeof(inf), "%s/%08u.inf", waitDir(), (unsigned)it->id);
  FILE* g = ok ? fopen(inf, "wb") : nullptr;
  if (g) { fprintf(g, "%s\n%s\n%d\n", it->file, it->at, it->live ? 1 : 0); fclose(g); } else ok = false;
  if (!ok) { remove(p); remove(inf); return false; }
  snprintf(it->disk, sizeof(it->disk), "%s", p);
  ramBytes -= it->len; heap_caps_free(it->wav); it->wav = nullptr;
  return true;
}
// after a restart: the pieces still waiting on the card / drive
static void loadWaiting(const char* dir) {
  DIR* d = opendir(dir);
  if (!d) return;
  std::vector<std::string> names;
  struct dirent* e;
  while ((e = readdir(d))) { std::string n = e->d_name; if (n.size() == 12 && n.substr(8) == ".wav") names.push_back(n); }
  closedir(d);
  std::sort(names.begin(), names.end());
  for (auto& n : names) {
    std::string wav = std::string(dir) + "/" + n, inf = wav.substr(0, wav.size() - 4) + ".inf";
    FILE* g = fopen(inf.c_str(), "rb");
    if (!g) { remove(wav.c_str()); continue; }
    char file[96] = "", at[12] = "", lv[4] = "0";
    if (fgets(file, sizeof(file), g)) file[strcspn(file, "\r\n")] = 0;
    if (fgets(at, sizeof(at), g)) at[strcspn(at, "\r\n")] = 0;
    if (fgets(lv, sizeof(lv), g)) {}
    fclose(g);
    Item* it = new Item();
    it->id = (uint32_t)atol(n.c_str()); snprintf(it->file, sizeof(it->file), "%s", file); snprintf(it->at, sizeof(it->at), "%s", at);
    it->wav = nullptr; it->len = 0; snprintf(it->disk, sizeof(it->disk), "%s", wav.c_str()); it->live = lv[0] == '1'; it->made = millis();
    if (it->id >= nextId) nextId = it->id + 1;
    q.push_back(it);
  }
}

// ------------------------------------------------------------------ sessions
static volatile int mode = EAR_IDLE;
static volatile int want = -1;
static char curFile[96] = "", lastFile[96] = "";
static char startErr[64] = "";
static uint32_t startMs = 0, startPos = 0;
static volatile int doneCnt = 0;
static volatile uint32_t words = 0;
static FILE* wavF = nullptr; static uint32_t wavPos = 0, wavBytes = 0; static int16_t* wavBuf = nullptr;
static volatile bool paused = false; static volatile int wantPause = -1;
static uint32_t pausedMs = 0, pauseAt = 0;
static void upQueue(const char* path);

const char* earDir() { sdMount(); return S.sd ? "/sd/ear" : "/c/ear"; }
static bool clockOk(struct tm& t, uint32_t agoSec = 0) {
  struct tm x;
  if (!sysLocalTime(x)) return false;
  time_t now = time(nullptr) - agoSec;
  localtime_r(&now, &t);
  return true;
}
static void writeWavHeader(uint8_t* h, uint32_t dataBytes) {
  uint32_t v;
  memcpy(h, "RIFF", 4); v = 36 + dataBytes; memcpy(h + 4, &v, 4); memcpy(h + 8, "WAVEfmt ", 8);
  v = 16; memcpy(h + 16, &v, 4); uint16_t s = 1; memcpy(h + 20, &s, 2); s = 1; memcpy(h + 22, &s, 2);
  v = EAR_RATE; memcpy(h + 24, &v, 4); v = EAR_RATE * 2; memcpy(h + 28, &v, 4); s = 2; memcpy(h + 32, &s, 2); s = 16; memcpy(h + 34, &s, 2);
  memcpy(h + 36, "data", 4); memcpy(h + 40, &dataBytes, 4);
}
static void fileHeader(FILE* f, bool live) {
  struct tm t; char when[40] = "?";
  if (clockOk(t)) strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &t);
  fprintf(f, "EAR-CUBE %s\nStarted: %s\nLanguage: %s%s\n\n", live ? "live text" : "transcript", when,
          ecfg.lang ? LANGS[ecfg.lang % N_LANGS].code : "auto", ecfg.task ? " (written in English)" : "");
}
static bool startRec() {
  const char* dir = earDir();
  if (!fsMkdir(dir)) { snprintf(startErr, sizeof(startErr), "CAN'T WRITE TO %s", S.sd ? "THE TF CARD" : "C:"); return false; }
  uint64_t tot, used;
  if (fsInfo(dir, tot, used) && tot - used < 256 * 1024) { snprintf(startErr, sizeof(startErr), "%s IS FULL", S.sd ? "THE TF CARD" : "C:"); return false; }
  struct tm t; char base[64];
  if (clockOk(t)) strftime(base, sizeof(base), "%Y-%m-%d_%H-%M", &t);
  else { int n = 1; char p[96]; do { snprintf(base, sizeof(base), "REC-%03d", n); snprintf(p, sizeof(p), "%s/%s.txt", dir, base); n++; } while (fsSize(p) >= 0 && n < 999); }
  char path[96]; snprintf(path, sizeof(path), "%s/%s.txt", dir, base);
  for (int k = 2; fsSize(path) >= 0 && k < 10; k++) snprintf(path, sizeof(path), "%s/%s-%d.txt", dir, base, k);
  FILE* f = fopen(path, "wb");
  if (!f) { snprintf(startErr, sizeof(startErr), "CAN'T MAKE THE FILE"); return false; }
  fileHeader(f, false);
  fclose(f);
  snprintf(curFile, sizeof(curFile), "%s", path); snprintf(lastFile, sizeof(lastFile), "%s", path);
  if (ecfg.keepAudio && S.sd) {
    std::string w = path; w.replace(w.size() - 4, 4, ".wav");
    wavF = fopen(w.c_str(), "wb");
    if (wavF) { uint8_t h[44]; writeWavHeader(h, 0); fwrite(h, 1, 44, wavF); wavBytes = 0; wavPos = micPos(); }
  }
  return true;
}
static void startLive() {
  curFile[0] = 0;
  if (!ecfg.liveKeep) return;
  const char* dir = earDir();
  fsMkdir(dir);
  struct tm t; char path[96];
  if (clockOk(t)) { char d[16]; strftime(d, sizeof(d), "%Y-%m-%d", &t); snprintf(path, sizeof(path), "%s/LIVE-%s.txt", dir, d); }
  else snprintf(path, sizeof(path), "%s/LIVE.txt", dir);
  bool isNew = fsSize(path) < 0;
  FILE* f = fopen(path, "ab");
  if (!f) return;
  if (isNew) fileHeader(f, true); else fprintf(f, "\n");
  fclose(f);
  snprintf(curFile, sizeof(curFile), "%s", path); snprintf(lastFile, sizeof(lastFile), "%s", path);
}
// all the sound since the last time into the session's WAV (KEEP AUDIO)
static void drainWav(bool all) {
  if (!wavF) return;
  if (!wavBuf) wavBuf = (int16_t*)heap_caps_malloc(16000 * 2, MALLOC_CAP_SPIRAM);
  if (!wavBuf) return;
  for (;;) {
    uint32_t w = micPos(), avail = w - wavPos;
    if (avail < (all ? 1u : 8000u)) return;
    int n = avail > 16000 ? 16000 : avail;
    if (micCopy(wavPos, wavBuf, n) < 0) { wavPos = w - 1600; continue; }   // fell behind: skip ahead
    fwrite(wavBuf, 2, n, wavF); wavBytes += n * 2; wavPos += n;
    if (!all) return;
  }
}
static void closeWav() {
  if (!wavF) return;
  drainWav(true);
  uint8_t h[44]; writeWavHeader(h, wavBytes);
  fseek(wavF, 0, SEEK_SET); fwrite(h, 1, 44, wavF); fclose(wavF); wavF = nullptr;
}
static void handlePiece(const Piece& p);
// the microphones stop: the piece so far, then the marker
static void flushMic() {
  micFlush();
  uint32_t t0 = millis();
  Piece p;
  while (millis() - t0 < 2000) {
    if (!micPiece(p, 100)) continue;
    if (p.flush) break;
    handlePiece(p);
  }
}
static void stopNow() {
  if (mode == EAR_IDLE) return;
  if (!paused) flushMic();
  closeWav();
  if (curFile[0]) upQueue(curFile);              // the whole transcript to the channel, once its pieces are written
  mode = EAR_IDLE; curFile[0] = 0; paused = false;
  uiDirty();
}
static void pauseNow(bool p) {
  if (mode == EAR_IDLE || p == paused) return;
  if (p) { flushMic(); if (wavF) { drainWav(true); fflush(wavF); } paused = true; pauseAt = millis(); }
  else { paused = false; pausedMs += millis() - pauseAt; if (wavF) wavPos = micPos(); micRun(mode == EAR_LIVE ? 1 : 2); }
  uiDirty();
}
static void startNow(int m) {
  startErr[0] = 0;
  if (!micOk()) { snprintf(startErr, sizeof(startErr), "NO MICROPHONES FOUND"); return; }
  if (m == EAR_REC) { if (!startRec()) return; }
  else startLive();
  doneCnt = 0; words = 0; startMs = millis(); startPos = micPos();
  paused = false; pausedMs = 0;
  mode = m;
  micRun(m == EAR_LIVE ? 1 : 2);
  uiDirty();
}

// a finished piece: into a WAV, into the queue
static void handlePiece(const Piece& p) {
  if (p.flush || mode == EAR_IDLE) return;
  uint32_t n = p.to - p.from;
  if (n > 30u * EAR_RATE) n = 30u * EAR_RATE;
  size_t len = 44 + n * 2;
  uint8_t* wav = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
  if (!wav) return;
  int16_t* s = (int16_t*)(wav + 44);
  if (micCopy(p.from, s, n) < 0) { heap_caps_free(wav); return; }
  // quiet speech louder (up to x8), so Whisper hears it well
  int pk = 1; for (uint32_t i = 0; i < n; i++) { int a = s[i] < 0 ? -s[i] : s[i]; if (a > pk) pk = a; }
  if (pk < 22000) {
    int g = 22000 * 256 / pk; if (g > 8 * 256) g = 8 * 256;
    for (uint32_t i = 0; i < n; i++) { int v = s[i] * g / 256; s[i] = v > 32767 ? 32767 : v < -32768 ? -32768 : v; }
  }
  writeWavHeader(wav, n * 2);
  Item* it = new Item();
  it->wav = wav; it->len = len; it->disk[0] = 0; it->live = mode == EAR_LIVE; it->made = millis();
  snprintf(it->file, sizeof(it->file), "%s", curFile);
  uint32_t agoSec = (micPos() - p.from) / EAR_RATE;
  struct tm t;
  if (clockOk(t, agoSec)) strftime(it->at, sizeof(it->at), "%H:%M:%S", &t);
  else { uint32_t s0 = (p.from - startPos) / EAR_RATE; snprintf(it->at, sizeof(it->at), "+%02u:%02u", (unsigned)(s0 / 60), (unsigned)(s0 % 60)); }
  QLOCK();
  it->id = nextId++;
  ramBytes += len;
  if (it->live) {
    // live: no internet = no use keeping it; more than 3 waiting = drop the oldest (keep up with the talk)
    if (S.net != NET_UP) { QUNLOCK(); freeItem(it, false); return; }
    int lv = 0; for (Item* x : q) if (x->live) lv++;
    while (lv >= 3) {
      auto k = std::find_if(q.begin(), q.end(), [](Item* x) { return x->live && x->id != sendingId; });
      if (k == q.end()) break;
      Item* x = *k; q.erase(k); freeItem(x, true); lv--;
    }
    q.push_back(it);
  } else {
    // RECORD: on the card straight away; without one, in memory until 3 MB, then on C:
    bool disk = S.sd || ramBytes > RAM_MAX;
    for (Item* x : q) if (x->disk[0] && !x->wav) disk = true;          // keep the order: once on disk, all on disk
    q.push_back(it);
    if (disk && !toDisk(it) && !S.sd) { /* C: full: it stays in memory */ }
  }
  QUNLOCK();
  uiDirty();
}

static void task(void*) {
  for (;;) {
    int w = want;
    if (w >= 0) {
      want = -1;
      if (w != mode) { stopNow(); if (w != EAR_IDLE) startNow(w); }
    }
    int wp = wantPause;
    if (wp >= 0) { wantPause = -1; pauseNow(wp == 1); }
    Piece p;
    if (micPiece(p, 40)) handlePiece(p);
    if (wavF && !paused) drainWav(false);
  }
}

void scribeBegin() {
  qmx = xSemaphoreCreateMutex(); lmx = xSemaphoreCreateMutex();
  lines = (EarLine*)heap_caps_calloc(NLINES, sizeof(EarLine), MALLOC_CAP_SPIRAM);
  chat = (ChatLine*)heap_caps_calloc(NCHAT, sizeof(ChatLine), MALLOC_CAP_SPIRAM);
  ups = (char (*)[96])heap_caps_calloc(NUP, 96, MALLOC_CAP_SPIRAM);
  sdMount();
  QLOCK();
  if (S.sd) { fsMkdir("/sd/ear"); loadWaiting("/sd/ear/wait"); }    // the folder is there from the start
  fsMkdir("/c/ear");
  loadWaiting("/c/ear/wait");
  QUNLOCK();
  xTaskCreatePinnedToCore(task, "scribe", 6144, nullptr, 2, nullptr, 0);
}

// ------------------------------------------------------------------ ERROR-CHAT: lines, whole transcripts, commands
static volatile uint32_t chatW = 0, chatR = 0;
static char chatErr[48] = "";
static const char* base(const char* path) { const char* b = strrchr(path, '/'); return b ? b + 1 : path; }
static void chatAdd(const char* file, const char* at, const char* text, bool late) {
  if (!ecfg.chatOn || !chat) return;
  LLOCK();
  if (chatW - chatR >= NCHAT) chatR++;    // full (no internet for long): the oldest goes
  ChatLine& l = chat[chatW % NCHAT];
  snprintf(l.file, sizeof(l.file), "%s", file && file[0] ? base(file) : "");
  snprintf(l.at, sizeof(l.at), "%s", at); snprintf(l.text, sizeof(l.text), "%s", text); l.late = late;
  chatW++;
  LUNLOCK();
}
bool chatNext(ChatLine& l) { LLOCK(); bool any = chatR != chatW && chat; if (any) l = chat[chatR % NCHAT]; LUNLOCK(); return any; }
void chatDone() { LLOCK(); if (chatR != chatW) chatR++; LUNLOCK(); }
int chatWaiting() { return (int)(chatW - chatR); }
void earChatNote(const char* err) { snprintf(chatErr, sizeof(chatErr), "%s", err ? err : ""); }
const char* earChatErr() { return chatErr; }

// transcripts to send whole: after a recording (its last pieces written first), or all of them (SYNC)
static int nUp = 0;
static void upQueue(const char* path) {
  if (!ecfg.chatOn || !ups || !path[0]) return;
  QLOCK();
  bool have = false; for (int i = 0; i < nUp; i++) if (!strcmp(ups[i], path)) have = true;
  if (!have && nUp < NUP) snprintf(ups[nUp++], 96, "%s", path);
  QUNLOCK();
}
bool upNext(char* path, size_t n) {
  QLOCK();
  for (int i = 0; i < nUp; i++) {
    bool busy = !strcmp(ups[i], curFile) && mode != EAR_IDLE;
    for (Item* x : q) if (!strcmp(x->file, ups[i])) busy = true;      // pieces of it still waiting
    if (!busy) { snprintf(path, n, "%s", ups[i]); QUNLOCK(); return true; }
  }
  QUNLOCK();
  return false;
}
void upDone(const char* path) {
  QLOCK();
  for (int i = 0; i < nUp; i++) if (!strcmp(ups[i], path)) { memmove(ups[i], ups[i + 1], (nUp - i - 1) * 96); nUp--; break; }
  QUNLOCK();
}
// a command from the channel (the HTTPS worker): what its buttons do
void earRemote(const char* cmd, const char* arg) {
  bool on = !strcmp(arg, "on");
  if (!strcmp(cmd, "rec")) { if (mode != EAR_REC) earWant(EAR_REC); }
  else if (!strcmp(cmd, "live")) { if (mode != EAR_LIVE) earWant(EAR_LIVE); }
  else if (!strcmp(cmd, "pause")) earPauseWant(true);
  else if (!strcmp(cmd, "resume")) earPauseWant(false);
  else if (!strcmp(cmd, "stop")) earWant(EAR_IDLE);
  else if (!strcmp(cmd, "screen")) { if (on) S.wakeAt = millis() | 1; else S.sleepAt = millis() | 1; }
  else if (!strcmp(cmd, "lock")) { uiLocked = on; uiDirty(); }
  else if (!strcmp(cmd, "sync")) {
    EarFile* f = (EarFile*)heap_caps_malloc(sizeof(EarFile) * NUP, MALLOC_CAP_SPIRAM);
    int n = f ? earFiles(f, NUP) : 0;
    for (int i = 0; i < n; i++) upQueue(f[i].path);
    heap_caps_free(f);
  } else if (!strcmp(cmd, "rm")) {
    if (!arg[0] || strchr(arg, '/') || strstr(arg, "..")) return;
    const char* dirs[2] = {"/sd/ear", "/c/ear"};
    for (const char* d : dirs) {
      char p[120]; snprintf(p, sizeof(p), "%s/%s", d, arg);
      if (!strcmp(p, curFile)) continue;            // not the one being recorded
      if (fsRemove(p)) { std::string w = p; w.replace(w.size() - 4, 4, ".wav"); fsRemove(w.c_str()); }
    }
  }
  uiDirty();
}

// ------------------------------------------------------------------ for enet.cpp (the HTTPS worker)
bool scribeWants() {
  if (S.net != NET_UP) return false;
  QLOCK(); bool any = !q.empty(); QUNLOCK();
  return any && (int32_t)(millis() - retryAt) >= 0;
}
bool scribeNext(SttJob& j) {
  QLOCK();
  // live pieces older than 20 s are no use any more
  while (!q.empty() && q.front()->live && millis() - q.front()->made > 20000) { Item* x = q.front(); q.pop_front(); freeItem(x, true); }
  if (q.empty()) { QUNLOCK(); return false; }
  Item* it = q.front();
  if (!it->wav && it->disk[0]) {                 // read it back from the card
    long n = fsSize(it->disk);
    uint8_t* b = n > 44 ? (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM) : nullptr;
    FILE* f = b ? fopen(it->disk, "rb") : nullptr;
    bool ok = f && fread(b, 1, n, f) == (size_t)n;
    if (f) fclose(f);
    if (!ok) {                                   // gone or broken: skip it (the next call takes the next one)
      if (b) heap_caps_free(b);
      q.pop_front(); freeItem(it, true);
      QUNLOCK(); return false;
    }
    it->wav = b; it->len = n; ramBytes += n;
  }
  j.wav = it->wav; j.len = it->len; j.id = it->id;
  j.lang = LANGS[ecfg.lang % N_LANGS].code; j.task = ecfg.task;
  sendingId = it->id;
  QUNLOCK();
  uiDirty();
  return true;
}
static int countWords(const char* s) { int n = 0; bool in = false; for (; *s; s++) { bool w = !isspace((unsigned char)*s); if (w && !in) n++; in = w; } return n; }
void scribeDone(uint32_t id, int code, const char* text, const char* err) {
  QLOCK();
  Item* it = (!q.empty() && q.front()->id == id) ? q.front() : nullptr;
  sendingId = 0;
  lastCode = code;
  if (!it) { QUNLOCK(); return; }
  bool drop = false, keep = false;
  if (code == 200) {
    fails = 0; lastErr[0] = 0; retryAt = millis();
    // one line: no new lines inside, no spaces at the ends
    std::string t = text ? text : "";
    for (char& c : t) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    size_t a = t.find_first_not_of(' '), b = t.find_last_not_of(' ');
    t = a == std::string::npos ? "" : t.substr(a, b - a + 1);
    if (t.size()) {
      if (it->file[0]) {
        FILE* f = fopen(it->file, "ab");
        if (f) { if (ecfg.stamps) fprintf(f, "[%s] %s\n", it->at, t.c_str()); else fprintf(f, "%s\n", t.c_str()); fclose(f); }
      }
      lineAdd(it->at, t.c_str());
      words += countWords(t.c_str());
      chatAdd(it->file, it->at, t.c_str(), millis() - it->made > 60000);
    }
    doneCnt++;
    drop = true;
  } else if (code == 429) {                       // error.blue's daily limit: try again in 10 minutes
    snprintf(lastErr, sizeof(lastErr), "%s", err && err[0] ? err : "DAILY LIMIT REACHED");
    retryAt = millis() + 600000; keep = true;
  } else if (code >= 400 && code < 500) {         // the piece itself is no good: skip it
    snprintf(lastErr, sizeof(lastErr), "%d %s", code, err ? err : "");
    drop = true;
  } else {                                        // no connection, or error.blue had a problem: again soon
    snprintf(lastErr, sizeof(lastErr), "%s", err && err[0] ? err : code > 0 ? "ERROR.BLUE HAD A PROBLEM" : "NO CONNECTION");
    fails++;
    static const uint32_t BACK[5] = {2000, 5000, 15000, 30000, 60000};
    retryAt = millis() + BACK[fails < 5 ? fails - 1 : 4];
    keep = true;
    if (it->live) drop = true, keep = false;     // live: it's gone, the next one is newer
  }
  if (drop) { q.pop_front(); freeItem(it, true); }
  else if (keep && it->disk[0] && it->wav) { ramBytes -= it->len; heap_caps_free(it->wav); it->wav = nullptr; }   // back to the card
  QUNLOCK();
  uiDirty();
}

// ------------------------------------------------------------------ what the screens and the page ask
void earWant(int m) { want = m; }
int earMode() { return mode; }
const char* earStartError() { return startErr; }
uint32_t earSecs() { return mode == EAR_IDLE ? 0 : (millis() - startMs - pausedMs - (paused ? millis() - pauseAt : 0)) / 1000; }
void earPauseWant(bool p) { wantPause = p ? 1 : 0; }
bool earPaused() { return paused; }
const char* earFile() { return curFile; }
const char* earLastFile() { return lastFile; }
int earWaiting() { return (int)q.size(); }
int earDoneCount() { return doneCnt; }
uint32_t earWords() { return words; }
bool earSending() { return sendingId != 0; }
const char* earErr() { return lastErr; }
int earStatus() {
  if (!micOk()) return ST_NOMIC;
  if (lastCode == 429 && !q.empty()) return ST_LIMIT;
  if (mode != EAR_IDLE) {
    if (paused) return ST_PAUSED;
    if (micSpeech()) return ST_HEAR;
    if (sendingId) return ST_SEND;
    if (S.net != NET_UP) return ST_OFFLINE;
    return ST_LISTEN;
  }
  if (sendingId) return ST_SEND;
  if (!q.empty() && S.net != NET_UP) return ST_OFFLINE;
  return ST_READY;
}
const char* earStatusText(int st) {
  static const char* T[9] = {"READY", "LISTENING", "HEARING SPEECH", "WRITING", "NO INTERNET", "DAILY LIMIT", "ERROR", "NO MICROPHONE", "PAUSED"};
  return T[st >= 0 && st < 9 ? st : 6];
}

bool earSaveLive(char* path, size_t n) {
  EarLine* l = (EarLine*)heap_caps_malloc(sizeof(EarLine) * NLINES, MALLOC_CAP_SPIRAM);
  if (!l) return false;
  int k = earLines(l, NLINES, clearedAt);
  if (!k) { heap_caps_free(l); return false; }
  const char* dir = earDir(); fsMkdir(dir);
  struct tm t; char base[48];
  if (clockOk(t)) strftime(base, sizeof(base), "LIVE-%Y-%m-%d_%H-%M", &t); else snprintf(base, sizeof(base), "LIVE-%u", (unsigned)(millis() / 1000));
  snprintf(path, n, "%s/%s.txt", dir, base);
  FILE* f = fopen(path, "wb");
  if (f) {
    fileHeader(f, true);
    for (int i = 0; i < k; i++) { if (ecfg.stamps) fprintf(f, "[%s] %s\n", l[i].at, l[i].text); else fprintf(f, "%s\n", l[i].text); }
    fclose(f);
    snprintf(lastFile, sizeof(lastFile), "%s", path);
  }
  heap_caps_free(l);
  return f != nullptr;
}

int earFiles(EarFile* out, int max) {
  static FileEnt* e = nullptr;
  if (!e) e = (FileEnt*)heap_caps_malloc(sizeof(FileEnt) * 200, MALLOC_CAP_SPIRAM);
  if (!e) return 0;
  sdMount();
  std::vector<EarFile> all;
  const char* dirs[2] = {"/sd/ear", "/c/ear"};
  for (int d = 0; d < 2; d++) {
    if (d == 0 && !S.sd) continue;
    int n = fsList(dirs[d], e, 200);
    for (int i = 0; i < n; i++) {
      size_t l = strlen(e[i].name);
      if (e[i].dir || l < 5 || strcasecmp(e[i].name + l - 4, ".txt")) continue;
      EarFile f;
      snprintf(f.path, sizeof(f.path), "%s/%s", dirs[d], e[i].name);
      earNiceName(e[i].name, f.name, sizeof(f.name));
      f.size = e[i].size; f.live = !strncmp(e[i].name, "LIVE", 4);
      std::string w = f.path; w.replace(w.size() - 4, 4, ".wav"); f.hasWav = fsSize(w.c_str()) > 0;
      all.push_back(f);
    }
  }
  // newest first (the names start with the date); the one being written on top
  std::sort(all.begin(), all.end(), [](const EarFile& a, const EarFile& b) {
    bool ac = !strcmp(a.path, curFile), bc = !strcmp(b.path, curFile);
    if (ac != bc) return ac;
    const char* x = strrchr(a.path, '/') + 1; const char* y = strrchr(b.path, '/') + 1;
    if (!strncmp(x, "LIVE-", 5)) x += 5; if (!strncmp(y, "LIVE-", 5)) y += 5;
    return strcmp(x, y) > 0;
  });
  int k = 0;
  for (auto& f : all) { if (k >= max) break; out[k++] = f; }
  return k;
}
