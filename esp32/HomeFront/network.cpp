// WiFi: saved networks, the setup WiFi HOMEFRONT-XXXX with its page (http://192.168.4.1), and the
// network task on core 0 that also runs the link to error.blue.
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "config.h"
#include "hf.h"

// ---------------- settings ----------------
static SemaphoreHandle_t prefLock = nullptr;
static void plock() { if (!prefLock) prefLock = xSemaphoreCreateRecursiveMutex(); xSemaphoreTakeRecursive(prefLock, portMAX_DELAY); }
static void punlock() { xSemaphoreGiveRecursive(prefLock); }
String prefGet(const char* key, const char* def) {
  plock();
  Preferences p; p.begin("hf", true);
  String v = p.isKey(key) ? p.getString(key, def) : String(def);
  p.end(); punlock();
  return v;
}
void prefSet(const char* key, const String& v) { plock(); Preferences p; p.begin("hf", false); p.putString(key, v); p.end(); punlock(); }
bool prefFlip() { return prefGet("flip", "0") == "1"; }
int netsLoad(Net* out) {
  plock();
  Preferences p; p.begin("hf", true);
  int n = p.isKey("n") ? min((int)p.getUChar("n", 0), MAX_NETS) : 0, k = 0;
  for (int i = 0; i < n; i++) {
    char ks[4], kp[4], kl[4]; snprintf(ks, 4, "s%d", i); snprintf(kp, 4, "p%d", i); snprintf(kl, 4, "l%d", i);
    String s = p.isKey(ks) ? p.getString(ks, "") : String("");
    if (!s.length()) continue;
    strlcpy(out[k].s, s.c_str(), sizeof(out[k].s));
    strlcpy(out[k].p, (p.isKey(kp) ? p.getString(kp, "") : String("")).c_str(), sizeof(out[k].p));
    out[k].low = p.isKey(kl) ? p.getBool(kl, false) : false;
    k++;
  }
  p.end(); punlock();
  return k;
}
void netsSave(const Net* nets, int n) {
  plock();
  Preferences p; p.begin("hf", false);
  p.putUChar("n", n);
  for (int i = 0; i < n; i++) {
    char ks[4], kp[4], kl[4]; snprintf(ks, 4, "s%d", i); snprintf(kp, 4, "p%d", i); snprintf(kl, 4, "l%d", i);
    p.putString(ks, nets[i].s); p.putString(kp, nets[i].p); p.putBool(kl, nets[i].low);
  }
  p.end(); punlock();
}
void netsAdd(const char* ssid, const char* pass) {              // newest first
  Net old[MAX_NETS], nw[MAX_NETS];
  int n = netsLoad(old), k = 0;
  strlcpy(nw[0].s, ssid, sizeof(nw[0].s)); strlcpy(nw[0].p, pass, sizeof(nw[0].p)); nw[0].low = false;
  k = 1;
  for (int i = 0; i < n && k < MAX_NETS; i++) {
    if (!strcmp(old[i].s, ssid)) { nw[0].low = old[i].low; continue; }
    nw[k++] = old[i];
  }
  netsSave(nw, k);
}
void netsForget(const char* ssid) {
  Net nets[MAX_NETS];
  int n = netsLoad(nets), k = 0;
  for (int i = 0; i < n; i++) if (strcmp(nets[i].s, ssid)) nets[k++] = nets[i];
  netsSave(nets, k);
}

// ---------------- WiFi ----------------
enum { W_START, W_SCAN, W_TRY, W_UP, W_SETUP };
static int wstate = W_START;
static Net cand[MAX_NETS + 1];
static int ncand = 0, ci = 0, pass = 0;
static bool usedLow = false;
static uint32_t tryStart = 0, nextScan = 0, apOffAt = 0, downSince = 0;
static volatile bool joinReq = false, scanReq = false, scanning = false;
static char joinS[33], joinP[65];
static String scanJson = "";
static bool mdnsOn = false;

