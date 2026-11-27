// enet.cpp: DREAM-CUBE and error.blue (EAR-CUBE's enet.cpp, with scenes instead of transcripts).
//  - speech to text: each piece (a WAV) goes to POST /api/stt?lang=.. and comes back as {text, lang}
//  - scenes: the words gathered go to POST /api/dream {text, lang, id, code, mind, prev} and come back as a scene
//  - the control page (error.blue/dream): every few seconds POST /api/dream/cube {code, st, last} -> its commands
//    All three on one kept-open connection (no new TLS handshake each time).
//  - updates over WiFi (OTA) with rollback: /dream/flash/firmware.json
//  - the switch back: install ERROR-CUBE (/cube/flash/firmware.json) the same way
// Everything runs in one HTTPS worker task, one request at a time.
#include <Arduino.h>
#include <string>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <Preferences.h>
#include "../ui/ui.h"
#include "../dream/dream.h"
#include "net.h"
#include "certs.h"

static void* psram(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

static int verCmp(const char* a, const char* b) {
  while (*a || *b) {
    int x = atoi(a), y = atoi(b);
    if (x != y) return x < y ? -1 : 1;
    while (*a && *a != '.') a++; while (*b && *b != '.') b++;
    if (*a == '.') a++; if (*b == '.') b++;
  }
  return 0;
}

// ================================================================== the HTTPS worker
enum { J_FW, J_OTA, J_OTAFILE, J_STT, J_SWITCH, J_DREAM, J_LINK, J_N };
struct Job { uint8_t kind; int id; char* a; char* b; };
static QueueHandle_t jobs;
static volatile uint32_t busy = 0;
static bool want(uint8_t k) { return !(busy & (1u << k)); }
static void post(uint8_t k, int id = 0, const char* a = nullptr, const char* b = nullptr) {
  Job j = {k, id, a ? strdup(a) : nullptr, b ? strdup(b) : nullptr};
  if (xQueueSend(jobs, &j, 0) == pdTRUE) busy |= 1u << k; else { free(j.a); free(j.b); }
}

static WiFiClientSecure tls;             // GETs (updates): one at a time, closed after each
static HTTPClient http;
static int lastTls = -1;
static void tlsSetup() {
  if (lastTls == cfg.tls) return;
  http.end(); tls.stop();
  if (cfg.tls) tls.setCACert(ROOT_CAS); else tls.setInsecure();
  lastTls = cfg.tls;
}
static void noteError(WiFiClientSecure& c, int code) {
  char e[64] = "";
  c.lastError(e, sizeof(e));
  snprintf(S.netErr, sizeof(S.netErr), "%s", e[0] ? e : HTTPClient::errorToString(code).c_str());
  // the certificate check failed: go on without it (like ERROR-CUBE)
  String es = e; es.toUpperCase();
  if (cfg.tls && (es.indexOf("X509") >= 0 || es.indexOf("VERIFY") >= 0 || es.indexOf("CERT") >= 0)) { cfg.tls = 0; cfgSave(); }
  c.stop();
}

// GET path, the answer piece by piece into sink(buf, n). Returns the HTTP code (200), or <= 0.
static int getStream(const char* path, std::function<bool(const uint8_t*, size_t)> sink, int* lenOut, size_t* got, int timeout = 20000) {
  tlsSetup();
  http.end();
  http.useHTTP10(true);
  *got = 0;
  int code = -1;
  if (http.begin(tls, String("https://") + CUBE_HOST + path)) {
    http.setConnectTimeout(8000);
    http.setTimeout(timeout);
    http.setUserAgent(FW_NAME "/" CUBE_VERSION);
    code = http.GET();
    if (code == 200) {
      int len = http.getSize();
      if (lenOut) *lenOut = len;
      WiFiClient* st = http.getStreamPtr();
      static uint8_t* buf = nullptr;
      if (!buf) buf = (uint8_t*)malloc(4096);
      uint32_t last = millis();
      bool ok = true;
      while (st && buf && ok && (len <= 0 || *got < (size_t)len)) {
        int a = st->available();
        int n = a > 0 ? st->read(buf, a > 4096 ? 4096 : a) : 0;
        if (n > 0) { ok = sink(buf, n); *got += n; last = millis(); vTaskDelay(1); continue; }
        if (!st->connected() && st->available() <= 0) break;
        if ((int)(millis() - last) > timeout) break;
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      if (len > 0 && *got != (size_t)len) code = -100;
    } else if (code <= 0) noteError(tls, code);
    http.end();
  }
  http.useHTTP10(false);
  if (code == 200) S.netErr[0] = 0;
  return code;
}
static int getBody(const char* path, char** out, size_t* n) {
  size_t cap = 8192, used = 0;
  char* b = (char*)psram(cap);
  if (!b) return -1;
  size_t got; int len = -1;
  int code = getStream(path, [&](const uint8_t* d, size_t k) {
    if (used + k + 1 > cap) {
      size_t nc = cap * 2; while (nc < used + k + 1) nc *= 2;
      if (nc > 256 * 1024) return false;
      char* nb = (char*)heap_caps_realloc(b, nc, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!nb) return false;
      b = nb; cap = nc;
    }
    memcpy(b + used, d, k); used += k; return true;
  }, &len, &got, 9000);
  b[used] = 0;
  if (code != 200) { free(b); return code; }
  *out = b; if (n) *n = used;
  return code;
}

// ================================================================== speech to text: POST the WAV, read {text}
static WiFiClientSecure stls;            // its own connection, kept open between pieces
static int sttTls = -1;
static uint32_t sttMs = 0;
uint32_t enetSttMs() { return sttMs; }
static bool writeAll(WiFiClientSecure& c, const uint8_t* p, size_t n) {
  size_t o = 0; uint32_t last = millis();
  while (o < n) {
    size_t k = n - o > 4096 ? 4096 : n - o;
    size_t w = c.write(p + o, k);
    if (w > 0) { o += w; last = millis(); continue; }
    if (!c.connected() || millis() - last > 15000) return false;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  return true;
}
// one line of the answer's head ("" at the empty line); false = timed out / closed
static bool readLine(WiFiClientSecure& c, char* out, size_t n, uint32_t until) {
  size_t k = 0;
  while ((int32_t)(until - millis()) > 0) {
    int ch = c.available() > 0 ? c.read() : -1;
    if (ch < 0) { if (!c.connected()) return false; vTaskDelay(pdMS_TO_TICKS(5)); continue; }
    if (ch == '\n') { while (k && out[k - 1] == '\r') k--; out[k] = 0; return true; }
    if (k + 1 < n) out[k++] = (char)ch;
  }
  return false;
}
// `want` bytes of the body: what fits into out (room, 0-terminated) is kept; returns how many were kept
static size_t readN(WiFiClientSecure& c, char* out, size_t room, size_t want, uint32_t until) {
  size_t got = 0, kept = 0;
  while (got < want && (int32_t)(until - millis()) > 0) {
    int a = c.available();
    if (a <= 0) { if (!c.connected()) break; vTaskDelay(pdMS_TO_TICKS(5)); continue; }
    uint8_t tmp[256];
    size_t k = (size_t)a < sizeof(tmp) ? (size_t)a : sizeof(tmp);
    if (k > want - got) k = want - got;
    int r = c.read(tmp, k);
    if (r <= 0) continue;
    for (int i = 0; i < r; i++) if (kept + 1 < room) out[kept++] = (char)tmp[i];
    got += r;
  }
  if (room) out[kept] = 0;
  return kept;
}
// a request on the kept-open connection (POST with a body, or GET with body = nullptr); the answer's body into out.
// Returns the HTTP code, or <= 0 (no connection).
static int req(const char* method, const char* path, const char* ctype, const uint8_t* body, size_t len, char* out, size_t room) {
  if (sttTls != cfg.tls) { stls.stop(); if (cfg.tls) stls.setCACert(ROOT_CAS); else stls.setInsecure(); sttTls = cfg.tls; }
  out[0] = 0;
  for (int tryN = 0; tryN < 2; tryN++) {
    bool fresh = false;
    if (!stls.connected()) {
      stls.stop();
      if (!stls.connect(CUBE_HOST, CUBE_PORT, 10000)) { noteError(stls, -1); return -1; }
      fresh = true;
    }
    char h[420];
    int hn = body ? snprintf(h, sizeof(h), "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: " FW_NAME "/" CUBE_VERSION "\r\n"
                             "Content-Type: %s\r\nContent-Length: %u\r\nConnection: keep-alive\r\n\r\n", method, path, CUBE_HOST, ctype, (unsigned)len)
                  : snprintf(h, sizeof(h), "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: " FW_NAME "/" CUBE_VERSION "\r\n"
                             "Connection: keep-alive\r\n\r\n", method, path, CUBE_HOST);
    if (!writeAll(stls, (const uint8_t*)h, hn) || (body && !writeAll(stls, body, len))) {
      stls.stop();
      if (fresh) return -2;
      continue;                               // the kept connection was closed by the server: once more, new
    }
    uint32_t until = millis() + 45000;        // Whisper takes a few seconds for 25 s of speech
    char line[160];
    if (!readLine(stls, line, sizeof(line), until)) { stls.stop(); if (fresh) return -3; continue; }
    int code = 0;
    if (sscanf(line, "HTTP/%*s %d", &code) != 1) { stls.stop(); return -4; }
    long clen = -1; bool chunked = false, close = false;
    while (readLine(stls, line, sizeof(line), until) && line[0]) {
      if (!strncasecmp(line, "Content-Length:", 15)) clen = atol(line + 15);
      else if (!strncasecmp(line, "Transfer-Encoding:", 18) && strcasestr(line, "chunked")) chunked = true;
      else if (!strncasecmp(line, "Connection:", 11) && strcasestr(line, "close")) close = true;
    }
    if (chunked) {
      size_t used = 0;
      for (;;) {
        if (!readLine(stls, line, sizeof(line), until)) { close = true; break; }
        long n = strtol(line, nullptr, 16);
        if (n <= 0) { readLine(stls, line, sizeof(line), until); break; }
        used += readN(stls, out + used, room - used, n, until);   // keeps used <= room - 1
        readLine(stls, line, sizeof(line), until);   // the CRLF after the chunk
      }
    } else if (clen >= 0) readN(stls, out, room, clen, until);
    else { readN(stls, out, room, room - 1, until); close = true; }
    if (close) stls.stop();
    return code;
  }
  return -5;
}
static char* body = nullptr;
static const size_t BODY = 12288;
static void doStt() {
  SttJob j;
  if (!scribeNext(j)) return;
  if (!body) body = (char*)psram(BODY);
  if (!body) { scribeDone(j.id, -1, nullptr, nullptr, "NO MEMORY"); return; }
  char path[160];
  snprintf(path, sizeof(path), STT_PATH "?task=transcribe&id=%s&v=" CUBE_VERSION "%s%s", S.host, j.lang[0] ? "&lang=" : "", j.lang);
  uint32_t t0 = millis();
  int code = req("POST", path, "audio/wav", j.wav, j.len, body, BODY);
  sttMs = millis() - t0;
  if (code <= 0) { scribeDone(j.id, code, nullptr, nullptr, S.netErr[0] ? S.netErr : "NO CONNECTION"); return; }
  JsonDocument d;
  bool parsed = !deserializeJson(d, body);
  if (code == 200 && !parsed) { scribeDone(j.id, 502, nullptr, nullptr, "BAD ANSWER"); return; }
  char e[48]; snprintf(e, sizeof(e), "%s", parsed ? (const char*)(d["error"] | "") : ""); for (char* p = e; *p; p++) *p = toupper((unsigned char)*p);
  scribeDone(j.id, code, parsed ? (const char*)(d["text"] | "") : "", parsed ? (const char*)(d["lang"] | "") : "", e);
}

// ---------------- scenes
static uint32_t dreamMs = 0;
uint32_t enetDreamMs() { return dreamMs; }
static void doDream() {
  static char text[900], prev[48], lang[4];
  if (!dreamNext(text, sizeof(text), prev, sizeof(prev), lang, sizeof(lang))) return;
  if (!body) body = (char*)psram(BODY);
  if (!body) { dreamDone(-1, nullptr, "NO MEMORY"); return; }
  JsonDocument q;
  q["text"] = text; q["lang"] = lang; q["id"] = S.host; q["prev"] = prev; q["mind"] = dcfg.mind ? "smart" : "quick";
  struct tm tm; if (sysLocalTime(tm)) q["hour"] = tm.tm_hour;     // (3D settings: TIME = THE CLOCK)
  if (dcfg.link) q["code"] = dcfg.code;
  static char out[1400];
  size_t n = serializeJson(q, out, sizeof(out));
  uint32_t t0 = millis();
  int code = req("POST", DREAM_PATH, "application/json", (const uint8_t*)out, n, body, BODY);
  dreamMs = millis() - t0;
  if (code <= 0) { dreamDone(code, nullptr, S.netErr[0] ? S.netErr : "NO CONNECTION"); return; }
  if (code != 200) { char e[32]; snprintf(e, sizeof(e), "ERROR.BLUE: %d", code); dreamDone(code, nullptr, e); return; }
  dreamDone(200, body, nullptr);
}

// ---------------- the control page: how the cube is, what it should do
static uint32_t linkAt = 0, linkTry = 0;
static int32_t lastCmd = -1;
static char lastCode[9] = "";
uint32_t enetLinkAt() { return linkAt; }
static void doLink() {
  if (!dcfg.link || strlen(dcfg.code) != 8) return;
  if (strcmp(lastCode, dcfg.code)) {          // a new code: start counting its commands from the newest
    snprintf(lastCode, sizeof(lastCode), "%s", dcfg.code);
    Preferences p; p.begin("dream", true); lastCmd = p.getInt(dcfg.code, -1); p.end();
  }
  if (!body) body = (char*)psram(BODY);
  static char* out = nullptr; static char *st = nullptr, *ln = nullptr, *pr = nullptr;
  if (!out) { out = (char*)psram(7000); st = (char*)psram(1500); ln = (char*)psram(4200); pr = (char*)psram(300); }
  if (!body || !out || !st || !ln || !pr) return;
  dreamState(st, 1500);
  int nl = dreamLinesJson(ln, 4200), np = dreamPairJson(pr, 300);
  size_t n = snprintf(out, 7000, "{\"code\":\"%s\",\"last\":%d,\"st\":%s,\"lines\":%s,\"pair\":%s}", dcfg.code, (int)(lastCmd < 0 ? 0 : lastCmd), st, ln, pr);
  int code = req("POST", LINK_PATH, "application/json", (const uint8_t*)out, n, body, BODY);
  if (code != 200) return;
  linkAt = millis();
  dreamLinesDone(nl); dreamPairDone(np);
  JsonDocument d;
  if (deserializeJson(d, body)) return;
  int top = d["top"] | 0;
  if (lastCmd < 0) {                          // the first time: what's already there is old
    lastCmd = top;
  } else {
    for (JsonObject c : d["cmds"].as<JsonArray>()) {
      int cn = c["n"] | 0;
      if (cn <= lastCmd) continue;
      dreamCmd(cn, c["c"] | "", c["a"] | "");
      lastCmd = cn;
    }
  }
  static int32_t saved = -2;
  if (saved != lastCmd) { saved = lastCmd; Preferences p; p.begin("dream", false); p.putInt(dcfg.code, lastCmd); p.end(); }
}

// ---------------- firmware: what's newest, and installing it
struct FwInfo { char ver[16], path[64], md5[36]; uint32_t size; };
static FwInfo fw, other;
volatile bool enetSwitching = false;
static bool readFw(const char* infoPath, FwInfo& f, bool notes) {
  char* body = nullptr; size_t n = 0;
  int code = getBody(infoPath, &body, &n);
  S.updCheckedAt = millis();
  if (code != 200 || !body) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "ERROR.BLUE: %d", code); free(body); return false; }
  JsonDocument d;
  bool ok = !deserializeJson(d, body);
  free(body);
  if (!ok) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "BAD ANSWER"); return false; }
  snprintf(f.ver, sizeof(f.ver), "%s", d["ver"] | ""); snprintf(f.path, sizeof(f.path), "%s", d["path"] | "");
  snprintf(f.md5, sizeof(f.md5), "%s", d["md5"] | ""); f.size = d["size"] | 0;
  if (notes) { snprintf(S.updVer, sizeof(S.updVer), "%s", f.ver); snprintf(S.updNotes, sizeof(S.updNotes), "%s", d["notes"] | ""); }
  return true;
}
static void install(const FwInfo& f, const char* what) {
  if (!f.ver[0] || f.path[0] != '/' || f.size < 100000) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "NOTHING TO INSTALL"); return; }
  stls.stop();
  S.upd = UPD_LOADING; S.updPct = 0; snprintf(S.updMsg, sizeof(S.updMsg), "%s %s", what, f.ver);
  const char* err = nullptr;
  if (!Update.begin(f.size)) err = "NO ROOM: INSTALL WITH USB";
  else {
    if (strlen(f.md5) == 32) Update.setMD5(f.md5);
    int len = -1; size_t got = 0; bool wok = true;
    int code = getStream(f.path, [&](const uint8_t* d, size_t k) {
      wok = Update.write((uint8_t*)d, k) == k;
      S.updPct = (int)((got + k) * 100 / f.size);
      return wok;
    }, &len, &got);
    if (code != 200 && code != -100) { err = "THE SERVER SAID NO"; Update.abort(); }
    else if (len > 0 && (uint32_t)len != f.size) { err = "WRONG SIZE"; Update.abort(); }
    else if (!wok || got != f.size) { err = "DOWNLOAD BROKE OFF"; Update.abort(); }
    else if (!Update.end()) err = "CHECK FAILED";
  }
  if (err) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "%s", err); tls.stop(); return; }
  S.upd = UPD_DONE; S.updPct = 100; snprintf(S.updMsg, sizeof(S.updMsg), "DONE. RESTARTING...");
  cfgFlushNow();
  delay(1500);
  sysRestart();
}
static void doOtaFile(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "CAN'T OPEN THE FILE"); return; }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  S.upd = UPD_LOADING; S.updPct = 0; snprintf(S.updMsg, sizeof(S.updMsg), "FROM THE TF CARD");
  const char* err = nullptr;
  uint8_t* buf = (uint8_t*)malloc(4096);
  if (!buf) err = "NO MEMORY";
  else if (!Update.begin(n)) err = "TOO BIG";
  else {
    long done = 0;
    while (done < n) {
      size_t r = fread(buf, 1, 4096, f);
      if (!r || Update.write(buf, r) != r) { err = "COULD NOT WRITE"; break; }
      done += r; S.updPct = done * 100 / n;
      vTaskDelay(1);
    }
    if (!err && !Update.end()) err = "NOT A GOOD FIRMWARE";
    if (err) Update.abort();
  }
  free(buf);
  fclose(f);
  if (!err) { S.upd = UPD_DONE; snprintf(S.updMsg, sizeof(S.updMsg), "DONE. RESTARTING..."); cfgFlushNow(); delay(1500); sysRestart(); }
  S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "%s", err);
}

