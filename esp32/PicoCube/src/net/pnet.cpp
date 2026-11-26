// pnet.cpp: PICO-CUBE and error.blue (ERROR-CUBE's cloud.cpp, cut down to what a PICO-8 player needs).
//  - the cart list (/pico8/carts.json) and cart downloads (/pico8/carts/<file> -> C:\pico8)
//  - updates over WiFi (OTA) with rollback: /pico8/flash/firmware.json
//  - the switch back: install ERROR-CUBE (/cube/flash/firmware.json) the same way
// Everything runs in one HTTPS worker task, one request at a time. Reads are our own loop that sleeps between
// pieces (HTTPClient's writeToStream can spin without sleeping and starve the idle task: the watchdog).
#include <Arduino.h>
#include <string>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include "../ui/ui.h"
#include "../pico/pico.h"
#include "net.h"
#include "certs.h"

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

// ================================================================== the HTTPS worker
enum { J_FW, J_OTA, J_OTAFILE, J_DL, J_CARTS, J_SWITCH, J_N };
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
static void tlsSetup() {
  if (lastTls == cfg.tls) return;
  http.end(); tls.stop();
  if (cfg.tls) tls.setCACert(ROOT_CAS); else tls.setInsecure();
  lastTls = cfg.tls;
}
static void noteError(int code) {
  char e[64] = "";
  tls.lastError(e, sizeof(e));
  snprintf(S.netErr, sizeof(S.netErr), "%s", e[0] ? e : HTTPClient::errorToString(code).c_str());
  // the certificate check failed: go on without it (like ERROR-CUBE)
  String es = e; es.toUpperCase();
  if (cfg.tls && (es.indexOf("X509") >= 0 || es.indexOf("VERIFY") >= 0 || es.indexOf("CERT") >= 0)) { cfg.tls = 0; cfgSave(); }
  tls.stop();
}

// GET path, the answer piece by piece into sink(buf, n) (false from the sink = stop). len: Content-Length or -1.
// Returns the HTTP code (200), or <= 0; *got = bytes passed on.
static int getStream(const char* path, std::function<bool(const uint8_t*, size_t)> sink, int* lenOut, size_t* got, int timeout = 20000) {
  tlsSetup();
  http.end();
  http.useHTTP10(true);                   // no chunks: the server closes at the end
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
        if (!st->connected() && st->available() <= 0) break;     // the end (or it broke off)
        if ((int)(millis() - last) > timeout) break;
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      if (len > 0 && *got != (size_t)len) code = -100;            // broke off
    } else if (code <= 0) noteError(code);
    http.end();
  }
  http.useHTTP10(false);
  if (code == 200) S.netErr[0] = 0;
  return code;
}
// GET a small thing into memory (PSRAM, 0-terminated)
static int getBody(const char* path, char** out, size_t* n) {
  size_t cap = 8192, used = 0;
  char* b = (char*)psram(cap);
  if (!b) return -1;
  size_t got; int len = -1;
  int code = getStream(path, [&](const uint8_t* d, size_t k) {
    if (used + k + 1 > cap) {
      size_t nc = cap * 2; while (nc < used + k + 1) nc *= 2;
      if (nc > 512 * 1024) return false;
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

// ---------------- the carts on error.blue
static PCart* carts = (PCart*)heap_caps_calloc(96, sizeof(PCart), MALLOC_CAP_SPIRAM);
static int nCarts = -1;
static void parseCarts(const char* s) {
  JsonDocument d;
  if (deserializeJson(d, s)) { nCarts = -2; return; }
  LOCK();
  int n = 0;
  for (JsonObject c : d["carts"].as<JsonArray>()) {
    if (n >= 96 || !carts) break;
    PCart& p = carts[n];
    memset(&p, 0, sizeof(p));
    snprintf(p.file, sizeof(p.file), "%s", c["file"] | "");
    snprintf(p.name, sizeof(p.name), "%s", c["name"] | "");
    snprintf(p.author, sizeof(p.author), "%s", c["author"] | "");
    snprintf(p.lic, sizeof(p.lic), "%s", c["license"] | "");
    snprintf(p.ver, sizeof(p.ver), "%s", c["v"] | "1");
    snprintf(p.about, sizeof(p.about), "%s", c["about"] | "");
    p.size = c["size"] | 0;
    if (p.file[0] && !strchr(p.file, '/') && !strstr(p.file, "..")) n++;   // a plain file name only
  }
  nCarts = n;
  UNLOCK();
}

// ---------------- downloads (carts) into a file
static void doDownload(const char* path, const char* file) {
  S.dlState = 1; S.dlPct = 0;
  char tmp[120]; snprintf(tmp, sizeof(tmp), "%s.part", file);
  if (!strncmp(file, "/sd", 3)) sdMount();
  fsMkdir(std::string(file).substr(0, std::string(file).rfind('/')).c_str());
  FILE* f = fopen(tmp, "wb");
  if (!f) { S.dlState = -1; uiDirty(); return; }
  int len = -1; size_t got = 0; bool wok = true;
  int code = getStream(path, [&](const uint8_t* d, size_t k) {
    wok = fwrite(d, 1, k, f) == k;
    if (len > 0) S.dlPct = (int)((got + k) * 100 / len);
    return wok;
  }, &len, &got);
  fclose(f);
  bool ok = code == 200 && wok && got > 0;
  if (ok) { remove(file); ok = rename(tmp, file) == 0; } else remove(tmp);
  S.dlState = ok ? 2 : -1;
  uiDirty();
}

// ---------------- firmware: what's newest, and installing it
struct FwInfo { char ver[16], path[64], md5[36]; uint32_t size; };
static FwInfo fw, other;
static uint32_t otaFailedAt = 0;
volatile bool pnetSwitching = false;
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
  if (err) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "%s", err); otaFailedAt = millis(); tls.stop(); return; }
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
      if (j.kind == J_DL) S.dlState = -1;
      if (j.kind == J_CARTS) nCarts = -2;
      if (j.kind == J_FW || j.kind == J_OTA || j.kind == J_SWITCH) { S.upd = UPD_FAIL; snprintf(S.updMsg, sizeof(S.updMsg), "NO WIFI"); }
      busy &= ~(1u << j.kind); free(j.a); free(j.b); uiDirty(); continue;
    }
    switch (j.kind) {
      case J_FW:
        if (readFw(FW_INFO_PATH, fw, true)) S.upd = verCmp(fw.ver, CUBE_VERSION) > 0 ? UPD_NEW : UPD_LATEST;
        break;
      case J_OTA: pnetSwitching = false; install(fw, FW_NAME); break;
      case J_OTAFILE: pnetSwitching = false; doOtaFile(j.a); break;
      case J_DL: doDownload(j.a, j.b); break;
      case J_CARTS: {
        char* body = nullptr; size_t n = 0;
        if (getBody(CARTS_PATH, &body, &n) == 200 && body) parseCarts(body); else nCarts = -2;
        free(body);
        break;
      }
      case J_SWITCH:
        pnetSwitching = true;
        S.upd = UPD_CHECKING; snprintf(S.updMsg, sizeof(S.updMsg), "LOOKING FOR ERROR-CUBE");
        if (readFw(ERROR_CUBE_INFO, other, false)) install(other, "ERROR-CUBE");
        break;
    }
    busy &= ~(1u << j.kind);
    free(j.a); free(j.b);
    uiDirty();
  }
}

