// The connection to error.blue: a WebSocket to wss://error.blue/thuisfront/dev/ws?id=HF-XXXXXX
// It brings the web app's state, the text picture (HFT1) and video frames (HFV1). See src/homefront.js.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <HTTPClient.h>
#include <Update.h>
#include "config.h"
#include "certs.h"
#include "hf.h"
extern "C" {
#include "esp32s3/rom/tjpgd.h"     // the JPEG decoder in the chip's ROM
}

Shared G;

static WiFiClientSecure tls;
static bool insecure = false;                  // the certificate check failed once: go on without it
static uint32_t nextTry = 0, backoff = 1000, lastPing = 0, lastRx = 0, saveAt = 0;
static uint8_t* msg = nullptr;                 // the message being read
static size_t msgCap = 0, msgLen = 0;
static uint8_t msgOp = 0;
static uint8_t* pendingText = nullptr;         // an HFT1 waiting for the state that names it
static size_t pendingLen = 0;
static uint8_t* rawState = nullptr;            // the last state message, saved to flash a bit later
static size_t rawStateLen = 0;
static uint8_t* pendingPhoto = nullptr;        // an HFP1 waiting for the state that names it
static size_t pendingPhotoLen = 0;
static uint8_t* photoJpg = nullptr;            // the photo as it came (to put a new filter on it)
static size_t photoJpgLen = 0;
static int photoPal = -1;                      // the filter the shown photo has
static uint16_t* photoBack = nullptr;          // decode here, then swap with G.photo
static uint16_t* colorBack = nullptr;          // the same for colour video
static bool photoFull = false;                 // the photo (or the whole animation) that st.pb names is here
static uint16_t* owned[64];                    // the frames of the animation (G.anim shows them)
static int ownedN = 0;
// new firmware over WiFi (error.blue says {t:"ota", ver, url, size, md5})
static char otaVer[16] = "", otaUrl[96] = "", otaMd5[40] = "", otaFailedVer[16] = "";
static size_t otaSize = 0;
static bool otaWant = false;
static uint32_t otaFailedAt = 0;

// ---------------- flash ----------------
static bool saveFile(const char* path, const uint8_t* d, size_t n) {
  File f = LittleFS.open(path, "w");
  if (!f) { Serial.printf("FS: can't write %s\n", path); return false; }
  size_t w = f.write(d, n); f.close();
  if (w != n) { Serial.printf("FS: %s didn't fit\n", path); LittleFS.remove(path); return false; }
  return true;
}
static uint8_t* loadFile(const char* path, size_t& n) {
  n = 0;
  File f = LittleFS.open(path, "r");
  if (!f) return nullptr;
  n = f.size();
  uint8_t* d = (uint8_t*)ps_malloc(n + 1);
  if (d) { f.read(d, n); d[n] = 0; }
  f.close();
  return d;
}

static const char* const APPS[] = { "", "comm", "clock", "eta" };
static const char* const MODES[] = { "text", "video", "both", "photo" };
static const char* const ANIMS[] = { "static", "scroll", "rise", "type", "blink", "wave", "glitch", "pulse" };
static uint8_t pickIdx(const char* v, const char* const* list, int n) { for (int i = 0; i < n; i++) if (v && !strcmp(v, list[i])) return i; return 0; }

