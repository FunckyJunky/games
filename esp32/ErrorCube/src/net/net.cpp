// net.cpp: see net.h
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <sys/stat.h>
#include <dirent.h>
#include "../ui/ui.h"
#include "../board/board.h"
#include "net.h"
#include "files_page.h"

// ------------------------------------------------------------------ saved networks (up to 6, newest first)
struct Saved { char ssid[33]; char pass[65]; };
static Saved saved[6];
static int nSaved = 0;
static Preferences wp;
static void savedLoad() {
  wp.begin("wifi", false);
  nSaved = 0;
  for (int i = 0; i < 6; i++) {
    char k[4]; snprintf(k, sizeof(k), "s%d", i);
    String s = wp.getString(k, "");
    if (!s.length()) continue;
    snprintf(k, sizeof(k), "p%d", i);
    snprintf(saved[nSaved].ssid, 33, "%s", s.c_str());
    snprintf(saved[nSaved].pass, 65, "%s", wp.getString(k, "").c_str());
    nSaved++;
  }
}
static void savedStore() {
  for (int i = 0; i < 6; i++) {
    char k[4];
    snprintf(k, sizeof(k), "s%d", i); wp.putString(k, i < nSaved ? saved[i].ssid : "");
    snprintf(k, sizeof(k), "p%d", i); wp.putString(k, i < nSaved ? saved[i].pass : "");
  }
}
static void savedPut(const char* ssid, const char* pass) {
  Saved s; snprintf(s.ssid, 33, "%s", ssid); snprintf(s.pass, 65, "%s", pass ? pass : "");
  int k = 0;
  for (int i = 0; i < nSaved; i++) if (strcmp(saved[i].ssid, ssid)) saved[k++] = saved[i];
  nSaved = k < 5 ? k : 5;
  memmove(&saved[1], &saved[0], sizeof(Saved) * nSaved);
  saved[0] = s; nSaved++;
  savedStore();
}
int netSaved(char (*ssids)[33], int max) { int n = nSaved < max ? nSaved : max; for (int i = 0; i < n; i++) strcpy(ssids[i], saved[i].ssid); return n; }
void netForget(const char* ssid) {
  int k = 0;
  for (int i = 0; i < nSaved; i++) if (strcmp(saved[i].ssid, ssid)) saved[k++] = saved[i];
  nSaved = k; savedStore();
  if (!strcmp(S.ssid, ssid)) WiFi.disconnect();
}
void netForgetAll() { nSaved = 0; savedStore(); }

// ------------------------------------------------------------------ WiFi state
static int tryIdx = -1;                  // which saved network we're trying
static uint32_t tryAt = 0, retryAt = 0, upAt = 0;
static char joinSsid[33] = "", joinPass[65] = "";
static bool joinNew = false;             // a network typed in: saved once it works
static bool timeStarted = false;
static WebServer* web = nullptr;
static DNSServer* dns = nullptr;
static WifiNet scanList[16];
static volatile int scanN = -1;
static bool scanning = false;

bool netUp() { return S.net == NET_UP; }
static bool paused = false;              // WiFi off while the screen is off (cfg.aodWifi = 2)
static bool radioWanted() { return cfg.wifiOn && !cfg.offline && !paused; }

