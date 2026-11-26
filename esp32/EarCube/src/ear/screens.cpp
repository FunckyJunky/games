// screens.cpp: EAR-CUBE's screens. The cube starts straight into them:
//   LIVE    what's said around the cube, as text, a few seconds later (nothing kept unless you want)
//   REC     a long recording: everything said goes into a text file, with the time (TF card: A:\EAR)
//   TEXTS   the transcripts: tap = read, hold = delete
//   SETUP   language, sensitivity, microphones, saving, the hotspot, WiFi, sound + screen, firmware
// Uses ERROR-CUBE's interface (src/ui), board and drawing as they are, in ERROR-CUBE's colours.
#include "../ui/ui.h"
#include "../lib/cubeqr.h"
#include "ear.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>

// ------------------------------------------------------------------ the tables behind the settings (ui.cpp, board)
const uint16_t OFF_SECS[] = {15, 30, 60, 120, 300, 600, 1800, 0};
const int N_OFF = 8;
const uint16_t CHARGE_MA[] = {100, 200, 300, 400, 500, 700, 1000};
const int N_CHARGE = 7;
const uint16_t AUTO_OFF_MIN[] = {0, 10, 30, 60, 180};
const int N_AUTO_OFF = 5;
const uint8_t UPD_HOURS[] = {1, 6, 24};
const int N_UPD = 3;
const Tz TIMEZONES[] = {
  {"AMSTERDAM/BERLIN", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"LONDON/DUBLIN", "GMT0IST,M3.5.0/1,M10.5.0"},
  {"LISBON", "WET0WEST,M3.5.0/1,M10.5.0"},
  {"KYIV/HELSINKI", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"ISTANBUL", "<+03>-3"},
  {"MOSCOW", "MSK-3"},
  {"DUBAI", "<+04>-4"},
  {"INDIA", "IST-5:30"},
  {"BANGKOK", "<+07>-7"},
  {"CHINA/SINGAPORE", "CST-8"},
  {"TOKYO/SEOUL", "JST-9"},
  {"SYDNEY", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"AUCKLAND", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
  {"UTC", "UTC0"},
  {"NEW YORK", "EST5EDT,M3.2.0,M11.1.0"},
  {"CHICAGO", "CST6CDT,M3.2.0,M11.1.0"},
  {"DENVER", "MST7MDT,M3.2.0,M11.1.0"},
  {"LOS ANGELES", "PST8PDT,M3.2.0,M11.1.0"},
  {"SAO PAULO", "<-03>3"},
};
const int N_TZ = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);
std::vector<Prog> PROGS;

void cfgChanged() {
  gfxTheme(THEME_COLOR, lowFx() ? 0 : cfg.glow);
  sysBrightness(uiBrightNow());
  sysApplyRotation();
  sysApplyButtons();
  cfgSave();
  uiDirty();
}