static bool parseState(const uint8_t* d, size_t n, HfState& o) {
  JsonDocument doc;
  if (deserializeJson(doc, (const char*)d, n)) return false;
  if (strcmp(doc["t"] | "", "state")) return false;
  JsonObjectConst s = doc["state"];
  if (s.isNull()) return false;
  o.rev = s["rev"] | 0;
  o.app = pickIdx(s["app"] | "", APPS, 4);
  o.screen = (s["screen"] | 1) != 0;
  o.bright = constrain((int)(s["bright"] | 80), 5, 100);
  o.scan = (s["scan"] | 0) != 0;
  o.fx = (s["fx"] | 1) != 0;
  strlcpy(o.name, s["name"] | "", sizeof(o.name));
  JsonObjectConst c = s["comm"];
  o.comm.mode = pickIdx(c["mode"] | "text", MODES, 4);
  o.comm.anim = pickIdx(c["anim"] | "static", ANIMS, 8);
  o.comm.speed = constrain((int)(c["speed"] | 5), 1, 10);
  o.comm.tpal = constrain((int)(c["tpal"] | 0), 0, 3);
  o.comm.vpal = constrain((int)(c["vpal"] | 0), 0, 4);
  o.comm.ppal = constrain((int)(c["ppal"] | NOFILTER), 0, 4);
  strlcpy(o.pb, c["pb"] | "", sizeof(o.pb));
  o.comm.bg = constrain((int)(c["bg"] | 0), 0, 2);
  strlcpy(o.tb, c["tb"] | "", sizeof(o.tb));
  JsonObjectConst k = s["clock"];
  o.clock.pal = constrain((int)(k["pal"] | 0), 0, 3);
  o.clock.bg = constrain((int)(k["bg"] | 0), 0, 1);
  o.clock.h24 = (k["h24"] | 1) != 0;
  o.clock.sec = constrain((int)(k["sec"] | 1), 0, 2);
  o.clock.date = (k["date"] | 1) != 0;
  strlcpy(o.ptz, k["ptz"] | "", sizeof(o.ptz));
  o.clock.motion = (k["motion"] | 0) != 0;
  o.clock.msens = constrain((int)(k["msens"] | 5), 1, 10);
  o.clock.mtime = constrain((int)(k["mtime"] | 60), 5, 3600);
  JsonObjectConst e = s["eta"];
  o.eta.from = e["from"] | 0;
  o.eta.to = e["to"] | 0;
  o.eta.pal = constrain((int)(e["pal"] | 0), 0, 3);
  o.eta.done = (e["done"] | 0) != 0;
  o.eta.lang = constrain((int)(e["lang"] | 0), 0, 1);
  strlcpy(o.eta.text, e["text"] | "", sizeof(o.eta.text));
  return true;
}

// ---------------- firmware over WiFi ----------------
static bool parseOta(const uint8_t* d, size_t n) {
  JsonDocument doc;
  if (deserializeJson(doc, (const char*)d, n)) return false;
  if (strcmp(doc["t"] | "", "ota")) return false;
  strlcpy(otaVer, doc["ver"] | "", sizeof(otaVer));
  strlcpy(otaUrl, doc["url"] | "", sizeof(otaUrl));
  strlcpy(otaMd5, doc["md5"] | "", sizeof(otaMd5));
  otaSize = doc["size"] | 0;
  if (!otaVer[0] || otaUrl[0] != '/' || otaSize < 100000 || !strcmp(otaVer, HF_VERSION)) return true;
  if (!strcmp(otaFailedVer, otaVer) && millis() - otaFailedAt < 3600000) return true;   // it failed: not again for an hour
  otaWant = true;
  return true;
}
class OtaSink : public Stream {                  // the download goes straight into the other app slot
 public:
  size_t total, done = 0, calls = 0;
  explicit OtaSink(size_t t) : total(t) {}
  size_t write(const uint8_t* b, size_t n) override {
    size_t w = Update.write((uint8_t*)b, n);
    done += w;
    G.otaPct = (int)(done * 100 / total);
    if ((++calls & 15) == 0) vTaskDelay(1);
    return w;
  }
  size_t write(uint8_t c) override { return write(&c, 1); }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};
static void doOta() {
  Serial.printf("OTA: %s -> %s, %u bytes\n", HF_VERSION, otaVer, (unsigned)otaSize);
  snprintf(G.otaMsg, sizeof(G.otaMsg), "%s > %s", HF_VERSION, otaVer);
  G.otaPct = 0; G.otaAt = millis();
  linkDrop();                                    // free the memory of the other connection first
  bool ok = false;
  const char* err = "";
  {
    WiFiClientSecure c;
    if (insecure) c.setInsecure(); else c.setCACert(ROOT_CAS);
    HTTPClient http;
    http.setTimeout(20000);
    String url = String("https://") + HF_HOST + otaUrl;
    if (!http.begin(c, url)) err = "NO CONNECTION";
    else {
      int code = http.GET();
      int len = http.getSize();
      if (code != 200) { err = "THE SERVER SAID NO"; Serial.printf("OTA: HTTP %d\n", code); }
      else if (len > 0 && (size_t)len != otaSize) err = "WRONG SIZE";
      else if (!Update.begin(otaSize)) err = "NO ROOM (USB UPDATE NEEDED)";
      else {
        if (strlen(otaMd5) == 32) Update.setMD5(otaMd5);
        OtaSink sink(otaSize);
        int n = http.writeToStream(&sink);
        if (n < 0 || sink.done != otaSize) { err = "DOWNLOAD BROKE OFF"; Update.abort(); Serial.printf("OTA: got %u of %u (%d)\n", (unsigned)sink.done, (unsigned)otaSize, n); }
        else if (!Update.end()) err = "CHECK FAILED";
        else ok = true;
      }
      http.end();
    }
  }
  if (ok) {
    Serial.println("OTA: done, restarting");
    G.otaPct = 100; strlcpy(G.otaMsg, "DONE. RESTARTING...", sizeof(G.otaMsg));
    delay(1200);
    ESP.restart();
  }
  Serial.printf("OTA: failed: %s (%s)\n", err, Update.errorString());
  strlcpy(otaFailedVer, otaVer, sizeof(otaFailedVer)); otaFailedAt = millis();
  snprintf(G.otaMsg, sizeof(G.otaMsg), "FAILED: %s", err);
  G.otaPct = -2; G.otaAt = millis();
  nextTry = millis() + 1000;
}

