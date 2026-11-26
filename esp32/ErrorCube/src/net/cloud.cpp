// cloud.cpp: everything with error.blue.
//  - HTTPS worker (own task): who's online, ERROR-CHAT, "I'm online", firmware.json, the web app list and files
//  - the live link: WebSocket room /api/room/error-cube/<CODE>, the control page error.blue/cube talks to the cube
//    through it (settings, messages, video + sound, open a web app, screen preview)
//  - updates over WiFi (OTA) with rollback: a new firmware that doesn't come up is replaced by the old one
// The link protocol is in public/cube/README.md.
#include <Arduino.h>
#include <string>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
#include "../ui/ui.h"
#include "../board/sound.h"
#include "net.h"
#include "certs.h"
extern "C" {
#include "esp32s3/rom/tjpgd.h"
}

static SemaphoreHandle_t lock;
#define LOCK() xSemaphoreTake(lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(lock)
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
static String jsonStr(const char* s) { JsonDocument d; d.set(s); String o; serializeJson(d, o); return o; }
static String urlEnc(const char* s) {
  String o;
  for (; *s; s++) { uint8_t c = *s; if (isalnum(c) || c == '-' || c == '_' || c == '.') o += (char)c; else { char b[4]; snprintf(b, 4, "%%%02X", c); o += b; } }
  return o;
}

// ================================================================== the HTTPS worker
enum { J_ONLINE, J_WHO, J_CHAT, J_SAY, J_FW, J_APPS, J_APPGET, J_FETCH, J_OTA, J_OTAFILE, J_DL, J_OTHER, J_N };
struct Job { uint8_t kind; int id; char* a; char* b; };
static QueueHandle_t jobs;
static volatile uint32_t busy = 0;
static bool want(uint8_t k) { return !(busy & (1u << k)); }
static void post(uint8_t k, int id = 0, const char* a = nullptr, const char* b = nullptr) {
  Job j = {k, id, a ? strdup(a) : nullptr, b ? strdup(b) : nullptr};
  if (xQueueSend(jobs, &j, 0) == pdTRUE) busy |= 1u << k; else { free(j.a); free(j.b); }
}