static void startAP() {
  if (G.apOn) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(G.ap);
  G.apSince = millis();
  G.apOn = true;
  Serial.printf("WIFI: setup WiFi %s is on, page at http://192.168.4.1\n", G.ap);
}
static void stopAP() {
  if (!G.apOn) return;
  G.apOn = false;
  delay(300);                                   // let the page notice
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  Serial.println("WIFI: setup WiFi off");
}
static void beginNet(const Net& n, bool low) {
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.begin(n.s, n.p);
  WiFi.setTxPower(low ? WIFI_POWER_8_5dBm : WIFI_POWER_19_5dBm);
  usedLow = low;
  G.joining = true; G.joinLow = low;
  strlcpy(G.joinSsid, n.s, sizeof(G.joinSsid));
  snprintf(G.setupMsg, sizeof(G.setupMsg), "JOINING %s%s", n.s, low ? " (LOW POWER)" : "");
  tryStart = millis();
  Serial.printf("WIFI: joining %s %s\n", n.s, low ? "at low power" : "at full power");
}
static void setupFail() {
  G.joining = false;
  startAP();
  snprintf(G.setupMsg, sizeof(G.setupMsg), "NO SAVED WIFI FOUND, TRYING AGAIN SOON");
  nextScan = millis() + 60000;
  wstate = W_SETUP;
}
static void onUp() {
  G.joining = false;
  if (cand[ci].low != usedLow) {                // remember which power worked for this network
    Net nets[MAX_NETS]; int n = netsLoad(nets);
    for (int i = 0; i < n; i++) if (!strcmp(nets[i].s, cand[ci].s)) nets[i].low = usedLow;
    netsSave(nets, n);
  }
  strlcpy(G.wifi, WiFi.SSID().c_str(), sizeof(G.wifi));
  strlcpy(G.ip, WiFi.localIP().toString().c_str(), sizeof(G.ip));
  bool show = !G.everStaUp || G.apOn;
  G.staUp = true; G.everStaUp = true;
  if (show) G.connectedAt = millis();           // the screen says CONNECTED for 3 s
  snprintf(G.setupMsg, sizeof(G.setupMsg), "CONNECTED TO %s", G.wifi);
  Serial.printf("WIFI: connected to %s, IP %s, %d dBm\n", G.wifi, G.ip, WiFi.RSSI());
  if (G.apOn) apOffAt = millis() + 30000;
  if (!mdnsOn) {
    String h = String(G.ap); h.toLowerCase();
    if (MDNS.begin(h.c_str())) { MDNS.addService("http", "tcp", 80); mdnsOn = true; }
  }
  downSince = 0;
  wstate = W_UP;
}
static int byRssi(const void* a, const void* b) { return ((const int*)b)[1] - ((const int*)a)[1]; }
static void doScan() {
  WiFi.mode(G.apOn ? WIFI_AP_STA : WIFI_STA);
  Net saved[MAX_NETS];
  int ns = netsLoad(saved);
  int found = WiFi.scanNetworks(false, true);
  int seen[MAX_NETS][2]; int k = 0;
  bool used[MAX_NETS] = { false };
  for (int i = 0; i < ns; i++) {
    int best = -999;
    for (int j = 0; j < found; j++) if (WiFi.SSID(j) == saved[i].s) best = max(best, (int)WiFi.RSSI(j));
    if (best > -999) { seen[k][0] = i; seen[k][1] = best; k++; used[i] = true; }
  }
  WiFi.scanDelete();
  qsort(seen, k, sizeof(seen[0]), byRssi);
  ncand = 0;
  for (int i = 0; i < k; i++) cand[ncand++] = saved[seen[i][0]];
  for (int i = 0; i < ns; i++) if (!used[i]) cand[ncand++] = saved[i];   // maybe a hidden network
  Serial.printf("WIFI: %d networks around, %d saved, %d of them here\n", found, ns, k);
  if (!ncand) { setupFail(); return; }
  ci = 0; pass = 0;
  beginNet(cand[0], cand[0].low);
  wstate = W_TRY;
}

