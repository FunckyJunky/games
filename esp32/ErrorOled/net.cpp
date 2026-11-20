// WiFi, PHONE SETUP and HTTPS to error.blue. See net.h.
#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <time.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "hid.h"
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

// ------------------------------------------------------------------ HTTPS task
static void netTask(void*) {
  WiFiClientSecure tls;
  WiFiClient plain;                        // TLS = HTTP in SETUP.EXE: no encryption, much less memory
  HTTPClient http;
  http.setReuse(true);                     // keep the connection: no new TLS handshake every time
  int lastCheck = -1;
  bool httpRedirected = false;               // error.blue sent plain HTTP to HTTPS: no point trying it again
  bool lastSecure = true;
  for (;;) {
    Job j;
    if (xQueueReceive(jobQ, &j, portMAX_DELAY) != pdTRUE) continue;
    Result r = {j.kind, -1, nullptr, 0};
    if (WiFi.status() == WL_CONNECTED) {
      if (lastCheck != C->tlsCheck) {
        http.end();
        tls.stop();
        plain.stop();
        if (C->tlsCheck == 1) tls.setCACert(ROOT_CAS); else tls.setInsecure();
        lastCheck = C->tlsCheck;
      }
      bool secure = C->tlsCheck != 2;
      // HTTPS needs ~45 KB (two 16 KB buffers + the handshake). With Bluetooth on that can be too much:
      // then this request goes over plain HTTP instead of failing.
      bool lowMem = !tls.connected() && (ESP.getFreeHeap() < 48000 || ESP.getMaxAllocHeap() < 20000);
      bool fallback = secure && lowMem && !httpRedirected;
      if (fallback) secure = false;
      if (secure != lastSecure) { http.end(); tls.stop(); plain.stop(); lastSecure = secure; }
      http.setReuse(ESP.getFreeHeap() > 70000);   // keep the connection only when there's memory to spare
      String url = String(secure ? "https://" : "http://") + SERVER_HOST + j.path;
      if (C->tlsCheck != 2 && lowMem && httpRedirected) {
        snprintf(err, sizeof(err), "LOW MEMORY FOR HTTPS (%uK)", (unsigned)(ESP.getFreeHeap() / 1024));
        r.status = -3;
      } else if (secure ? http.begin(tls, url) : http.begin(plain, url)) {
        http.setConnectTimeout(8000);
        http.setTimeout(8000);
        http.setUserAgent("ERROR-OLED/1.0");
        int code;
        if (j.post) {
          http.addHeader("Content-Type", "application/json");
          code = http.POST((uint8_t*)j.body, strlen(j.body));
        } else code = http.GET();
        r.status = code;
        if (fallback && code >= 300 && code < 400) {      // the site wants HTTPS only
          httpRedirected = true;
          snprintf(err, sizeof(err), "LOW MEMORY FOR HTTPS (%uK)", (unsigned)(ESP.getFreeHeap() / 1024));
          r.status = -3;
        } else if (code > 0) {
          String s = http.getString();
          r.len = s.length();
          r.body = (char*)malloc(r.len + 1);
          if (r.body) memcpy(r.body, s.c_str(), r.len + 1); else r.len = 0;
          if (code == 200) { if (fallback) snprintf(err, sizeof(err), "VIA HTTP (LOW MEMORY)"); else err[0] = 0; }
          else snprintf(err, sizeof(err), "HTTP %d", code);
        } else {
          char tlsErr[64] = "";
          if (secure && tls.lastError(tlsErr, sizeof(tlsErr)) != 0) snprintf(err, sizeof(err), "TLS %s", tlsErr);
          else snprintf(err, sizeof(err), "%s", HTTPClient::errorToString(code).c_str());
          if (secure && ESP.getMaxAllocHeap() < 18000) snprintf(err, sizeof(err), "TLS: LOW MEMORY (%uK)", (unsigned)(ESP.getMaxAllocHeap() / 1024));
          tls.stop();
          plain.stop();
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

// ------------------------------------------------------------------ setup web page
// The same page works two ways:
//   - PHONE SETUP: the box makes its own WiFi "ERROR-OLED", the page is at 192.168.4.1
//   - on your WiFi: http://<the box's IP>/ or http://error-oled.local/ (shown by IP / WEB on the box)
// It sets WiFi, name, chat channel, and does Bluetooth: scan settings, scan, pair, forget.
static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>ERROR-OLED SETUP</title><style>
body{background:#000;color:#9fe6ff;font:15px/1.4 ui-monospace,Consolas,monospace;margin:0;padding:12px;max-width:760px}
h1{font-size:17px;background:#9fe6ff;color:#000;padding:4px 8px;margin:0 0 8px}
h2{font-size:15px;margin:22px 0 6px;border-bottom:1px solid #2a6a80;padding-bottom:2px}
label{display:block;margin:10px 0 3px;color:#6cc3e0}
input,select{background:#000;color:#9fe6ff;border:1px solid #9fe6ff;font:inherit;padding:7px;box-sizing:border-box}
input[type=text],input[type=password]{width:100%}
button{background:#9fe6ff;color:#000;border:0;font:bold 15px ui-monospace,Consolas,monospace;padding:8px 12px;margin:8px 6px 0 0;cursor:pointer}
button.b2{background:#000;color:#9fe6ff;border:1px solid #9fe6ff}button:disabled{opacity:.4}
.chip{display:inline-block;border:1px solid #2a6a80;margin:3px;padding:3px 7px;cursor:pointer}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}.row label{display:inline;margin:0}
table{border-collapse:collapse;width:100%;margin-top:6px}td,th{text-align:left;padding:4px 6px;border-bottom:1px solid #123}
th{color:#6cc3e0;font-weight:normal}tr.hid td{color:#fff}td.a{font-size:12px;color:#5aa}
.pk{font-size:34px;letter-spacing:6px;background:#9fe6ff;color:#000;padding:6px 14px;display:inline-block;margin:6px 0}
#st,#bst{color:#fff}.dim{color:#5aa}.bar{display:inline-block;width:5px;margin-right:1px;background:#9fe6ff;vertical-align:bottom}
</style></head><body>
<h1>ERROR-OLED SETUP</h1><div id=st>...</div>
<h2>WIFI / NAME / CHAT</h2>
<form id=f><label>WIFI NETWORK</label><input type=text name=ssid id=ssid autocomplete=off><div id=nets></div>
<label>PASSWORD <span class=dim>(empty = keep the saved one)</span></label><input type=password name=pass autocomplete=off>
<label>YOUR NAME (CHAT + WHO)</label><input type=text name=name id=name maxlength=16>
<label>ERROR-CHAT CHANNEL CODE</label><input type=text name=chan id=chan maxlength=8>
<button type=submit>SAVE</button><button type=button class=b2 onclick="post('/wifi/scan').then(()=>setTimeout(info,4000))">SCAN WIFI</button></form>
<h2>BLUETOOTH KEYBOARD / MOUSE</h2>
<div id=conn></div><div id=bst></div><div id=pk></div>
<form id=bo><div class=row>
<label>SCAN TIME <select name=secs><option>5<option>10<option>20<option>30<option>60</select> s</label>
<label>MODE <select name=active><option value=1>active (asks names)<option value=0>passive (only listen)</select></label></div>
<div class=row style="margin-top:8px"><label>SHOW <select name=show><option value=0>keyboards + mice only<option value=1>+ every device with a name<option value=2>everything</select></label>
<label>MIN SIGNAL <select name=rssi><option value=-100>any<option value=-90>-90 dBm<option value=-80>-80 dBm<option value=-70>-70 dBm<option value=-60>-60 dBm (very close)</select></label></div>
<div class=row style="margin-top:8px"><label><input type=checkbox name=auto value=1> reconnect paired keyboards/mice by themselves</label></div>
<button type=submit id=sb>SCAN</button><button type=button class=b2 onclick="saveOpts()">SAVE SETTINGS</button>
<button type=button class=b2 onclick="if(confirm('Forget all paired keyboards and mice?'))post('/bt/forget')">FORGET ALL PAIRED</button></form>
<div id=found></div>
<label>PAIR BY ADDRESS</label><div class=row><input type=text id=ma placeholder="aa:bb:cc:dd:ee:ff" style="width:220px">
<select id=mt><option value=1>random address<option value=0>public address</select><button type=button onclick="pair(ma.value,mt.value)">PAIR</button></div>
<p class=dim>Put the keyboard in pairing mode (light blinking) before SCAN. Only Bluetooth Low Energy (BLE / Bluetooth 5) keyboards work:
the ESP32-C3 has no Bluetooth Classic, so many cheap keyboards named "Bluetooth Keyboard" never show up here.
If the box shows a 6-digit code, type it on the keyboard and press Enter.</p>
<script>
const $=id=>document.getElementById(id),esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const post=(u,d)=>fetch(u,{method:'POST',body:new URLSearchParams(d||{})}).then(r=>r.json()).catch(()=>({}));
const bars=r=>{let n=r>-60?4:r>-70?3:r>-80?2:1,h='';for(let i=0;i<4;i++)h+='<span class=bar style="height:'+(4+i*3)+'px;opacity:'+(i<n?1:.25)+'"></span>';return h};
const TY=['?','KEYBOARD','MOUSE','KB+MOUSE'];let optsSet=false;
async function info(){const j=await fetch('/info.json').then(r=>r.json()).catch(()=>null);if(!j)return;
 $('st').textContent='WIFI '+j.state+' '+j.ssid+(j.ip?'  ·  http://'+j.ip+'/  ·  http://error-oled.local/':'')+(j.err?'  ·  '+j.err:'');
 if(!$('ssid').value)$('ssid').value=j.ssid;if(!$('name').value)$('name').value=j.name;if(!$('chan').value)$('chan').value=j.chan;
 $('nets').innerHTML=j.nets.map(n=>'<span class=chip onclick="ssid.value=this.dataset.s" data-s="'+esc(n.ssid)+'">'+esc(n.ssid)+' '+bars(n.rssi)+(n.open?'':' *')+'</span>').join('')}
async function bt(){const j=await fetch('/bt.json').then(r=>r.json()).catch(()=>null);if(!j)return;const f=$('bo');
 if(!optsSet){f.secs.value=j.opts.secs;f.active.value=j.opts.active;f.show.value=j.opts.show;f.rssi.value=j.opts.rssi;f.auto.checked=!!j.opts.auto;optsSet=true}
 $('conn').innerHTML=j.connected.length?j.connected.map(d=>'CONNECTED: <b>'+esc(d.name)+'</b> ('+TY[d.type]+')').join('<br>'):'<span class=dim>nothing connected'+(j.paired?' ('+j.paired+' paired, not here now)':'')+'</span>';
 $('bst').textContent=j.status?'STATUS: '+j.status:'';
 $('pk').innerHTML=j.passkey?'TYPE THIS ON THE KEYBOARD, THEN ENTER:<br><span class=pk>'+String(j.passkey).padStart(6,'0')+'</span>':'';
 $('sb').disabled=j.scanning;$('sb').textContent=j.scanning?'SCANNING... '+j.left+' s':'SCAN';
 $('found').innerHTML=(j.found.length||j.scanning)?'<table><tr><th>NAME<th>TYPE<th>SIGNAL<th>ADDRESS<th></tr>'+j.found.map(d=>'<tr class="'+(d.hid?'hid':'')+'"><td>'+(esc(d.name)||'<span class=dim>(no name)</span>')+'<td>'+(d.hid?TY[d.type]:'<span class=dim>other</span>')+'<td>'+bars(d.rssi)+' '+d.rssi+'<td class=a>'+d.addr+'<td><button onclick="pair(\''+d.addr+'\','+d.atype+')">PAIR</button>').join('')+'</table>'+(j.found.length?'':'<p class=dim>nothing yet...</p>'):''}
function opts(){const f=$('bo');return{secs:f.secs.value,active:f.active.value,show:f.show.value,rssi:f.rssi.value,auto:f.auto.checked?1:0}}
function saveOpts(){post('/bt/opts',opts())}
function pair(a,t){a=a.trim();if(!/^([0-9a-f]{2}:){5}[0-9a-f]{2}$/i.test(a)){alert('address like aa:bb:cc:dd:ee:ff');return}post('/bt/pair',{addr:a.toLowerCase(),atype:t})}
$('bo').onsubmit=e=>{e.preventDefault();post('/bt/scan',opts())};
$('f').onsubmit=async e=>{e.preventDefault();const r=await post('/save',Object.fromEntries(new FormData($('f'))));$('st').textContent=r.msg||'SAVED';setTimeout(info,3000)};
info();bt();setInterval(bt,1000);setInterval(info,10000);
</script></body></html>)HTML";

static const char* stateWord() {
  switch (state) { case NET_UP: return "ON"; case NET_CONNECTING: return "JOINING"; case NET_FAIL: return "FAILED"; default: return "OFF"; }
}
static void sendJson(JsonDocument& d) {
  String out;
  serializeJson(d, out);
  web->sendHeader("Cache-Control", "no-store");
  web->send(200, "application/json", out);
}
static void okJson(const char* msg) { JsonDocument d; d["ok"] = true; d["msg"] = msg; sendJson(d); }

static void pageInfo() {
  JsonDocument d;
  d["state"] = stateWord();
  d["ssid"] = joinPending ? joinSsid : C->ssid;
  d["ip"] = state == NET_UP ? ip : "";
  d["name"] = C->name;
  d["chan"] = C->channel;
  d["err"] = err;
  d["portal"] = dns != nullptr;
  JsonArray nets = d["nets"].to<JsonArray>();
  for (int i = 0; i < (scanN > 0 ? scanN : 0); i++) { JsonObject n = nets.add<JsonObject>(); n["ssid"] = scanList[i].ssid; n["rssi"] = scanList[i].rssi; n["open"] = scanList[i].open; }
  sendJson(d);
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
  ssid.trim();
  bool sameNet = ssid == C->ssid;
  if (ssid.length() && (!sameNet || pass.length() || state != NET_UP)) {
    if (!pass.length() && sameNet) pass = C->pass;
    netJoin(ssid.c_str(), pass.c_str());
    okJson("SAVED - JOINING WIFI, WATCH THE LITTLE SCREEN");
  } else okJson("SAVED");
  uiToast("SETUP PAGE: SAVED");
}

static void pageBt() {
  JsonDocument d;
  d["on"] = hidStarted();
  d["scanning"] = hidScanning();
  uint32_t ran = (millis() - hidScanStartedMs()) / 1000;
  d["left"] = hidScanning() && ran < hidOpts().secs ? hidOpts().secs - ran : 0;
  d["status"] = hidStatus();
  d["passkey"] = hidPasskey();
  d["paired"] = hidBonds();
  JsonObject o = d["opts"].to<JsonObject>();
  o["secs"] = hidOpts().secs; o["active"] = hidOpts().active; o["show"] = hidOpts().show;
  o["rssi"] = hidOpts().minRssi; o["auto"] = hidOpts().autoReconnect;
  BtDev list[24];
  int n = hidConnected(list, 3);
  JsonArray c = d["connected"].to<JsonArray>();
  for (int i = 0; i < n; i++) { JsonObject x = c.add<JsonObject>(); x["name"] = list[i].name; x["addr"] = list[i].addr; x["type"] = list[i].type; }
  n = hidResults(list, 24);
  JsonArray f = d["found"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject x = f.add<JsonObject>();
    x["name"] = list[i].name; x["addr"] = list[i].addr; x["rssi"] = list[i].rssi;
    x["type"] = list[i].type; x["hid"] = list[i].hid; x["atype"] = list[i].atype;
  }
  sendJson(d);
}
static void readOpts() {
  BtOpts o = hidOpts();
  if (web->hasArg("secs")) o.secs = web->arg("secs").toInt();
  if (web->hasArg("active")) o.active = web->arg("active").toInt();
  if (web->hasArg("show")) o.show = web->arg("show").toInt();
  if (web->hasArg("rssi")) o.minRssi = web->arg("rssi").toInt();
  if (web->hasArg("auto")) o.autoReconnect = web->arg("auto").toInt();
  hidSetOpts(o);
}

static bool mdnsOn = false;
static void webStart() {
  if (web) return;
  web = new WebServer(80);
  web->on("/", HTTP_GET, [] { web->sendHeader("Cache-Control", "no-store"); web->send_P(200, "text/html; charset=utf-8", PAGE); });
  web->on("/info.json", HTTP_GET, pageInfo);
  web->on("/save", HTTP_POST, pageSave);
  web->on("/wifi/scan", HTTP_POST, [] { netScan(); okJson("SCANNING"); });
  web->on("/bt.json", HTTP_GET, pageBt);
  web->on("/bt/opts", HTTP_POST, [] { readOpts(); okJson("SAVED"); });
  web->on("/bt/scan", HTTP_POST, [] { readOpts(); hidScan(); okJson("SCANNING"); });
  web->on("/bt/pair", HTTP_POST, [] {
    String a = web->arg("addr");
    hidPairAddr(a.c_str(), web->arg("atype").toInt());
    uiToast("PAIRING...");
    okJson("PAIRING");
  });
  web->on("/bt/forget", HTTP_POST, [] { hidForget(); okJson("FORGOT ALL"); });
  web->onNotFound([] {
    if (dns) { web->sendHeader("Location", "http://192.168.4.1/", true); web->send(302, "text/plain", ""); }   // phone setup: every address leads here
    else web->send(404, "text/plain", "not found");
  });
  web->begin();
}
static void webStop() {
  if (!web) return;
  web->stop();
  delete web;
  web = nullptr;
}

void netPortal(bool on) {
  if (on && !dns) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(DEVICE_NAME);
    dns = new DNSServer();
    dns->start(53, "*", WiFi.softAPIP());
    webStart();
    netScan();
  } else if (!on && dns) {
    dns->stop(); delete dns; dns = nullptr;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    if (state != NET_UP) webStop();
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
      webStart();                            // the setup page, on your own WiFi too
      if (!mdnsOn && MDNS.begin("error-oled")) { MDNS.addService("http", "tcp", 80); mdnsOn = true; }
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