// ------------------------------------------------------------------ little drawings
// a microphone: capsule, holder, stand (s = scale, 1 = about 60 px high)
static void drawMic(int cx, int cy, float s, uint16_t c, bool filled) {
  int w = (int)(26 * s), h = (int)(44 * s), r = w / 2, t = s > 1.4f ? 4 : 3;
  int top = cy - (int)(34 * s);
  if (filled) { fillCircle(cx, top + r, r, c); fillRect(cx - r, top + r, w, h - w, c); fillCircle(cx, top + h - r, r, c); }
  else {
    ring(cx, top + r, r, t, c); ring(cx, top + h - r, r, t, c);
    fillRect(cx - r, top + r, w, h - w, C_BG);
    fillRect(cx - r, top + r, t, h - w, c); fillRect(cx + r - t, top + r, t, h - w, c);
    fillRect(cx - r + t, top + r - 2, w - 2 * t, 4, C_BG);
    fillRect(cx - r + t, top + h - r - 2, w - 2 * t, 4, C_BG);
    ring(cx, top + r, r, t, c);
  }
  // the holder: a U around the lower half, then the stand
  int R = (int)(22 * s), yc = top + h - r - (int)(4 * s);
  for (int a = 0; a <= 180; a += 4) {
    float rad = a * 3.14159f / 180;
    int x = cx + (int)(cosf(rad) * R), y = yc + (int)(sinf(rad) * R);
    fillRect(x - t / 2, y - t / 2, t, t, c);
  }
  fillRect(cx - t / 2, yc + R, t, (int)(12 * s), c);
  fillRect(cx - (int)(14 * s), yc + R + (int)(12 * s), (int)(28 * s), t, c);
}
// sound waves left and right of a point, louder = more of them
static void waves(int cx, int cy, int lvl, uint16_t c) {
  int n = lvl > 70 ? 3 : lvl > 40 ? 2 : lvl > 12 ? 1 : 0;
  for (int i = 0; i < n; i++) {
    int R = 52 + i * 18;
    for (int a = -40; a <= 40; a += 3) {
      float rad = a * 3.14159f / 180;
      int dx = (int)(cosf(rad) * R), dy = (int)(sinf(rad) * R);
      fillRect(cx + dx - 1, cy + dy - 1, 3, 3, c);
      fillRect(cx - dx - 1, cy + dy - 1, 3, 3, c);
    }
  }
}
static uint16_t stColor(int st) {
  switch (st) {
    case ST_HEAR: return mix(C_FG, C_WHITE, 40);
    case ST_SEND: case ST_PAUSED: return C_WARN;
    case ST_OFFLINE: case ST_LIMIT: case ST_ERROR: case ST_NOMIC: return C_RED;
    case ST_READY: return C_DIM;
  }
  return C_FG;
}
// a small level bar with its letter
static void miniMeter(int x, int y, const char* k, int pct) {
  drawText(FT, x, y - 4, k, C_DIM, TX_FLAT);
  int bx = x + 18, bw = 54, bh = 20;
  rect(bx, y, bw, bh, C_DIM);
  int iw = (bw - 4) * (pct < 0 ? 0 : pct > 100 ? 100 : pct) / 100;
  fillRect(bx + 2, y + 2, iw, bh - 4, pct > 92 ? C_RED : C_FG);
}
static void fmtSecs(char* o, size_t n, uint32_t s) {
  if (s >= 3600) snprintf(o, n, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  else snprintf(o, n, "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}
static void dosPath(const char* path, char* o, size_t n) {      // "/sd/EAR/x.txt" -> "A:\EAR\x.txt"
  const char* p = path; std::string s;
  if (!strncmp(p, "/sd", 3)) { s = "A:"; p += 3; } else if (!strncmp(p, "/c", 2)) { s = "C:"; p += 2; }
  for (; *p; p++) s += *p == '/' ? '\\' : *p;
  snprintf(o, n, "%s", s.c_str());
}

// ------------------------------------------------------------------ settings rows (as in ERROR-CUBE's SETUP)
static Row heading(const char* n, uint8_t ic = 0) { Row r; r.name = n; r.kind = 4; r.icon = ic; return r; }
static Row info(const char* n, std::function<void(char*)> v) { Row r; r.name = n; r.val = v; r.kind = 3; return r; }
static Row action(const char* n, std::function<void()> f, std::function<void(char*)> v = nullptr) {
  Row r; r.name = n; r.val = v; r.kind = 0; r.act = [f](int) { f(); }; return r;
}
// a choice in ERROR-CUBE's settings (cfg) or EAR-CUBE's own (ecfg)
static Row choice(const char* n, uint8_t* v, int count, std::function<const char*(int)> label, std::function<void()> after = nullptr, bool ear = false) {
  Row r; r.name = n; r.kind = 1;
  r.val = [v, label](char* o) { snprintf(o, 40, "%s", label(*v)); };
  r.act = [v, count, after, ear](int d) {
    *v = (*v + (d < 0 ? count - 1 : 1)) % count;
    if (after) after();
    if (ear) { earCfgSave(); uiDirty(); } else cfgChanged();
  };
  return r;
}
static Row echoice(const char* n, uint8_t* v, int count, std::function<const char*(int)> label, std::function<void()> after = nullptr) {
  return choice(n, v, count, label, after, true);
}
static Row etoggle(const char* n, uint8_t* v, std::function<void()> after = nullptr) {
  return echoice(n, v, 2, [](int i) { return i ? "ON" : "OFF"; }, after);
}
static Row toggle(const char* n, uint8_t* v, std::function<void()> after = nullptr) {
  return choice(n, v, 2, [](int i) { return i ? "ON" : "OFF"; }, after);
}
static Row slider(const char* n, uint8_t* v, int lo, std::function<void()> live = nullptr) {
  Row r; r.name = n; r.kind = 2;
  r.pct = [v]() { return (int)*v; };
  r.setPct = [v, lo, live](int p) { *v = p < lo ? lo : p; if (live) live(); };
  r.act = [v, lo, live](int) { *v = *v >= 100 ? lo : (*v + 10 > 100 ? 100 : *v + 10); if (live) live(); cfgChanged(); };
  return r;
}
static const char* offLabel(int i) {
  static char b[12]; int s = OFF_SECS[i];
  if (!s) return "NEVER";
  if (s < 60) snprintf(b, sizeof(b), "%d SEC", s); else snprintf(b, sizeof(b), "%d MIN", s / 60);
  return b;
}

// ------------------------------------------------------------------ SWIPE TO CONFIRM: something that can't be undone
// Drag the knob all the way to the right (a tap or a short drag does nothing; let go early and it slides back).
// ESC / - = cancel, hold + = confirm (the buttons are a deliberate press too).
struct SwipeConfirm : Screen {
  std::string title, text, label; Act act;
  float kx = 0, vx = 0; bool drag = false; int grab = 0; uint32_t lastT = 0;
  static const int TX = 30, TY = 280, TW = 420, TH = 76, KW = 76;
  SwipeConfirm(const char* t, const char* x, const char* l, Act a) : title(t), text(x), label(l), act(a) {}
  const char* name() override { return "SURE?"; }
  bool noSwipeBack() override { return true; }
  bool keepAwake() override { return true; }
  float maxX() const { return TW - KW; }
  void done() { Act a = act; snd(SND_OK); uiBack(); if (a) a(); }   // this screen is gone after uiBack: the copy runs
  void tick() override {
    if (!drag && kx > 0) { kx -= 18; if (kx < 0) kx = 0; uiDirty(); redrawIn(16); }   // slides back
  }
  void draw() override {
    pathLine("C:\\EAR\\SURE?>");
    drawTextC(FB, 240, 104, title.c_str(), C_FG);
    char l[4][64]; int n = wrapText(text.c_str(), 28, l, 4);
    for (int i = 0; i < n; i++) drawTextC(FT, 240, 160 + i * 30, l[i], C_DIM, TX_FLAT);
    // the track, what's done so far lit up, the knob
    rect(TX, TY, TW, TH, C_FG);
    int k = (int)kx;
    if (k > 0) fillRect(TX + 3, TY + 3, k, TH - 6, mix(C_BG, C_RED, 45));
    int pct = (int)(kx * 100 / maxX());
    drawTextC(FT, TX + KW + (TW - KW) / 2, TY + (TH - LH) / 2, label.c_str(), pct > 40 ? C_WHITE : C_DIM, TX_FLAT);
    invBar(TX + k, TY, KW, TH);
    drawTextC(FB, TX + k + KW / 2, TY + (TH - 42) / 2, "\xE2\x86\x92", C_BG, TX_FLAT);
    fkeys({{"ESC", "Cancel", [] { snd(SND_OFF); uiBack(); }}});
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_DOWN) {
      if (e.x >= TX + kx - 10 && e.x < TX + kx + KW + 10 && e.y >= TY - 20 && e.y < TY + TH + 20) { drag = true; grab = e.x - (TX + (int)kx); }
      return true;
    }
    if (e.t == EV_MOVE && drag) {
      kx = e.x - TX - grab; if (kx < 0) kx = 0; if (kx > maxX()) kx = maxX();
      uiDirty(); return true;
    }
    if (e.t == EV_UP && drag) {
      drag = false;
      if (kx >= maxX() * 0.92f) { done(); return true; }
      uiDirty(); redrawIn(16); return true;
    }
    if (e.t == EV_KEY && e.key == K_OK) { done(); return true; }
    if (e.t == EV_KEY && (e.key == K_BACK || e.key == K_PREV)) { snd(SND_OFF); uiBack(); return true; }
    return e.t == EV_SWIPE || e.t == EV_TAP || e.t == EV_LONG;
  }
};
static void confirmClear() {
  if (!earLineSeq()) { toast("NOTHING TO CLEAR"); return; }
  uiOpen(new SwipeConfirm("CLEAR THE TEXT?", "The lines on this screen go away (saved transcripts stay). Swipe to clear.",
                          "SWIPE TO CLEAR", [] { earClearLines(); toast("CLEARED"); }));
}

// ------------------------------------------------------------------ READ: a transcript, scrolling
struct Reader : Screen {
  std::string path, title; std::vector<std::string> lines; std::vector<uint8_t> kind; Scroll sc; bool atEnd = true; long seenSize = -1;
  explicit Reader(const char* p) : path(p) { char t[48]; earNiceName(p, t, sizeof(t)); title = t; load(); }
  const char* name() override { return "READ"; }
  void load() {
    lines.clear(); kind.clear();
    long n = fsSize(path.c_str()); seenSize = n;
    if (n < 0) { lines.push_back("(THE FILE IS GONE)"); kind.push_back(1); return; }
    const long MAX = 192 * 1024;                 // a very long one: its last 192 KB
    void* f = fsOpen(path.c_str(), "rb");
    if (!f) return;
    long from = n > MAX ? n - MAX : 0;
    fsSeekF(f, from);
    std::string s; s.resize(n - from);
    size_t got = fsReadF(f, &s[0], n - from); s.resize(got);
    fsClose(f);
    if (from) { size_t k = s.find('\n'); s = "(...)\n" + (k == std::string::npos ? s : s.substr(k + 1)); }
    size_t p = 0;
    while (p <= s.size()) {
      size_t e = s.find('\n', p); if (e == std::string::npos) e = s.size();
      std::string l = s.substr(p, e - p);
      for (auto& ch : l) if (ch == '\r' || ch == '\t') ch = ' ';
      uint8_t k = l.size() && l[0] == '[' ? 0 : 2;              // a line with its time / the head of the file
      if (l.empty()) { lines.push_back(""); kind.push_back(2); }
      else {
        static char wl[40][64];
        int m = wrapText(l.c_str(), 29, wl, 40);
        for (int i = 0; i < m; i++) { lines.push_back(wl[i]); kind.push_back(i == 0 ? k : (k == 0 ? 3 : 2)); }
      }
      p = e + 1;
      if (lines.size() > 6000) break;
    }
    while (lines.size() && lines.back().empty()) { lines.pop_back(); kind.pop_back(); }
  }
  void layout() { sc.x = 15; sc.y = 92; sc.w = 450; sc.h = 330; sc.view = 330; sc.total = lines.size() * LH; if (atEnd) sc.pos = sc.maxPos(); sc.clamp(); }
  void enter() override { layout(); }
  void tick() override {
    sc.tick();
    // the one being written: new lines come in
    static uint32_t at = 0;
    if (msNow() - at > 1500) {
      at = msNow();
      if (!strcmp(earFile(), path.c_str()) && fsSize(path.c_str()) != seenSize) { atEnd = sc.pos >= sc.maxPos() - 2; load(); layout(); uiDirty(); }
    }
  }
  void draw() override {
    char t[48]; snprintf(t, sizeof(t), "%s", title.c_str()); utf8Cut(t, 26);
    drawText(FT, colX(0), TOP_Y, t, C_FG);
    if (!strcmp(earFile(), path.c_str())) { fillCircle(455, TOP_Y + 16, 7, (msNow() / 500) % 2 ? C_RED : C_BG); redrawIn(500); }
    hline(15, 90, 450, C_DIM);
    sc.view = sc.h; sc.total = lines.size() * LH; sc.clamp(); sc.tick();
    clipSet(15, 92, 436, 330);
    int first = (int)(sc.pos / LH);
    for (int i = first; i < (int)lines.size() && i <= first + 11; i++) {
      int y = 92 + i * LH - (int)sc.pos;
      const std::string& l = lines[i];
      if (kind[i] == 0 && l.size() > 10 && l[0] == '[') {       // "[22:55:07] text": the time dim
        size_t b = l.find(']');
        if (b != std::string::npos && b < 14) {
          std::string tm = l.substr(0, b + 1);
          int x = drawText(FT, 15, y, tm.c_str(), C_DIM, TX_FLAT);
          drawText(FT, x, y, l.c_str() + b + 1, C_FG);
          continue;
        }
      }
      drawText(FT, 15, y, l.c_str(), kind[i] == 2 ? C_DIM : C_FG, kind[i] == 2 ? TX_FLAT : TX_GLOW);
    }
    clipReset();
    sc.drawBar();
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"\xE2\x86\x93", "End", [this] { atEnd = true; layout(); uiDirty(); }},
           {"DEL", "Delete", [this] { askDelete(); }}});
  }
  void askDelete() {
    if (!strcmp(earFile(), path.c_str())) { toast("STOP THE RECORDING FIRST"); return; }
    std::string p = path;
    ask("DELETE", (title + "?").c_str(), {"DELETE", "KEEP"}, [p](int i) {
      if (i) return;
      std::string w = p; w.replace(w.size() - 4, 4, ".wav");
      bool ok = fsRemove(p.c_str()); fsRemove(w.c_str());
      toast(ok ? "DELETED" : "CAN'T DELETE IT");
      if (ok) uiBack();
    });
  }
  bool ev(const Ev& e) override {
    if (sc.ev(e)) { atEnd = false; return true; }
    if (e.t == EV_KEY && e.key == K_NEXT) { sc.vel = 0; sc.pos += LH * 8; sc.clamp(); atEnd = sc.pos >= sc.maxPos() - 2; uiDirty(); return true; }
    if (e.t == EV_KEY && e.key == K_PREV) { sc.vel = 0; sc.pos -= LH * 8; sc.clamp(); atEnd = false; uiDirty(); return true; }
    return false;
  }
};