static WiFiClientSecure tls;
static HTTPClient http;
static int lastTls = -1;
// Internal RAM and TLS: mbedTLS keeps a 16 KB in and a 16 KB out buffer per connection, always in internal RAM.
// Two connections (these requests + the live link) only fit with room to spare, so: one handshake at a time
// (tlsMx), the requests' kept-alive connection is closed when RAM is short, and when a request that matters
// (updates...) can't fit next to the link, the link steps aside for a moment (linkYield) and comes back after.
static SemaphoreHandle_t tlsMx;
static volatile bool linkYield = false;
static bool wsUp = false;
static uint32_t ramFree() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
static uint32_t ramBig() { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
static bool tlsRoom() { return ramBig() > 18500 && ramFree() > 50000; }
static void linkStepAside() {
  if (!wsUp) return;
  linkYield = true;
  for (int i = 0; i < 60 && wsUp; i++) delay(50);       // the net task drops it at its next step
}
static void tlsSetup() {
  if (lastTls == cfg.tls) return;
  http.end(); tls.stop();
  if (cfg.tls) tls.setCACert(ROOT_CAS); else tls.setInsecure();
  lastTls = cfg.tls;
}
// one request; body (malloc'd) in *out
static int requestRaw(const char* method, const String& path, const char* body, char** out, size_t* outLen, int timeout) {
  tlsSetup();
  String url = String("https://") + CUBE_HOST + path;
  if (!http.begin(tls, url)) return -1;
  http.setReuse(true);
  http.setConnectTimeout(8000);
  http.setTimeout(timeout);
  http.setUserAgent(FW_NAME "/" CUBE_VERSION);
  int code;
  if (!strcmp(method, "POST")) { http.addHeader("Content-Type", "application/json"); code = http.POST((uint8_t*)body, strlen(body)); }
  else code = http.GET();
  if (code > 0 && out) {
    String s = http.getString();
    *out = (char*)psram(s.length() + 1);
    if (*out) { memcpy(*out, s.c_str(), s.length() + 1); if (outLen) *outLen = s.length(); }
  }
  if (code <= 0) {
    char e[64] = "";
    tls.lastError(e, sizeof(e));
    snprintf(S.netErr, sizeof(S.netErr), "%s", e[0] ? e : HTTPClient::errorToString(code).c_str());
    // the certificate check failed once: go on without it (like HOME FRONT)
    String es = e; es.toUpperCase();
    if (cfg.tls && (es.indexOf("X509") >= 0 || es.indexOf("VERIFY") >= 0 || es.indexOf("CERT") >= 0)) { cfg.tls = 0; cfgSave(); }
    tls.stop();
  } else if (code == 200) S.netErr[0] = 0;
  http.end();
  return code;
}
// must: updates, apps, what you asked for. Background ones (I'm online, chat in the background) wait for a
// better moment instead of pushing the link aside.
static int request(const char* method, const String& path, const char* body, char** out, size_t* outLen, int timeout = 9000, bool must = true) {
  xSemaphoreTake(tlsMx, portMAX_DELAY);
  if (!tls.connected() && !tlsRoom()) {
    if (!must && wsUp) { xSemaphoreGive(tlsMx); return -2; }
    linkStepAside();
  }
  int code = requestRaw(method, path, body, out, outLen, timeout);
  // keep the connection for the next request only while there's room for the link's next to it
  if (tls.connected() && (ramFree() < 70000 || ramBig() < 20000)) { http.end(); tls.stop(); }
  linkYield = false;
  xSemaphoreGive(tlsMx);
  return code;
}

// ---------------- results
// the big things live in PSRAM (internal RAM is for WiFi and TLS)
static WhoList& who = *(WhoList*)heap_caps_calloc(1, sizeof(WhoList), MALLOC_CAP_SPIRAM); static bool whoHave = false;
static ChatLog& chat = *(ChatLog*)heap_caps_calloc(1, sizeof(ChatLog), MALLOC_CAP_SPIRAM); static bool chatHave = false; static uint32_t chatLast = 0; static bool chatFresh = true;
static AppInfo* apps = (AppInfo*)heap_caps_calloc(32, sizeof(AppInfo), MALLOC_CAP_SPIRAM); static int nApps = -1; static uint32_t appsAt = 0;
struct AppDl { char id[24]; int st; };
static AppDl dls[8];
struct Fetched { int id, status; char* body; };
static QueueHandle_t fetched, appMsgs;
struct FwInfo { char ver[16], path[64], md5[36]; uint32_t size; } fw;
static uint32_t otaFailedAt = 0;
static char otaFile[96];

static void parseWho(const char* s) {
  JsonDocument d;
  if (deserializeJson(d, s)) return;
  static WhoList* wp = (WhoList*)psram(sizeof(WhoList)); WhoList& w = *wp; w = WhoList(); w.ok = true; w.at = millis(); w.total = d["total"] | 0;
  for (JsonObject g : d["games"].as<JsonArray>()) {
    if (w.ng >= 14) break;
    WhoGame& o = w.g[w.ng++];
    snprintf(o.title, sizeof(o.title), "%s", g["title"] | "?");
    snprintf(o.dos, sizeof(o.dos), "%s", g["dos"] | "?");
    o.n = g["n"] | 0;
    String pl;
    for (JsonObject p : g["players"].as<JsonArray>()) { if (pl.length()) pl += ", "; pl += (const char*)(p["name"] | "?"); }
    snprintf(o.players, sizeof(o.players), "%s", pl.c_str());
  }
  LOCK(); who = w; whoHave = true; UNLOCK();
}
static void parseChat(const char* s, bool bg) {
  JsonDocument d;
  if (deserializeJson(d, s)) return;
  LOCK();
  if (chatFresh) { chat = ChatLog(); chatFresh = false; }
  chat.ok = true;
  int added = 0;
  for (JsonObject m : d["msgs"].as<JsonArray>()) {
    uint32_t id = m["id"] | 0;
    if (id <= chatLast) continue;
    chatLast = id;
    if (chat.n == 40) { memmove(&chat.m[0], &chat.m[1], sizeof(ChatMsg) * 39); chat.n--; }
    ChatMsg& c = chat.m[chat.n++];
    c.id = id;
    const char* kind = m["kind"] | "msg";
    snprintf(c.name, sizeof(c.name), "%s", m["name"] | "?");
    c.sys = strcmp(kind, "msg") != 0;
    if (c.sys) snprintf(c.text, sizeof(c.text), "%s %s", c.name, !strcmp(kind, "join") ? "joined" : !strcmp(kind, "leave") ? "left" : (const char*)(m["text"] | kind));
    else snprintf(c.text, sizeof(c.text), "%s", m["text"] | "");
    c.me = !strcmp(c.name, cfg.name);
    if (!c.sys && !c.me) { added++; snprintf(S.lastFrom, sizeof(S.lastFrom), "%s", c.name); snprintf(S.lastText, sizeof(S.lastText), "%s", c.text); }
  }
  String w;
  for (JsonVariant v : d["who"].as<JsonArray>()) { if (w.length()) w += " "; w += (const char*)(v | ""); }
  snprintf(chat.who, sizeof(chat.who), "%s", w.c_str());
  chat.at = millis();
  chatHave = true;
  UNLOCK();
  if (bg && added) { S.chatUnread += added; if (cfg.chatBeep) snd(SND_MSG); }
}
static void parseApps(const char* s) {
  JsonDocument d;
  if (deserializeJson(d, s)) { nApps = -2; return; }
  LOCK();
  nApps = 0;
  for (JsonObject a : d["apps"].as<JsonArray>()) {
    if (nApps >= 32) break;
    AppInfo& o = apps[nApps++];
    snprintf(o.id, sizeof(o.id), "%s", a["id"] | "");
    snprintf(o.name, sizeof(o.name), "%s", a["name"] | o.id);
    snprintf(o.info, sizeof(o.info), "%s", a["info"] | "");
    snprintf(o.file, sizeof(o.file), "%s", a["file"] | "");
    snprintf(o.v, sizeof(o.v), "%s", a["v"] | "1");
    const char* c = a["color"] | "#39ff7a";
    o.color = strtoul(c[0] == '#' ? c + 1 : c, nullptr, 16);
  }
  appsAt = millis();
  UNLOCK();
}
static AppDl* dl(const char* id, bool make) {
  for (auto& d : dls) if (!strcmp(d.id, id)) return &d;
  if (!make) return nullptr;
  for (auto& d : dls) if (!d.id[0]) { snprintf(d.id, sizeof(d.id), "%s", id); d.st = 0; return &d; }
  dls[0].id[0] = 0; snprintf(dls[0].id, sizeof(dls[0].id), "%s", id); dls[0].st = 0; return &dls[0];
}
static bool appInfo(const char* id, AppInfo& out) {
  LOCK();
  bool f = false;
  for (int i = 0; i < nApps; i++) if (!strcmp(apps[i].id, id)) { out = apps[i]; f = true; }
  UNLOCK();
  return f;
}

// ---------------- OTA
// a download into a file (maps from error.blue...): j.a = the path on error.blue, j.b = the file
class FileSink : public Stream {
 public:
  FILE* f; size_t total, done = 0; bool ok = true;
  FileSink(FILE* fp, size_t t) : f(fp), total(t) {}
  size_t write(const uint8_t* b, size_t n) override {
    size_t w = fwrite(b, 1, n, f); if (w != n) ok = false;
    done += w; S.dlPct = total ? (int)(done * 100 / total) : (int)(done >> 10);
    return w;
  }
  size_t write(uint8_t c) override { return write(&c, 1); }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};
static void doDownload(const char* path, const char* file) {
  S.dlState = 1; S.dlPct = 0;
  char tmp[120]; snprintf(tmp, sizeof(tmp), "%s.part", file);
  if (!strncmp(file, "/sd", 3)) sdMount();
  fsMkdir(std::string(file).substr(0, std::string(file).rfind('/')).c_str());
  FILE* f = fopen(tmp, "wb");
  if (!f) { S.dlState = -1; return; }
  xSemaphoreTake(tlsMx, portMAX_DELAY);
  if (!tls.connected() && !tlsRoom()) linkStepAside();
  tlsSetup();
  bool ok = false;
  http.end();
  // HTTP/1.0: no chunks, the server closes at the end. Our own read loop (1.5.1): HTTPClient's writeToStream can
  // spin without ever sleeping (delay(0)) when the TLS client says bytes are there but gives none: the idle task
  // starved and the watchdog restarted the cube ("FROZE IN https" after GET on a map)
  http.useHTTP10(true);
  if (http.begin(tls, String("https://") + CUBE_HOST + path)) {
    http.setTimeout(20000);
    int code = http.GET();
    if (code == 200) {
      int len = http.getSize();
      FileSink sink(f, len > 0 ? len : 0);
      WiFiClient* st = http.getStreamPtr();
      static uint8_t* buf = nullptr;
      if (!buf) buf = (uint8_t*)malloc(4096);
      uint32_t last = millis();
      while (st && buf && sink.ok && (len <= 0 || sink.done < (size_t)len)) {
        int a = st->available();
        int n = a > 0 ? st->read(buf, a > 4096 ? 4096 : a) : 0;
        if (n > 0) { sink.write(buf, n); last = millis(); vTaskDelay(1); continue; }
        if (!st->connected() && st->available() <= 0) break;     // the end (or it broke off)
        if (millis() - last > 15000) break;                        // nothing for 15 s
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      ok = sink.ok && sink.done > 0 && (len <= 0 || sink.done == (size_t)len);
    }
    http.end();
  }
  http.useHTTP10(false);
  if (!ok) tls.stop();
  linkYield = false;
  xSemaphoreGive(tlsMx);
  fclose(f);
  if (ok) { remove(file); ok = rename(tmp, file) == 0; } else remove(tmp);
  S.dlState = ok ? 2 : -1;
  uiDirty();
}
void netDownload(const char* path, const char* file) { if (want(J_DL)) { S.dlState = 1; S.dlPct = 0; post(J_DL, 0, path, file); } }

class OtaSink : public Stream {
 public:
  size_t total, done = 0;
  explicit OtaSink(size_t t) : total(t) {}
  size_t write(const uint8_t* b, size_t n) override {
    size_t w = Update.write((uint8_t*)b, n);
    done += w; S.updPct = total ? (int)(done * 100 / total) : 0;
    return w;
  }
  size_t write(uint8_t c) override { return write(&c, 1); }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};
static void linkDrop();
// install a firmware from error.blue: this one's update (fw), or the other firmware for the cube (PICO-CUBE)
static void doOta(const FwInfo& f, const char* what) {
  if (!f.ver[0] || f.path[0] != '/' || f.size < 100000) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "NOTHING TO INSTALL"); return; }
  S.upd = UPD_LOADING; S.updPct = 0;
  if (what) snprintf(S.updMsg, sizeof(S.updMsg), "%s %s", what, f.ver); else snprintf(S.updMsg, sizeof(S.updMsg), "%s > %s", CUBE_VERSION, f.ver);
  xSemaphoreTake(tlsMx, portMAX_DELAY);
  linkStepAside();                                     // all the RAM for the download
  const char* err = nullptr;
  tlsSetup();
  String url = String("https://") + CUBE_HOST + f.path;
  http.end();
  http.useHTTP10(true);                                // no chunks; our own read loop that sleeps (1.5.3, as downloads)
  if (!http.begin(tls, url)) err = "NO CONNECTION";
  else {
    http.setTimeout(20000);
    int code = http.GET();
    int len = http.getSize();
    if (code != 200) err = "THE SERVER SAID NO";
    else if (len > 0 && (uint32_t)len != f.size) err = "WRONG SIZE";
    else if (!Update.begin(f.size)) err = "NO ROOM: INSTALL WITH USB";
    else {
      if (strlen(f.md5) == 32) Update.setMD5(f.md5);
      OtaSink sink(f.size);
      WiFiClient* st = http.getStreamPtr();
      static uint8_t* buf = nullptr;
      if (!buf) buf = (uint8_t*)malloc(4096);
      uint32_t last = millis();
      bool wok = true;
      while (st && buf && wok && sink.done < f.size) {
        int a = st->available();
        int n = a > 0 ? st->read(buf, a > 4096 ? 4096 : a) : 0;
        if (n > 0) { wok = sink.write(buf, n) == (size_t)n; last = millis(); vTaskDelay(1); continue; }
        if (!st->connected() && st->available() <= 0) break;
        if (millis() - last > 20000) break;
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      if (!wok || sink.done != f.size) { err = "DOWNLOAD BROKE OFF"; Update.abort(); }
      else if (!Update.end()) err = "CHECK FAILED";
    }
    http.end();
  }
  http.useHTTP10(false);
  if (err) { char e[64] = ""; tls.lastError(e, sizeof(e)); if (e[0]) snprintf(S.netErr, sizeof(S.netErr), "%s", e); tls.stop(); }
  linkYield = false;
  xSemaphoreGive(tlsMx);
  if (!err) {
    S.upd = UPD_DONE; S.updPct = 100; snprintf(S.updMsg, sizeof(S.updMsg), "DONE. RESTARTING...");
    cfgFlushNow();
    delay(1500);
    sysRestart();
  }
  S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "%s", err); otaFailedAt = millis();
}
static void doOtaFile(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "CAN'T OPEN THE FILE"); return; }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  S.upd = UPD_LOADING; S.updPct = 0; snprintf(S.updMsg, sizeof(S.updMsg), "FROM THE TF CARD");
  linkStepAside(); linkYield = false;
  const char* err = nullptr;
  if (!Update.begin(n)) err = "TOO BIG";
  else {
    uint8_t* buf = (uint8_t*)psram(4096);
    long done = 0;
    if (!buf) err = "NO MEMORY";
    while (buf && done < n) {
      size_t r = fread(buf, 1, 4096, f);          // 1.5.3: was sizeof(buf), 4 bytes at a time
      if (!r || Update.write(buf, r) != r) { err = "COULD NOT WRITE"; break; }
      done += r; S.updPct = done * 100 / n;
    }
    free(buf);
    if (!err && !Update.end()) err = "NOT A GOOD FIRMWARE";
    if (err) Update.abort();
  }
  fclose(f);
  if (!err) { S.upd = UPD_DONE; snprintf(S.updMsg, sizeof(S.updMsg), "DONE. RESTARTING..."); delay(1500); sysRestart(); }
  S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "%s", err);
}

