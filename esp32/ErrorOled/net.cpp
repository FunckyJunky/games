// WiFi, PHONE SETUP and HTTPS to error.blue. See net.h.
#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <time.h>
#include "config.h"
#include "certs.h"
#include "ui.h"

static Settings* C;
static void (*saveCfg)();
static volatile uint8_t state = NET_OFF;
static uint32_t joinAt = 0, retryAt = 0;
static char joinSsid[33], joinPass[65];
static bool joinPending = false;           // new network: saved only once it works
static char ip[16] = "";
static bool timeStarted = false;
static char err[72] = "";

static WifiNet scanList[16];
static int scanN = -1;
static bool scanRunning = false;

static WebServer* web = nullptr;
static DNSServer* dns = nullptr;

struct Job { uint8_t kind; bool post; char* path; char* body; };
struct Result { uint8_t kind; int status; char* body; size_t len; };
static QueueHandle_t jobQ, resQ;
static volatile uint32_t busy = 0;

String jsonEscape(const char* s) {
  String o;
  for (; *s; s++) {
    char c = *s;
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", (uint8_t)c); o += b; }
    else o += c;
  }
  return o;
}
String urlEncode(const char* s) {
  String o;
  for (; *s; s++) {
    uint8_t c = *s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
    else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); o += b; }
  }
  return o;
}
static String htmlEscape(const char* s) {
  String o;
  for (; *s; s++) {
    if (*s == '<') o += "&lt;"; else if (*s == '>') o += "&gt;"; else if (*s == '&') o += "&amp;";
    else if (*s == '"') o += "&quot;"; else o += *s;
  }
  return o;
}

// ------------------------------------------------------------------ HTTPS task
static void netTask(void*) {
  WiFiClientSecure tls;
  HTTPClient http;
  http.setReuse(true);                     // keep the connection: no new TLS handshake every time
  int lastCheck = -1;
  for (;;) {
    Job j;
    if (xQueueReceive(jobQ, &j, portMAX_DELAY) != pdTRUE) continue;
    Result r = {j.kind, -1, nullptr, 0};
    if (WiFi.status() == WL_CONNECTED) {
      if (lastCheck != C->tlsCheck) {
        tls.stop();
        if (C->tlsCheck) tls.setCACert(ROOT_CAS); else tls.setInsecure();
        lastCheck = C->tlsCheck;
      }
      String url = String("https://") + SERVER_HOST + j.path;
      if (http.begin(tls, url)) {
        http.setConnectTimeout(8000);
        http.setTimeout(8000);
        http.setUserAgent("ERROR-OLED/1.0");
        int code;
        if (j.post) {
          http.addHeader("Content-Type", "application/json");
          code = http.POST((uint8_t*)j.body, strlen(j.body));
        } else code = http.GET();
        r.status = code;
        if (code > 0) {
          String s = http.getString();
          r.len = s.length();
          r.body = (char*)malloc(r.len + 1);
          if (r.body) memcpy(r.body, s.c_str(), r.len + 1); else r.len = 0;
          if (code == 200) err[0] = 0; else snprintf(err, sizeof(err), "HTTP %d", code);
        } else {
          char tlsErr[64] = "";
          if (tls.lastError(tlsErr, sizeof(tlsErr)) != 0) snprintf(err, sizeof(err), "TLS %s", tlsErr);
          else snprintf(err, sizeof(err), "%s", HTTPClient::errorToString(code).c_str());
          tls.stop();
        }
        http.end();
      } else snprintf(err, sizeof(err), "BAD ADDRESS");
    } else snprintf(err, sizeof(err), "NO WIFI");
    free(j.path);
    free(j.body);
    xQueueSend(resQ, &r, portMAX_DELAY);
  }
}

bool netJobBusy(uint8_t kind) { return busy & (1u << kind); }
void netRequest(uint8_t kind, const char* method, const String& path, const String& body) {
  Job j;
  j.kind = kind;
  j.post = !strcmp(method, "POST");
  j.path = strdup(path.c_str());
  j.body = strdup(body.c_str());
  if (!j.path || !j.body || xQueueSend(jobQ, &j, 0) != pdTRUE) { free(j.path); free(j.body); return; }
  busy |= 1u << kind;
}