// ------------------------------------------------------------------ MIC TEST: what the microphones hear
struct MicTest : Screen {
  const char* name() override { return "MIC TEST"; }
  bool keepAwake() override { return true; }
  void tick() override { redrawIn(50); }
  void gain(int d) {
    int g = ecfg.gain + d; if (g < 0) g = 0; if (g > 14) g = 14;
    ecfg.gain = g; micGain(g); earCfgSave(); uiDirty();
  }
  void draw() override {
    pathLine("C:\\EAR\\MICTEST>");
    if (!micOk()) { drawTextC(FT, 240, 200, "NO MICROPHONES FOUND", C_RED); fkeysBack(); return; }
    // the three bars, in dB: -66 .. -6
    const char* K[3] = {"MIX", "L", "R"};
    for (int i = 0; i < 3; i++) {
      int y = 104 + i * 46;
      drawText(FT, 15, y, K[i], C_DIM, TX_FLAT);
      meter(75, y + 2, 390, 30, micLevel(i), true);
    }
    // the scale: background noise and the speech line
    int x0 = 75, w = 390, y = 250;
    auto px = [&](float db) { int p = (int)((db + 66) * w / 60); return x0 + (p < 0 ? 0 : p > w ? w : p); };
    rect(x0, y, w, 34, C_DIM);
    int now = px(micDb()), fl = px(micFloorDb()), gt = px(micGateDb());
    bool sp = micDb() > micGateDb();
    fillRect(x0 + 2, y + 2, now - x0 - 2 > 0 ? now - x0 - 2 : 0, 30, sp ? C_FG : C_DIM);
    fillRect(fl - 1, y - 8, 3, 50, C_WARN);
    fillRect(gt - 1, y - 8, 3, 50, C_RED);
    drawText(FT, 15, y, "NOW", C_DIM, TX_FLAT);
    char b[64];
    snprintf(b, sizeof(b), "NOISE %d dB", (int)micFloorDb()); drawText(FT, 15, y + 44, b, C_WARN, TX_FLAT);
    snprintf(b, sizeof(b), "SPEECH > %d dB", (int)micGateDb()); drawTextR(FT, 465, y + 44, b, C_RED, TX_FLAT);
    drawTextC(FT, 240, y + 84, sp ? "SPEECH" : "QUIET", sp ? C_FG : C_DIM);
    snprintf(b, sizeof(b), "GAIN %s", gainLabel(ecfg.gain)); drawTextC(FT, 240, y + 120, b, C_FG);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"-", "Less", [this] { gain(-1); }}, {"+", "More", [this] { gain(1); }}});
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY && e.key == K_PREV) { gain(-1); return true; }
    if (e.t == EV_KEY && e.key == K_NEXT) { gain(1); return true; }
    return false;
  }
};

// ------------------------------------------------------------------ PHONE: QR codes to join the hotspot and open the page
static std::string wifiQrText() {
  char name[33]; earApName(name, sizeof(name));
  auto esc = [](const char* s) { std::string o; for (; *s; s++) { if (strchr("\\;,:\"", *s)) o += '\\'; o += *s; } return o; };
  if (strlen(ecfg.apPass) >= 8) return "WIFI:T:WPA;S:" + esc(name) + ";P:" + esc(ecfg.apPass) + ";;";
  return "WIFI:T:nopass;S:" + esc(name) + ";;";
}
static void drawQr(const char* text, int cx, int y0, int size) {
  static uint8_t qr[cubeqr_BUFFER_LEN_FOR_VERSION(8)], tmp[cubeqr_BUFFER_LEN_FOR_VERSION(8)];
  if (!cubeqr_encodeText(text, tmp, qr, cubeqr_Ecc_MEDIUM, 1, 8, cubeqr_Mask_AUTO, true)) return;
  int n = cubeqr_getSize(qr), sc = size / (n + 4), sz = sc * (n + 4), x0 = cx - sz / 2;
  fillRect(x0, y0, sz, sz, C_WHITE);
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (cubeqr_getModule(qr, x, y)) fillRect(x0 + (x + 2) * sc, y0 + (y + 2) * sc, sc, sc, C_BLACK);
}
struct Phone : Screen {
  int page = 0;
  const char* name() override { return "PHONE"; }
  bool keepAwake() override { return true; }
  void draw() override {
    char name[33]; earApName(name, sizeof(name));
    if (!ecfg.hotspot) {
      drawTextC(FT, 240, 160, "THE HOTSPOT IS OFF", C_WARN);
      drawTextC(FT, 240, 200, "SETUP > HOTSPOT: ON", C_DIM, TX_FLAT);
      if (S.net == NET_UP) { char b[48]; snprintf(b, sizeof(b), "OR ON YOUR WIFI: %s", S.ip); drawTextC(FT, 240, 260, b, C_FG); }
      fkeysBack(); return;
    }
    if (page == 0) {
      drawText(FT, 15, TOP_Y, "1. JOIN THE CUBE'S WIFI", C_FG);
      drawQr(wifiQrText().c_str(), 240, 96, 230);
      drawTextC(FT, 240, 334, name, C_FG);
      char pw[48];
      if (strlen(ecfg.apPass) >= 8) snprintf(pw, sizeof(pw), "PASSWORD %s", ecfg.apPass); else snprintf(pw, sizeof(pw), "NO PASSWORD (OPEN)");
      drawTextC(FT, 240, 366, pw, C_DIM, TX_FLAT);
    } else {
      drawText(FT, 15, TOP_Y, "2. OPEN THE PAGE", C_FG);
      drawQr("http://192.168.4.1/", 240, 96, 230);
      drawTextC(FT, 240, 334, "http://192.168.4.1", C_FG);
      char b[48];
      if (S.net == NET_UP) snprintf(b, sizeof(b), "ON YOUR WIFI: %s", S.ip); else snprintf(b, sizeof(b), "LIVE TEXT + TRANSCRIPTS");
      drawTextC(FT, 240, 366, b, C_DIM, TX_FLAT);
    }
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {page ? "1" : "2", page ? "Join" : "Open", [this] { page = !page; uiDirty(); }}});
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_TAP && e.y < BOT_Y) { page = !page; uiDirty(); return true; }
    if (e.t == EV_KEY && (e.key == K_NEXT || e.key == K_PREV || e.key == K_OK)) { page = !page; uiDirty(); return true; }
    return false;
  }
};