// ---------------- JPEG -> 320x172 RGB565 (stretched to fit), through a filter ----------------
struct JpgJob { const uint8_t* d; size_t n, pos; uint16_t* out; int w, h; const uint16_t* ramp; };
static UINT jpgRead(JDEC* jd, BYTE* buf, UINT len) {
  JpgJob* j = (JpgJob*)jd->device;
  if (j->pos + len > j->n) len = j->n - j->pos;
  if (buf) memcpy(buf, j->d + j->pos, len);
  j->pos += len;
  return len;
}
static UINT jpgWrite(JDEC* jd, void* bitmap, JRECT* r) {
  JpgJob* j = (JpgJob*)jd->device;
  const uint8_t* px = (const uint8_t*)bitmap;            // RGB888
  for (int y = r->top; y <= r->bottom; y++) {
    int y0 = y * LCD_H / j->h, y1 = (y + 1) * LCD_H / j->h;
    for (int x = r->left; x <= r->right; x++, px += 3) {
      uint16_t c = j->ramp ? j->ramp[(px[0] * 77 + px[1] * 150 + px[2] * 29) >> 8] : lcdColor(px[0], px[1], px[2]);
      int x0 = x * LCD_W / j->w, x1 = (x + 1) * LCD_W / j->w;
      for (int yy = y0; yy < y1 && yy < LCD_H; yy++) for (int xx = x0; xx < x1 && xx < LCD_W; xx++) j->out[yy * LCD_W + xx] = c;
    }
  }
  return 1;
}
static bool jpegToPlane(const uint8_t* d, size_t n, uint16_t* out, int pal) {
  for (int i = 0; i < 50 && G.pushing == out; i++) vTaskDelay(1);   // the screen is still reading this one
  static uint8_t* work = nullptr;
  static uint16_t ramp[256];
  if (!work) work = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!work || !out) return false;
  JpgJob j = { d, n, 0, out, 0, 0, nullptr };
  if (pal >= 0 && pal < NOFILTER) { scrPhotoRamp(pal, ramp); j.ramp = ramp; }
  JDEC jd;
  JRESULT r = jd_prepare(&jd, jpgRead, work, 4096, &j);
  if (r != JDR_OK) { Serial.printf("JPEG: can't read it (%d)\n", (int)r); return false; }
  j.w = jd.width; j.h = jd.height;
  if (!j.w || !j.h) return false;
  r = jd_decomp(&jd, jpgWrite, 0);
  if (r != JDR_OK) { Serial.printf("JPEG: broken (%d)\n", (int)r); return false; }
  return true;
}
// the clock's time zone (POSIX TZ); "" = UTC. Under the lock: the screen side reads the time with it.
// Compared with the TZ that is really set, not with the last one asked for: configTime() (the internet time)
// sets TZ to UTC behind our back, and then the clock showed UTC (an hour early in Amsterdam) until the next change.
static void applyTz(const char* ptz) {
  const char* want = ptz[0] ? ptz : "UTC0";
  xSemaphoreTake(G.lock, portMAX_DELAY);
  const char* cur = getenv("TZ");
  bool same = cur && !strcmp(cur, want);
  if (!same) { setenv("TZ", want, 1); tzset(); }
  xSemaphoreGive(G.lock);
  if (same) return;
  Serial.printf("CLOCK: time zone %s\n", ptz[0] ? ptz : "UTC");
}
// show the photo (a JPEG) with this filter; keeps the JPEG to change the filter later
static void showPhoto(const uint8_t* jpg, size_t n, int pal) {
  if (!photoBack) photoBack = (uint16_t*)ps_malloc(LCD_W * LCD_H * 2);
  if (!jpegToPlane(jpg, n, photoBack, pal)) return;
  xSemaphoreTake(G.lock, portMAX_DELAY);
  uint16_t* t = G.photo; G.photo = photoBack; photoBack = t;
  xSemaphoreGive(G.lock);
  photoPal = pal;
}
// HFA1, an animated photo: "HFA1" u16 n, u16 0, n x (u16 delay ms, u32 bytes), then the n JPEGs. -> n (0 = not one)
struct AnimFrame { const uint8_t* jpg; uint32_t len; uint16_t delay; };
static int animParse(const uint8_t* d, size_t n, AnimFrame* out) {
  if (n < 8 || memcmp(d, "HFA1", 4)) return 0;
  int cnt = d[4] | d[5] << 8;
  if (cnt < 1 || cnt > 64) return 0;
  size_t o = 8 + 6 * (size_t)cnt;
  if (o > n) return 0;
  for (int i = 0; i < cnt; i++) {
    const uint8_t* t = d + 8 + 6 * i;
    uint32_t len = t[2] | t[3] << 8 | t[4] << 16 | (uint32_t)t[5] << 24;
    if (len > n - o) return 0;
    out[i] = { d + o, len, (uint16_t)constrain((int)(t[0] | t[1] << 8), 20, 10000) };
    o += len;
  }
  return cnt;
}
// stop the animation and free its frames (the screen side may be sending one right now: wait for it)
static void animStop() {
  xSemaphoreTake(G.lock, portMAX_DELAY);
  G.animN = 0; G.animTotal = 0;
  xSemaphoreGive(G.lock);
  for (int w = 0; w < 100; w++) {
    bool busy = false;
    for (int k = 0; k < ownedN; k++) if (G.pushing == owned[k]) busy = true;
    if (!busy) break;
    vTaskDelay(1);
  }
  for (int k = 0; k < ownedN; k++) free(owned[k]);
  ownedN = 0;
}
// show photoJpg with this filter: a JPEG, or an animation (its first frame at once, then every frame decoded
// into its own picture so playing it is only choosing one; as many as fit in memory)
static void showPicture(int pal) {
  static AnimFrame fr[64];
  int n = animParse(photoJpg, photoJpgLen, fr);
  animStop();
  if (!n) { showPhoto(photoJpg, photoJpgLen, pal); return; }
  showPhoto(fr[0].jpg, fr[0].len, pal);
  photoPal = pal;
  if (n < 2) return;
  // how many frames fit (keeping 700 KB for the rest); fewer than n: every step-th frame, shown as long
  const size_t one = LCD_W * LCD_H * 2, freeNow = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  int fit = freeNow > 700000 + one ? (int)((freeNow - 700000) / one) : 0;
  if (fit < 2) { Serial.println("PHOTO: no room for the animation, showing its first frame"); return; }
  int step = (n + fit - 1) / fit;
  static uint16_t delays[64];
  uint32_t total = 0, t0 = millis();
  for (int k = 0; k < n; k += step) {
    uint16_t* p = (uint16_t*)ps_malloc(one);
    if (!p) break;
    if (!jpegToPlane(fr[k].jpg, fr[k].len, p, pal)) { free(p); break; }
    uint32_t d = 0;
    for (int j = k; j < n && j < k + step; j++) d += fr[j].delay;
    delays[ownedN] = (uint16_t)min(d, (uint32_t)60000);
    owned[ownedN++] = p; total += delays[ownedN - 1];
    vTaskDelay(1);                               // let the rest of the network side breathe
  }
  Serial.printf("PHOTO: animation, %d of %d frames ready in %u ms, %u ms long, %u KB memory left\n", ownedN, n,
                (unsigned)(millis() - t0), (unsigned)total, (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10));
  if (ownedN < 2) return;
  xSemaphoreTake(G.lock, portMAX_DELAY);
  for (int k = 0; k < ownedN; k++) { G.anim[k] = owned[k]; G.animDelay[k] = delays[k]; }
  G.animN = ownedN; G.animTotal = total; G.animT0 = millis();
  xSemaphoreGive(G.lock);
}