// ------------------------------------------------------------------ WiFi
static void begin(const char* ssid, const char* pass) {
  WiFi.disconnect();
  WiFi.begin(ssid, pass);
  // the small ESP32-C3 OLED boards have a tiny antenna and often can't connect at full power
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  state = NET_CONNECTING;
  joinAt = millis();
}

void netBegin(Settings* cfg, void (*save)()) {
  C = cfg;
  saveCfg = save;
  jobQ = xQueueCreate(8, sizeof(Job));
  resQ = xQueueCreate(8, sizeof(Result));
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_NAME);
  WiFi.setAutoReconnect(true);
  if (!C->ssid[0] && WIFI_SSID[0]) { strncpy(C->ssid, WIFI_SSID, sizeof(C->ssid) - 1); strncpy(C->pass, WIFI_PASS, sizeof(C->pass) - 1); }
  if (C->ssid[0]) begin(C->ssid, C->pass);
  xTaskCreate(netTask, "net", 8192, nullptr, 1, nullptr);
}

void netJoin(const char* ssid, const char* pass) {
  strncpy(joinSsid, ssid, sizeof(joinSsid) - 1); joinSsid[sizeof(joinSsid) - 1] = 0;
  strncpy(joinPass, pass, sizeof(joinPass) - 1); joinPass[sizeof(joinPass) - 1] = 0;
  joinPending = true;
  begin(joinSsid, joinPass);
}

void netScan() {
  if (scanRunning) return;
  scanN = -1;
  scanRunning = WiFi.scanNetworks(true, false) == WIFI_SCAN_RUNNING;
  if (!scanRunning) scanN = 0;
}
int netScanResults(WifiNet* out, int max) {
  if (scanN < 0) return -1;
  int n = scanN < max ? scanN : max;
  memcpy(out, scanList, sizeof(WifiNet) * n);
  return n;
}

static void scanDone(int n) {
  scanN = 0;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    int k = 0;
    for (; k < scanN; k++) if (s == scanList[k].ssid) break;
    int rssi = WiFi.RSSI(i);
    if (k < scanN) { if (rssi > scanList[k].rssi) scanList[k].rssi = rssi; continue; }
    if (scanN == 16) continue;
    WifiNet& w = scanList[scanN++];
    strncpy(w.ssid, s.c_str(), sizeof(w.ssid) - 1); w.ssid[sizeof(w.ssid) - 1] = 0;
    w.rssi = rssi;
    w.open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  // strongest first
  for (int a = 0; a < scanN; a++)
    for (int b = a + 1; b < scanN; b++)
      if (scanList[b].rssi > scanList[a].rssi) { WifiNet t = scanList[a]; scanList[a] = scanList[b]; scanList[b] = t; }
  WiFi.scanDelete();
}

uint8_t netStateNow() { return state; }
const char* netSsidNow() { return joinPending ? joinSsid : C->ssid; }
const char* netIpNow() { return ip; }
int netRssiNow() { return state == NET_UP ? WiFi.RSSI() : -100; }
const char* netErr() { return err; }

// ------------------------------------------------------------------ PHONE SETUP
static const char PAGE_HEAD[] PROGMEM =
  "<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
  "<title>ERROR-OLED</title><style>body{background:#000;color:#9fe6ff;font:16px monospace;margin:0;padding:16px}"
  "h1{font-size:18px;background:#9fe6ff;color:#000;padding:4px 8px;margin:0 0 12px}label{display:block;margin:12px 0 4px}"
  "input{width:100%;box-sizing:border-box;background:#000;color:#9fe6ff;border:1px solid #9fe6ff;font:16px monospace;padding:8px}"
  "button{margin-top:16px;width:100%;background:#9fe6ff;color:#000;border:0;font:bold 16px monospace;padding:10px}"
  ".n{display:inline-block;border:1px solid #2a6a80;margin:3px;padding:4px 8px;cursor:pointer}p{color:#5aa}</style></head><body>";

