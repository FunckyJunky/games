// The connection to error.blue: a WebSocket to wss://error.blue/thuisfront/dev/ws?id=HF-XXXXXX
// It brings the web app's state, the text picture (HFT1) and video frames (HFV1). See src/homefront.js.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "certs.h"
#include "hf.h"

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

// ---------------- flash ----------------
static void saveFile(const char* path, const uint8_t* d, size_t n) {
  File f = LittleFS.open(path, "w");
  if (!f) { Serial.printf("FS: can't write %s\n", path); return; }
  f.write(d, n); f.close();
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

static const char* const MODES[] = { "text", "video", "both" };
static const char* const ANIMS[] = { "static", "scroll", "rise", "type", "blink", "wave", "glitch", "pulse" };
static uint8_t pickIdx(const char* v, const char* const* list, int n) { for (int i = 0; i < n; i++) if (v && !strcmp(v, list[i])) return i; return 0; }

static bool parseState(const uint8_t* d, size_t n, HfState& o) {
  JsonDocument doc;
  if (deserializeJson(doc, (const char*)d, n)) return false;
  if (strcmp(doc["t"] | "", "state")) return false;
  JsonObjectConst s = doc["state"];
  if (s.isNull()) return false;
  o.rev = s["rev"] | 0;
  o.app = !strcmp(s["app"] | "", "comm");
  o.screen = (s["screen"] | 1) != 0;
  o.bright = constrain((int)(s["bright"] | 80), 5, 100);
  o.scan = (s["scan"] | 0) != 0;
  strlcpy(o.name, s["name"] | "", sizeof(o.name));
  JsonObjectConst c = s["comm"];
  o.comm.mode = pickIdx(c["mode"] | "text", MODES, 3);
  o.comm.anim = pickIdx(c["anim"] | "static", ANIMS, 8);
  o.comm.speed = constrain((int)(c["speed"] | 5), 1, 10);
  o.comm.tpal = constrain((int)(c["tpal"] | 0), 0, 3);
  o.comm.vpal = constrain((int)(c["vpal"] | 0), 0, 3);
  o.comm.bg = constrain((int)(c["bg"] | 0), 0, 2);
  strlcpy(o.tb, c["tb"] | "", sizeof(o.tb));
  return true;
}

void linkLoadSaved() {
  size_t n;
  uint8_t* d = loadFile("/state.json", n);
  HfState st;
  if (d && parseState(d, n, st)) {
    G.st = st; G.haveState = true;
    Serial.printf("FS: last web app: %s, rev %u\n", st.app ? "COMM" : "none", (unsigned)st.rev);
    size_t bn = 0, tn = 0;
    uint8_t* tbn = loadFile("/text.tb", bn);
    uint8_t* tx = loadFile("/text.bin", tn);
    if (tbn && tx && !strcmp((char*)tbn, st.tb)) G.text = textDecode(tx, tn);
    free(tbn); free(tx);
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
  d["t"] = "hi"; d["id"] = G.id; d["kind"] = "esp"; d["ver"] = HF_VERSION;
  d["name"] = prefGet("name", G.id); d["rename"] = prefGet("renamed", "0") == "1" ? 1 : 0;
  d["home"] = prefGet("home"); d["wifi"] = G.wifi; d["ip"] = G.ip; d["rssi"] = WiFi.RSSI();
  xSemaphoreTake(G.lock, portMAX_DELAY);
  d["tb"] = G.text ? G.st.tb : ""; d["rev"] = G.st.rev;
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
      if (nt && pendingLen < 400000) { saveFile("/text.bin", pendingText, pendingLen); saveFile("/text.tb", (const uint8_t*)st.tb, strlen(st.tb) + 1); }
    }
    free(pendingText); pendingText = nullptr;
  }
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
  if (msgOp == 1) { handleState(msg, msgLen); return; }
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
  if (fin) handleMessage();
  return true;
}

void linkStep() {
  uint32_t now = millis();
  if (saveAt && (int32_t)(now - saveAt) > 0) {
    saveAt = 0;
    if (rawState) saveFile("/state.json", rawState, rawStateLen);
  }
  if (G.wantRelink) { G.wantRelink = false; linkDrop(); nextTry = 0; }
  if (!G.wsUp) {
    if ((int32_t)(now - nextTry) < 0) return;
    if (time(nullptr) < 1600000000) {             // the certificate check needs the date
      configTime(0, 0, "pool.ntp.org", "time.google.com");
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
    char b[96];
    snprintf(b, sizeof(b), "{\"t\":\"ping\",\"rssi\":%d,\"fps\":%d,\"heap\":%u}", WiFi.RSSI(), G.renderFps, (unsigned)ESP.getFreeHeap());
    if (!wsText(b)) { Serial.println("LINK: ping failed"); linkDrop(); nextTry = millis() + 1000; }
  }
}