// ------------------------------------------------------------------ WIFI (as in ERROR-CUBE), with the hotspot
struct Wifi : Screen {
  WifiNet nets[16]; int nn = -1; char saved[6][33]; int ns = 0;
  std::vector<Row> rows; ListView L;
  const char* name() override { return "WIFI"; }
  void enter() override { L.setBox(15, 92, 450, 330); L.rowH = 55; if (cfg.wifiOn) netScan(); }
  void join(const char* ssid, bool open, bool known) {
    std::string s = ssid;
    if (open || known) { netJoin(ssid, nullptr); toast("JOINING..."); return; }
    kbOpen("PASSWORD", "", 63, [s](const char* p) { netJoin(s.c_str(), p); toast("JOINING..."); }, true);
  }
  void build() {
    rows.clear();
    rows.push_back(toggle("WIFI", &cfg.wifiOn, [] { netWifi(cfg.wifiOn); }));
    rows.push_back(info("STATUS", [](char* o) {
      static const char* N[5] = {"OFF", "JOINING", "CONNECTED", "FAILED", "NONE SAVED"};
      snprintf(o, 40, "%s", N[S.net % 5]);
    }));
    if (S.net == NET_UP) {
      rows.push_back(info("NETWORK", [](char* o) { snprintf(o, 40, "%s", S.ssid); utf8Cut(o, 16); }));
      rows.push_back(info("ADDRESS", [](char* o) { snprintf(o, 40, "%s", S.ip); }));
    }
    if (!cfg.wifiOn) return;
    rows.push_back(heading("NETWORKS"));
    if (nn < 0) rows.push_back(info("SCANNING...", nullptr));
    for (int i = 0; i < nn; i++) {
      WifiNet* w = &nets[i];
      Row r; r.name = w->ssid; r.kind = 0;
      r.val = [w](char* o) { sprintf(o, "%s%s %s", w->saved ? "*" : "", w->open ? "" : "\xE2\x96\xA0", w->rssi > -60 ? "####" : w->rssi > -70 ? "###" : w->rssi > -80 ? "##" : "#"); };
      r.act = [this, w](int) { join(w->ssid, w->open, w->saved); };
      rows.push_back(r);
    }
    rows.push_back(action("SCAN AGAIN", [this] { nn = -1; netScan(); }));
    if (ns) rows.push_back(heading("SAVED"));
    for (int i = 0; i < ns; i++) {
      char* s = saved[i];
      Row r; r.name = s; r.kind = 0; r.val = [](char* o) { strcpy(o, "..."); };
      r.act = [s](int) {
        std::string ss = s;
        ask("SAVED", s, {"JOIN", "FORGET", "CANCEL"}, [ss](int k) { if (k == 0) netJoin(ss.c_str(), nullptr); else if (k == 1) { netForget(ss.c_str()); toast("FORGOTTEN"); } });
      };
      rows.push_back(r);
    }
    rows.push_back(heading("FROM A PHONE"));
    rows.push_back(info("1. JOIN WIFI", [](char* o) { if (S.portal) snprintf(o, 40, "%s", S.portalName); else strcpy(o, ecfg.hotspot ? "STARTING..." : "HOTSPOT OFF"); }));
    rows.push_back(info("   PASSWORD", [](char* o) { snprintf(o, 40, "%s", ecfg.apPass[0] ? ecfg.apPass : "NONE"); }));
    rows.push_back(info("2. OPEN", [](char* o) { strcpy(o, "192.168.4.1"); }));
    rows.push_back(action("QR CODES", [] { uiOpen(new Phone()); }, [](char* o) { strcpy(o, ">"); }));
    if (S.net == NET_UP) rows.push_back(info("OR ON THIS WIFI", [](char* o) { sprintf(o, "%s", S.ip); }));
  }
  void tick() override {
    if (nn < 0) { int k = netScanResults(nets, 16); if (k >= 0) { nn = k; uiDirty(); } }
    ns = netSaved(saved, 6);
    L.tick();
    redrawIn(1000);
  }
  void draw() override { build(); pathLine("C:\\EAR\\WIFI>"); drawRows(L, rows); fkeysBack(); }
  bool ev(const Ev& e) override { build(); return rowsEv(L, rows, e); }
};

// ------------------------------------------------------------------ FIRMWARE: update EAR-CUBE, or go back to ERROR-CUBE
struct Firmware : Screen {
  bool sw;
  explicit Firmware(bool s) : sw(s) {}
  const char* name() override { return sw ? "SWITCH" : "UPDATE"; }
  bool keepAwake() override { return S.upd == UPD_LOADING || S.upd == UPD_CHECKING; }
  bool noSwipeBack() override { return S.upd == UPD_LOADING; }
  void enter() override {
    if (sw) enetSwitch();
    else if (S.upd != UPD_LOADING && S.upd != UPD_NEW) netUpdateCheck();
  }
  void tick() override { redrawIn(300); }
  void draw() override {
    pathLine(sw ? "C:\\>ERROR-CUBE" : "C:\\EAR\\UPDATE>");
    char b[96];
    int y = 110;
    if (sw) {
      drawText(FT, 30, y, "EAR-CUBE > ERROR-CUBE", C_FG); y += 40;
      drawText(FT, 30, y, "The cube gets ERROR-CUBE back.", C_DIM, TX_FLAT); y += 32;
      drawText(FT, 30, y, "Your WiFi, settings and", C_DIM, TX_FLAT); y += 32;
      drawText(FT, 30, y, "transcripts stay.", C_DIM, TX_FLAT); y += 44;
    } else {
      snprintf(b, sizeof(b), "THIS CUBE: EAR-CUBE %s", CUBE_VERSION); drawText(FT, 30, y, b, C_FG); y += 40;
      if (S.updVer[0] && (S.upd == UPD_NEW || S.upd == UPD_LATEST || S.upd == UPD_LOADING)) {
        snprintf(b, sizeof(b), "NEWEST:    EAR-CUBE %s", S.updVer); drawText(FT, 30, y, b, C_FG); y += 36;
        char lines[3][64]; int n = wrapText(S.updNotes, 28, lines, 3);
        for (int i = 0; i < n; i++) { drawText(FT, 30, y, lines[i], C_DIM, TX_FLAT); y += 30; }
        y += 10;
      }
    }
    const char* st = "";
    switch (S.upd) {
      case UPD_CHECKING: st = sw ? "LOOKING ON ERROR.BLUE..." : "CHECKING ERROR.BLUE..."; break;
      case UPD_NEW: st = sw ? "" : "A NEW ONE IS READY"; break;
      case UPD_LATEST: st = sw ? "" : "YOU HAVE THE NEWEST"; break;
      case UPD_LOADING: case UPD_DONE: case UPD_FAIL: st = S.updMsg; break;
    }
    if (S.upd == UPD_FAIL) { drawText(FT, 30, y, "DIDN'T WORK:", C_RED); y += 34; }
    if (st[0]) { drawText(FT, 30, y, st, S.upd == UPD_FAIL ? C_RED : C_FG); y += 40; }
    if (S.upd == UPD_LOADING || S.upd == UPD_DONE) {
      meter(30, y, 420, 30, S.updPct, true);
      snprintf(b, sizeof(b), "%d%%  DON'T SWITCH IT OFF", S.updPct); drawText(FT, 30, y + 40, b, C_DIM, TX_FLAT);
      return;
    }
    if (sw) {
      if (S.upd == UPD_FAIL) fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Try again", [] { enetSwitch(); }}});
      else fkeysBack();
    } else if (S.upd == UPD_NEW) fkeys({{"ESC", "Back", [] { uiBack(); }}, {"\xE2\x86\xB5", "Install", [] { netUpdateInstall(); }}});
    else fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Check", [] { netUpdateCheck(); }}});
  }
  bool ev(const Ev& e) override {
    if (S.upd == UPD_LOADING) return true;
    if (e.t == EV_KEY && e.key == K_OK) { if (!sw && S.upd == UPD_NEW) netUpdateInstall(); else if (sw && S.upd == UPD_FAIL) enetSwitch(); return true; }
    return false;
  }
};
static void askSwitch() {
  ask("BACK TO ERROR-CUBE?", "The cube downloads ERROR-CUBE from error.blue and restarts in it. Your WiFi, settings and transcripts stay. To come back: ERROR-CUBE > UPDATE > FW > EAR-CUBE.",
      {"SWITCH", "CANCEL"}, [](int i) {
    if (i) return;
    if (S.net != NET_UP) { ask("NO WIFI", "Join a WiFi network first: SETUP > WIFI.", {"OK"}, nullptr); return; }
    uiOpen(new Firmware(true));
  });
}