static void worker(void*) {
  for (;;) {
    Job j;
    if (xQueueReceive(jobs, &j, portMAX_DELAY) != pdTRUE) continue;
    if (S.net != NET_UP && j.kind != J_OTAFILE) {
      if (j.kind == J_FW || j.kind == J_OTA || j.kind == J_SWITCH) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "NO WIFI"); }
      busy &= ~(1u << j.kind); free(j.a); free(j.b); uiDirty(); continue;
    }
    switch (j.kind) {
      case J_FW:
        stls.stop();
        if (readFw(FW_INFO_PATH, fw, true)) S.upd = verCmp(fw.ver, CUBE_VERSION) > 0 ? UPD_NEW : UPD_LATEST;
        break;
      case J_OTA: enetSwitching = false; install(fw, FW_NAME); break;
      case J_OTAFILE: enetSwitching = false; doOtaFile(j.a); break;
      case J_STT: doStt(); break;
      case J_DREAM: doDream(); break;
      case J_LINK: doLink(); break;
      case J_SWITCH:
        enetSwitching = true;
        S.upd = UPD_CHECKING; snprintf(S.updMsg, sizeof(S.updMsg), "LOOKING FOR ERROR-CUBE");
        stls.stop();
        if (readFw(ERROR_CUBE_INFO, other, false)) install(other, "ERROR-CUBE");
        break;
    }
    busy &= ~(1u << j.kind);
    free(j.a); free(j.b);
    uiDirty();
  }
}

