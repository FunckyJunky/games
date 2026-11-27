// listen.cpp: DREAM-CUBE's listening. The microphones (mic.cpp) cut speech into pieces at the pauses; each piece
// becomes a WAV that waits here for error.blue (speech to text). The words that come back are gathered; when there
// are enough (SETUP > NEW SCENE: OFTEN / NORMAL / CALM), they go to error.blue as one text and come back as a scene.
// The scenes are kept (40 in memory, to go back with PREV / NEXT; every one also on the TF card: A:\DREAM, a file a
// day), and the control page (error.blue/dream) can send commands: they run here, in the main loop.
#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <ArduinoJson.h>
#include <deque>
#include <string>
#include "../core/sys.h"
#include "../ui/ui.h"
#include "dream.h"
#include "musicdet.h"

DreamCfg dcfg;
const Lang LANGS[] = {
  {"", "AUTO"}, {"nl", "NEDERLANDS"}, {"en", "ENGLISH"}, {"de", "DEUTSCH"}, {"fr", "FRAN\xC3\x87" "AIS"},
  {"uk", "\xD0\xA3\xD0\x9A\xD0\xA0\xD0\x90\xD0\x87\xD0\x9D\xD0\xA1\xD0\xAC\xD0\x9A\xD0\x90"},
  {"ru", "\xD0\xA0\xD0\xA3\xD0\xA1\xD0\xA1\xD0\x9A\xD0\x98\xD0\x99"},
  {"es", "ESPA\xC3\x91OL"}, {"it", "ITALIANO"}, {"pl", "POLSKI"}, {"pt", "PORTUGU\xC3\x8AS"}, {"tr", "T\xC3\x9CRK\xC3\x87" "E"},
};
const int N_LANGS = sizeof(LANGS) / sizeof(LANGS[0]);
const char* gainLabel(int step) {
  static const char* G[15] = {"0 dB", "3 dB", "6 dB", "9 dB", "12 dB", "15 dB", "18 dB", "21 dB", "24 dB", "27 dB", "30 dB", "33 dB", "34.5 dB", "36 dB", "37.5 dB"};
  return G[step < 0 ? 0 : step > 14 ? 14 : step];
}
void dreamApName(char* out, size_t n) {
  if (dcfg.apName[0]) { snprintf(out, n, "%s", dcfg.apName); return; }
  const char* h = S.host; const char* d = strchr(h, '-');
  snprintf(out, n, "DREAM-CUBE-%s", d ? d + 1 : h);
}
void dreamNewCode() {
  static const char* A = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   // no I, O, 0, 1 (error.blue's link codes)
  for (int i = 0; i < 8; i++) dcfg.code[i] = A[esp_random() % 32];
  dcfg.code[8] = 0;
}
void dreamCfgSave() { Preferences p; p.begin("dream", false); p.putBytes("cfg", &dcfg, sizeof(dcfg)); p.end(); }
void dreamCfgLoad() {
  Preferences p; p.begin("dream", true);
  DreamCfg c;
  size_t n = p.getBytesLength("cfg");
  if (n >= 8 && n <= sizeof(DreamCfg) * 2) {
    uint8_t* b = (uint8_t*)malloc(n);
    uint32_t mg = 0;
    if (b && p.getBytes("cfg", b, n) == n) memcpy(&mg, b, 4);
    if (mg == c.magic) memcpy(&c, b, n < sizeof(c) ? n : sizeof(c));
    free(b);
  }
  p.end();
  bool fresh = n == 0;
  dcfg = c;
  dcfg.apName[sizeof(dcfg.apName) - 1] = 0; dcfg.apPass[sizeof(dcfg.apPass) - 1] = 0; dcfg.code[8] = 0;
  // the first start: a hotspot password (8 digits) and the control page's link code
  if (fresh) snprintf(dcfg.apPass, sizeof(dcfg.apPass), "%08u", (unsigned)(esp_random() % 100000000u));
  if (strlen(dcfg.code) != 8) dreamNewCode();
  if (fresh || n != sizeof(DreamCfg)) dreamCfgSave();
}