static void beginJoin(const char* ssid, const char* pass) {
  WiFi.disconnect(false, false);
  delay(20);
  WiFi.begin(ssid, pass && pass[0] ? pass : nullptr);
  snprintf(S.ssid, sizeof(S.ssid), "%s", ssid);
  S.net = NET_JOINING;
  tryAt = millis();
}
static void tryNext() {
  if (!nSaved) { S.net = NET_NOSAVED; return; }
  tryIdx = (tryIdx + 1) % nSaved;
  beginJoin(saved[tryIdx].ssid, saved[tryIdx].pass);
}
void netJoin(const char* ssid, const char* pass) {
  snprintf(joinSsid, sizeof(joinSsid), "%s", ssid);
  if (!pass) { for (int i = 0; i < nSaved; i++) if (!strcmp(saved[i].ssid, ssid)) pass = saved[i].pass; }
  snprintf(joinPass, sizeof(joinPass), "%s", pass ? pass : "");
  joinNew = true;
  if (!cfg.wifiOn) { cfg.wifiOn = 1; cfgSave(); }
  beginJoin(joinSsid, joinPass);
}
void netWifi(bool on) {
  if (!on || cfg.offline) { WiFi.disconnect(true, false); S.net = NET_OFF; S.link = false; return; }
  WiFi.mode(S.portal ? WIFI_AP_STA : WIFI_STA);
  tryIdx = -1; tryNext();
}
void netScan() {
  if (scanning) return;
  scanN = -1;
  scanning = WiFi.scanNetworks(true, false) == WIFI_SCAN_RUNNING;
  if (!scanning) scanN = 0;
}
int netScanResults(WifiNet* out, int max) {
  if (scanN < 0) return -1;
  int n = scanN < max ? scanN : max;
  memcpy(out, scanList, sizeof(WifiNet) * n);
  return n;
}
static void scanCheck() {
  if (!scanning) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  scanning = false;
  int k = 0;
  for (int i = 0; i < n && k < 16; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    int j = 0; for (; j < k; j++) if (s == scanList[j].ssid) break;
    if (j < k) { if (WiFi.RSSI(i) > scanList[j].rssi) scanList[j].rssi = WiFi.RSSI(i); continue; }
    WifiNet& w = scanList[k++];
    snprintf(w.ssid, 33, "%s", s.c_str()); w.rssi = WiFi.RSSI(i); w.open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN; w.saved = false;
    for (int q = 0; q < nSaved; q++) if (!strcmp(saved[q].ssid, w.ssid)) w.saved = true;
  }
  for (int a = 0; a < k; a++) for (int b = a + 1; b < k; b++) if (scanList[b].rssi > scanList[a].rssi) { WifiNet t = scanList[a]; scanList[a] = scanList[b]; scanList[b] = t; }
  WiFi.scanDelete();
  scanN = k;
}

static void timeSynced(struct timeval*) { S.timeOk = true; boardSetRtcFromSystem(); }