static void worker(void*) {
  for (;;) {
    Job j;
    if (xQueueReceive(jobs, &j, portMAX_DELAY) != pdTRUE) continue;
    if (S.net != NET_UP && j.kind == J_DL) S.dlState = -1;
    if (S.net != NET_UP && j.kind != J_OTAFILE) {
      if (j.kind == J_APPGET) { LOCK(); if (AppDl* d = dl(j.a, true)) d->st = -1; UNLOCK(); }
      if (j.kind == J_APPS && nApps == -1) nApps = -2;
      if (j.kind == J_FW || j.kind == J_OTHER) S.upd = UPD_FAIL, snprintf(S.updMsg, sizeof(S.updMsg), "NO WIFI");
      if (j.kind == J_FETCH) { Fetched r = {j.id, 0, strdup("")}; xQueueSend(fetched, &r, 0); }
      busy &= ~(1u << j.kind); free(j.a); free(j.b); continue;
    }
    char* body = nullptr; size_t n = 0; int code;
    switch (j.kind) {
      case J_ONLINE: request("POST", "/api/online", j.b, nullptr, nullptr, 9000, false); break;
      case J_WHO: if (request("GET", "/api/online", nullptr, &body, &n) == 200 && body) parseWho(body); break;
      case J_CHAT: {
        bool bg = j.id;
        String p = String("/api/chat/") + cfg.chan + "/log?limit=30&after=" + chatLast + "&name=" + urlEnc(cfg.name) + (bg ? "&bg=1" : "");
        if (request("GET", p, nullptr, &body, &n, 9000, !bg) == 200 && body) parseChat(body, bg);
        else { LOCK(); chat.ok = chatHave; UNLOCK(); }
        break;
      }
      case J_SAY: {
        String b = String("{\"name\":") + jsonStr(cfg.name) + ",\"text\":" + jsonStr(j.a) + "}";
        request("POST", String("/api/chat/") + cfg.chan + "/say", b.c_str(), nullptr, nullptr);
        String p = String("/api/chat/") + cfg.chan + "/log?limit=30&after=" + chatLast + "&name=" + urlEnc(cfg.name);
        if (request("GET", p, nullptr, &body, &n) == 200 && body) parseChat(body, false);
        break;
      }
      case J_OTHER: {     // PICO-CUBE: look it up, install it
        S.upd = UPD_CHECKING; snprintf(S.updMsg, sizeof(S.updMsg), "LOOKING FOR PICO-CUBE");
        code = request("GET", PICO_CUBE_INFO, nullptr, &body, &n);
        FwInfo o = {};
        JsonDocument d;
        if (code != 200 || !body) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "ERROR.BLUE: %d", code); break; }
        if (deserializeJson(d, body)) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "BAD ANSWER"); break; }
        snprintf(o.ver, sizeof(o.ver), "%s", d["ver"] | ""); snprintf(o.path, sizeof(o.path), "%s", d["path"] | "");
        snprintf(o.md5, sizeof(o.md5), "%s", d["md5"] | ""); o.size = d["size"] | 0;
        free(body); body = nullptr;
        doOta(o, "PICO-CUBE");
        break;
      }
      case J_FW:
        code = request("GET", FW_INFO_PATH, nullptr, &body, &n);
        S.updCheckedAt = millis();
        if (code == 200 && body) {
          JsonDocument d;
          if (!deserializeJson(d, body)) {
            snprintf(fw.ver, sizeof(fw.ver), "%s", d["ver"] | ""); snprintf(fw.path, sizeof(fw.path), "%s", d["path"] | "");
            snprintf(fw.md5, sizeof(fw.md5), "%s", d["md5"] | ""); fw.size = d["size"] | 0;
            snprintf(S.updVer, sizeof(S.updVer), "%s", fw.ver); snprintf(S.updNotes, sizeof(S.updNotes), "%s", d["notes"] | "");
            S.upd = verCmp(fw.ver, CUBE_VERSION) > 0 ? UPD_NEW : UPD_LATEST;
          } else { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "BAD ANSWER"); }
        } else { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "ERROR.BLUE: %d", code); }
        break;
      case J_APPS:
        if (request("GET", "/cube/apps/apps.json", nullptr, &body, &n) == 200 && body) parseApps(body);
        else if (nApps < 0) nApps = -2;
        break;
      case J_APPGET: {
        AppInfo a; int st = -1;
        if (appInfo(j.a, a) && a.file[0]) {
          String p = String("/cube/apps/") + a.file + "?v=" + a.v;
          if (request("GET", p, nullptr, &body, &n, 15000) == 200 && body) {
            char f[64]; snprintf(f, sizeof(f), "/c/apps/%s.js", a.id);
            char vf[64]; snprintf(vf, sizeof(vf), "/c/apps/%s.v", a.id);
            if (fsWrite(f, (uint8_t*)body, n) && fsWrite(vf, (uint8_t*)a.v, strlen(a.v))) st = 2;
          }
        }
        LOCK(); if (AppDl* d = dl(j.a, true)) d->st = st; UNLOCK();
        break;
      }
      case J_FETCH: {
        String p = j.a; if (!p.startsWith("/")) p = "/" + p;
        code = request("GET", p, nullptr, &body, &n);
        Fetched r = {j.id, code, body ? strdup(body) : strdup("")};
        if (xQueueSend(fetched, &r, 0) != pdTRUE) free(r.body);
        break;
      }
      case J_OTA: doOta(fw, nullptr); break;
      case J_OTAFILE: doOtaFile(j.a); break;
      case J_DL: doDownload(j.a, j.b); break;
    }
    if (body) free(body);
    busy &= ~(1u << j.kind);
    free(j.a); free(j.b);
  }
}

