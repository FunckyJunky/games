// enet.cpp: EAR-CUBE and error.blue (PICO-CUBE's pnet.cpp, with speech to text instead of carts).
//  - speech to text: each piece (a WAV) goes to POST /api/stt?lang=..&task=.. and comes back as {text}
//    (src/stt.js on error.blue: Whisper large v3 turbo on Cloudflare Workers AI). One connection, kept open
//    between pieces (no new TLS handshake each time)
//  - ERROR-CHAT (1.1): the lines into a channel (POST /api/chat/<CH>/ear/line), a finished transcript whole
//    (POST .../ear/file), and every few seconds: what the channel's buttons want (GET .../ear/cmds, with how the
//    cube is). All on the same kept-open connection as the speech.
//  - updates over WiFi (OTA) with rollback: /ear/flash/firmware.json
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
#include "../ear/ear.h"
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
enum { J_FW, J_OTA, J_OTAFILE, J_STT, J_SWITCH, J_CHAT, J_UP, J_CMDS, J_N };
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
static char* sttBody = nullptr;
static void doStt() {
  SttJob j;
  if (!scribeNext(j)) return;
  if (!sttBody) sttBody = (char*)psram(8192);
  if (!sttBody) { scribeDone(j.id, -1, nullptr, "NO MEMORY"); return; }
  char path[160];
  snprintf(path, sizeof(path), STT_PATH "?task=%s&id=%s&v=" CUBE_VERSION "%s%s", j.task ? "translate" : "transcribe", S.host,
           j.lang[0] ? "&lang=" : "", j.lang);
  uint32_t t0 = millis();
  int code = req("POST", path, "audio/wav", j.wav, j.len, sttBody, 8192);
  sttMs = millis() - t0;
  if (code <= 0) { scribeDone(j.id, code, nullptr, S.netErr[0] ? S.netErr : "NO CONNECTION"); return; }
  JsonDocument d;
  bool parsed = !deserializeJson(d, sttBody);
  const char* text = parsed ? (d["text"] | "") : "";
  const char* err = parsed ? (d["error"] | "") : "";
  if (code == 200 && !parsed) { scribeDone(j.id, 502, nullptr, "BAD ANSWER"); return; }
  char e[48]; snprintf(e, sizeof(e), "%s", err); for (char* p = e; *p; p++) *p = toupper((unsigned char)*p);
  scribeDone(j.id, code, text, e);
}