// ------------------------------------------------------------------ EAR-CUBE: LIVE | REC | TEXTS | SETUP
static const char* TABS[4] = {"LIVE", "REC", "TEXTS", "SETUP"};
static void hotspotApply() { if (S.portal) netPortal(false); }   // net.cpp turns it on again with the new name / password
struct Home : Screen {
  int tab = 0, selT[4] = {0, 0, 0, 0};
  ListView L;
  std::vector<Row> rows;
  EarFile* files = (EarFile*)malloc(sizeof(EarFile) * 120); int nFiles = 0;
  // LIVE: the lines that came back, wrapped
  struct TL { std::string s; uint8_t kind; };   // 0 time, 1 text, 2 text (more of it)
  std::vector<TL> tl; Scroll sc; bool pinned = true; uint32_t seenSeq = (uint32_t)-1, seenClear = 0;
  EarLine* buf = (EarLine*)malloc(sizeof(EarLine) * 24);
  uint32_t lastStatusDraw = 0;
  const char* name() override { return "EAR-CUBE"; }
  int count() { return tab == 2 ? nFiles : tab == 3 ? (int)rows.size() : 0; }
  void fill() {
    if (tab == 2) nFiles = files ? earFiles(files, 120) : 0;
    if (tab == 3) buildSetup();
    L.n = count(); if (L.sel >= L.n) L.sel = L.n ? L.n - 1 : 0;
    if (tab == 3 && L.n && rows[L.sel].kind >= 3) { L.sel = 0; while (L.sel < L.n - 1 && rows[L.sel].kind >= 3) L.sel++; }
  }
  void enter() override { L.setBox(15, 104, 450, 320); L.rowH = 52; fill(); L.showSel(); uiReveal(); }
  void setTab(int t) {
    if (t < 0 || t > 3 || t == tab) return;
    selT[tab] = L.sel; tab = t;
    L.sel = selT[t]; L.scroll = 0; fill(); L.showSel();
    snd(SND_MOVE); uiDirty();
  }
  // ---- start / stop
  void toggle(int m) {
    if (earMode() == m) { earWant(EAR_IDLE); snd(SND_OFF); toast(m == EAR_REC ? "RECORDING STOPPED" : "STOPPED"); return; }
    if (!micOk()) { ask("NO MICROPHONES", "The cube's microphones (ES7210) didn't answer at start. Restart the cube.", {"OK"}, nullptr); return; }
    if (earMode() == EAR_REC && m == EAR_LIVE) {
      ask("RECORDING", "The cube is recording. Stop the recording and listen live instead?", {"STOP + LIVE", "KEEP RECORDING"}, [this](int i) {
        if (i == 0) { earWant(EAR_LIVE); pinned = true; snd(SND_ON); }
      });
      return;
    }
    if (S.net != NET_UP && m == EAR_LIVE) {
      ask("NO INTERNET", "LIVE needs the internet: error.blue writes the words. Connect the cube here (WIFI), or from a phone (PHONE: join the cube's own WiFi, open the page, WIFI tab). RECORD works without: it writes it all down later.",
          {"WIFI", "PHONE", "RECORD", "CANCEL"}, [this](int i) {
        if (i == 0) uiOpen(new Wifi()); else if (i == 1) uiOpen(new Phone()); else if (i == 2) { setTab(1); toggle(EAR_REC); }
      });
      return;
    }
    if (earMode() == EAR_LIVE && m == EAR_REC) toast("LIVE STOPPED: RECORDING NOW", 2500);   // LIVE -> RECORD: just switch
    earWant(m); snd(SND_ON);
    if (m == EAR_LIVE) { pinned = true; }
  }
  // ---- setup
  void buildSetup() {
    rows.clear();
    rows.push_back(heading("LISTENING", IC_SOUND));
    rows.push_back(echoice("LANGUAGE", &ecfg.lang, N_LANGS, [](int i) { return LANGS[i].name; }));
    rows.push_back(echoice("WRITE IT", &ecfg.task, 2, [](int i) { return i ? "IN ENGLISH" : "AS SPOKEN"; }));
    rows.push_back(echoice("SENSITIVITY", &ecfg.sens, 4, [](int i) { static const char* S4[4] = {"LOW", "NORMAL", "HIGH", "EVERYTHING"}; return S4[i]; }));
    rows.push_back(echoice("MIC GAIN", &ecfg.gain, 15, [](int i) { return gainLabel(i); }, [] { micGain(ecfg.gain); }));
    rows.push_back(echoice("MICROPHONES", &ecfg.mics, 3, [](int i) { static const char* M[3] = {"BOTH", "LEFT", "RIGHT"}; return M[i]; }));
    rows.push_back(echoice("PAUSE LENGTH", &ecfg.pause, 3, [](int i) { static const char* P[3] = {"SHORT", "NORMAL", "LONG"}; return P[i]; }));
    rows.push_back(action("MIC TEST", [] { uiOpen(new MicTest()); }, [](char* o) { strcpy(o, ">"); }));
    rows.push_back(heading("SAVING", IC_FILES));
    rows.push_back(etoggle("TIMES IN THE TEXT", &ecfg.stamps));
    rows.push_back(echoice("REC: KEEP THE SOUND", &ecfg.keepAudio, 2, [](int i) { return i ? (S.sd ? "ON (WAV)" : "ON: NO TF CARD") : "OFF"; }));
    rows.push_back(etoggle("LIVE: SAVE THE TEXT", &ecfg.liveKeep));
    rows.push_back(info("SAVES TO", [](char* o) {
      uint64_t t, u;
      bool sd = S.sd; const char* d = sd ? "/sd" : "/c";
      if (fsInfo(d, t, u)) snprintf(o, 40, "%s %uM FREE", sd ? "A:\\ear" : "C:\\ear", (unsigned)((t - u) >> 20)); else strcpy(o, sd ? "A:\\ear" : "C:\\ear");
    }));
    rows.push_back(heading("HOTSPOT", IC_WIFI));
    rows.push_back(etoggle("CUBE'S OWN WIFI", &ecfg.hotspot, [] { hotspotApply(); }));
    rows.push_back(action("NAME", [] {
      char n[33]; earApName(n, sizeof(n));
      kbOpen("HOTSPOT NAME", n, 23, [](const char* t) {
        std::string s = t; while (s.size() && s.back() == ' ') s.pop_back();
        snprintf(ecfg.apName, sizeof(ecfg.apName), "%s", s.c_str()); earCfgSave(); hotspotApply(); toast("NAME SAVED");
      });
    }, [](char* o) { earApName(o, 40); utf8Cut(o, 14); }));
    rows.push_back(action("PASSWORD", [] {
      kbOpen("PASSWORD (EMPTY = NONE)", ecfg.apPass, 23, [](const char* t) {
        if (t[0] && strlen(t) < 8) { toast("8 OR MORE LETTERS, OR NONE"); return; }
        snprintf(ecfg.apPass, sizeof(ecfg.apPass), "%s", t); earCfgSave(); hotspotApply(); toast(t[0] ? "PASSWORD SAVED" : "NO PASSWORD");
      }, false);
    }, [](char* o) { snprintf(o, 40, "%s", ecfg.apPass[0] ? ecfg.apPass : "NONE (OPEN)"); utf8Cut(o, 14); }));
    rows.push_back(action("PHONE: QR CODES", [] { uiOpen(new Phone()); }, [](char* o) { strcpy(o, ">"); }));
    rows.push_back(info("PAGE", [](char* o) { strcpy(o, S.portal ? "192.168.4.1" : ecfg.hotspot ? "STARTING..." : "-"); }));
    rows.push_back(heading("ERROR-CHAT", IC_CLOUD));
    rows.push_back(etoggle("SEND TO ERROR-CHAT", &ecfg.chatOn));
    rows.push_back(action("CHANNEL", [] {
      kbOpen("ERROR-CHAT CHANNEL", ecfg.chatChan, 8, [](const char* t) {
        char c[9] = ""; int k = 0;
        for (const char* p = t; *p && k < 8; p++) if (isalnum((unsigned char)*p)) c[k++] = toupper((unsigned char)*p);
        if (k < 3) { toast("3-8 LETTERS OR DIGITS"); return; }
        snprintf(ecfg.chatChan, sizeof(ecfg.chatChan), "%s", c); earCfgSave(); toast("CHANNEL CHANGED");
      });
    }, [](char* o) { snprintf(o, 40, "#%s", ecfg.chatChan); }));
    rows.push_back(info("THERE", [](char* o) {
      if (!ecfg.chatOn) strcpy(o, "OFF");
      else if (earChatErr()[0]) { snprintf(o, 40, "%s", earChatErr()); utf8Cut(o, 16); }
      else if (chatWaiting()) snprintf(o, 40, "%d LINES WAITING", chatWaiting());
      else strcpy(o, S.net == NET_UP ? "OK" : "NO INTERNET");
    }));
    rows.push_back(info("OPEN", [](char* o) { strcpy(o, "ERROR-CHAT"); }));
    rows.push_back(heading("INTERNET", IC_CLOUD));
    rows.push_back(action("WIFI", [] { uiOpen(new Wifi()); }, [](char* o) {
      if (S.net == NET_UP) { snprintf(o, 40, "%s", S.ssid); utf8Cut(o, 12); } else strcpy(o, cfg.wifiOn ? "NOT CONNECTED" : "OFF");
    }));
    rows.push_back(info("SPEECH TO TEXT", [](char* o) { strcpy(o, "ERROR.BLUE"); }));
    rows.push_back(info("LAST ANSWER", [](char* o) {
      if (earErr()[0]) { snprintf(o, 40, "%s", earErr()); utf8Cut(o, 14); }
      else if (enetSttMs()) snprintf(o, 40, "OK, %u.%u S", (unsigned)(enetSttMs() / 1000), (unsigned)(enetSttMs() / 100 % 10));
      else strcpy(o, "-");
    }));
    rows.push_back(heading("SOUND + SCREEN", IC_SCREEN));
    rows.push_back(slider("VOLUME", &cfg.vol, 0, [] { tone(880, 40); }));
    rows.push_back(slider("BRIGHTNESS", &cfg.bright, 5, [] { sysBrightness(uiBrightNow()); }));
    rows.push_back(choice("ROTATION", &cfg.rot, 4, [](int i) { static const char* R[4] = {"0", "90", "180", "270"}; return R[i]; }));
    rows.push_back(choice("SCREEN OFF AFTER", &cfg.offIdx, N_OFF, [](int i) { return offLabel(i); }));
    rows.push_back(choice("COLOURS", &cfg.color, N_SCHEMES, [](int i) { return SCHEMES[i].name; }));
    rows.push_back(heading("FIRMWARE", IC_UPDATE));
    rows.push_back(action("UPDATE EAR-CUBE", [] { uiOpen(new Firmware(false)); }, [](char* o) {
      if (S.upd == UPD_NEW && !enetSwitching) snprintf(o, 40, "%s IS OUT", S.updVer); else snprintf(o, 40, "%s", CUBE_VERSION);
    }));
    rows.push_back(action("BACK TO ERROR-CUBE", [] { askSwitch(); }, [](char* o) { strcpy(o, ">"); }));
    rows.push_back(heading("THIS CUBE", IC_INFO));
    rows.push_back(info("FREE MEMORY", [](char* o) { snprintf(o, 40, "%uK + %uK", (unsigned)(freeHeap() / 1024), (unsigned)(freePsram() / 1024)); }));
    rows.push_back(info("BATTERY", [](char* o) { if (S.battery) snprintf(o, 40, "%d%%%s", S.batPct, S.charging ? " CHARGING" : ""); else strcpy(o, S.usb ? "USB" : "-"); }));
    rows.push_back(info("TF CARD", [](char* o) { snprintf(o, 40, "%s", S.sd ? S.sdInfo : "NONE"); utf8Cut(o, 14); }));
  }
  // ---- LIVE: build the wrapped lines from what came back
  void buildLive() {
    int n = buf ? earLines(buf, 24, 0) : 0;
    tl.clear();
    for (int i = 0; i < n; i++) {
      EarLine& l = buf[i];
      static char wl[24][64];
      bool st = ecfg.stamps && l.at[0];
      int w = st ? 23 : 29;
      int m = wrapText(l.text, w, wl, 24);
      for (int k = 0; k < m; k++) {
        if (st) {
          char at[8] = ""; if (k == 0) { snprintf(at, sizeof(at), "%.5s", l.at[0] == '+' ? l.at + 1 : l.at); }
          tl.push_back({std::string(at), 0});
        }
        tl.push_back({wl[k], (uint8_t)(k ? 2 : 1)});
      }
    }
  }
  int liveRows() { int n = 0; for (auto& t : tl) if (t.kind) n++; return n; }
  void drawStatus(int y) {
    int st = earStatus();
    uint16_t c = stColor(st);
    bool blink = (st == ST_HEAR || st == ST_SEND) && (msNow() / 300) % 2;
    fillCircle(28, y + 16, 7, blink ? C_BG : c);
    if (!blink) ring(28, y + 16, 7, 2, c);
    char b[40];
    if (st == ST_OFFLINE && earMode() == EAR_REC) snprintf(b, sizeof(b), "NO INTERNET: SAVING");
    else if (earWaiting() > 1 && (st == ST_SEND || st == ST_OFFLINE || st == ST_LIMIT)) snprintf(b, sizeof(b), "%s %d", earStatusText(st), earWaiting());
    else snprintf(b, sizeof(b), "%s", earStatusText(st));
    drawText(FT, 44, y, b, c, TX_FLAT);
    miniMeter(298, y + 6, "L", micLevel(1));
    miniMeter(390, y + 6, "R", micLevel(2));
  }
  void lockedBar() {                     // the buttons do nothing: say so where the F-keys were
    invBar(15, rowY(13), 450, LH);
    char b[48]; snprintf(b, sizeof(b), "LOCKED FROM ERROR-CHAT #%s", ecfg.chatChan);
    drawTextC(FT, 240, rowY(13), b, C_BG, TX_FLAT);
  }
  void drawLive() {
    drawStatus(102);
    hline(15, 138, 450, C_DIM);
    bool on = earMode() == EAR_LIVE;
    int rowsN = liveRows();
    if (!on && !rowsN) {
      // nothing yet: a big microphone, tap it
      int lv = micLevel(0);
      drawMic(240, 250, 1.6f, C_FG, false);
      waves(240, 230, lv, C_DIM);
      drawTextC(FT, 240, 330, earMode() == EAR_REC ? "RECORDING (REC)" : "TAP TO LISTEN", C_FG);
      char l[48]; snprintf(l, sizeof(l), "%s%s%s%s", LANGS[ecfg.lang % N_LANGS].name, ecfg.task ? " > EN" : "", ecfg.chatOn ? " \xC2\xB7 CHAT #" : "", ecfg.chatOn ? ecfg.chatChan : "");
      if (S.net != NET_UP && earMode() == EAR_IDLE) drawTextC(FT, 240, 366, "NO INTERNET YET: SETUP > WIFI", C_WARN, TX_FLAT);
      else drawTextC(FT, 240, 366, l, C_DIM, TX_FLAT);
      hit(140, 150, 200, 210, [this] { toggle(EAR_LIVE); });
    } else {
      sc.x = 15; sc.y = 144; sc.w = 450; sc.h = 8 * LH + 24; sc.view = sc.h; sc.total = rowsN * LH;
      if (pinned && !sc.drag) sc.pos = sc.maxPos();
      sc.clamp(); sc.tick();
      if (!sc.drag && !sc.gliding()) pinned = sc.pos >= sc.maxPos() - 2;
      clipSet(15, sc.y, 436, sc.h);
      int r = 0; std::string pendingAt;
      for (auto& t : tl) {
        if (t.kind == 0) { pendingAt = t.s; continue; }
        int y = sc.y + r * LH - (int)sc.pos;
        if (y > sc.y - LH && y < sc.y + sc.h) {
          int x = 15;
          if (ecfg.stamps) { if (pendingAt.size()) drawText(FT, 15, y, pendingAt.c_str(), C_DIM, TX_FLAT); x = 15 + 6 * CW; }
          drawText(FT, x, y, t.s.c_str(), C_FG);
        }
        pendingAt.clear();
        r++;
      }
      // still hearing / writing: a blinking block after the last line
      if (on && (micSpeech() || earSending())) {
        int y = sc.y + r * LH - (int)sc.pos;
        if (y < sc.y + sc.h && (msNow() / 400) % 2) fillRect(ecfg.stamps ? 15 + 6 * CW : 15, y + 6, CW, LH - 10, micSpeech() ? C_FG : C_WARN);
        redrawIn(200);
      }
      clipReset();
      sc.drawBar();
      if (!rowsN) drawTextC(FT, 240, 230, "SAY SOMETHING...", C_DIM, TX_FLAT);
    }
    if (uiLocked) lockedBar();
    else if (on) fkeys({{"\xE2\x86\xB5", "Stop", [this] { toggle(EAR_LIVE); }}, {"P", earPaused() ? "Go on" : "Pause", [] { earPauseWant(!earPaused()); }},
                   {"S", "Save", [this] { saveLive(); }}, {"C", "Clear", [] { confirmClear(); }}});
    else if (rowsN) fkeys({{"\xE2\x86\xB5", "Listen", [this] { toggle(EAR_LIVE); }}, {"S", "Save", [this] { saveLive(); }}, {"C", "Clear", [] { confirmClear(); }}});
    else fkeys({{"\xE2\x86\xB5", "Listen", [this] { toggle(EAR_LIVE); }}, {"\xE2\x86\x90\xE2\x86\x92", "Tab", [this] { setTab(1); }}});
  }
  void saveLive() {
    char p[96];
    if (earSaveLive(p, sizeof(p))) { char d[96], t[120]; dosPath(p, d, sizeof(d)); snprintf(t, sizeof(t), "SAVED: %s", strrchr(d, '\\') + 1); toast(t, 3000); snd(SND_OK); }
    else toast("NOTHING TO SAVE YET");
  }
  void drawRec() {
    drawStatus(102);
    bool rec = earMode() == EAR_REC;
    int cx = 240, cy = 214, R = 64;
    uint32_t t = msNow();
    if (rec && earPaused()) {                                  // paused: the ring, two bars
      ring(cx, cy, R, 4, C_WARN);
      fillRect(cx - 22, cy - 24, 14, 48, C_WARN); fillRect(cx + 8, cy - 24, 14, 48, C_WARN);
    } else if (rec) {
      int pulse = (int)(6 * sinf(t / 300.0f));
      fillCircle(cx, cy, R + 4 + pulse, mix(C_BG, C_RED, 25));
      fillCircle(cx, cy, R, C_RED);
      fillRect(cx - 20, cy - 20, 40, 40, C_WHITE);            // stop square
      redrawIn(60);
    } else {
      ring(cx, cy, R, 4, C_RED);
      fillCircle(cx, cy, R - 14, mix(C_BG, C_RED, 70));
      drawTextC(FB, cx, cy - 21, "REC", C_WHITE, TX_FLAT);
    }
    hit(cx - R - 10, cy - R - 10, 2 * R + 20, 2 * R + 20, [this] { toggle(EAR_REC); });
    char b[80], d[96];
    if (rec) {
      fmtSecs(b, sizeof(b), earSecs());
      drawTextC(FB, 240, 290, b, earPaused() && (msNow() / 600) % 2 ? C_DIM : C_FG);
      if (earPaused()) redrawIn(300);
      dosPath(earFile(), d, sizeof(d));
      const char* fn = strrchr(d, '\\'); snprintf(b, sizeof(b), "%s", fn ? fn + 1 : d); utf8Cut(b, 29);
      drawTextC(FT, 240, 338, b, C_DIM, TX_FLAT);
      snprintf(b, sizeof(b), "%u WORDS \xC2\xB7 %d WAITING", (unsigned)earWords(), earWaiting());
      drawTextC(FT, 240, 370, b, earWaiting() > 2 ? C_WARN : C_FG, TX_FLAT);
      EarLine* l = buf;
      if (l && earLines(l, 1, 0) == 1) { char wl[1][64]; wrapText(l[0].text, 29, wl, 1); drawTextC(FT, 240, 398, wl[0], C_DIM, TX_FLAT); }
    } else {
      drawTextC(FT, 240, 294, "TAP TO RECORD", C_FG);
      snprintf(b, sizeof(b), "INTO %s", S.sd ? "A:\\ear (TF CARD)" : "C:\\ear (NO TF CARD)");
      drawTextC(FT, 240, 330, b, C_DIM, TX_FLAT);
      snprintf(b, sizeof(b), "LANGUAGE: %s%s%s", LANGS[ecfg.lang % N_LANGS].name, ecfg.task ? " > EN" : "", ecfg.keepAudio && S.sd ? " + WAV" : "");
      drawTextC(FT, 240, 362, b, C_DIM, TX_FLAT);
      if (earWaiting()) { snprintf(b, sizeof(b), "%d PIECES STILL WAITING", earWaiting()); drawTextC(FT, 240, 394, b, C_WARN, TX_FLAT); }
      else if (earLastFile()[0]) { dosPath(earLastFile(), d, sizeof(d)); snprintf(b, sizeof(b), "LAST: %s", strrchr(d, '\\') ? strrchr(d, '\\') + 1 : d); utf8Cut(b, 29); drawTextC(FT, 240, 394, b, C_DIM, TX_FLAT); }
    }
    if (earStartError()[0] && !rec) drawTextC(FT, 240, 394, earStartError(), C_RED);
    const char* last = rec ? earFile() : earLastFile();
    if (uiLocked) lockedBar();
    else if (rec) fkeys({{"\xE2\x86\xB5", "Stop", [this] { toggle(EAR_REC); }}, {"P", earPaused() ? "Go on" : "Pause", [] { earPauseWant(!earPaused()); }},
                        {"V", "View", [last] { uiOpen(new Reader(last)); }}, {"\xE2\x86\x90\xE2\x86\x92", "Tab", [this] { setTab(2); }}});
    else if (last[0]) fkeys({{"\xE2\x86\xB5", rec ? "Stop" : "Record", [this] { toggle(EAR_REC); }}, {"V", "View", [last] { uiOpen(new Reader(last)); }},
                        {"\xE2\x86\x90\xE2\x86\x92", "Tab", [this] { setTab(2); }}});
    else fkeys({{"\xE2\x86\xB5", rec ? "Stop" : "Record", [this] { toggle(EAR_REC); }}, {"\xE2\x86\x90\xE2\x86\x92", "Tab", [this] { setTab(2); }}});
  }
  void drawTexts() {
    L.draw([&](int i, int x, int y, int w, int h, bool sel) {
      EarFile& f = files[i];
      int ty = y + (h - LH) / 2;
      bool cur = !strcmp(f.path, earFile());
      if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
      if (cur) fillCircle(x + 16, y + h / 2, 7, C_RED);
      else if (f.live) ring(x + 16, y + h / 2, 6, 2, sel ? C_BG : C_DIM);
      else fillRect(x + 11, y + h / 2 - 7, 10, 14, sel ? C_BG : C_FG);
      char nm[48]; snprintf(nm, sizeof(nm), "%s", f.name); utf8Cut(nm, 19);
      drawText(FT, x + 32, ty, nm, sel ? C_BG : C_FG, sel ? TX_FLAT : TX_GLOW);
      char sz[20];
      if (cur) strcpy(sz, "REC"); else snprintf(sz, sizeof(sz), "%s%uK", f.hasWav ? "\xE2\x99\xAA" : "", (unsigned)((f.size + 1023) / 1024));
      drawTextR(FT, x + w - 10, ty, sz, sel ? C_BG : cur ? C_RED : C_DIM, TX_FLAT);
    }, [this](int i) { open(i); });
    if (!nFiles) {
      drawTextC(FT, 240, 200, "NO TRANSCRIPTS YET", C_DIM, TX_FLAT);
      drawTextC(FT, 240, 240, "RECORD ONE: REC", C_DIM, TX_FLAT);
    }
    if (uiLocked) lockedBar();
    else fkeys({{"\xE2\x86\xB5", "Read", [this] { open(L.sel); }}, {"R", "Reload", [this] { fill(); uiDirty(); }}, {"\xE2\x86\x90\xE2\x86\x92", "Tab", [this] { setTab(3); }}});
  }
  void open(int i) { if (i >= 0 && i < nFiles) uiOpen(new Reader(files[i].path)); }
  void more(int i) {
    if (i < 0 || i >= nFiles) return;
    EarFile f = files[i];
    if (!strcmp(f.path, earFile())) { toast("BEING RECORDED NOW"); return; }
    ask(f.name, f.hasWav ? "A transcript, with its sound (WAV)" : "A transcript", {"READ", "DELETE", "CANCEL"}, [this, f](int k) {
      if (k == 0) uiOpen(new Reader(f.path));
      else if (k == 1) ask("DELETE", (std::string(f.name) + "?").c_str(), {"DELETE", "KEEP"}, [this, f](int j) {
        if (j) return;
        std::string w = f.path; w.replace(w.size() - 4, 4, ".wav");
        if (fsRemove(f.path)) { fsRemove(w.c_str()); toast("DELETED"); } else toast("CAN'T DELETE IT");
        fill(); uiDirty();
      });
    });
  }
  void draw() override {
    for (int t = 0; t < 4; t++) {
      int x = 15 + t * 113, w = 109;
      bool lit = (t == 0 && earMode() == EAR_LIVE) || (t == 1 && earMode() == EAR_REC);
      if (t == tab) invBar(x, 52, w, 38);
      else rect(x, 52, w, 38, C_DIM);
      drawTextC(FT, x + w / 2, 55, TABS[t], t == tab ? C_BG : lit ? C_RED : C_DIM, TX_FLAT);
      if (lit) fillCircle(x + 12, 71, 5, (msNow() / 500) % 2 ? C_RED : (t == tab ? C_BG : C_DIM));
      hit(x, 46, w, 50, [this, t] { setTab(t); }, nullptr, false);
    }
    if (earMode() != EAR_IDLE) redrawIn(500);
    if (tab == 0) { uint32_t s = earLineSeq(); if (s != seenSeq) { seenSeq = s; buildLive(); } drawLive(); }
    else if (tab == 1) drawRec();
    else if (tab == 2) { L.n = count(); drawTexts(); }
    else {
      L.n = count();
      buildSetup(); drawRows(L, rows);
      if (uiLocked) lockedBar(); else fkeys({{"\xE2\x86\x90\xE2\x86\x92", "Tab", [this] { setTab(0); }}});
    }
  }
  void tick() override {
    L.tick(); sc.tick();
    if (tab == 0 && earLineSeq() != seenSeq) uiDirty();
    // the meters and the status move: draw a few times a second
    if (tab <= 1 && msNow() - lastStatusDraw > 120) { lastStatusDraw = msNow(); uiDirty(); }
    static int lastMode = -1;
    if (earMode() != lastMode) { lastMode = earMode(); if (tab == 2) fill(); uiDirty(); }
    static bool told = false;
    if (!told && S.upd == UPD_NEW && !enetSwitching && uiCurrent() == this) { told = true; toast((std::string("EAR-CUBE ") + S.updVer + " IS OUT: SETUP").c_str(), 4000); }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_SWIPE && e.dir == SW_LEFT) { setTab(tab < 3 ? tab + 1 : 3); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_RIGHT) { setTab(tab > 0 ? tab - 1 : 0); return true; }
    if (e.t == EV_KEY && e.key == K_BACK) { setTab((tab + 1) % 4); return true; }
    if (tab == 0) {
      if (e.t == EV_KEY && e.key == K_OK) { toggle(EAR_LIVE); return true; }
      if (sc.ev(e)) { pinned = false; return true; }
      if (e.t == EV_KEY && e.key == K_PREV) { sc.vel = 0; sc.pos -= 3 * LH; sc.clamp(); pinned = false; uiDirty(); return true; }
      if (e.t == EV_KEY && e.key == K_NEXT) { sc.vel = 0; sc.pos += 3 * LH; sc.clamp(); pinned = sc.pos >= sc.maxPos() - 2; uiDirty(); return true; }
      return false;
    }
    if (tab == 1) { if (e.t == EV_KEY && e.key == K_OK) { toggle(EAR_REC); return true; } return false; }
    if (tab == 3) { buildSetup(); return rowsEv(L, rows, e); }
    if (e.t == EV_LONG && e.y0 >= L.y && e.y0 < L.y + L.h) {
      int i = (int)((e.y0 - L.y + L.scroll) / L.rowH);
      if (i >= 0 && i < nFiles) { L.sel = i; more(i); return true; }
    }
    return L.ev(e, [this](int i) { open(i); });
  }
};
static Home* home = nullptr;
Screen* homeScreen() { return home = new Home(); }