// ---------------- what the interface calls
static int focus = FOCUS_NONE;
void netSetFocus(int f) { focus = f; }
bool netWho(WhoList& out) { LOCK(); bool h = whoHave; if (h) out = who; UNLOCK(); if (!h) { out = WhoList(); out.ok = S.net == NET_UP; } return true; }
bool netChat(ChatLog& out) { LOCK(); out = chat; out.ok = chatHave; UNLOCK(); return true; }
void netChatSay(const char* t) { post(J_SAY, 0, t); }
void netChatReset() { LOCK(); chat = ChatLog(); chatHave = false; chatLast = 0; chatFresh = true; UNLOCK(); }
void netUpdateCheck() { if (want(J_FW)) { S.upd = UPD_CHECKING; post(J_FW); } }
void netUpdateInstall() { if (S.upd == UPD_NEW && want(J_OTA)) post(J_OTA); }
void netInstallPico() { if (want(J_OTHER) && S.upd != UPD_LOADING) { S.upd = UPD_CHECKING; post(J_OTHER); } }
bool netInstallFile(const char* path) {
  const char* e = strrchr(path, '.');
  if (!e || strcasecmp(e, ".bin")) return false;
  if (want(J_OTAFILE)) post(J_OTAFILE, 0, path);
  return true;
}
int netApps(AppInfo* out, int max) {
  if (nApps == -1 && want(J_APPS)) post(J_APPS);
  LOCK(); int n = nApps; if (n > max) n = max; if (n > 0) memcpy(out, apps, sizeof(AppInfo) * n); UNLOCK();
  return n;
}
void netAppsRefresh() { if (millis() - appsAt > 60000 && want(J_APPS)) post(J_APPS); }
int netAppGet(const char* id) {
  LOCK(); AppDl* d = dl(id, false); int st = d ? d->st : 0; UNLOCK();
  if (st == 2 || st == 1) return st;
  if (st == -1 && d) { LOCK(); d->id[0] = 0; UNLOCK(); return -1; }
  // a copy on the cube with the right version: use it (also without WiFi)
  AppInfo a; char vf[64], v[8] = ""; size_t n = 0;
  snprintf(vf, sizeof(vf), "/c/apps/%s.v", id);
  bool known = appInfo(id, a);
  if (fsRead(vf, (uint8_t*)v, 7, n)) v[n] = 0;
  if (v[0] && (!known || !strcmp(v, a.v))) { LOCK(); dl(id, true)->st = 2; UNLOCK(); return 2; }
  if (!known) { if (nApps == -1 && want(J_APPS)) post(J_APPS); if (nApps == -2) return -1; return 0; }
  LOCK(); dl(id, true)->st = 1; UNLOCK();
  post(J_APPGET, 0, id);
  return 1;
}
char* netAppSource(const char* id, size_t* n) {
  char f[64]; snprintf(f, sizeof(f), "/c/apps/%s.js", id);
  FILE* fp = fopen(f, "rb"); if (!fp) return nullptr;
  fseek(fp, 0, SEEK_END); long l = ftell(fp); fseek(fp, 0, SEEK_SET);
  char* b = (char*)psram(l + 1);
  if (b) { *n = fread(b, 1, l, fp); b[*n] = 0; }
  fclose(fp);
  return b;
}
void netFetch(int id, const char* path) { post(J_FETCH, id, path); }
bool netFetchDone(int& id, int& status, char*& body) {
  Fetched r;
  if (xQueueReceive(fetched, &r, 0) != pdTRUE) return false;
  id = r.id; status = r.status; body = r.body;
  return true;
}
bool netAppMsg(char* out, size_t n) {
  char* m;
  if (xQueueReceive(appMsgs, &m, 0) != pdTRUE) return false;
  snprintf(out, n, "%s", m); free(m);
  return true;
}