void linkLoadSaved() {
  size_t n;
  uint8_t* d = loadFile("/state.json", n);
  HfState st;
  if (d && parseState(d, n, st)) {
    G.st = st; G.haveState = true;
    Serial.printf("FS: last web app: %s, rev %u\n", st.app == APP_COMM ? "COMM" : st.app == APP_CLOCK ? "CLOCK" : st.app == APP_ETA ? "ETA" : "none", (unsigned)st.rev);
    applyTz(st.ptz);
    size_t bn = 0, tn = 0;
    uint8_t* tbn = loadFile("/text.tb", bn);
    uint8_t* tx = loadFile("/text.bin", tn);
    if (tbn && tx && !strcmp((char*)tbn, st.tb)) G.text = textDecode(tx, tn);
    free(tbn); free(tx);
    size_t pn = 0, jn = 0;
    uint8_t* pbn = loadFile("/photo.pb", pn);
    uint8_t* jpg = loadFile("/photo.jpg", jn);
    char part[24]; snprintf(part, sizeof(part), "%s*", st.pb);   // "name*" = only the first frame of an animation was saved
    if (pbn && jpg && st.pb[0] && (!strcmp((char*)pbn, st.pb) || !strcmp((char*)pbn, part))) {
      photoJpg = jpg; photoJpgLen = jn; jpg = nullptr;
      showPicture(st.comm.ppal);
      photoFull = !strcmp((char*)pbn, st.pb);    // not whole: error.blue sends it again when the board says hi
    }
    free(pbn); free(jpg);
  }
  free(d);
}