// ------------------------------------------------------------------ start-up: an ear listening, then LIVE
struct Boot : Screen {
  uint32_t t0 = 0;
  const char* name() override { return "BOOT"; }
  bool fullScreen() override { return true; }
  bool keepAwake() override { return true; }
  void enter() override { t0 = msNow(); snd(SND_ON); }
  void tick() override {
    uint32_t t = msNow() - t0;
    if (t > (cfg.boot == 2 ? 300u : 2000u)) { uiStart(); return; }
    redrawIn(30);
  }
  void draw() override {
    uint32_t t = msNow() - t0;
    gfxClearTo(C_BG);
    // a sound wave runs across, then settles into bars
    int n = 24;
    for (int i = 0; i < n; i++) {
      float ph = t / 140.0f + i * 0.55f;
      float amp = t < 900 ? t / 900.0f : 1.0f;
      int h = 8 + (int)(fabsf(sinf(ph) * sinf(ph * 0.37f + i)) * 110 * amp);
      int x = 48 + i * 16;
      fillRect(x, 150 - h / 2, 9, h, i % 6 == 0 ? C_FG : mix(C_BG, C_FG, 60));
    }
    drawMic(240, 300, 1.0f, C_FG, false);
    drawTextC(FB, 240, 330, "EAR-CUBE", C_FG);
    drawTextC(FT, 240, 382, "VERSION " CUBE_VERSION, C_DIM, TX_FLAT);
    drawTextC(FT, 240, 418, "SPEECH TO TEXT \xC2\xB7 ERROR.BLUE", C_DIM, TX_FLAT);
  }
  bool ev(const Ev& e) override { if (e.t == EV_TAP || e.t == EV_KEY) t0 = msNow() - 5000; return true; }
};
Screen* bootScreen() { return new Boot(); }
Screen* introScreen() { return new Boot(); }