static void pageRoot() {
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>ERROR-OLED SETUP</h1><form method=post action=/save><label>WIFI NETWORK</label><input name=ssid id=s value=\"";
  h += htmlEscape(joinPending ? joinSsid : C->ssid);
  h += "\"><div>";
  for (int i = 0; i < (scanN > 0 ? scanN : 0); i++) {
    String e = htmlEscape(scanList[i].ssid);
    h += "<span class=n onclick=\"s.value=this.textContent\">" + e + "</span>";
  }
  h += "</div><label>PASSWORD</label><input name=pass type=password>";
  h += "<label>YOUR NAME (CHAT + WHO)</label><input name=name maxlength=16 value=\"" + htmlEscape(C->name) + "\">";
  h += "<label>ERROR-CHAT CHANNEL CODE</label><input name=chan maxlength=8 value=\"" + htmlEscape(C->channel) + "\">";
  h += "<button>SAVE</button></form><p>C:\\&gt; error.blue</p></body></html>";
  web->send(200, "text/html; charset=utf-8", h);
}

static void pageSave() {
  String ssid = web->arg("ssid"), pass = web->arg("pass"), name = web->arg("name"), chan = web->arg("chan");
  name.trim();
  if (name.length()) { strncpy(C->name, name.c_str(), sizeof(C->name) - 1); C->name[sizeof(C->name) - 1] = 0; }
  chan.trim();
  chan.toUpperCase();
  bool ok = chan.length() >= 3 && chan.length() <= 8;
  for (size_t i = 0; i < chan.length(); i++) if (!isalnum((uint8_t)chan[i])) ok = false;
  if (ok) strcpy(C->channel, chan.c_str());
  saveCfg();
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>SAVED</h1>";
  if (ssid.length()) {
    if (!pass.length() && ssid == C->ssid) pass = C->pass;
    netJoin(ssid.c_str(), pass.c_str());
    h += "<p>ERROR-OLED is joining <b>" + htmlEscape(ssid.c_str()) + "</b>. Watch the little screen.</p>";
  }
  h += "<p><a style=color:#9fe6ff href=/>BACK</a></p></body></html>";
  web->send(200, "text/html; charset=utf-8", h);
  uiToast("PHONE SETUP SAVED");
}

void netPortal(bool on) {
  if (on && !web) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(DEVICE_NAME);
    dns = new DNSServer();
    dns->start(53, "*", WiFi.softAPIP());
    web = new WebServer(80);
    web->on("/", HTTP_GET, pageRoot);
    web->on("/save", HTTP_POST, pageSave);
    web->onNotFound([] { web->sendHeader("Location", "http://192.168.4.1/", true); web->send(302, "text/plain", ""); });
    web->begin();
    netScan();
  } else if (!on && web) {
    web->stop(); delete web; web = nullptr;
    dns->stop(); delete dns; dns = nullptr;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
}

// ------------------------------------------------------------------ every loop
void netLoop() {
  uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (state != NET_UP) {
      state = NET_UP;
      strncpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip) - 1);
      if (joinPending) {                     // it works: keep it
        strcpy(C->ssid, joinSsid);
        strcpy(C->pass, joinPass);
        joinPending = false;
        saveCfg();
      }
      if (!timeStarted) { configTzTime(TIMEZONE, "pool.ntp.org", "time.cloudflare.com"); timeStarted = true; }
      err[0] = 0;
    }
  } else {
    if (state == NET_UP) { state = NET_CONNECTING; joinAt = now; }      // lost it: WiFi tries again by itself
    if (state == NET_CONNECTING && now - joinAt > 20000) {
      state = NET_FAIL;
      retryAt = now + 30000;
      if (joinPending) {                     // wrong password or out of reach: back to the old network
        snprintf(err, sizeof(err), "CAN'T JOIN %s", joinSsid);
        joinPending = false;
        if (C->ssid[0]) { begin(C->ssid, C->pass); state = NET_FAIL; }
      }
    }
    if (state == NET_FAIL && C->ssid[0] && (int32_t)(now - retryAt) > 0) { retryAt = now + 30000; begin(C->ssid, C->pass); }
  }
  if (scanRunning) {
    int n = WiFi.scanComplete();
    if (n >= 0) { scanDone(n); scanRunning = false; }
    else if (n == WIFI_SCAN_FAILED) { scanN = 0; scanRunning = false; }
  }
  if (dns) dns->processNextRequest();
  if (web) web->handleClient();
  Result r;
  while (xQueueReceive(resQ, &r, 0) == pdTRUE) {
    busy &= ~(1u << r.kind);
    uiNetResult(r.kind, r.status, r.body ? r.body : "", r.len);
    free(r.body);
  }
}