// ---------------- WebSocket ----------------
static bool readExact(uint8_t* p, size_t n, uint32_t ms = 8000) {
  uint32_t t0 = millis();
  while (n) {
    int a = tls.available();
    if (a > 0) {
      int r = tls.read(p, min((size_t)a, n));
      if (r > 0) { p += r; n -= r; t0 = millis(); continue; }
    }
    if (!tls.connected()) return false;
    if (millis() - t0 > ms) return false;
    delay(1);
  }
  return true;
}
static bool wsSend(uint8_t op, const uint8_t* d, size_t n) {
  uint8_t h[14];
  size_t hl = 2;
  h[0] = 0x80 | op;
  if (n < 126) h[1] = 0x80 | n;
  else if (n < 65536) { h[1] = 0x80 | 126; h[2] = n >> 8; h[3] = n & 255; hl = 4; }
  else { h[1] = 0x80 | 127; for (int i = 0; i < 8; i++) h[2 + i] = (uint64_t)n >> (56 - 8 * i); hl = 10; }
  uint32_t m = esp_random();
  memcpy(h + hl, &m, 4); hl += 4;
  const uint8_t* mk = h + hl - 4;
  if (tls.write(h, hl) != hl) return false;
  uint8_t buf[256];
  for (size_t o = 0; o < n; o += sizeof(buf)) {
    size_t k = min(sizeof(buf), n - o);
    for (size_t i = 0; i < k; i++) buf[i] = d[o + i] ^ mk[(o + i) & 3];
    if (tls.write(buf, k) != k) return false;
  }
  return true;
}
static bool wsText(const String& s) { return wsSend(1, (const uint8_t*)s.c_str(), s.length()); }

static void sendHi() {
  JsonDocument d;
  d["t"] = "hi"; d["id"] = G.id; d["kind"] = "esp"; d["ver"] = HF_VERSION; d["ota"] = 1; d["imu"] = G.imu ? 1 : 0;
  d["flash"] = (int)(ESP.getFlashChipSize() >> 20);
  if (otaFailedVer[0]) d["otafail"] = otaFailedVer;
  d["name"] = prefGet("name", G.id); d["rename"] = prefGet("renamed", "0") == "1" ? 1 : 0;
  d["home"] = prefGet("home"); d["wifi"] = G.wifi; d["ip"] = G.ip; d["rssi"] = WiFi.RSSI();
  xSemaphoreTake(G.lock, portMAX_DELAY);
  d["tb"] = G.text ? G.st.tb : ""; d["rev"] = G.st.rev;
  d["pb"] = G.photo && photoFull ? G.st.pb : "";
  xSemaphoreGive(G.lock);
  String s; serializeJson(d, s);
  wsText(s);
}