// ------------------------------------------------------------------ the dim clock while the screen is "off"
struct Aod : Screen {
  const char* name() override { return "AOD"; }
  bool fullScreen() override { return true; }
  void draw() override {
    struct tm t;
    char c[12] = "--:--";
    if (sysLocalTime(t)) fmtTime(c, t.tm_hour, t.tm_min, 0, false);
    uint32_t m = msNow() / 60000;
    int ox = (int)(m * 37 % 61) - 30, oy = (int)(m * 53 % 81) - 40;
    drawTextC(FH, 240 + ox, 160 + oy, c, mix(C_BLACK, C_FG, 50), TX_FLAT);
    if (earMode() != EAR_IDLE && earPaused()) drawTextC(FT, 240 + ox, 280 + oy, "PAUSED", mix(C_BLACK, C_WARN, 50), TX_FLAT);
    else if (earMode() == EAR_REC) {
      char b[24]; fmtSecs(b, sizeof(b), earSecs());
      fillCircle(196 + ox, 296 + oy, 7, mix(C_BLACK, C_RED, 60));
      drawText(FT, 212 + ox, 280 + oy, b, mix(C_BLACK, C_RED, 60), TX_FLAT);
      redrawIn(1000);
    } else if (earMode() == EAR_LIVE) drawTextC(FT, 240 + ox, 280 + oy, "LISTENING", mix(C_BLACK, C_FG, 40), TX_FLAT);
    else if (cfg.aodBatt && S.battery) { char b[16]; snprintf(b, sizeof(b), "%d%%%s", S.batPct, S.charging ? "+" : ""); drawTextC(FT, 240 + ox, 280 + oy, b, mix(C_BLACK, C_FG, 30), TX_FLAT); }
  }
};
Screen* aodScreen() { return new Aod(); }