static void netLoop(void*) {
  for (;;) {
    uint32_t now = millis();
    G.apClients = G.apOn ? WiFi.softAPgetStationNum() : 0;
    // a scan for the setup page
    if (scanReq && !scanning && wstate != W_TRY) { scanReq = false; scanning = true; WiFi.scanNetworks(true, false); }
    if (scanning) {
      int n = WiFi.scanComplete();
      if (n >= 0) {
        JsonDocument d;
        JsonArray a = d["nets"].to<JsonArray>();
        for (int i = 0; i < n && i < 30; i++) {
          if (!WiFi.SSID(i).length()) continue;
          bool dup = false;
          for (JsonObject o : a) if (o["s"] == WiFi.SSID(i)) dup = true;
          if (dup) continue;
          JsonObject o = a.add<JsonObject>(); o["s"] = WiFi.SSID(i); o["r"] = WiFi.RSSI(i); o["l"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
        String s; serializeJson(d, s);
        plock(); scanJson = s; punlock();
        WiFi.scanDelete(); scanning = false;
      } else if (n == WIFI_SCAN_FAILED) scanning = false;
    }
    if (joinReq && !scanning) {                  // the setup page asked to join a network
      joinReq = false;
      startAP();
      Net n; plock(); strlcpy(n.s, joinS, sizeof(n.s)); strlcpy(n.p, joinP, sizeof(n.p)); punlock();
      n.low = false;
      Net saved[MAX_NETS]; int ns = netsLoad(saved);
      for (int i = 0; i < ns; i++) if (!strcmp(saved[i].s, n.s)) n.low = saved[i].low;
      linkDrop(); G.staUp = false;
      cand[0] = n; ncand = 1; ci = 0; pass = 0;
      beginNet(n, n.low);
      wstate = W_TRY;
    }
    if (G.wantSetup) { G.wantSetup = false; startAP(); snprintf(G.setupMsg, sizeof(G.setupMsg), "WIFI SETUP IS OPEN"); }

    switch (wstate) {
      case W_START: {
        Net nets[MAX_NETS];
        if (!netsLoad(nets)) { startAP(); snprintf(G.setupMsg, sizeof(G.setupMsg), "NO WIFI SAVED YET"); nextScan = 0; wstate = W_SETUP; }
        else wstate = W_SCAN;
        break;
      }
      case W_SCAN:
        if (!scanning) doScan();
        break;
      case W_TRY:
        if (WiFi.status() == WL_CONNECTED) onUp();
        else if (millis() - tryStart > WIFI_TRY_MS) {
          if (pass == 0) { pass = 1; beginNet(cand[ci], !cand[ci].low); }
          else if (++ci < ncand) { pass = 0; beginNet(cand[ci], cand[ci].low); }
          else { Serial.println("WIFI: no saved network worked"); setupFail(); }
        }
        break;
      case W_UP:
        if (WiFi.status() != WL_CONNECTED) {
          if (!downSince) { downSince = now; G.staUp = false; linkDrop(); Serial.println("WIFI: lost"); }
          if (now - downSince > 20000) { downSince = 0; wstate = W_SCAN; }   // look for any saved WiFi; none = setup WiFi on
        } else {
          if (downSince) {                       // came back by itself
            downSince = 0; G.staUp = true;
            strlcpy(G.ip, WiFi.localIP().toString().c_str(), sizeof(G.ip));
          }
          linkStep();
          if (apOffAt && (int32_t)(now - apOffAt) > 0 && G.apClients == 0) { apOffAt = 0; stopAP(); }
          if (apOffAt && (int32_t)(now - apOffAt) > 120000) { apOffAt = 0; stopAP(); }
        }
        break;
      case W_SETUP:
        if (nextScan && (int32_t)(now - nextScan) > 0 && G.apClients == 0 && !scanning) { nextScan = 0; wstate = W_SCAN; }
        else if (nextScan) {
          int s = max(0, (int)((int32_t)(nextScan - now) / 1000));
          snprintf(G.setupMsg, sizeof(G.setupMsg), G.apClients ? "PHONE CONNECTED, OPEN 192.168.4.1" : "NO WIFI. TRYING SAVED WIFI IN %d S", s);
        }
        break;
    }
    delay(G.wsUp ? 2 : 10);
  }
}

// ---------------- the setup page (runs on core 1, from loop()) ----------------
static WebServer* web = nullptr;
static DNSServer* dns = nullptr;
static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>HOME FRONT SETUP</title><style>
body{margin:0;background:#031208;color:#39ff7a;font:18px ui-monospace,Menlo,Consolas,monospace;padding:12px;max-width:560px}
h1{background:#39ff7a;color:#031208;font-size:20px;margin:0 0 10px;padding:4px 8px}h2{font-size:16px;color:#1b7a3c;margin:20px 0 6px;font-weight:normal}
.box{border:2px solid #1b7a3c;padding:8px;line-height:1.4}input,button{font:inherit}
input[type=text],input[type=password]{width:100%;box-sizing:border-box;background:#000;color:#39ff7a;border:2px solid #1b7a3c;padding:9px;margin:4px 0;text-transform:none}
button{background:#031208;color:#39ff7a;border:2px solid #39ff7a;padding:8px 14px;margin:6px 6px 0 0;cursor:pointer}button:active{background:#39ff7a;color:#031208}
.net{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:8px;border:1px solid #1b7a3c;margin:4px 0;cursor:pointer}
.net button{margin:0;padding:4px 8px;font-size:15px}.dim{color:#1b7a3c}b{color:#b8ffd0}#msg{min-height:1.3em;margin:8px 0;color:#b8ffd0}
</style></head><body><h1>HOME FRONT &middot; SETUP</h1><div class=box id=st>...</div>
<h2>WIFI NEARBY (TAP ONE)</h2><div id=nets class=dim>SEARCHING...</div><button onclick=scan()>SEARCH AGAIN</button>
<h2>JOIN A WIFI</h2><input type=text id=s placeholder="WIFI NAME" autocomplete=off autocapitalize=off><input type=password id=p placeholder="PASSWORD"><button onclick=join()>JOIN</button><div id=msg></div>
<h2>SAVED WIFI</h2><div id=saved class=dim></div>
<h2>THIS SCREEN</h2><input type=text id=nm placeholder="NAME" maxlength=20><input type=text id=hm placeholder="HOME CODE (EMPTY = NONE)" maxlength=12>
<label class=dim><input type=checkbox id=fl> TURN THE PICTURE ROUND</label><br><button onclick=scr()>SAVE</button><button onclick="post('/reboot');msg('RESTARTING...')">RESTART</button>
<p class=dim>Control it from <b>error.blue/thuisfront</b></p>
<script>
const $=i=>document.getElementById(i),esc=s=>String(s).replace(/[&<>"']/g,c=>'&#'+c.charCodeAt(0)+';');
const post=(u,d)=>fetch(u,{method:'POST',body:new URLSearchParams(d||{})}).then(r=>r.json()).catch(()=>({}));
const msg=t=>$('msg').textContent=t;let first=1,want='';
async function st(){const j=await fetch('/status').then(r=>r.json()).catch(()=>null);if(!j)return;
$('st').innerHTML=`<b>${esc(j.name)}</b> &middot; ${j.id} &middot; v${j.ver}<br>WIFI: ${j.sta?'<b>'+esc(j.ssid)+'</b> &middot; '+j.ip:(j.joining?'JOINING '+esc(j.join)+'...':'NOT CONNECTED')}<br><span class=dim>${esc(j.msg)}</span>`;
$('saved').innerHTML=j.nets.length?j.nets.map(n=>`<div class=net><span>${esc(n.s)}${n.low?' <span class=dim>(LOW POWER)</span>':''}</span><button data-f="${esc(n.s)}">FORGET</button></div>`).join(''):'NONE YET';
for(const b of $('saved').querySelectorAll('[data-f]'))b.onclick=()=>post('/forget',{s:b.dataset.f}).then(st);
if(want&&j.sta&&j.ssid===want){msg('CONNECTED! THE SCREEN SAYS CONNECTED. YOU CAN CLOSE THIS PAGE.');want=''}
if(first){first=0;$('nm').value=j.name;$('hm').value=j.home;$('fl').checked=!!j.flip}}
async function scan(){$('nets').textContent='SEARCHING...';await fetch('/scan?new=1').catch(()=>0);setTimeout(nets,2500)}
async function nets(){const j=await fetch('/scan').then(r=>r.json()).catch(()=>null);if(!j||j.busy){setTimeout(nets,1500);return}
const bars=r=>r>-55?'||||':r>-67?'|||':r>-77?'||':'|';
$('nets').innerHTML=j.nets.length?j.nets.sort((a,b)=>b.r-a.r).map(n=>`<div class=net data-s="${esc(n.s)}"><span>${esc(n.s)}${n.l?'':' (OPEN)'}</span><span>${bars(n.r)}</span></div>`).join(''):'NOTHING FOUND';
for(const d of $('nets').querySelectorAll('[data-s]'))d.onclick=()=>{$('s').value=d.dataset.s;$('p').focus()}}
function join(){const s=$('s').value.trim();if(!s)return msg('TYPE OR TAP A WIFI NAME');want=s;post('/join',{s,p:$('p').value});msg('JOINING '+s+'... (UP TO HALF A MINUTE)')}
function scr(){post('/screen',{name:$('nm').value,home:$('hm').value,flip:$('fl').checked?1:0}).then(()=>msg('SAVED'))}
st();setInterval(st,2000);scan();
</script></body></html>)HTML";

static void sendJson(JsonDocument& d) { String s; serializeJson(d, s); web->send(200, "application/json", s); }
static void hStatus() {
  JsonDocument d;
  d["id"] = G.id; d["ver"] = HF_VERSION; d["name"] = prefGet("name", G.id); d["home"] = prefGet("home"); d["flip"] = prefFlip() ? 1 : 0;
  d["sta"] = (bool)G.staUp; d["ssid"] = G.wifi; d["ip"] = G.ip; d["joining"] = (bool)G.joining; d["join"] = G.joinSsid; d["msg"] = G.setupMsg;
  d["online"] = (bool)G.wsUp;
  JsonArray a = d["nets"].to<JsonArray>();
  Net nets[MAX_NETS]; int n = netsLoad(nets);
  for (int i = 0; i < n; i++) { JsonObject o = a.add<JsonObject>(); o["s"] = nets[i].s; o["low"] = nets[i].low; }
  sendJson(d);
}
static void hScan() {
  String s;
  plock(); s = scanJson; punlock();
  if (web->hasArg("new") || !s.length()) { scanReq = true; plock(); scanJson = ""; punlock(); web->send(200, "application/json", "{\"busy\":1}"); return; }
  web->send(200, "application/json", s);
}
static void hJoin() {
  String s = web->arg("s"), p = web->arg("p");
  s.trim();
  if (!s.length()) { web->send(400, "application/json", "{\"error\":\"no name\"}"); return; }
  netsAdd(s.c_str(), p.c_str());
  plock(); strlcpy(joinS, s.c_str(), sizeof(joinS)); strlcpy(joinP, p.c_str(), sizeof(joinP)); punlock();
  joinReq = true;
  web->send(200, "application/json", "{\"ok\":1}");
}
static void hForget() { netsForget(web->arg("s").c_str()); web->send(200, "application/json", "{\"ok\":1}"); }
static void hScreen() {
  String nm = web->arg("name"); nm.trim(); nm.toUpperCase();
  String hm = web->arg("home"); hm.toUpperCase();
  String h2; for (char c : hm) if (isalnum((unsigned char)c) && h2.length() < 12) h2 += c;
  if (nm.length() && nm != prefGet("name", G.id)) { prefSet("name", nm.substring(0, 20)); prefSet("renamed", "1"); }
  prefSet("home", h2);
  bool fl = web->arg("flip") == "1";
  if (fl != prefFlip()) { prefSet("flip", fl ? "1" : "0"); G.flipChanged = true; }
  G.wantRelink = true;                          // tell error.blue the new name / home code
  web->send(200, "application/json", "{\"ok\":1}");
}
static void hRoot() { web->send_P(200, "text/html", PAGE); }
static void hOther() {
  // a phone checking for a captive portal (or any other address) gets the setup page
  if (G.apOn) { web->sendHeader("Location", "http://192.168.4.1/", true); web->send(302, "text/plain", ""); }
  else web->send(404, "text/plain", "not here");
}

void webLoop() {
  if (!web && (G.apOn || G.staUp)) {
    web = new WebServer(80);
    web->on("/", hRoot);
    web->on("/status", hStatus);
    web->on("/scan", hScan);
    web->on("/join", HTTP_POST, hJoin);
    web->on("/forget", HTTP_POST, hForget);
    web->on("/screen", HTTP_POST, hScreen);
    web->on("/reboot", HTTP_POST, []() { web->send(200, "application/json", "{\"ok\":1}"); delay(300); ESP.restart(); });
    web->onNotFound(hOther);
    web->begin();
  }
  if (G.apOn && !dns) { dns = new DNSServer(); dns->start(53, "*", IPAddress(192, 168, 4, 1)); }
  if (!G.apOn && dns) { dns->stop(); delete dns; dns = nullptr; }
  if (dns) dns->processNextRequest();
  if (web) web->handleClient();
}

void netBegin() {
  String h = String(G.ap); h.toLowerCase();
  WiFi.setHostname(h.c_str());
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  xTaskCreatePinnedToCore(netLoop, "net", 20480, nullptr, 2, nullptr, 0);   // TLS + JSON + flash need room
}