static bool wsOpen() {
  if (!insecure) tls.setCACert(ROOT_CAS); else tls.setInsecure();
  tls.setHandshakeTimeout(12);
  if (!tls.connect(HF_HOST, HF_PORT)) {
    char e[96] = "";
    int code = tls.lastError(e, sizeof(e));
    Serial.printf("LINK: TLS failed (%d) %s\n", code, e);
    String es = e; es.toUpperCase();
    if (!insecure && (es.indexOf("X509") >= 0 || es.indexOf("VERIFY") >= 0 || es.indexOf("CERT") >= 0)) {
      insecure = true;
      Serial.println("LINK: certificate check failed, going on without it");
    }
    return false;
  }
  uint8_t key[16];
  for (int i = 0; i < 16; i += 4) { uint32_t r = esp_random(); memcpy(key + i, &r, 4); }
  static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char k64[25]; int o = 0;
  for (int i = 0; i < 16; i += 3) {
    uint32_t v = key[i] << 16 | (i + 1 < 16 ? key[i + 1] << 8 : 0) | (i + 2 < 16 ? key[i + 2] : 0);
    k64[o++] = B64[v >> 18 & 63]; k64[o++] = B64[v >> 12 & 63];
    k64[o++] = i + 1 < 16 ? B64[v >> 6 & 63] : '='; k64[o++] = i + 2 < 16 ? B64[v & 63] : '=';
  }
  k64[o] = 0;
  String req = String("GET ") + HF_WS_PATH + "?id=" + G.id + " HTTP/1.1\r\nHost: " HF_HOST "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
               "Sec-WebSocket-Key: " + k64 + "\r\nSec-WebSocket-Version: 13\r\nUser-Agent: HomeFront/" HF_VERSION "\r\n\r\n";
  tls.print(req);
  // the answer: "HTTP/1.1 101 ..." and headers up to an empty line
  String line; bool first = true, ok = false;
  uint32_t t0 = millis();
  while (millis() - t0 < 10000) {
    if (!tls.available()) { if (!tls.connected()) break; delay(2); continue; }
    char c = tls.read();
    if (c == '\r') continue;
    if (c != '\n') { if (line.length() < 200) line += c; continue; }
    if (first) { ok = line.indexOf(" 101") > 0; first = false; if (!ok) Serial.printf("LINK: server said %s\n", line.c_str()); }
    else if (!line.length()) break;
    line = "";
  }
  if (!ok) { tls.stop(); return false; }
  Serial.println("LINK: connected to error.blue");
  lastRx = lastPing = millis();
  sendHi();
  return true;
}

void linkDrop() {
  if (G.wsUp || tls.connected()) { tls.stop(); }
  G.wsUp = false;
}

static void handleState(const uint8_t* d, size_t n) {
  HfState st;
  if (!parseState(d, n, st)) return;
  TextPic* nt = nullptr;
  if (pendingText) {
    if (st.tb[0]) {
      nt = textDecode(pendingText, pendingLen);
      LittleFS.remove("/text.tb");               // the name only once the picture is saved whole
      if (nt && pendingLen < 90000 && saveFile("/text.bin", pendingText, pendingLen)) saveFile("/text.tb", (const uint8_t*)st.tb, strlen(st.tb) + 1);
    }
    free(pendingText); pendingText = nullptr;
  }
  if (pendingPhoto) {                            // a new photo
    if (st.pb[0]) {
      free(photoJpg); photoJpg = pendingPhoto; photoJpgLen = pendingPhotoLen; pendingPhoto = nullptr;
      showPicture(st.comm.ppal);
      photoFull = true;
      LittleFS.remove("/photo.pb");               // the flash is small (128 KB): a big animation keeps only its first frame
      AnimFrame f0;
      if (photoJpgLen < 100000 && saveFile("/photo.jpg", photoJpg, photoJpgLen)) saveFile("/photo.pb", (const uint8_t*)st.pb, strlen(st.pb) + 1);
      else if (animParse(photoJpg, photoJpgLen, &f0) && f0.len < 100000 && saveFile("/photo.jpg", f0.jpg, f0.len)) {
        char part[24]; snprintf(part, sizeof(part), "%s*", st.pb);
        saveFile("/photo.pb", (const uint8_t*)part, strlen(part) + 1);
      }
    }
    free(pendingPhoto); pendingPhoto = nullptr;
  } else if (photoJpg && st.comm.ppal != photoPal) showPicture(st.comm.ppal);   // another filter
  applyTz(st.ptz);
  xSemaphoreTake(G.lock, portMAX_DELAY);
  G.st = st; G.haveState = true;
  TextPic* old = nullptr;
  if (nt) { old = G.text; G.text = nt; G.textAt = millis(); }
  xSemaphoreGive(G.lock);
  textFree(old);
  if (st.name[0] && prefGet("name") != st.name) prefSet("name", st.name);
  if (prefGet("renamed", "0") != "0") prefSet("renamed", "0");
  free(rawState);
  rawState = (uint8_t*)ps_malloc(n); memcpy(rawState, d, n); rawStateLen = n;
  saveAt = millis() + 2000;                    // to flash when it stops changing
}