// ------------------------------------------------------------------ the cube's own web page (also the phone setup page)
static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>ERROR-CUBE</title><style>
:root{--fg:#39ff7a;--dim:#1b7a3c;--bg:#031208}
body{background:var(--bg);color:var(--fg);font:16px/1.45 ui-monospace,Consolas,monospace;margin:0 auto;padding:12px;max-width:640px;text-shadow:0 0 5px rgba(57,255,122,.5)}
h1{font-size:18px;background:var(--fg);color:var(--bg);text-shadow:none;margin:0 0 10px;padding:4px 10px}
h2{font-size:15px;margin:22px 0 6px;border-bottom:2px double var(--fg);padding-bottom:3px}
label{display:block;margin:10px 0 3px;color:var(--dim)}
input,select{background:#000;color:var(--fg);border:1px solid var(--fg);font:inherit;padding:8px;box-sizing:border-box;width:100%}
button{background:var(--fg);color:var(--bg);border:0;font:bold 15px ui-monospace,monospace;padding:9px 14px;margin:10px 8px 0 0;cursor:pointer}
button.b2{background:none;color:var(--fg);border:1px solid var(--fg)}
.chip{display:inline-block;border:1px solid var(--dim);margin:3px;padding:3px 8px;cursor:pointer}
.dim{color:var(--dim)}#st{white-space:pre-wrap}a{color:#b8ffd0}
progress{width:100%;height:18px;accent-color:var(--fg)}
</style></head><body>
<h1>C:\&gt;ERROR-CUBE</h1><div id=st>...</div>
<h2>WIFI + NAME</h2>
<form id=f><label>WIFI NETWORK</label><input name=ssid id=ssid autocomplete=off><div id=nets></div>
<label>PASSWORD <span class=dim>(empty = keep the saved one)</span></label><input type=password name=pass autocomplete=off>
<label>NAME (CHAT + WHO'S ONLINE)</label><input name=name id=name maxlength=16>
<label>ERROR-CHAT CHANNEL</label><input name=chan id=chan maxlength=8>
<button>SAVE</button><button type=button class=b2 onclick="post('/scan').then(()=>setTimeout(info,4000))">SCAN WIFI</button></form>
<h2>CONTROL IT FROM YOUR PHONE</h2><p>Open <a id=lnk href="https://error.blue/cube/">error.blue/cube</a> (link code <b id=code></b>).</p>
<h2>FIRMWARE</h2><p class=dim>Put any ERROR-CUBE .bin on the cube. It restarts when done. Normally the cube gets new firmware from error.blue by itself.</p>
<input type=file id=fw accept=.bin><button type=button onclick=up()>INSTALL</button><progress id=pg max=100 value=0 hidden></progress>
<script>
const $=id=>document.getElementById(id),esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const post=(u,d)=>fetch(u,{method:'POST',body:new URLSearchParams(d||{})}).then(r=>r.json()).catch(()=>({}));
async function info(){const j=await fetch('/info.json').then(r=>r.json()).catch(()=>null);if(!j)return;
 $('st').textContent='ERROR-CUBE '+j.ver+'  '+j.host+'\nWIFI '+j.state+' '+j.ssid+(j.ip?'  '+j.ip:'')+'\nBATTERY '+(j.bat<0?'none':j.bat+'%')+'   ERROR.BLUE '+(j.link?'linked':'-');
 if(!$('ssid').value)$('ssid').value=j.ssid;if(!$('name').value)$('name').value=j.name;if(!$('chan').value)$('chan').value=j.chan;
 $('code').textContent=j.code;$('lnk').href='https://error.blue/cube/?code='+j.code;
 $('nets').innerHTML=j.nets.map(n=>'<span class=chip onclick="ssid.value=this.dataset.s" data-s="'+esc(n.ssid)+'">'+esc(n.ssid)+' '+(n.rssi>-60?'####':n.rssi>-70?'###':n.rssi>-80?'##':'#')+(n.open?'':' *')+'</span>').join('')}
$('f').onsubmit=async e=>{e.preventDefault();const r=await post('/save',Object.fromEntries(new FormData($('f'))));$('st').textContent=r.msg||'SAVED';setTimeout(info,4000)};
function up(){const f=$('fw').files[0];if(!f)return;const x=new XMLHttpRequest(),d=new FormData();d.append('fw',f);$('pg').hidden=false;
 x.upload.onprogress=e=>$('pg').value=e.loaded*100/e.total;x.onload=()=>{$('st').textContent=x.responseText+' - restarting'};x.open('POST','/update');x.send(d)}
info();setInterval(info,8000);
</script></body></html>)HTML";

static const char* stateWord() {
  switch (S.net) { case NET_UP: return "ON"; case NET_JOINING: return "JOINING"; case NET_FAIL: return "FAILED"; case NET_NOSAVED: return "NOT SET UP"; default: return "OFF"; }
}
static void sendJson(JsonDocument& d) { String o; serializeJson(d, o); web->sendHeader("Cache-Control", "no-store"); web->send(200, "application/json", o); }
static bool otaUpOk = false;
// ---------------- FILES on the cube's own page: list, upload (fast, same WiFi), download, delete, new folder
static bool okPath(const String& p) {
  return (p == "/sd" || p == "/c" || p.startsWith("/sd/") || p.startsWith("/c/")) && p.indexOf("..") < 0 && p.length() < 180;
}
static FILE* upF = nullptr; static bool upOk = false; static size_t upN = 0;
static void filesApi() {
  web->on("/files", [] { web->send_P(200, "text/html", FILES_PAGE); });
  web->on("/api/ls", [] {
    String d = web->arg("d"); if (!d.length()) d = "/sd";
    JsonDocument j;
    if (d.startsWith("/sd")) sdMount();
    j["d"] = d; j["sd"] = S.sd; j["sdInfo"] = S.sdInfo;
    if (!okPath(d)) { j["ok"] = false; sendJson(j); return; }
    static FileEnt* ents = nullptr;
    if (!ents) ents = (FileEnt*)malloc(sizeof(FileEnt) * 200);
    int n = ents ? fsList(d.c_str(), ents, 200) : -1;
    j["ok"] = n >= 0;
    JsonArray e = j["e"].to<JsonArray>();
    for (int i = 0; i < n; i++) { JsonArray a = e.add<JsonArray>(); a.add(ents[i].name); a.add(ents[i].dir ? 1 : 0); a.add(ents[i].size); }
    uint64_t t, u; if (fsInfo(d.c_str(), t, u)) { j["free"] = (uint32_t)((t - u) >> 20); j["total"] = (uint32_t)(t >> 20); }
    sendJson(j);
  });
  web->on("/api/up", HTTP_POST, [] {
    JsonDocument j; j["ok"] = upOk; j["n"] = (uint32_t)upN; sendJson(j); uiDirty();
  }, [] {
    HTTPUpload& u = web->upload();
    if (u.status == UPLOAD_FILE_START) {
      String dir = web->arg("d"), p = dir + "/" + u.filename;
      upOk = false; upN = 0;
      if (upF) { fclose(upF); upF = nullptr; }
      if (!okPath(dir) || u.filename.indexOf('/') >= 0) return;
      if (dir.startsWith("/sd")) sdMount();
      fsMkdir(dir.c_str());
      upF = fopen(p.c_str(), "wb");
      upOk = upF != nullptr;
    } else if (u.status == UPLOAD_FILE_WRITE) {
      if (upF && fwrite(u.buf, 1, u.currentSize, upF) != u.currentSize) upOk = false;
      upN += u.currentSize;
    } else if (u.status == UPLOAD_FILE_END || u.status == UPLOAD_FILE_ABORTED) {
      if (upF) { fclose(upF); upF = nullptr; }
      if (u.status == UPLOAD_FILE_ABORTED) upOk = false;
    }
  });
  web->on("/api/rm", HTTP_POST, [] {
    String p = web->arg("p"); JsonDocument j;
    j["ok"] = okPath(p) && p != "/sd" && p != "/c" && (fsRemove(p.c_str()) || fsRmdir(p.c_str()));
    sendJson(j); uiDirty();
  });
  web->on("/api/open", HTTP_POST, [] {
    String p = web->arg("p"); JsonDocument j; j["ok"] = okPath(p);
    if (okPath(p)) { snprintf(S.openPath, sizeof(S.openPath), "%s", p.c_str()); S.openAt = millis() | 1; }
    sendJson(j);
  });
  web->on("/api/mk", HTTP_POST, [] { String p = web->arg("p"); JsonDocument j; j["ok"] = okPath(p) && fsMkdir(p.c_str()); sendJson(j); uiDirty(); });
  web->on("/api/mv", HTTP_POST, [] {
    String a = web->arg("a"), b = web->arg("b"); JsonDocument j;
    j["ok"] = okPath(a) && okPath(b) && fsRename(a.c_str(), b.c_str()); sendJson(j); uiDirty();
  });
  web->on("/dl", [] {
    String p = web->arg("p");
    FILE* f = okPath(p) ? fopen(p.c_str(), "rb") : nullptr;
    if (!f) { web->send(404, "text/plain", "Not found"); return; }
    fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
    String name = p.substring(p.lastIndexOf('/') + 1);
    web->sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    web->setContentLength(n);
    web->send(200, "application/octet-stream", "");
    static uint8_t* b = nullptr; if (!b) b = (uint8_t*)malloc(8192);
    size_t k;
    while (b && (k = fread(b, 1, 8192, f)) > 0) web->sendContent((const char*)b, k);
    fclose(f);
  });
}

static void webSetup() {
  web = new WebServer(80);
  filesApi();
  web->on("/", [] { web->send_P(200, "text/html", PAGE); });
  web->on("/info.json", [] {
    JsonDocument d;
    d["ver"] = CUBE_VERSION; d["host"] = S.host; d["state"] = stateWord(); d["ssid"] = S.ssid; d["ip"] = S.net == NET_UP ? S.ip : "";
    d["name"] = cfg.name; d["chan"] = cfg.chan; d["code"] = cfg.code; d["bat"] = S.battery ? S.batPct : -1; d["link"] = S.link;
    JsonArray n = d["nets"].to<JsonArray>();
    for (int i = 0; i < (scanN > 0 ? scanN : 0); i++) { JsonObject o = n.add<JsonObject>(); o["ssid"] = scanList[i].ssid; o["rssi"] = scanList[i].rssi; o["open"] = scanList[i].open; }
    sendJson(d);
  });
  web->on("/scan", HTTP_POST, [] { netScan(); JsonDocument d; d["ok"] = true; sendJson(d); });
  web->on("/save", HTTP_POST, [] {
    String ssid = web->arg("ssid"), pass = web->arg("pass"), name = web->arg("name"), chan = web->arg("chan");
    name.trim(); chan.trim(); chan.toUpperCase();
    if (name.length()) snprintf(cfg.name, sizeof(cfg.name), "%s", name.c_str());
    if (chan.length() >= 3 && chan.length() <= 8) { snprintf(cfg.chan, sizeof(cfg.chan), "%s", chan.c_str()); netChatReset(); }
    cfgSave();
    JsonDocument d; d["ok"] = true;
    if (ssid.length()) { netJoin(ssid.c_str(), pass.length() ? pass.c_str() : nullptr); d["msg"] = "SAVED. JOINING " + ssid + "..."; }
    else d["msg"] = "SAVED";
    sendJson(d);
    uiDirty();
  });
  web->on("/update", HTTP_POST, [] {
    web->send(200, "text/plain", otaUpOk ? "INSTALLED" : String("FAILED: ") + Update.errorString());
    if (otaUpOk) { delay(500); sysRestart(); }
  }, [] {
    HTTPUpload& u = web->upload();
    if (u.status == UPLOAD_FILE_START) { otaUpOk = false; S.upd = UPD_LOADING; S.updPct = 0; snprintf(S.updMsg, sizeof(S.updMsg), "FROM THE WEB PAGE"); Update.begin(UPDATE_SIZE_UNKNOWN); }
    else if (u.status == UPLOAD_FILE_WRITE) { Update.write(u.buf, u.currentSize); S.updPct = (S.updPct + 1) % 100; }
    else if (u.status == UPLOAD_FILE_END) { otaUpOk = Update.end(true); S.upd = otaUpOk ? UPD_DONE : UPD_FAIL; if (!otaUpOk) snprintf(S.updMsg, sizeof(S.updMsg), "%s", Update.errorString()); }
  });
  // phone setup: every other address opens the page (captive portal)
  web->onNotFound([] {
    if (S.portal) { web->sendHeader("Location", "http://192.168.4.1/"); web->send(302, "text/plain", ""); }
    else web->send(404, "text/plain", "Not found");
  });
  web->begin();
}

void netPortal(bool on) {
  if (on == (dns != nullptr)) { S.portal = on; return; }
  if (on) {
    WiFi.mode(WIFI_AP_STA);
    snprintf(S.portalName, sizeof(S.portalName), "ERROR-%s", S.host);
    WiFi.softAP(S.portalName);
    delay(100);
    dns = new DNSServer();
    dns->start(53, "*", WiFi.softAPIP());
    S.portal = true;
    netScan();
  } else {
    dns->stop(); delete dns; dns = nullptr;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    S.portal = false;
  }
}

// OFFLINE (airplane mode): the radio off, nothing goes to the internet; back on: join again
void netOffline(bool on) {
  if (on) {
    if (S.portal) netPortal(false);
    WiFi.disconnect(true, false); WiFi.mode(WIFI_OFF);
    S.net = NET_OFF; S.link = false;
  } else if (cfg.wifiOn) {
    WiFi.mode(WIFI_STA); tryIdx = -1; tryNext();
  }
  uiDirty();
}
// the screen is off: 0 as normal, 1 the radio sleeps between beacons (saves power, a bit slower), 2 the radio off
void netSleepMode(int m) {
  static int now = 0;
  if (m == now) return;
  if (now == 2 && paused) {                       // back from off
    paused = false;
    if (radioWanted()) { WiFi.mode(WIFI_STA); tryIdx = -1; tryNext(); }
  }
  now = m;
  if (m == 2 && radioWanted()) { paused = true; WiFi.disconnect(true, false); WiFi.mode(WIFI_OFF); S.net = NET_OFF; S.link = false; return; }
  if (S.net != NET_OFF) esp_wifi_set_ps(m == 1 ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM);
}

void netBegin() {
  savedLoad();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  char host[24]; snprintf(host, sizeof(host), "%s", S.host);
  for (char* p = host; *p; p++) *p = tolower(*p);
  WiFi.setHostname(host);
  WiFi.setAutoReconnect(true);
  webSetup();
  if (!cfg.wifiOn || cfg.offline) { S.net = NET_OFF; if (cfg.offline) WiFi.mode(WIFI_OFF); }
  else if (!nSaved) { S.net = NET_NOSAVED; netPortal(true); }     // nothing saved: phone setup straight away
  else tryNext();
}

void netStep() {
  uint32_t now = millis();
  scanCheck();
  if (web) web->handleClient();
  if (dns) dns->processNextRequest();
  if (!radioWanted()) return;
  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    if (S.net != NET_UP) {
      S.net = NET_UP; upAt = now;
      snprintf(S.ip, sizeof(S.ip), "%s", WiFi.localIP().toString().c_str());
      snprintf(S.ssid, sizeof(S.ssid), "%s", WiFi.SSID().c_str());
      if (joinNew) { savedPut(joinSsid, joinPass); joinNew = false; }
      else if (tryIdx > 0 && tryIdx < nSaved) { Saved s = saved[tryIdx]; savedPut(s.ssid, s.pass); }
      tryIdx = -1;
      char host[24]; snprintf(host, sizeof(host), "%s", S.host); for (char* p = host; *p; p++) *p = tolower(*p);
      MDNS.end(); MDNS.begin(host); MDNS.addService("http", "tcp", 80);
      if (cfg.ntp && !timeStarted) {
        sntp_set_time_sync_notification_cb(timeSynced);
        configTzTime(TIMEZONES[cfg.tzIdx % N_TZ].posix, "pool.ntp.org", "time.cloudflare.com");
        timeStarted = true;
      }
      // connected: the phone setup WiFi can go after a minute
      if (S.portal) retryAt = now + 60000;
      uiDirty();
    }
    if (S.portal && retryAt && now > retryAt) { netPortal(false); retryAt = 0; }
    static uint32_t rs = 0;
    if (now - rs > 3000) { rs = now; S.rssi = WiFi.RSSI(); }
    return;
  }
  if (S.net == NET_UP) { S.net = NET_JOINING; tryAt = now; S.link = false; uiDirty(); }
  if (S.net == NET_JOINING && now - tryAt > 15000) {
    if (joinNew) {                           // the typed network didn't work
      joinNew = false; S.net = NET_FAIL; snprintf(S.netErr, sizeof(S.netErr), "COULD NOT JOIN %s", joinSsid);
      retryAt = now + 5000;
    } else if (tryIdx + 1 < nSaved) tryNext();
    else { S.net = NET_FAIL; retryAt = now + 60000; tryIdx = -1; }
    uiDirty();
  }
  if ((S.net == NET_FAIL || S.net == NET_NOSAVED) && retryAt && now > retryAt && nSaved) { retryAt = 0; tryIdx = -1; tryNext(); }
}
