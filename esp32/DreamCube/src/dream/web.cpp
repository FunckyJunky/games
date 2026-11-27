// web.cpp: DREAM-CUBE's page and its API, added to the cube's own web server (net.cpp: netWebExtra), and the
// hotspot that stays on (net.cpp: netHotspot). A phone joins the cube's WiFi (DREAM-CUBE-XXXX) and opens
// http://192.168.4.1 (or http://<IP> on the home WiFi): the home WiFi + password, how the cube is, a few buttons,
// the hotspot's name and password, and the way to the control page at error.blue/dream.
// Shared with net.cpp: /info.json + /scan + /save (WiFi), /update (firmware).
//   GET  /                    the page (web_page.h)
//   GET  /api/dream           how it is (+ the settings and their lists)
//   POST /api/dream/go        m=listen|pause|prev|next|wake|sleep  or  m=say&t=WORDS
//   POST /api/dream/cfg       settings (the same names as the control page's), hotspot name / password
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "../ui/ui.h"
#include "../net/net.h"
#include "dream.h"
#include "web_page.h"

bool netHotspot(char* name, size_t nn, char* pass, size_t pn) {
  if (!dcfg.hotspot) return false;
  if (name) dreamApName(name, nn);
  if (pass) snprintf(pass, pn, "%s", dcfg.apPass);
  return true;
}

static WebServer* W = nullptr;
static void sendJson(JsonDocument& d) {
  String o; serializeJson(d, o);
  W->sendHeader("Cache-Control", "no-store");
  W->send(200, "application/json", o);
}
void netWebExtra(WebServer& w) {
  W = &w;
  w.on("/", HTTP_GET, [] { W->sendHeader("Cache-Control", "no-store"); W->send_P(200, "text/html", DREAM_PAGE); });
  // phones look for these when they join a WiFi: answering with the page opens it by itself
  w.on("/generate_204", [] { W->sendHeader("Location", "http://192.168.4.1/"); W->send(302, "text/plain", ""); });
  w.on("/hotspot-detect.html", [] { W->send_P(200, "text/html", DREAM_PAGE); });
  w.on("/api/dream", HTTP_GET, [] {
    JsonDocument d;
    d["v"] = FW_NAME " " CUBE_VERSION; d["host"] = S.host;
    static char st[900];
    dreamState(st, sizeof(st));
    JsonDocument s; deserializeJson(s, st); d["s"] = s;
    d["code"] = dcfg.code; d["link"] = dcfg.link; d["linked"] = enetLinkAt() && millis() - enetLinkAt() < 60000;
    d["net"] = S.net == NET_UP; d["ip"] = S.net == NET_UP ? S.ip : ""; d["ssid"] = S.ssid;
    d["mic"] = micOk(); d["speech"] = micSpeech(); d["lvl"] = micLevel(0); d["err"] = dreamErr(); d["think"] = dreamThinking();
    char n[33]; dreamApName(n, sizeof(n)); d["apName"] = n; d["apPass"] = dcfg.apPass; d["hotspot"] = dcfg.hotspot;
    JsonArray l = d["langs"].to<JsonArray>(); for (int i = 0; i < N_LANGS; i++) l.add(LANGS[i].name);
    JsonArray g = d["gains"].to<JsonArray>(); for (int i = 0; i < 15; i++) g.add(gainLabel(i));
    sendJson(d);
  });
  w.on("/api/dream/go", HTTP_POST, [] {
    String m = W->arg("m");
    JsonDocument d; d["ok"] = true;
    if (m == "say") {
      String t = W->arg("t"); t.trim();
      if (!t.length()) d["ok"] = false;
      else if (S.net != NET_UP) { d["ok"] = false; d["err"] = "THE CUBE HAS NO INTERNET: SET THE WIFI BELOW"; }
      else dreamSay(t.c_str());
    } else if (m == "listen" || m == "pause" || m == "prev" || m == "next" || m == "wake" || m == "sleep") dreamCmd(0, m.c_str(), "");
    else d["ok"] = false;
    if (m != "sleep") S.wakeAt = millis() | 1;   // the screen comes on (screens.cpp: appsLoop)
    uiDirty();
    sendJson(d);
  });
  w.on("/api/dream/cfg", HTTP_POST, [] {
    // the same settings as the control page: through the main loop (listen.cpp: dreamCmd "cfg")
    JsonDocument c;
    static const char* K[] = {"often", "mind", "words", "title", "cam", "soft", "detail", "wake", "lang", "sens", "gain", "mics"};
    for (auto k : K) if (W->hasArg(k)) c[k] = W->arg(k).toInt();
    String js; serializeJson(c, js);
    dreamCmd(0, "cfg", js.c_str());
    char oldName[24], oldPass[24]; memcpy(oldName, dcfg.apName, 24); memcpy(oldPass, dcfg.apPass, 24);
    uint8_t oldHs = dcfg.hotspot;
    if (W->hasArg("hotspot")) dcfg.hotspot = W->arg("hotspot").toInt() ? 1 : 0;
    if (W->hasArg("link")) dcfg.link = W->arg("link").toInt() ? 1 : 0;
    if (W->hasArg("apName")) { String s = W->arg("apName"); s.trim(); snprintf(dcfg.apName, sizeof(dcfg.apName), "%s", s.c_str()); }
    if (W->hasArg("apPass")) { String s = W->arg("apPass"); if (!s.length() || s.length() >= 8) snprintf(dcfg.apPass, sizeof(dcfg.apPass), "%s", s.c_str()); }
    dreamCfgSave();
    bool hs = dcfg.hotspot != oldHs || memcmp(oldName, dcfg.apName, 24) || memcmp(oldPass, dcfg.apPass, 24);
    JsonDocument d; d["ok"] = true; d["hs"] = hs;
    sendJson(d);
    uiDirty();
    if (hs && S.portal) { delay(300); netPortal(false); }    // net.cpp starts it again with the new name / password
  });
}