// ---------------- what the screens call
void enetSwitch() { if (want(J_SWITCH) && S.upd != UPD_LOADING) { S.upd = UPD_CHECKING; enetSwitching = true; post(J_SWITCH); } }
void netUpdateCheck() { if (want(J_FW) && S.upd != UPD_LOADING) { S.upd = UPD_CHECKING; post(J_FW); } }
void netUpdateInstall() { if (S.upd == UPD_NEW && want(J_OTA)) post(J_OTA); }
bool netInstallFile(const char* path) {
  const char* e = strrchr(path, '.');
  if (!e || strcasecmp(e, ".bin")) return false;
  if (want(J_OTAFILE)) post(J_OTAFILE, 0, path);
  return true;
}
void netDownload(const char*, const char*) { S.dlState = -1; }
// ERROR-CUBE's live link and focus: not in DREAM-CUBE
void netSetFocus(int) {}
void netLinkNewCode() {}
void netLinkState() {}
void netReportProg(const char*) {}
void netChatReset() {}

// ================================================================== timers
static uint32_t fwAt = 0, upAt = 0;
void cloudBegin() {
  jobs = xQueueCreate(8, sizeof(Job));
  xTaskCreatePinnedToCore(worker, "https", 16384, nullptr, 1, nullptr, 0);
}
void cloudMarkGood() { esp_ota_mark_app_valid_cancel_rollback(); }
void cloudStep() {
  uint32_t now = millis();
  if (S.net != NET_UP) { upAt = 0; return; }
  // speech first (the words), then a scene when there are enough
  if (want(J_STT) && scribeWants()) post(J_STT);
  if (want(J_DREAM) && want(J_STT) && dreamWants()) post(J_DREAM);
  // the control page: every 4 s while the screen is on, every 12 s when it's off
  // (at once when it heard something new, or answered "link a phone?")
  uint32_t every = uiAwake() ? 4000 : 12000;
  if (dcfg.link && (dreamLinesWaiting() || dreamPairJson(nullptr, 0) > 0)) every = 1200;
  if (dcfg.link && want(J_LINK) && (!linkTry || now - linkTry > every)) { linkTry = now; post(J_LINK); }
  if (!upAt) { upAt = now; fwAt = now - (UPD_HOURS[cfg.updIdx % N_UPD] * 3600000u) + 30000; }   // first check 30 s after WiFi came
  uint32_t fwEvery = UPD_HOURS[cfg.updIdx % N_UPD] * 3600000u;
  bool quiet = !scribeWaiting() && !dreamThinking();
  // AUTO UPDATE (cfg.autoUpd, as in ERROR-CUBE): a newer DREAM-CUBE installs by itself while the screen is off
  if (cfg.autoUpd && S.upd == UPD_NEW && !enetSwitching && want(J_OTA) && quiet && !uiAwake()) post(J_OTA);
  if (now - fwAt > fwEvery && want(J_FW) && S.upd != UPD_LOADING && quiet) { fwAt = now; post(J_FW); }
}