// ---------------- what the screens call
int pnetCarts(PCart* out, int max) {
  if (nCarts == -1 && want(J_CARTS)) post(J_CARTS);
  LOCK(); int n = nCarts; if (n > max) n = max; if (n > 0) memcpy(out, carts, sizeof(PCart) * n); UNLOCK();
  return n;
}
void pnetCartsRefresh() { if (want(J_CARTS)) { nCarts = -1; post(J_CARTS); } }
void pnetSwitch() { if (want(J_SWITCH) && S.upd != UPD_LOADING) { S.upd = UPD_CHECKING; pnetSwitching = true; post(J_SWITCH); } }
void netDownload(const char* path, const char* file) { if (want(J_DL)) { S.dlState = 1; S.dlPct = 0; post(J_DL, 0, path, file); } }
void netUpdateCheck() { if (want(J_FW) && S.upd != UPD_LOADING) { S.upd = UPD_CHECKING; post(J_FW); } }
void netUpdateInstall() { if (S.upd == UPD_NEW && want(J_OTA)) post(J_OTA); }
bool netInstallFile(const char* path) {
  const char* e = strrchr(path, '.');
  if (!e || strcasecmp(e, ".bin")) return false;
  if (want(J_OTAFILE)) post(J_OTAFILE, 0, path);
  return true;
}
// ERROR-CUBE's live link and focus: not in PICO-CUBE
void netSetFocus(int) {}
void netLinkNewCode() {}
void netLinkState() {}
void netReportProg(const char*) {}
void netChatReset() {}

// ================================================================== timers
static uint32_t fwAt = 0, upAt = 0;
void cloudBegin() {
  lock = xSemaphoreCreateMutex();
  jobs = xQueueCreate(8, sizeof(Job));
  xTaskCreatePinnedToCore(worker, "https", 16384, nullptr, 1, nullptr, 0);
}
void cloudMarkGood() { esp_ota_mark_app_valid_cancel_rollback(); }
void cloudStep() {
  uint32_t now = millis();
  if (S.net != NET_UP) { upAt = 0; return; }
  if (!upAt) { upAt = now; fwAt = now - (UPD_HOURS[cfg.updIdx % N_UPD] * 3600000u) + 20000; }   // first check 20 s after WiFi came
  uint32_t every = UPD_HOURS[cfg.updIdx % N_UPD] * 3600000u;
  if (now - fwAt > every && want(J_FW) && S.upd != UPD_LOADING) { fwAt = now; post(J_FW); }
}