// ------------------------------------------------------------------ the pieces waiting for speech to text
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define LOCK() portENTER_CRITICAL(&mux)
#define UNLOCK() portEXIT_CRITICAL(&mux)
struct Item { uint8_t* wav; size_t len; uint32_t id, made; };
static std::deque<Item> q;
static uint32_t nextId = 1, retryAt = 0, sendingId = 0;
static volatile int status = ST_READY;
static char err[64] = "";
static char detLang[4] = "";
static void writeWavHeader(uint8_t* h, uint32_t dataBytes) {
  uint32_t v; uint16_t s;
  memcpy(h, "RIFF", 4); v = 36 + dataBytes; memcpy(h + 4, &v, 4); memcpy(h + 8, "WAVEfmt ", 8);
  v = 16; memcpy(h + 16, &v, 4); s = 1; memcpy(h + 20, &s, 2); s = 1; memcpy(h + 22, &s, 2);
  v = EAR_RATE; memcpy(h + 24, &v, 4); v = EAR_RATE * 2; memcpy(h + 28, &v, 4); s = 2; memcpy(h + 32, &s, 2); s = 16; memcpy(h + 34, &s, 2);
  memcpy(h + 36, "data", 4); memcpy(h + 40, &dataBytes, 4);
}
static void pieceTask(void*) {
  for (;;) {
    Piece p;
    if (!micPiece(p, 100)) continue;
    if (p.flush || !dcfg.listen) continue;
    uint32_t n = p.to - p.from;
    if (n < EAR_RATE / 3 || p.speechMs < 300) continue;          // a cough, a click
    if (n > EAR_RATE * 28) { p.from = p.to - EAR_RATE * 28; n = EAR_RATE * 28; }
    uint8_t* w = (uint8_t*)heap_caps_malloc(44 + n * 2, MALLOC_CAP_SPIRAM);
    if (!w) continue;
    writeWavHeader(w, n * 2);
    if (micCopy(p.from, (int16_t*)(w + 44), n) < 0) { heap_caps_free(w); continue; }
    Item it = {w, 44 + n * 2, 0, millis()};
    uint8_t* drop = nullptr;
    LOCK();
    it.id = nextId++;
    // never more than 5 waiting (no WiFi for a while): the oldest goes
    if (q.size() >= 5 && q.front().id != sendingId) { drop = q.front().wav; q.pop_front(); }
    q.push_back(it);
    UNLOCK();
    if (drop) heap_caps_free(drop);
    uiDirty();
  }
}
int scribeWaiting() { LOCK(); int n = q.size(); UNLOCK(); return n; }
bool scribeWants() { LOCK(); bool w = !q.empty() && (int32_t)(millis() - retryAt) >= 0; UNLOCK(); return w; }
bool scribeNext(SttJob& j) {
  uint8_t* drop[6]; int nd = 0;
  LOCK();
  // pieces older than 25 s are no use any more
  while (!q.empty() && millis() - q.front().made > 25000 && nd < 6) { drop[nd++] = q.front().wav; q.pop_front(); }
  bool ok = !q.empty();
  if (ok) { const Item& it = q.front(); j.wav = it.wav; j.len = it.len; j.id = it.id; sendingId = it.id; }
  UNLOCK();
  for (int i = 0; i < nd; i++) heap_caps_free(drop[i]);
  if (!ok) return false;
  j.lang = LANGS[dcfg.lang % N_LANGS].code;
  status = ST_SEND;
  uiDirty();
  return true;
}