// ================================================================== the live link (WebSocket)
static WiFiClientSecure ws;
static uint32_t wsNext = 0, wsBackoff = 2000, wsPing = 0, stateAt = 0, shotAt = 0;
static bool stateDirty = true, watching = false;
static char myId[12] = "";
static char prog[24] = "ERROR-DOS";
static char* rx = nullptr; static size_t rxCap = 0;
void netReportProg(const char* p) { snprintf(prog, sizeof(prog), "%s", p); stateDirty = true; }
void netLinkState() { stateDirty = true; if (!cfg.link) linkDrop(); }
void netLinkNewCode() {
  static const char A[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  for (int i = 0; i < 6; i++) cfg.code[i] = A[esp_random() % 32];
  cfg.code[6] = 0; cfgSave();
  linkDrop();
}

static bool readExact(uint8_t* p, size_t n, uint32_t ms = 5000) {
  uint32_t t0 = millis();
  while (n) {
    int a = ws.available();
    if (a > 0) { int r = ws.read(p, a < (int)n ? a : n); if (r > 0) { p += r; n -= r; t0 = millis(); continue; } }
    if (!ws.connected() || millis() - t0 > ms) return false;
    delay(1);
  }
  return true;
}
static bool wsSend(uint8_t op, const uint8_t* d, size_t n) {
  if (!wsUp) return false;
  uint8_t h[14]; size_t hl = 2;
  h[0] = 0x80 | op;
  if (n < 126) h[1] = 0x80 | n;
  else if (n < 65536) { h[1] = 0x80 | 126; h[2] = n >> 8; h[3] = n & 255; hl = 4; }
  else return false;
  uint32_t m = esp_random(); memcpy(h + hl, &m, 4); hl += 4;
  const uint8_t* mk = h + hl - 4;
  if (ws.write(h, hl) != hl) return false;
  uint8_t buf[512];
  for (size_t o = 0; o < n; o += sizeof(buf)) {
    size_t k = n - o < sizeof(buf) ? n - o : sizeof(buf);
    for (size_t i = 0; i < k; i++) buf[i] = d[o + i] ^ mk[(o + i) & 3];
    if (ws.write(buf, k) != k) return false;
  }
  return true;
}
static void wsText(const String& s) { if (!wsSend(1, (const uint8_t*)s.c_str(), s.length())) linkDrop(); }
static void linkDrop() {
  if (wsUp || ws.connected()) ws.stop();
  wsUp = false; S.link = false; S.panels = 0; watching = false;
}

// ---------------- settings the control page can change (also its menu: the schema)
enum { T_TOGGLE, T_CHOICE, T_SLIDER, T_TEXT };
struct Set { const char* group; const char* k; const char* n; uint8_t t; uint8_t* p; int lo, hi; const char* (*label)(int); char* s; int slen; };
static const char* lScheme(int i) { return SCHEMES[i].name; }
static const char* lGlow(int i) { static const char* G[] = {"OFF", "LOW", "HIGH"}; return G[i]; }
static const char* lGlitch(int i) { static const char* G[] = {"OFF", "LOW", "MEDIUM", "HIGH"}; return G[i]; }
static const char* lBoot(int i) { static const char* B[] = {"FULL", "SHORT", "OFF"}; return B[i]; }
static const char* lOff(int i) { static char b[8][10]; int s = OFF_SECS[i]; if (!s) return "NEVER"; if (s < 60) snprintf(b[i], 10, "%d SEC", s); else snprintf(b[i], 10, "%d MIN", s / 60); return b[i]; }
static const char* lRot(int i) { static const char* R[] = {"0", "90", "180", "270", "AUTO"}; return R[i]; }
static const char* lTz(int i) { return TIMEZONES[i].name; }
static const char* lChg(int i) { static char b[7][10]; snprintf(b[i], 10, "%d MA", CHARGE_MA[i]); return b[i]; }
static const char* lAuto(int i) { static char b[5][10]; int m = AUTO_OFF_MIN[i]; if (!m) return "NEVER"; if (m < 60) snprintf(b[i], 10, "%d MIN", m); else snprintf(b[i], 10, "%d H", m / 60); return b[i]; }
static const char* lUpd(int i) { static char b[3][10]; snprintf(b[i], 10, "%d H", UPD_HOURS[i]); return b[i]; }
static const char* lKbd(int i) { return i ? "UKRAINIAN" : "ENGLISH"; }
static const char* lView(int i) { return i ? "3D" : "DOS"; }
static const char* lAnim(int i) { static const char* A[] = {"OFF", "ON", "ON"}; return A[i]; }
static const char* lBack(int i) { static const char* B[] = {"PLAIN", "DOTS", "GRID", "STARS", "ROOM"}; return B[i]; }
static const char* lAct(int i) { return btnName(i); }
static const char* lAodStyle(int i) { static const char* A[] = {"BIG", "SMALL", "ANALOG", "DOS"}; return A[i]; }
static const char* lAodWifi(int i) { static const char* W[] = {"AS NORMAL", "SAVE POWER", "OFF"}; return W[i]; }
static const char* lPwrHold(int i) { static const char* H[] = {"1 S", "1.5 S", "2 S", "2.5 S"}; return H[i]; }
static const char* lPwrOff(int i) { static const char* H[] = {"4 S", "6 S", "8 S", "10 S"}; return H[i]; }
static const char* lWake(int i) { return i ? "PWR ONLY" : "ANY BUTTON"; }
static Set SETS[] = {
  {"DISPLAY", "bright", "BRIGHTNESS", T_SLIDER, &cfg.bright, 5, 100, nullptr},
  {"DISPLAY", "offIdx", "SCREEN OFF AFTER", T_CHOICE, &cfg.offIdx, 0, 7, lOff},
  {"DISPLAY", "dim", "DIM FIRST", T_TOGGLE, &cfg.dim, 0, 1, nullptr},
  {"DISPLAY", "aod", "LOW POWER CLOCK WHEN OFF", T_TOGGLE, &cfg.aod, 0, 1, nullptr},
  {"DISPLAY", "aodStyle", "CLOCK STYLE", T_CHOICE, &cfg.aodStyle, 0, 3, lAodStyle},
  {"DISPLAY", "aodLight", "CLOCK LIGHT %", T_SLIDER, &cfg.aodLight, 1, 30, nullptr},
  {"DISPLAY", "aodDate", "CLOCK: DATE", T_TOGGLE, &cfg.aodDate, 0, 1, nullptr},
  {"DISPLAY", "aodBatt", "CLOCK: BATTERY", T_TOGGLE, &cfg.aodBatt, 0, 1, nullptr},
  {"DISPLAY", "aodWifi", "WIFI WHEN OFF", T_CHOICE, &cfg.aodWifi, 0, 2, lAodWifi},
  {"DISPLAY", "tapWake", "TAP TO WAKE", T_TOGGLE, &cfg.tapWake, 0, 1, nullptr},
  {"DISPLAY", "moveWake", "LIFT TO WAKE", T_TOGGLE, &cfg.moveWake, 0, 1, nullptr},
  {"DISPLAY", "rot", "ROTATION", T_CHOICE, &cfg.rot, 0, 4, lRot},
  {"LOOK", "lowGfx", "LOW GRAPHICS (PLAINEST)", T_TOGGLE, &cfg.lowGfx, 0, 1, nullptr},
  {"LOOK", "view", "MENU", T_CHOICE, &cfg.view, 0, 1, lView},
  {"LOOK", "anim", "ANIMATION", T_CHOICE, &cfg.anim, 0, 1, lAnim},
  {"LOOK", "parallax", "MOVES WITH YOU", T_TOGGLE, &cfg.parallax, 0, 1, nullptr},
  {"LOOK", "color", "COLOUR", T_CHOICE, &cfg.color, 0, N_SCHEMES - 1, lScheme},
  {"LOOK", "glow", "GLOW", T_CHOICE, &cfg.glow, 0, 2, lGlow},
  {"LOOK", "scan", "SCANLINES", T_TOGGLE, &cfg.scan, 0, 1, nullptr},
  {"LOOK", "glitch", "GLITCH", T_CHOICE, &cfg.glitch, 0, 3, lGlitch},
  {"LOOK", "crt", "CRT ON/OFF", T_TOGGLE, &cfg.crt, 0, 1, nullptr},
  {"LOOK", "boot", "BOOT", T_CHOICE, &cfg.boot, 0, 2, lBoot},
  {"LOOK", "intro", "3D INTRO", T_TOGGLE, &cfg.intro, 0, 1, nullptr},
  {"DISPLAY", "holoBack", "WALLPAPER", T_CHOICE, &cfg.holoBack, 0, 4, lBack},
  {"SOUND", "vol", "VOLUME", T_SLIDER, &cfg.vol, 0, 100, nullptr},
  {"SOUND", "clicks", "KEY CLICKS", T_TOGGLE, &cfg.clicks, 0, 1, nullptr},
  {"SOUND", "sounds", "SOUNDS", T_TOGGLE, &cfg.sounds, 0, 1, nullptr},
  {"SOUND", "chatBeep", "CHAT BEEP", T_TOGGLE, &cfg.chatBeep, 0, 1, nullptr},
  {"TIME", "ntp", "FROM INTERNET", T_TOGGLE, &cfg.ntp, 0, 1, nullptr},
  {"TIME", "tzIdx", "TIME ZONE", T_CHOICE, &cfg.tzIdx, 0, N_TZ - 1, lTz},
  {"TIME", "h24", "24 HOURS", T_TOGGLE, &cfg.h24, 0, 1, nullptr},
  {"TIME", "secs", "SECONDS", T_TOGGLE, &cfg.secs, 0, 1, nullptr},
  {"TIME", "alarmOn", "ALARM", T_TOGGLE, &cfg.alarmOn, 0, 1, nullptr},
  {"TIME", "alarmH", "ALARM HOUR", T_SLIDER, &cfg.alarmH, 0, 23, nullptr},
  {"TIME", "alarmM", "ALARM MINUTE", T_SLIDER, &cfg.alarmM, 0, 59, nullptr},
  {"TIME", "alarmLoud", "ALARM AT FULL VOLUME", T_TOGGLE, &cfg.alarmLoud, 0, 1, nullptr},
  {"TIME", "alarmMove", "MOVE THE CUBE = SNOOZE", T_TOGGLE, &cfg.alarmMove, 0, 1, nullptr},
  {"TIME", "alarmSnooze", "SNOOZE (MIN)", T_SLIDER, &cfg.alarmSnooze, 1, 30, nullptr},
  {"POWER", "chgIdx", "CHARGE CURRENT", T_CHOICE, &cfg.chgIdx, 0, N_CHARGE - 1, lChg},
  {"POWER", "autoOffIdx", "OFF WHEN ASLEEP", T_CHOICE, &cfg.autoOffIdx, 0, N_AUTO_OFF - 1, lAuto},
  {"POWER", "saver", "BATTERY SAVER", T_TOGGLE, &cfg.saver, 0, 1, nullptr},
  {"ERROR.BLUE", "name", "NAME", T_TEXT, nullptr, 0, 16, nullptr, cfg.name, sizeof(cfg.name)},
  {"ERROR.BLUE", "chan", "CHAT CHANNEL", T_TEXT, nullptr, 3, 8, nullptr, cfg.chan, sizeof(cfg.chan)},
  {"ERROR.BLUE", "online", "SHOW ME ONLINE", T_TOGGLE, &cfg.online, 0, 1, nullptr},
  {"UPDATES", "autoUpd", "INSTALL BY ITSELF (SCREEN OFF)", T_TOGGLE, &cfg.autoUpd, 0, 1, nullptr},
  {"UPDATES", "updAsk", "MESSAGE WHEN NEW", T_TOGGLE, &cfg.updAsk, 0, 1, nullptr},
  {"UPDATES", "updIdx", "CHECK EVERY", T_CHOICE, &cfg.updIdx, 0, N_UPD - 1, lUpd},
  {"SYSTEM", "kbd", "KEYBOARD", T_CHOICE, &cfg.kbd, 0, 1, lKbd},
  {"BUTTONS", "b00", "- PRESS", T_CHOICE, &cfg.btnAct[0][0], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "b01", "- DOUBLE PRESS", T_CHOICE, &cfg.btnAct[0][1], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "b02", "- HOLD", T_CHOICE, &cfg.btnAct[0][2], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "b10", "+ PRESS", T_CHOICE, &cfg.btnAct[1][0], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "b11", "+ DOUBLE PRESS", T_CHOICE, &cfg.btnAct[1][1], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "b12", "+ HOLD", T_CHOICE, &cfg.btnAct[1][2], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "b20", "PWR PRESS", T_CHOICE, &cfg.btnAct[2][0], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "b21", "PWR DOUBLE PRESS", T_CHOICE, &cfg.btnAct[2][1], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "b22", "PWR HOLD", T_CHOICE, &cfg.btnAct[2][2], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "c0", "- AND +", T_CHOICE, &cfg.btnCombo[0], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "c1", "HOLD - AND +", T_CHOICE, &cfg.btnCombo[1], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "c2", "HOLD -, PWR", T_CHOICE, &cfg.btnCombo[2], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "c3", "HOLD +, PWR", T_CHOICE, &cfg.btnCombo[3], 0, BA_COUNT - 1, lAct},
  {"BUTTONS", "btnHold", "HOLD AFTER (x50 MS)", T_SLIDER, &cfg.btnHold, 6, 40, nullptr},
  {"BUTTONS", "btnDouble", "DOUBLE GAP (x10 MS, 0 OFF)", T_SLIDER, &cfg.btnDouble, 0, 50, nullptr},
  {"BUTTONS", "btnRepeat", "REPEAT (x10 MS, 0 OFF)", T_SLIDER, &cfg.btnRepeat, 0, 40, nullptr},
  {"BUTTONS", "pwrHold", "PWR HOLD AFTER", T_CHOICE, &cfg.pwrHold, 0, 3, lPwrHold},
  {"BUTTONS", "pwrOff", "PWR FORCE OFF", T_CHOICE, &cfg.pwrOff, 0, 3, lPwrOff},
  {"BUTTONS", "btnSwap", "SWAP - AND +", T_TOGGLE, &cfg.btnSwap, 0, 1, nullptr},
  {"BUTTONS", "btnWake", "WAKE WITH", T_CHOICE, &cfg.btnWake, 0, 1, lWake},
};
static const int N_SETS = sizeof(SETS) / sizeof(SETS[0]);

static void sendSchema() {
  JsonDocument d;
  d["t"] = "schema"; d["dev"] = "error-cube"; d["v"] = CUBE_VERSION;
  JsonArray caps = d["caps"].to<JsonArray>();
  for (const char* c : {"say", "video", "audio", "apps", "shot", "update", "power", "files", "map"}) caps.add(c);
  JsonArray g = d["sets"].to<JsonArray>();
  static const char* TY[] = {"toggle", "choice", "slider", "text"};
  for (int i = 0; i < N_SETS; i++) {
    Set& s = SETS[i];
    JsonObject o = g.add<JsonObject>();
    o["g"] = s.group; o["k"] = s.k; o["n"] = s.n; o["t"] = TY[s.t];
    if (s.t == T_SLIDER || s.t == T_TEXT) { o["lo"] = s.lo; o["hi"] = s.hi; }
    if (s.t == T_CHOICE) { JsonArray op = o["o"].to<JsonArray>(); for (int k = s.lo; k <= s.hi; k++) op.add(s.label(k)); }
  }
  String out; serializeJson(d, out);
  wsText(out);
}
static void sendState() {
  JsonDocument d;
  d["t"] = "state"; d["dev"] = "error-cube"; d["id"] = S.host; d["name"] = cfg.name; d["v"] = CUBE_VERSION;
  d["prog"] = prog; d["awake"] = uiAwake();
  d["bat"] = S.battery ? S.batPct : -1; d["chg"] = S.charging; d["usb"] = S.usb; d["mv"] = S.batMv;
  d["ssid"] = S.ssid; d["ip"] = S.ip; d["rssi"] = S.rssi; d["up"] = uptimeSecs();
  d["heap"] = freeHeap() / 1024; d["psram"] = freePsram() / 1024; d["temp"] = (int)S.chipTemp;
  d["sd"] = S.sd ? S.sdInfo : "";
  if (S.crash[0]) d["crash"] = S.crash;
  JsonObject u = d["upd"].to<JsonObject>();
  static const char* US[] = {"idle", "checking", "new", "loading", "done", "failed", "latest"};
  u["st"] = US[S.upd % 7]; u["ver"] = S.updVer; u["pct"] = S.updPct; u["msg"] = S.updMsg; u["notes"] = S.updNotes;
  JsonObject c = d["cfg"].to<JsonObject>();
  for (int i = 0; i < N_SETS; i++) { if (SETS[i].t == T_TEXT) c[SETS[i].k] = SETS[i].s; else c[SETS[i].k] = *SETS[i].p; }
  String out; serializeJson(d, out);
  wsText(out);
  stateAt = millis(); stateDirty = false;
}
static void applySet(const char* k, JsonVariant v) {
  for (int i = 0; i < N_SETS; i++) {
    Set& s = SETS[i];
    if (strcmp(s.k, k)) continue;
    if (s.t == T_TEXT) {
      String t = v.as<String>(); t.trim();
      if (!strcmp(k, "chan")) { t.toUpperCase(); String c; for (char ch : t) if (isalnum((unsigned char)ch)) c += ch; t = c; }
      if ((int)t.length() < s.lo || (int)t.length() > s.hi) return;
      snprintf(s.s, s.slen, "%s", t.c_str());
      if (!strcmp(k, "chan") || !strcmp(k, "name")) netChatReset();
    } else {
      int x = v.as<int>(); if (x < s.lo) x = s.lo; if (x > s.hi) x = s.hi;
      *s.p = x;
      if (!strcmp(k, "chgIdx")) sysChargeCurrent(CHARGE_MA[cfg.chgIdx]);
    }
    S.cfgAt = millis() | 1;
    stateDirty = true;
    return;
  }
}

// ---------------- video and sound from the phone
static uint16_t* vidBack = nullptr;
uint16_t* vidBuf = nullptr;
struct JpgJob { const uint8_t* d; size_t n, pos; uint16_t* out; int w, h; };
static UINT jpgRead(JDEC* jd, BYTE* buf, UINT len) {
  JpgJob* j = (JpgJob*)jd->device;
  if (j->pos + len > j->n) len = j->n - j->pos;
  if (buf) memcpy(buf, j->d + j->pos, len);
  j->pos += len;
  return len;
}
static UINT jpgWrite(JDEC* jd, void* bitmap, JRECT* r) {
  JpgJob* j = (JpgJob*)jd->device;
  const uint8_t* px = (const uint8_t*)bitmap;
  for (int y = r->top; y <= r->bottom; y++) {
    int y0 = y * SCR_H / j->h, y1 = (y + 1) * SCR_H / j->h;
    for (int x = r->left; x <= r->right; x++, px += 3) {
      uint16_t c = rgb3(px[0], px[1], px[2]);
      int x0 = x * SCR_W / j->w, x1 = (x + 1) * SCR_W / j->w;
      for (int yy = y0; yy < y1 && yy < SCR_H; yy++) for (int xx = x0; xx < x1 && xx < SCR_W; xx++) j->out[yy * SCR_W + xx] = c;
    }
  }
  return 1;
}
static void showJpeg(const uint8_t* d, size_t n) {
  static uint8_t* work = nullptr;
  if (!work) work = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!vidBack) vidBack = (uint16_t*)psram(SCR_W * SCR_H * 2);
  static uint16_t* other = nullptr;
  if (!other) other = (uint16_t*)psram(SCR_W * SCR_H * 2);
  if (!work || !vidBack || !other) return;
  JpgJob j = {d, n, 0, vidBack, 0, 0};
  JDEC jd;
  if (jd_prepare(&jd, jpgRead, work, 4096, &j) != JDR_OK) return;
  j.w = jd.width; j.h = jd.height;
  if (!j.w || !j.h || jd_decomp(&jd, jpgWrite, 0) != JDR_OK) return;
  uint16_t* done = vidBack; vidBack = vidBuf ? vidBuf : other; vidBuf = done;   // swap: the screen reads the finished one
  S.vidSeq++; S.vidAt = millis();
}
static uint8_t* b64dec(const char* s, size_t& n) {
  size_t len = strlen(s), cap = len * 3 / 4 + 4;
  uint8_t* o = (uint8_t*)psram(cap);
  if (!o) return nullptr;
  if (mbedtls_base64_decode(o, cap, &n, (const uint8_t*)s, len) != 0) { free(o); return nullptr; }
  return o;
}
static char* vidParts = nullptr; static size_t vidLen = 0; static int vidN = -1, vidGot = 0;
static void onVideo(JsonObject m) {
  int n = m["n"] | 0, i = m["i"] | 0, of = m["of"] | 1;
  const char* d = m["d"] | "";
  if (of <= 1) {
    size_t bn; uint8_t* b = b64dec(d, bn);
    if (b) { showJpeg(b, bn); free(b); }
    return;
  }
  // a frame in parts: put them together in order
  if (n != vidN || i == 0) { vidN = n; vidGot = 0; vidLen = 0; }
  if (i != vidGot) return;
  size_t dl = strlen(d);
  if (!vidParts) vidParts = (char*)psram(96 * 1024);
  if (!vidParts || vidLen + dl >= 96 * 1024) return;
  memcpy(vidParts + vidLen, d, dl); vidLen += dl; vidParts[vidLen] = 0; vidGot++;
  if (vidGot == of) { size_t bn; uint8_t* b = b64dec(vidParts, bn); if (b) { showJpeg(b, bn); free(b); } vidN = -1; }
}
static int16_t ulaw(uint8_t u) {
  u = ~u;
  int t = ((u & 0x0F) << 3) + 0x84;
  t <<= (u & 0x70) >> 4;
  return (u & 0x80) ? (0x84 - t) : (t - 0x84);
}
static void onAudio(JsonObject m) {
  size_t n; uint8_t* b = b64dec(m["d"] | "", n);
  if (!b) return;
  static int16_t* pcm = (int16_t*)psram(4096 * 2);
  if (!pcm) { free(b); return; }
  if (n > 4096) n = 4096;
  for (size_t i = 0; i < n; i++) pcm[i] = ulaw(b[i]);
  free(b);
  sndFeed(pcm, n);
}
// the screen, small (96 x 96, 1 byte a pixel: RGB332), for the preview on the phone
static void sendShot() {
  static uint8_t* px = (uint8_t*)psram(96 * 96);
  static char* b64 = (char*)psram(96 * 96 * 4 / 3 + 8);
  if (!px || !b64) return;
  for (int y = 0; y < 96; y++) for (int x = 0; x < 96; x++) {
    uint16_t c = fb[(y * 5 + 2) * SCR_W + x * 5 + 2];
    px[y * 96 + x] = ((c >> 13) << 5) | (((c >> 8) & 7) << 2) | ((c >> 3) & 3);
  }
  size_t o;
  mbedtls_base64_encode((uint8_t*)b64, 96 * 96 * 4 / 3 + 8, &o, px, 96 * 96);
  b64[o] = 0;
  String s = String("{\"t\":\"shot\",\"w\":96,\"h\":96,\"d\":\"") + b64 + "\"}";
  wsText(s);
  shotAt = millis();
}
void netAppSend(const char* id, const char* json) {
  if (!wsUp) return;
  String s = String("{\"t\":\"app\",\"id\":") + jsonStr(id) + ",\"data\":" + json + "}";
  if (s.length() < 15000) wsText(s);
}

