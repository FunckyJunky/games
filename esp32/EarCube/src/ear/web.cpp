// web.cpp: EAR-CUBE's page and its API, added to the cube's own web server (net.cpp: netWebExtra), and the
// hotspot that stays on (net.cpp: netHotspot). A phone joins the cube's WiFi (EAR-CUBE-XXXX) and opens
// http://192.168.4.1 (or http://<IP> on the home WiFi). Shared with net.cpp: /dl (download), /api/rm (delete),
// /info.json + /scan + /save (WiFi), /update (firmware).
//   GET  /                     the page (web_page.h)
//   GET  /api/ear?since=N      status, levels and the lines after number N
//   POST /api/ear/go  m=live|rec|stop
//   POST /api/ear/save         LIVE: the lines so far into a transcript      POST /api/ear/clear
//   GET  /api/ear/files        the transcripts           GET /api/ear/text?p=PATH   one of them, as text
//   GET  /api/ear/cfg          settings (+ the lists)    POST /api/ear/cfg          change them
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include "../ui/ui.h"
#include "ear.h"
#include "web_page.h"

bool netHotspot(char* name, size_t nn, char* pass, size_t pn) {
  if (!ecfg.hotspot) return false;
  if (name) earApName(name, nn);
  if (pass) snprintf(pass, pn, "%s", ecfg.apPass);
  return true;
}

static WebServer* W = nullptr;
static void sendJson(JsonDocument& d) {
  String o; serializeJson(d, o);
  W->sendHeader("Cache-Control", "no-store");
  W->send(200, "application/json", o);
}
static bool earPath(const String& p) {
  return (p.startsWith("/sd/EAR/") || p.startsWith("/c/ear/")) && p.indexOf("..") < 0 && p.length() < 120;
}
static const char* modeName(int m) { return m == EAR_LIVE ? "live" : m == EAR_REC ? "rec" : "idle"; }