// ------------------------------------------------------------------ the words
static char pend[900] = "", lastSent[900] = "";
static int pendWords = 0;
static uint32_t lastWordsAt = 0, lastSceneAt = 0, scenesMade = 0;
static char heard[200] = "";
static uint32_t heardAt = 0;
static bool inFlight = false, forced = false;
// the lines for the control page's live transcript (sent with the next link, which comes at once)
struct Line { char t[400]; uint32_t at; };
#define NL 24
static Line* lq = nullptr; static volatile int lHead = 0, lN = 0;
static uint32_t heardN = 0;                      // pieces with words (the page sees it count)
static void lineAdd(const char* t) {
  if (!dcfg.link || !t) return;
  while (*t == ' ') t++;
  if (!*t) return;
  if (!lq) lq = (Line*)heap_caps_malloc(sizeof(Line) * NL, MALLOC_CAP_SPIRAM);
  if (!lq) lq = (Line*)malloc(sizeof(Line) * NL);
  if (!lq) return;
  LOCK();
  if (lN == NL) { lHead = (lHead + 1) % NL; lN--; }     // too many waiting (no internet): the oldest goes
  Line& l = lq[(lHead + lN) % NL];
  snprintf(l.t, sizeof(l.t), "%s", t); l.at = millis();
  lN++;
  UNLOCK();
}
int dreamLinesWaiting() { return lN; }
// the lines are copied while locked; the JSON is made after (nothing that allocates inside the lock)
int dreamLinesJson(char* out, size_t n) {
  static Line* snap = nullptr;
  if (!snap) snap = (Line*)heap_caps_malloc(sizeof(Line) * 8, MALLOC_CAP_SPIRAM);
  if (!snap) snap = (Line*)malloc(sizeof(Line) * 8);
  if (!out || n < 3) return 0;
  int k = 0;
  if (lq && snap) {
    LOCK();
    k = lN < 8 ? lN : 8;
    for (int i = 0; i < k; i++) snap[i] = lq[(lHead + i) % NL];
    UNLOCK();
  }
  uint32_t now = millis();
  // as many as fit (the rest go with the next link)
  size_t w = 0;
  out[w++] = '[';
  int sent = 0;
  for (int i = 0; i < k; i++) {
    JsonDocument d;
    d["t"] = (const char*)snap[i].t; d["ago"] = now - snap[i].at;
    size_t need = measureJson(d) + 2;
    if (w + need + 2 > n) break;
    if (sent) out[w++] = ',';
    w += serializeJson(d, out + w, n - w);
    sent++;
  }
  out[w++] = ']'; out[w] = 0;
  return sent;
}
void dreamLinesDone(int k) { if (k <= 0) return; LOCK(); if (k > lN) k = lN; lHead = (lHead + k) % NL; lN -= k; UNLOCK(); }
// "link a phone?": the answers (YES / NO on the cube) wait for the next link
struct PairAck { char id[16]; int8_t ok; };
static PairAck acks[4]; static int nAck = 0;
int dreamPairJson(char* out, size_t n) {
  if (!out) return nAck;
  JsonDocument d; JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < nAck; i++) { JsonObject o = a.add<JsonObject>(); o["id"] = (const char*)acks[i].id; o["ok"] = acks[i].ok; }
  serializeJson(d, out, n);
  return nAck;
}
void dreamPairDone(int k) { if (k >= nAck) nAck = 0; else { memmove(acks, acks + k, sizeof(PairAck) * (nAck - k)); nAck -= k; } }
static int countWords(const char* s) { int n = 0; bool in = false; for (; *s; s++) { bool w = !isspace((unsigned char)*s); if (w && !in) n++; in = w; } return n; }
static void addWords(const char* t, bool keep = true) {   // keep: towards the next scene (else only shown)
  while (*t == ' ') t++;
  if (!*t) return;
  if (keep) {
    LOCK();
    size_t a = strlen(pend), b = strlen(t);
    if (a + b + 2 >= sizeof(pend)) {                  // too much waiting: the oldest words go
      size_t cut = a + b + 2 - sizeof(pend) + 1;
      if (cut > a) { a = 0; pend[0] = 0; } else { memmove(pend, pend + cut, a - cut + 1); a -= cut; }
    }
    if (a) pend[a++] = ' ';
    snprintf(pend + a, sizeof(pend) - a, "%s", t);
    pendWords += countWords(t);
    lastWordsAt = millis();
    UNLOCK();
  }
  size_t l = strlen(t);
  snprintf(heard, sizeof(heard), "%s", l > sizeof(heard) - 1 ? t + l - (sizeof(heard) - 1) : t);
  heardAt = millis();
  uiDirty();
}
// speech to text on music: "♪ la la ♪", "[Music]", "(muziek)", "*music*" -> a hint for the music side, and
// those marks out of the words ([...] tags always: [Applause], [Laughter])
static bool musicMarks(const char* in, char* out, size_t n) {
  bool found = false; size_t o = 0;
  for (const char* p = in; *p && o + 1 < n; ) {
    const uint8_t* u = (const uint8_t*)p;
    if (u[0] == 0xE2 && u[1] == 0x99 && (u[2] == 0xAA || u[2] == 0xAB)) { found = true; p += 3; continue; }   // ♪ ♫
    if (*p == '[' || *p == '(' || *p == '*') {
      char close = *p == '[' ? ']' : *p == '(' ? ')' : '*';
      const char* e = strchr(p + 1, close);
      if (e && e - p < 48) {
        bool m = false;
        for (const char* q = p + 1; q + 2 < e; q++)
          if (tolower((uint8_t)q[0]) == 'm' && tolower((uint8_t)q[1]) == 'u' && (tolower((uint8_t)q[2]) == 's' || tolower((uint8_t)q[2]) == 'z')) m = true;
        if (m) found = true;
        if (m || *p == '[') { p = e + 1; continue; }
      }
    }
    out[o++] = *p++;
  }
  out[o] = 0;
  // spaces tidied
  char* w = out; bool sp = true;
  for (char* r = out; *r; r++) { if (*r == ' ') { if (!sp) *w++ = ' '; sp = true; } else { *w++ = *r; sp = false; } }
  if (w > out && w[-1] == ' ') w--;
  *w = 0;
  return found;
}
void micFrameHook(const int16_t* last512, float gateDb) { mdFrame(last512, gateDb); }   // mic.cpp, every 20 ms
bool dreamMusic() {
  if (!dcfg.listen || !dcfg.music || !micOk()) return false;
  return dcfg.music == 2 || mdState().music;
}
float dreamMusicScore() { return mdState().score; }
void dreamMusicIn(MusicIn& m) {
  const MusicState& s = mdState();
  for (int i = 0; i < 16; i++) m.band[i] = s.band[i];
  m.bass = s.bass; m.mid = s.mid; m.high = s.high; m.level = s.level; m.beat = s.beat; m.beats = s.beats; m.bpm = s.bpm;
}
void scribeDone(uint32_t id, int code, const char* text, const char* lang, const char* e) {
  uint8_t* drop = nullptr;
  bool retry = code <= 0;
  LOCK();
  sendingId = 0;
  if (!q.empty() && q.front().id == id && !retry) { drop = q.front().wav; q.pop_front(); }
  if (retry) retryAt = millis() + 3000;
  UNLOCK();
  if (drop) heap_caps_free(drop);
  if (code == 200) {
    err[0] = 0;
    if (lang && strlen(lang) == 2) snprintf(detLang, sizeof(detLang), "%s", lang);
    if (text && *text) {
      static char clean[400];
      if (musicMarks(text, clean, sizeof(clean))) mdHint(1);
      // during the music the words are only shown (3D line art) and kept on the page, no scenes from song lyrics
      if (clean[0]) { addWords(clean, !dreamMusic()); lineAdd(clean); heardN++; }
    }
    status = ST_LISTEN;
  } else {
    snprintf(err, sizeof(err), "%s", e && *e ? e : "ERROR");
    status = code <= 0 ? ST_OFFLINE : code == 429 ? ST_LIMIT : ST_ERROR;
  }
  uiDirty();
}
int dreamWordsWaiting() { return pendWords; }
const char* dreamHeard() { return heard; }
uint32_t dreamHeardAt() { return heardAt; }
bool dreamThinking() { return inFlight; }
uint32_t dreamScenes() { return scenesMade; }
void dreamSay(const char* text) {
  if (!text || !*text) return;
  LOCK(); pend[0] = 0; pendWords = 0; UNLOCK();
  addWords(text);
  forced = true;
}
bool dreamWants() {
  if (inFlight) return false;
  if (forced) return pendWords > 0;
  if (dreamMusic()) return false;                // music: the visualizer, no new scenes
  if (!dcfg.listen || !pendWords) return false;
  static const uint8_t MINW[3] = {6, 12, 22}, MINS[3] = {8, 15, 30}, QW[3] = {3, 5, 8}, QS[3] = {4, 6, 10};
  int o = dcfg.often % 3;
  uint32_t now = millis(), since = now - lastSceneAt, quiet = now - lastWordsAt;
  if (!scenesMade && pendWords >= 3 && quiet > 1500) return true;          // the very first one: soon
  if (since < MINS[o] * 1000u) return false;
  if (pendWords >= MINW[o]) return true;
  return pendWords >= QW[o] && quiet > QS[o] * 1000u;
}
static char shownTitle[48] = "";
bool dreamNext(char* text, size_t n, char* prev, size_t pn, char* lang, size_t ln) {
  LOCK();
  if (inFlight || !pendWords) { UNLOCK(); return false; }
  snprintf(text, n, "%s", pend);
  snprintf(lastSent, sizeof(lastSent), "%s", pend);
  pend[0] = 0; pendWords = 0; inFlight = true; forced = false;
  UNLOCK();
  snprintf(prev, pn, "%s", shownTitle);
  snprintf(lang, ln, "%s", dcfg.lang ? LANGS[dcfg.lang % N_LANGS].code : detLang);
  status = ST_THINK;
  uiDirty();
  return true;
}