static void handleMessage() {
  if (msgOp == 1) { if (!parseOta(msg, msgLen)) handleState(msg, msgLen); return; }
  if (msgOp != 2 || msgLen < 12) return;
  if (!memcmp(msg, "HFV1", 4)) {
    int w = msg[4] | msg[5] << 8, h = msg[6] | msg[7] << 8;
    uint32_t seq = msg[8] | msg[9] << 8 | msg[10] << 16 | (uint32_t)msg[11] << 24;
    size_t bytes = ((size_t)w * h + 1) / 2;
    if (w > 0 && h > 0 && w <= LCD_W && h <= LCD_H && 12 + bytes <= msgLen) {
      xSemaphoreTake(G.lock, portMAX_DELAY);
      if (!G.frame) G.frame = (uint8_t*)ps_malloc(LCD_W * LCD_H / 2 + 2);
      if (G.frame) { memcpy(G.frame, msg + 12, bytes); G.fw = w; G.fh = h; G.frameAt = millis(); }
      xSemaphoreGive(G.lock);
      char b[40]; snprintf(b, sizeof(b), "{\"t\":\"got\",\"s\":%u}", (unsigned)seq);
      wsText(b);
    }
  } else if (!memcmp(msg, "HFV2", 4)) {         // colour video: a JPEG
    uint32_t seq = msg[8] | msg[9] << 8 | msg[10] << 16 | (uint32_t)msg[11] << 24;
    if (!colorBack) colorBack = (uint16_t*)ps_malloc(LCD_W * LCD_H * 2);
    if (jpegToPlane(msg + 12, msgLen - 12, colorBack, NOFILTER)) {
      xSemaphoreTake(G.lock, portMAX_DELAY);
      uint16_t* t = G.color; G.color = colorBack; colorBack = t; G.colorAt = millis();
      xSemaphoreGive(G.lock);
    }
    char b[40]; snprintf(b, sizeof(b), "{\"t\":\"got\",\"s\":%u}", (unsigned)seq);
    wsText(b);
  } else if (!memcmp(msg, "HFP1", 4)) {          // a photo: a JPEG after the 4 letters
    free(pendingPhoto);
    pendingPhoto = (uint8_t*)ps_malloc(msgLen - 4);
    if (pendingPhoto) { memcpy(pendingPhoto, msg + 4, msgLen - 4); pendingPhotoLen = msgLen - 4; }
  } else if (!memcmp(msg, "HFA1", 4)) {          // an animated photo: kept whole ("HFA1" tells it apart from a JPEG);
    free(pendingPhoto);                          // it takes the read buffer itself (up to 1 MB: no copy)
    pendingPhoto = msg; pendingPhotoLen = msgLen;
    msg = nullptr; msgCap = 0; msgLen = 0;
  } else if (!memcmp(msg, "HFT1", 4)) {
    free(pendingText);
    pendingText = (uint8_t*)ps_malloc(msgLen);
    if (pendingText) { memcpy(pendingText, msg, msgLen); pendingLen = msgLen; }
  }
}