// ---------------- files from the control page (error.blue/cube > FILES): list, upload in pieces, delete, folders
static bool okPath(const char* p) {
  bool root = !strcmp(p, "/sd") || !strcmp(p, "/c") || !strncmp(p, "/sd/", 4) || !strncmp(p, "/c/", 3);
  return root && !strstr(p, "..") && strlen(p) < 180;
}
static void onFls(JsonObject m) {
  const char* d = m["d"] | "/sd";
  JsonDocument r;
  r["t"] = "fls"; r["d"] = d;
  if (!strncmp(d, "/sd", 3)) sdMount();
  r["sd"] = S.sd; r["sdInfo"] = S.sdInfo;
  static FileEnt* ents = nullptr;
  if (!ents) ents = (FileEnt*)psram(sizeof(FileEnt) * 150);
  int n = ents && okPath(d) ? fsList(d, ents, 150) : -1;
  r["ok"] = n >= 0;
  JsonArray e = r["e"].to<JsonArray>();
  size_t room = 13000;
  for (int i = 0; i < n; i++) {
    size_t l = strlen(ents[i].name) + 20;
    if (l > room) { r["more"] = true; break; }
    room -= l;
    JsonArray a = e.add<JsonArray>(); a.add(ents[i].name); a.add(ents[i].dir ? 1 : 0); a.add(ents[i].size);
  }
  uint64_t t, u; if (n >= 0 && fsInfo(d, t, u)) { r["free"] = (uint32_t)((t - u) >> 20); r["total"] = (uint32_t)(t >> 20); }
  String out; serializeJson(r, out); wsText(out);
}
static void onFput(JsonObject m) {
  const char* p = m["p"] | "";
  uint32_t o = m["o"] | 0, z = m["z"] | 0;
  const char* d = m["d"] | "";
  JsonDocument r; r["t"] = "fack"; r["p"] = p;
  bool ok = okPath(p) && strchr(p + 1, '/');
  size_t n = 0;
  if (ok) {
    if (!o && !strncmp(p, "/sd", 3)) ok = sdMount();
    uint8_t* b = ok ? b64dec(d, n) : nullptr;
    ok = b && fsAppend(p, b, n, o);
    free(b);
  }
  r["ok"] = ok; r["o"] = o + (ok ? n : 0);
  if (!ok) r["err"] = !strncmp(p, "/sd", 3) && !S.sd ? "NO SD CARD" : "COULD NOT WRITE";
  if (ok && z && o + n >= z) {                   // the whole file is there: tell the screen
    const char* nm = strrchr(p, '/');
    snprintf(S.fileName, sizeof(S.fileName), "%s", nm ? nm + 1 : p); S.fileAt = millis() | 1;
  }
  String out; serializeJson(r, out); wsText(out);
}
static void onFop(const char* t, JsonObject m) {
  const char* p = m["p"] | "";
  bool ok = false;
  if (!strcmp(t, "frm")) ok = okPath(p) && strchr(p + 1, '/') && (fsRemove(p) || fsRmdir(p));
  else if (!strcmp(t, "fmk")) ok = okPath(p) && fsMkdir(p);
  else if (!strcmp(t, "fmv")) { const char* b = m["b"] | ""; ok = okPath(p) && okPath(b) && fsRename(p, b); }
  JsonDocument r; r["t"] = "fdone"; r["c"] = t; r["p"] = p; r["ok"] = ok;
  String out; serializeJson(r, out); wsText(out);
  uiDirty();
}