// ------------------------------------------------------------------ what ui.cpp and the board ask of the programs
void appsRegister() {}
void appsLoop() {
  // the page or ERROR-CHAT: screen on / off
  static uint32_t seenWake = 0, seenSleep = 0;
  if (S.wakeAt != seenWake) { seenWake = S.wakeAt; uiWake(); }
  if (S.sleepAt != seenSleep) { seenSleep = S.sleepAt; uiSleep(); }
  static char msg[40]; snprintf(msg, sizeof(msg), "LOCKED FROM ERROR-CHAT #%s", ecfg.chatChan); uiLockMsg = msg;
}
void clockTick() {}
uint32_t tmrEnd = 0, tmrLeft = 0; bool tmrRun = false;
Screen* findProg(const char* dos) {
  if (!strcmp(dos, "WIFI")) return new Wifi();
  if (!strcmp(dos, "UPDATE")) return new Firmware(false);
  if (!strcmp(dos, "PHONE")) return new Phone();
  if (!strcmp(dos, "MICTEST")) return new MicTest();
  return nullptr;
}
void openProg(const char* dos) { Screen* s = findProg(dos); if (s) uiOpen(s); }
bool openFile(const char* path) {
  const char* e = strrchr(path, '.');
  if (e && !strcasecmp(e, ".txt")) { uiOpen(new Reader(path)); return true; }
  if (netInstallFile(path)) { uiOpen(new Firmware(false)); return true; }
  return false;
}
void musicNext(int) {}
void musicToggle() {}
void musicTick() {}
void musicPlayFolder(const char*, const char*) {}
Screen* makeNowPlaying() { return nullptr; }
const char* bootLine(int) { return ""; }