// read one WebSocket frame (and its continuation frames); false = connection broken
static bool readFrame() {
  uint8_t h[2];
  if (!readExact(h, 2)) return false;
  uint8_t op = h[0] & 15;
  bool fin = h[0] & 0x80, masked = h[1] & 0x80;
  uint64_t n = h[1] & 127;
  if (n == 126) { uint8_t e[2]; if (!readExact(e, 2)) return false; n = e[0] << 8 | e[1]; }
  else if (n == 127) { uint8_t e[8]; if (!readExact(e, 8)) return false; n = 0; for (int i = 0; i < 8; i++) n = n << 8 | e[i]; }
  uint8_t mk[4] = { 0, 0, 0, 0 };
  if (masked && !readExact(mk, 4)) return false;
  if (n > 2500000) return false;
  if (op >= 8) {                                 // control: close, ping, pong
    uint8_t c[126];
    if (n > sizeof(c) || !readExact(c, n)) return false;
    if (op == 8) return false;
    if (op == 9) wsSend(10, c, n);
    return true;
  }
  if (op) { msgOp = op; msgLen = 0; }            // a new message (op 0 = more of the last one)
  if (msgLen + n > msgCap) {
    size_t cap = max((size_t)(msgLen + n), (size_t)32768);
    uint8_t* nb = (uint8_t*)ps_realloc(msg, cap);
    if (!nb) return false;
    msg = nb; msgCap = cap;
  }
  if (!readExact(msg + msgLen, n, 15000)) return false;
  if (masked) for (uint64_t i = 0; i < n; i++) msg[msgLen + i] ^= mk[i & 3];
  msgLen += n;
  if (fin) {
    handleMessage();
    if (msgCap > 262144) { free(msg); msg = nullptr; msgCap = 0; }   // don't keep a big read buffer: the memory is for pictures
  }
  return true;
}

void linkStep() {
  uint32_t now = millis();
  if (saveAt && (int32_t)(now - saveAt) > 0) {
    saveAt = 0;
    if (rawState) saveFile("/state.json", rawState, rawStateLen);
  }
  if (G.wantRelink) { G.wantRelink = false; linkDrop(); nextTry = 0; }
  if (otaWant) { otaWant = false; doOta(); return; }
  if (!G.wsUp) {
    if ((int32_t)(now - nextTry) < 0) return;
    if (time(nullptr) < 1600000000) {             // the certificate check needs the date
      xSemaphoreTake(G.lock, portMAX_DELAY);     // (it also sets TZ: the screen side must not read the time meanwhile)
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      xSemaphoreGive(G.lock);
      char ptz[64];
      xSemaphoreTake(G.lock, portMAX_DELAY); strlcpy(ptz, G.st.ptz, sizeof(ptz)); xSemaphoreGive(G.lock);
      applyTz(ptz);                              // configTime() just set TZ to UTC: put the clock's time zone back
      uint32_t t0 = millis();
      while (time(nullptr) < 1600000000 && millis() - t0 < 4000) delay(50);
    }
    G.wsUp = wsOpen();
    if (!G.wsUp) { nextTry = millis() + backoff; backoff = min(backoff * 2, (uint32_t)30000); return; }
    backoff = 1000;
  }
  // read what's there
  uint32_t t0 = millis();
  while (tls.available() >= 2 && millis() - t0 < 50) {
    if (!readFrame()) { Serial.println("LINK: connection lost"); linkDrop(); nextTry = millis() + 1000; return; }
    lastRx = millis();
  }
  if (!tls.connected()) { Serial.println("LINK: closed"); linkDrop(); nextTry = millis() + 1000; return; }
  if (millis() - lastPing > 15000) {
    lastPing = millis();
    char b[200], lt[8] = "", tz[64] = "";
    xSemaphoreTake(G.lock, portMAX_DELAY);       // the time as the screen shows it (and the TZ it uses), to check from afar
    time_t tt = time(nullptr); struct tm tmv;
    if (tt > 1600000000) { localtime_r(&tt, &tmv); strftime(lt, sizeof(lt), "%H:%M", &tmv); }
    const char* e = getenv("TZ"); if (e) strlcpy(tz, e, sizeof(tz));
    xSemaphoreGive(G.lock);
    snprintf(b, sizeof(b), "{\"t\":\"ping\",\"rssi\":%d,\"fps\":%d,\"heap\":%u,\"psram\":%u,\"anim\":%d,\"lt\":\"%s\",\"tz\":\"%s\"}", WiFi.RSSI(), G.renderFps,
             (unsigned)ESP.getFreeHeap(), (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10), G.animN, lt, tz);
    if (!wsText(b)) { Serial.println("LINK: ping failed"); linkDrop(); nextTry = millis() + 1000; }
  }
}