static void onMessage(char* txt, size_t n) {
  JsonDocument d;
  if (deserializeJson(d, txt, n)) return;
  const char* type = d["type"] | "";
  if (!strcmp(type, "welcome")) {
    snprintf(myId, sizeof(myId), "%s", d["id"] | "");
    int p = 0; for (JsonObject pl : d["players"].as<JsonArray>()) if (strcmp(pl["id"] | "", myId)) p++;
    S.panels = p; sendSchema(); sendState(); return;
  }
  if (!strcmp(type, "join")) { S.panels++; return; }
  if (!strcmp(type, "leave")) { if (S.panels > 0) S.panels--; if (!S.panels) watching = false; return; }
  if (strcmp(type, "msg")) return;
  JsonObject m = d["data"];
  const char* t = m["t"] | "";
  if (!strcmp(t, "hello")) { sendSchema(); sendState(); }
  else if (!strcmp(t, "set")) applySet(m["k"] | "", m["v"]);
  else if (!strcmp(t, "say")) { snprintf(S.msgText, sizeof(S.msgText), "%s", m["text"] | ""); S.msgAt = millis() | 1; }
  else if (!strcmp(t, "vid")) onVideo(m);
  else if (!strcmp(t, "aud")) onAudio(m);
  else if (!strcmp(t, "app")) { snprintf(S.wantApp, sizeof(S.wantApp), "%s", m["id"] | ""); S.wantAppAt = millis() | 1; }
  else if (!strcmp(t, "appmsg")) { String s; serializeJson(m["data"], s); char* c = strdup(s.c_str()); if (xQueueSend(appMsgs, &c, 0) != pdTRUE) free(c); }
  else if (!strcmp(t, "watch")) { watching = m["on"] | 0; if (watching) shotAt = 0; }
  else if (!strcmp(t, "fls")) onFls(m);
  else if (!strcmp(t, "fput")) onFput(m);
  else if (!strcmp(t, "fopen")) { const char* p = m["p"] | ""; if (okPath(p)) { snprintf(S.openPath, sizeof(S.openPath), "%s", p); S.openAt = millis() | 1; } }
  else if (!strcmp(t, "frm") || !strcmp(t, "fmk") || !strcmp(t, "fmv")) onFop(t, m);
  else if (!strcmp(t, "cmd")) {
    const char* c = m["c"] | "";
    if (!strcmp(c, "wake")) S.wakeAt = millis() | 1;
    else if (!strcmp(c, "sleep")) S.sleepAt = millis() | 1;
    else if (!strcmp(c, "home")) S.wantHomeAt = millis() | 1;
    else if (!strcmp(c, "restart")) { wsText("{\"t\":\"ack\",\"c\":\"restart\"}"); delay(300); sysRestart(); }
    else if (!strcmp(c, "check")) netUpdateCheck();
    else if (!strcmp(c, "update")) { if (S.upd == UPD_NEW) netUpdateInstall(); else netUpdateCheck(); }
    else if (!strcmp(c, "beep")) snd(SND_BEEP);
    stateDirty = true;
  }
}
static void linkErr(const char* what) {
  char e[64] = ""; ws.lastError(e, sizeof(e));
  snprintf(S.linkErr, sizeof(S.linkErr), "%s%s%s", what, e[0] ? ": " : "", e);
}
// 1 up, 0 failed, -1 not now (a request is using TLS: try again in a moment)
static int wsOpen() {
  if (xSemaphoreTake(tlsMx, 0) != pdTRUE) return -1;
  if (tls.connected()) { http.end(); tls.stop(); }     // the requests' idle connection: the link comes first
  if (cfg.tls) ws.setCACert(ROOT_CAS); else ws.setInsecure();
  ws.setHandshakeTimeout(12);
  if (!ws.connect(CUBE_HOST, CUBE_PORT)) {
    linkErr("TLS");
    char e[64] = ""; ws.lastError(e, sizeof(e)); String es = e; es.toUpperCase();
    if (cfg.tls && (es.indexOf("X509") >= 0 || es.indexOf("VERIFY") >= 0 || es.indexOf("CERT") >= 0)) { cfg.tls = 0; cfgSave(); }
    ws.stop();
    xSemaphoreGive(tlsMx); return 0;
  }
  uint8_t key[16]; for (int i = 0; i < 16; i += 4) { uint32_t r = esp_random(); memcpy(key + i, &r, 4); }
  char k64[32]; size_t o; mbedtls_base64_encode((uint8_t*)k64, sizeof(k64), &o, key, 16); k64[o] = 0;
  String req = String("GET /api/room/" LINK_GAME "/") + cfg.code + "?name=" + urlEnc(S.host) + " HTTP/1.1\r\nHost: " CUBE_HOST "\r\n" +
               "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + k64 + "\r\nSec-WebSocket-Version: 13\r\n" +
               "User-Agent: ERROR-CUBE/" CUBE_VERSION "\r\n\r\n";
  ws.print(req);
  String line; bool first = true, ok = false; uint32_t t0 = millis();
  while (millis() - t0 < 10000) {
    if (!ws.available()) { if (!ws.connected()) break; delay(2); continue; }
    char c = ws.read();
    if (c == '\r') continue;
    if (c != '\n') { if (line.length() < 200) line += c; continue; }
    if (first) { ok = line.indexOf(" 101") > 0; first = false; if (!ok) snprintf(S.linkErr, sizeof(S.linkErr), "SERVER: %s", line.c_str()); }
    else if (!line.length()) break;
    line = "";
  }
  if (first) snprintf(S.linkErr, sizeof(S.linkErr), "NO ANSWER FROM ERROR.BLUE");
  if (!ok) { ws.stop(); xSemaphoreGive(tlsMx); return 0; }
  wsUp = true; S.link = true; wsPing = millis(); S.linkErr[0] = 0;
  xSemaphoreGive(tlsMx);
  return 1;
}
static void wsRead() {
  // whole frames only: header first, then the rest (they come in one piece)
  for (int guard = 0; guard < 8 && ws.available() >= 2; guard++) {
    uint8_t h[2];
    if (!readExact(h, 2)) { linkDrop(); return; }
    uint8_t op = h[0] & 15; uint64_t len = h[1] & 127;
    if (len == 126) { uint8_t e[2]; if (!readExact(e, 2)) { linkDrop(); return; } len = e[0] << 8 | e[1]; }
    else if (len == 127) { uint8_t e[8]; if (!readExact(e, 8)) { linkDrop(); return; } len = 0; for (int i = 0; i < 8; i++) len = len << 8 | e[i]; }
    if (len > 200000) { linkDrop(); return; }
    if (len + 1 > rxCap) { free(rx); rxCap = len + 1024; rx = (char*)psram(rxCap); if (!rx) { rxCap = 0; linkDrop(); return; } }
    if (len && !readExact((uint8_t*)rx, len)) { linkDrop(); return; }
    rx[len] = 0;
    if (op == 1) onMessage(rx, len);
    else if (op == 8) { linkDrop(); return; }
    else if (op == 9) wsSend(10, (uint8_t*)rx, len);
  }
}