// ------------------------------------------------------------------ the scenes kept
#define NH 40
struct Hist { char* json; char title[40]; char when[8]; };
static Hist hist[NH]; static int nHist = 0, pos = 0;
static char* incoming = nullptr; static bool incomingFresh = false;
static int wantPos = -1;
static void put(char* json, bool fresh) {            // (main loop) a new scene from error.blue or the page
  LOCK(); char* old = incoming; incoming = json; incomingFresh = fresh; UNLOCK();
  if (old) heap_caps_free(old);
}
static char* psdup(const char* s) { size_t n = strlen(s) + 1; char* p = (char*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM); if (p) memcpy(p, s, n); return p; }
void dreamDone(int code, const char* json, const char* e) {
  inFlight = false;
  lastSceneAt = millis();
  if (code == 200 && json && *json) {
    char* p = psdup(json);
    if (p) put(p, true);
    status = dcfg.listen ? ST_LISTEN : ST_PAUSED;
    err[0] = 0;
    return;
  }
  // nothing came back: the words wait for the next try
  if (code <= 0) {
    LOCK();
    if (strlen(lastSent) + strlen(pend) + 2 < sizeof(pend)) {
      char tmp[900]; snprintf(tmp, sizeof(tmp), "%s%s%s", lastSent, pend[0] ? " " : "", pend);
      snprintf(pend, sizeof(pend), "%s", tmp); pendWords = countWords(pend);
    }
    UNLOCK();
  }
  snprintf(err, sizeof(err), "%s", e && *e ? e : "ERROR");
  status = code <= 0 ? ST_OFFLINE : code == 429 ? ST_LIMIT : ST_ERROR;
  uiDirty();
}
int dreamCount() { return nHist; }
int dreamPos() { return pos; }
void dreamShowAt(int p) { if (p < 0) p = 0; if (p >= nHist) p = nHist - 1; if (p >= 0) wantPos = p; }
bool dreamTitleAt(int p, char* out, size_t n, char* when, size_t wn) {
  if (p < 0 || p >= nHist) return false;
  snprintf(out, n, "%s", hist[p].title);
  if (when) snprintf(when, wn, "%s", hist[p].when);
  return true;
}
static const char* dreamDir() { return S.sd ? "/sd/DREAM" : "/c/dream"; }
static void keepOnCard(const char* json, const SceneDef& d) {
  struct tm t;
  char path[64], when[8] = "--:--";
  bool timeOk = sysLocalTime(t);
  if (timeOk) strftime(when, sizeof(when), "%H:%M", &t);
  if (!fsMkdir(dreamDir())) return;
  if (timeOk) { char day[16]; strftime(day, sizeof(day), "%Y-%m-%d", &t); snprintf(path, sizeof(path), "%s/%s.txt", dreamDir(), day); }
  else snprintf(path, sizeof(path), "%s/scenes.txt", dreamDir());
  FILE* f = fopen(path, "a");
  if (!f) return;
  fprintf(f, "%s  %s\n  \"%s\"\n  %s\n\n", when, d.title, d.said, json);
  fclose(f);
  // the newest also on C: (shown again after a restart), at most every 2 minutes
  static uint32_t lastAt = 0;
  if (!lastAt || millis() - lastAt > 120000) {
    lastAt = millis();
    fsMkdir("/c/dream");
    FILE* g = fopen("/c/dream/last.json", "w");
    if (g) { fputs(json, g); fclose(g); }
  }
}
bool dreamPoll(SceneDef& d, bool& fresh) {
  LOCK(); char* in = incoming; bool fr = incomingFresh; incoming = nullptr; UNLOCK();
  if (in) {
    if (!sceneParse(in, d)) { heap_caps_free(in); return false; }
    if (nHist == NH) { heap_caps_free(hist[NH - 1].json); nHist--; }
    memmove(hist + 1, hist, sizeof(Hist) * nHist);
    hist[0].json = in;
    snprintf(hist[0].title, sizeof(hist[0].title), "%s", d.title);
    struct tm t; if (sysLocalTime(t)) strftime(hist[0].when, sizeof(hist[0].when), "%H:%M", &t); else strcpy(hist[0].when, "");
    nHist++; pos = 0; wantPos = -1;
    if (fr) { scenesMade++; keepOnCard(in, d); }
    snprintf(shownTitle, sizeof(shownTitle), "%s", d.title);
    fresh = fr;
    return true;
  }
  if (wantPos >= 0 && wantPos < nHist && wantPos != pos) {
    int p = wantPos; wantPos = -1;
    if (!sceneParse(hist[p].json, d)) return false;
    pos = p; fresh = false;
    snprintf(shownTitle, sizeof(shownTitle), "%s", d.title);
    return true;
  }
  wantPos = -1;
  return false;
}