// ---------------- ERROR-CHAT: the channel
static uint32_t chatRetryAt = 0, cmdsAt = 0, cmdsEvery = 15000;
static int chatFails = 0;
static int32_t lastCmd = -1; static bool cmdLoaded = false;
static char cmdChan[9] = "";
static void chatTrouble(int code) {
  char e[48]; snprintf(e, sizeof(e), code > 0 ? "ERROR-CHAT: %d" : "ERROR-CHAT: NO CONNECTION", code);
  earChatNote(e);
  static const uint32_t BACK[4] = {3000, 10000, 30000, 60000};
  chatRetryAt = millis() + BACK[chatFails < 4 ? chatFails : 3]; chatFails++;
}
static void doChat() {
  ChatLine l;
  char nick[17]; earNick(nick, sizeof(nick));
  for (int k = 0; k < 8 && chatNext(l); k++) {          // a few at a time, then the speech goes first again
    JsonDocument d;
    d["cube"] = S.host; d["nick"] = nick; d["file"] = l.file; d["at"] = l.at; d["late"] = l.late != 0; d["text"] = l.text;
    static char body[1200]; size_t n = serializeJson(d, body, sizeof(body));
    char path[64]; snprintf(path, sizeof(path), "/api/chat/%s/ear/line", ecfg.chatChan);
    char out[256];
    int code = req("POST", path, "application/json", (const uint8_t*)body, n, out, sizeof(out));
    if (code == 200 || (code >= 400 && code < 500 && code != 429)) { chatDone(); chatFails = 0; earChatNote(code == 200 ? "" : out); continue; }
    chatTrouble(code); return;
  }
}
static void doUp() {
  char path[96];
  if (!upNext(path, sizeof(path))) return;
  long n = fsSize(path);
  if (n < 0 || n > 1400 * 1024) { upDone(path); return; }    // gone, or too big for the channel
  char* b = (char*)psram(n + 1);
  FILE* f = b ? fopen(path, "rb") : nullptr;
  size_t got = f ? fread(b, 1, n, f) : 0;
  if (f) fclose(f);
  if (!b || got != (size_t)n) { free(b); upDone(path); return; }
  const char* name = strrchr(path, '/'); name = name ? name + 1 : path;
  char url[160]; snprintf(url, sizeof(url), "/api/chat/%s/ear/file?cube=%s&file=%s", ecfg.chatChan, S.host, name);
  char out[256];
  int code = req("POST", url, "text/plain; charset=utf-8", (const uint8_t*)b, n, out, sizeof(out));
  free(b);
  if (code == 200 || (code >= 400 && code < 500 && code != 429)) { upDone(path); chatFails = 0; earChatNote(code == 200 ? "" : out); }
  else chatTrouble(code);
}
static void doCmds() {
  // the last command done is kept (a restart doesn't do it twice); a new channel starts from its newest
  if (!cmdLoaded || strcmp(cmdChan, ecfg.chatChan)) {
    Preferences p; p.begin("ear", true);
    String ch = p.getString("cmdch", ""); int32_t id = p.getInt("cmdid", -1); p.end();
    lastCmd = ch == ecfg.chatChan ? id : -1;
    snprintf(cmdChan, sizeof(cmdChan), "%s", ecfg.chatChan); cmdLoaded = true;
  }
  char nick[17]; earNick(nick, sizeof(nick));
  const char* f = earFile(); const char* fb = strrchr(f, '/'); fb = fb ? fb + 1 : f;
  int m = earMode();
  char url[360];
  snprintf(url, sizeof(url), "/api/chat/%s/ear/cmds?cube=%s&nick=%s&after=%d&mode=%s&paused=%d&secs=%u&words=%u&wait=%d&file=%s&scr=%d&lock=%d&bat=%d&v=" CUBE_VERSION "&net=",
           ecfg.chatChan, S.host, nick, (int)lastCmd, m == EAR_REC ? "rec" : m == EAR_LIVE ? "live" : "idle", earPaused() ? 1 : 0,
           (unsigned)earSecs(), (unsigned)earWords(), earWaiting(), fb, uiAwake() ? 1 : 0, uiLocked ? 1 : 0, S.battery ? S.batPct : -1);
  // the WiFi name can have any letters: as %XX
  String u = url;
  for (const char* p = S.ssid; *p; p++) { char c = *p; if (isalnum((unsigned char)c) || c == '-' || c == '_') u += c; else { char h[4]; snprintf(h, sizeof(h), "%%%02X", (uint8_t)c); u += h; } }
  char out[2048];
  int code = req("GET", u.c_str(), nullptr, nullptr, 0, out, sizeof(out));
  if (code != 200) { if (code <= 0 || code >= 500) chatTrouble(code); return; }
  JsonDocument d;
  if (deserializeJson(d, out)) return;
  cmdsEvery = d["next"] | 15000; if (cmdsEvery < 2000) cmdsEvery = 2000; if (cmdsEvery > 60000) cmdsEvery = 60000;
  int32_t last = d["last"] | 0, was = lastCmd;
  if (lastCmd < 0) lastCmd = last;
  for (JsonObject c : d["cmds"].as<JsonArray>()) {
    int32_t id = c["id"] | 0;
    if (id <= lastCmd && was >= 0) continue;
    earRemote(c["cmd"] | "", c["name"] | "");
    lastCmd = id;
    cmdsAt = 0;                                         // something happened: ask again soon (the reply shows it)
  }
  if (lastCmd != was) { Preferences p; p.begin("ear", false); p.putString("cmdch", ecfg.chatChan); p.putInt("cmdid", lastCmd); p.end(); }
  earChatNote("");
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
  if (earMode() != EAR_IDLE) { earWant(EAR_IDLE); vTaskDelay(pdMS_TO_TICKS(1500)); }   // a recording ends first
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
      case J_CHAT: doChat(); break;
      case J_UP: doUp(); break;
      case J_CMDS: doCmds(); break;
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
// ERROR-CUBE's live link and focus: not in EAR-CUBE
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
  // the next piece as soon as the last one is back
  if (want(J_STT) && scribeWants()) post(J_STT);
  // ERROR-CHAT: lines, whole transcripts, what its buttons want
  if (ecfg.chatOn && chanOk(ecfg.chatChan)) {
    bool ok = (int32_t)(now - chatRetryAt) >= 0;
    if (ok && want(J_CHAT) && chatWaiting()) post(J_CHAT);
    if (ok && want(J_UP) && want(J_CHAT) && !chatWaiting()) { char p[96]; if (upNext(p, sizeof(p))) post(J_UP); }
    if (ok && want(J_CMDS) && (!cmdsAt || now - cmdsAt > cmdsEvery)) { cmdsAt = now; post(J_CMDS); }
  }
  if (!upAt) { upAt = now; fwAt = now - (UPD_HOURS[cfg.updIdx % N_UPD] * 3600000u) + 30000; }   // first check 30 s after WiFi came
  uint32_t every = UPD_HOURS[cfg.updIdx % N_UPD] * 3600000u;
  // AUTO UPDATE (cfg.autoUpd, as in ERROR-CUBE): a newer EAR-CUBE installs by itself while the cube rests
  // (nothing recorded or waiting, the screen off)
  if (cfg.autoUpd && S.upd == UPD_NEW && !enetSwitching && want(J_OTA) && earMode() == EAR_IDLE && !earWaiting() && !uiAwake()) post(J_OTA);
  // updates only while it isn't listening (the check closes the speech connection)
  if (now - fwAt > every && want(J_FW) && S.upd != UPD_LOADING && earMode() == EAR_IDLE && !earWaiting()) { fwAt = now; post(J_FW); }
}