// ================================================================== timers
static uint32_t onlineAt = 0, whoAt = 0, chatAt = 0, fwAt = 0, upAt = 0;
static char sid[20];
void cloudBegin() {
  lock = xSemaphoreCreateMutex();
  tlsMx = xSemaphoreCreateMutex();
  jobs = xQueueCreate(12, sizeof(Job));
  fetched = xQueueCreate(8, sizeof(Fetched));
  appMsgs = xQueueCreate(8, sizeof(char*));
  snprintf(sid, sizeof(sid), "cube%08x", (unsigned)esp_random());
  xTaskCreatePinnedToCore(worker, "https", 16384, nullptr, 1, nullptr, 0);
}
void cloudMarkGood() { esp_ota_mark_app_valid_cancel_rollback(); }

void cloudStep() {
  uint32_t now = millis();
  if (S.net != NET_UP) { if (wsUp) linkDrop(); upAt = 0; return; }
  if (!upAt) { upAt = now; fwAt = now - (UPD_HOURS[cfg.updIdx % N_UPD] * 3600000u) + 20000; }   // first check 20 s after WiFi came
  // "I'm online" (shows the cube in WHO lists everywhere)
  if (cfg.online && now - onlineAt > 25000 && want(J_ONLINE)) {
    onlineAt = now;
    String b = String("{\"sid\":\"") + sid + "\",\"path\":\"/cube\",\"name\":" + jsonStr(cfg.name) + ",\"detail\":" + jsonStr(prog) + "}";
    post(J_ONLINE, 0, nullptr, b.c_str());
  }
  // who's online / chat: often while on screen, chat now and then in the background (new message beep)
  if (focus == FOCUS_WHO && now - whoAt > 10000 && want(J_WHO)) { whoAt = now; post(J_WHO); }
  uint32_t chatEvery = focus == FOCUS_CHAT ? 3000 : 30000;
  if (now - chatAt > chatEvery && want(J_CHAT)) { chatAt = now; post(J_CHAT, focus == FOCUS_CHAT ? 0 : 1); }
  // updates
  uint32_t every = UPD_HOURS[cfg.updIdx % N_UPD] * 3600000u;
  if (now - fwAt > every && want(J_FW)) { fwAt = now; post(J_FW); }
  if (S.upd == UPD_NEW && cfg.autoUpd && want(J_OTA) && (!otaFailedAt || now - otaFailedAt > 3600000)) {
    // on its own: when nobody is using the cube (screen asleep), or right away from the control page
    if (!uiAwake()) post(J_OTA);
  }
  // the live link
  if (!cfg.link) { if (wsUp) linkDrop(); return; }
  if (linkYield) { if (wsUp) linkDrop(); wsNext = now + 500; return; }   // a request needs the RAM for a moment
  if (!wsUp) {
    if (now > wsNext && S.upd != UPD_LOADING) {
      int r = wsOpen();
      if (r > 0) { wsBackoff = 2000; }
      else if (r < 0) wsNext = now + 700;
      else { wsNext = now + wsBackoff; wsBackoff = wsBackoff * 2 > 30000 ? 30000 : wsBackoff * 2; }
    }
    return;
  }
  if (!ws.connected()) { linkDrop(); wsNext = now + 2000; return; }
  wsRead();
  if (!wsUp) return;
  if (now - wsPing > 25000) { wsPing = now; wsText("{\"type\":\"ping\"}"); }
  if (S.panels && (stateDirty ? now - stateAt > 700 : now - stateAt > 5000)) sendState();
  if (watching && S.panels && now - shotAt > 1500) sendShot();
}