void netWebExtra(WebServer& w) {
  W = &w;
  w.on("/", HTTP_GET, [] { W->sendHeader("Cache-Control", "no-store"); W->send_P(200, "text/html", EAR_PAGE); });
  // phones look for these when they join a WiFi: answering with the page opens it by itself
  w.on("/generate_204", [] { W->sendHeader("Location", "http://192.168.4.1/"); W->send(302, "text/plain", ""); });
  w.on("/hotspot-detect.html", [] { W->send_P(200, "text/html", EAR_PAGE); });
  w.on("/api/ear", HTTP_GET, [] {
    static EarLine* buf = nullptr;
    if (!buf) buf = (EarLine*)heap_caps_malloc(sizeof(EarLine) * 64, MALLOC_CAP_SPIRAM);
    uint32_t since = W->arg("since").toInt();
    JsonDocument d;
    d["v"] = FW_NAME " " CUBE_VERSION; d["host"] = S.host;
    int st = earStatus();
    d["mode"] = modeName(earMode()); d["st"] = st; d["stText"] = earStatusText(st);
    d["secs"] = earSecs(); d["words"] = earWords(); d["waiting"] = earWaiting(); d["err"] = earErr();
    d["file"] = earFile(); d["speech"] = micSpeech();
    JsonArray lv = d["lvl"].to<JsonArray>(); for (int i = 0; i < 3; i++) lv.add(micLevel(i));
    uint32_t seq = earLineSeq();
    d["seq"] = seq;
    JsonArray ls = d["lines"].to<JsonArray>();
    int n = buf && seq > since ? earLines(buf, 64, since) : 0;
    for (int i = 0; i < n; i++) { JsonObject o = ls.add<JsonObject>(); o["n"] = buf[i].n; if (ecfg.stamps) o["at"] = buf[i].at; o["t"] = buf[i].text; }
    d["net"] = S.net == NET_UP; d["ip"] = S.net == NET_UP ? S.ip : ""; d["sd"] = S.sd; d["bat"] = S.battery ? S.batPct : -1;
    sendJson(d);
  });
  w.on("/api/ear/go", HTTP_POST, [] {
    String m = W->arg("m");
    JsonDocument d; d["ok"] = true;
    if (m == "stop") earWant(EAR_IDLE);
    else if (!micOk()) { d["ok"] = false; d["err"] = "NO MICROPHONES FOUND"; }
    else if (earMode() != EAR_IDLE) { d["ok"] = false; d["err"] = "STOP FIRST"; }
    else if (m == "live") { if (S.net != NET_UP) { d["ok"] = false; d["err"] = "LIVE NEEDS THE INTERNET (WIFI TAB). RECORD WORKS WITHOUT."; } else earWant(EAR_LIVE); }
    else if (m == "rec") earWant(EAR_REC);
    S.wakeAt = millis() | 1; uiDirty();       // the screen comes on (screens.cpp: appsLoop)
    sendJson(d);
  });
  w.on("/api/ear/save", HTTP_POST, [] {
    char p[96]; JsonDocument d;
    d["ok"] = earSaveLive(p, sizeof(p));
    if (d["ok"]) { d["path"] = p; d["name"] = strrchr(p, '/') + 1; }
    sendJson(d);
  });
  w.on("/api/ear/clear", HTTP_POST, [] { earClearLines(); JsonDocument d; d["ok"] = true; sendJson(d); });
  w.on("/api/ear/files", HTTP_GET, [] {
    static EarFile* f = nullptr;
    if (!f) f = (EarFile*)heap_caps_malloc(sizeof(EarFile) * 150, MALLOC_CAP_SPIRAM);
    int n = f ? earFiles(f, 150) : 0;
    JsonDocument d;
    d["where"] = S.sd ? "A:\\EAR (TF CARD)" : "C:\\EAR (THE CUBE, NO TF CARD)";
    JsonArray a = d["files"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
      JsonObject o = a.add<JsonObject>();
      o["p"] = f[i].path; o["name"] = f[i].name; o["size"] = f[i].size; o["wav"] = f[i].hasWav; o["live"] = f[i].live;
      o["cur"] = !strcmp(f[i].path, earFile());
    }
    sendJson(d);
  });
  w.on("/api/ear/text", HTTP_GET, [] {
    String p = W->arg("p");
    FILE* f = earPath(p) ? fopen(p.c_str(), "rb") : nullptr;
    if (!f) { W->send(404, "text/plain", "Not found"); return; }
    fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
    W->sendHeader("Cache-Control", "no-store");
    W->setContentLength(n);
    W->send(200, "text/plain; charset=utf-8", "");
    static uint8_t* b = nullptr; if (!b) b = (uint8_t*)malloc(4096);
    size_t k;
    while (b && (k = fread(b, 1, 4096, f)) > 0) W->sendContent((const char*)b, k);
    fclose(f);
  });
  w.on("/api/ear/cfg", HTTP_GET, [] {
    JsonDocument d;
    JsonObject c = d["cfg"].to<JsonObject>();
    c["lang"] = ecfg.lang; c["task"] = ecfg.task; c["sens"] = ecfg.sens; c["gain"] = ecfg.gain; c["mics"] = ecfg.mics;
    c["pause"] = ecfg.pause; c["stamps"] = ecfg.stamps; c["keepAudio"] = ecfg.keepAudio; c["liveKeep"] = ecfg.liveKeep;
    c["hotspot"] = ecfg.hotspot; c["apName"] = ecfg.apName; c["apPass"] = ecfg.apPass;
    char n[33]; earApName(n, sizeof(n)); d["apName"] = n;
    JsonArray l = d["langs"].to<JsonArray>(); for (int i = 0; i < N_LANGS; i++) l.add(LANGS[i].name);
    JsonArray g = d["gains"].to<JsonArray>(); for (int i = 0; i < 15; i++) g.add(gainLabel(i));
    sendJson(d);
  });
  w.on("/api/ear/cfg", HTTP_POST, [] {
    auto num = [](const char* k, uint8_t& v, int hi) { if (W->hasArg(k)) { int x = W->arg(k).toInt(); if (x >= 0 && x <= hi) v = x; } };
    char oldName[24], oldPass[24]; memcpy(oldName, ecfg.apName, 24); memcpy(oldPass, ecfg.apPass, 24);
    uint8_t oldHs = ecfg.hotspot, oldGain = ecfg.gain;
    num("lang", ecfg.lang, N_LANGS - 1); num("task", ecfg.task, 1); num("sens", ecfg.sens, 3); num("gain", ecfg.gain, 14);
    num("mics", ecfg.mics, 2); num("pause", ecfg.pause, 2); num("stamps", ecfg.stamps, 1); num("keepAudio", ecfg.keepAudio, 1);
    num("liveKeep", ecfg.liveKeep, 1); num("hotspot", ecfg.hotspot, 1);
    if (W->hasArg("apName")) { String s = W->arg("apName"); s.trim(); snprintf(ecfg.apName, sizeof(ecfg.apName), "%s", s.c_str()); }
    if (W->hasArg("apPass")) { String s = W->arg("apPass"); if (!s.length() || s.length() >= 8) snprintf(ecfg.apPass, sizeof(ecfg.apPass), "%s", s.c_str()); }
    earCfgSave();
    if (ecfg.gain != oldGain) micGain(ecfg.gain);
    bool hs = ecfg.hotspot != oldHs || memcmp(oldName, ecfg.apName, 24) || memcmp(oldPass, ecfg.apPass, 24);
    JsonDocument d; d["ok"] = true; d["hs"] = hs;
    sendJson(d);
    uiDirty();
    if (hs && S.portal) { delay(300); netPortal(false); }    // net.cpp starts it again with the new name / password
  });
}