// ------------------------------------------------------------------ listening on / off, status
void dreamListen(bool on) {
  dcfg.listen = on;
  dreamCfgSave();
  micRun(on ? 1 : 0);
  if (!on) { LOCK(); pend[0] = 0; pendWords = 0; UNLOCK(); }
  status = on ? ST_LISTEN : ST_PAUSED;
  uiDirty();
}
bool dreamListening() { return dcfg.listen; }
const char* dreamErr() { return err; }
int dreamStatus() {
  if (!micOk()) return ST_NOMIC;
  if (inFlight) return ST_THINK;
  if (sendingId) return ST_SEND;
  if (S.net != NET_UP) return ST_OFFLINE;
  if (!dcfg.listen) return ST_PAUSED;
  if (status == ST_LIMIT || status == ST_ERROR || status == ST_OFFLINE) return status;
  return micSpeech() ? ST_HEAR : ST_LISTEN;
}
const char* dreamStatusText(int st) {
  static const char* T[] = {"READY", "LISTENING", "HEARING", "SENDING", "DREAMING...", "NO INTERNET", "DAILY LIMIT", "ERROR", "NO MICROPHONES", "PAUSED"};
  return T[st < 0 || st > ST_PAUSED ? 0 : st];
}

// ------------------------------------------------------------------ the control page's commands
struct Cmd { char c[8]; char* a; };
static std::deque<Cmd> cmds;
void dreamCmd(int, const char* c, const char* a) {
  Cmd x; snprintf(x.c, sizeof(x.c), "%s", c); x.a = psdup(a ? a : "");
  LOCK(); if (cmds.size() < 12) { cmds.push_back(x); x.a = nullptr; } UNLOCK();
  if (x.a) heap_caps_free(x.a);
}
static void setCfg(const char* json) {
  JsonDocument d;
  if (deserializeJson(d, json)) return;
  auto u8 = [&](const char* k, uint8_t& v, int max) { if (d[k].is<int>()) { int x = d[k]; if (x >= 0 && x <= max) v = x; } };
  u8("often", dcfg.often, 2); u8("mind", dcfg.mind, 1); u8("words", dcfg.words, 1); u8("title", dcfg.title, 1);
  u8("cam", dcfg.cam, 3); u8("soft", dcfg.soft, 1); u8("detail", dcfg.detail, 1); u8("wake", dcfg.wake, 1);
  u8("lang", dcfg.lang, N_LANGS - 1); u8("sens", dcfg.sens, 3); u8("mics", dcfg.mics, 2);
  uint8_t g = dcfg.gain; u8("gain", g, 14); if (g != dcfg.gain) { dcfg.gain = g; micGain(g); }
  u8("pause", dcfg.pause, 2); u8("colors", dcfg.colors, 2); u8("fx", dcfg.fxOn, 1); u8("fps", dcfg.fps, 1);
  u8("music", dcfg.music, 2); u8("mwords", dcfg.mwords, 1);
  if (d["listen"].is<int>()) dreamListen((int)d["listen"] != 0);
  dreamCfgSave();
  // ERROR-CUBE's own: screen and sound
  bool c = false;
  auto cu8 = [&](const char* k, uint8_t& v, int lo, int hi) { if (d[k].is<int>()) { int x = d[k]; if (x >= lo && x <= hi && x != v) { v = x; c = true; } } };
  cu8("bright", cfg.bright, 5, 100); cu8("off", cfg.offIdx, 0, N_OFF - 1); cu8("color", cfg.color, 0, N_SCHEMES - 1);
  cu8("vol", cfg.vol, 0, 100); cu8("autoUpd", cfg.autoUpd, 0, 1);
  if (c) cfgChanged();
  uiDirty();
}
void dreamLoop() {
  for (int k = 0; k < 4; k++) {
    Cmd x; bool have = false;
    LOCK(); if (!cmds.empty()) { x = cmds.front(); cmds.pop_front(); have = true; } UNLOCK();
    if (!have) break;
    const char* c = x.c;
    if (!strcmp(c, "listen")) dreamListen(true);
    else if (!strcmp(c, "pause")) dreamListen(false);
    else if (!strcmp(c, "wake")) uiWake();
    else if (!strcmp(c, "sleep")) uiSleep();
    else if (!strcmp(c, "prev")) dreamShowAt(pos + 1);
    else if (!strcmp(c, "next")) dreamShowAt(pos - 1);
    else if (!strcmp(c, "scene")) { put(x.a, false); x.a = nullptr; if (dcfg.wake) uiWake(); }
    else if (!strcmp(c, "cfg")) setCfg(x.a);
    else if (!strcmp(c, "unlink")) { dcfg.link = 0; dreamCfgSave(); }
    else if (!strcmp(c, "pair")) {           // a phone at error.blue/dream wants to link: "id|1234"
      std::string a = x.a ? x.a : "", id = a.substr(0, a.find('|')), num = a.find('|') != std::string::npos ? a.substr(a.find('|') + 1) : "";
      uiWake();
      char msg[200];
      snprintf(msg, sizeof(msg), "A phone at error.blue/dream wants to see what this cube hears and use its buttons.\nThe phone shows: %s", num.c_str());
      ask("LINK A PHONE?", msg, {"YES", "NO"}, [id](int k) {
        if (nAck < 4) { snprintf(acks[nAck].id, sizeof(acks[nAck].id), "%s", id.c_str()); acks[nAck].ok = k == 0 ? 1 : 0; nAck++; }
        toast(k == 0 ? "PHONE LINKED" : "NOT LINKED");
      });
    }
    if (x.a) heap_caps_free(x.a);
  }
}
void dreamState(char* out, size_t n) {
  JsonDocument d;
  d["v"] = CUBE_VERSION; d["listen"] = dcfg.listen; d["st"] = dreamMusic() && dreamStatus() <= ST_HEAR ? "MUSIC" : dreamStatusText(dreamStatus());
  d["music"] = dreamMusic() ? 1 : 0; d["bpm"] = (int)(mdState().bpm + 0.5f); d["ms"] = (int)(mdState().score * 100);
  d["title"] = shownTitle; d["heard"] = heard; d["hn"] = heardN; d["lw"] = lN; d["pos"] = pos; d["kept"] = nHist; d["made"] = scenesMade;
  d["lang"] = dcfg.lang ? LANGS[dcfg.lang % N_LANGS].code : detLang;
  JsonObject c = d["cfg"].to<JsonObject>();
  c["often"] = dcfg.often; c["mind"] = dcfg.mind; c["words"] = dcfg.words; c["title"] = dcfg.title; c["cam"] = dcfg.cam;
  c["soft"] = dcfg.soft; c["detail"] = dcfg.detail; c["wake"] = dcfg.wake; c["lang"] = dcfg.lang; c["sens"] = dcfg.sens;
  c["gain"] = dcfg.gain; c["mics"] = dcfg.mics; c["pause"] = dcfg.pause; c["colors"] = dcfg.colors; c["fx"] = dcfg.fxOn; c["fps"] = dcfg.fps;
  c["music"] = dcfg.music; c["mwords"] = dcfg.mwords;
  c["bright"] = cfg.bright; c["off"] = cfg.offIdx; c["color"] = cfg.color; c["vol"] = cfg.vol; c["autoUpd"] = cfg.autoUpd;
  d["awake"] = uiAwake(); d["bat"] = S.battery ? S.batPct : -1; d["chg"] = S.charging; d["wifi"] = S.ssid;
  serializeJson(d, out, n);
}

// ------------------------------------------------------------------ start
void dreamBegin() {
  mdInit();
  xTaskCreatePinnedToCore(pieceTask, "pieces", 6144, nullptr, 3, nullptr, 1);
  micRun(dcfg.listen ? 1 : 0);
  status = dcfg.listen ? ST_LISTEN : ST_PAUSED;
  // the last scene again (C:\dream\last.json)
  FILE* f = fopen("/c/dream/last.json", "r");
  if (f) {
    char* b = (char*)heap_caps_malloc(6000, MALLOC_CAP_SPIRAM);
    size_t n = b ? fread(b, 1, 5999, f) : 0;
    fclose(f);
    if (b && n > 10) { b[n] = 0; put(b, false); } else if (b) heap_caps_free(b);
  }
}
