// pico.cpp: PICO-CUBE's screens. The cube starts straight into a PICO-8 player:
//   CARTS   the carts on the cube (C:\pico8) and the TF card (A:\PICO8): tap one to play
//   ONLINE  the carts on error.blue (error.blue/pico8): tap to get one (and play it)
//   SETUP   controls (tilt, buttons, calibrate, test), sound + screen, WiFi, updates, back to ERROR-CUBE
// Playing: tilt = the arrows, - = O, + = X, PWR = the cube's menu. The emulator is FAKE-08 (src/p8).
// Uses ERROR-CUBE's interface (src/ui), board and drawing as they are, in PICO-8's colours.
#include "../ui/ui.h"
#include "../p8/p8.h"
#include "../p8/carts.h"
#include "pico.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#ifdef PICO_SIM
#include <chrono>
static double simUs = 0, simMax = 0; static int simN = 0;   // how long the emulator takes per step (PC)
#endif

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

// PICO-8's 16 colours
static const uint32_t PAL[16] = {0x000000, 0x1d2b53, 0x7e2553, 0x008751, 0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
                                 0xff004d, 0xffa300, 0xffec27, 0x00e436, 0x29adff, 0x83769c, 0xff77a8, 0xffccaa};
static uint16_t pc(int i) { return rgb(PAL[i & 15]); }
// a stripe of the 16 colours (the PICO-CUBE mark)
static void stripe(int x, int y, int w, int h) {
  for (int i = 0; i < 16; i++) { int x0 = x + w * i / 16, x1 = x + w * (i + 1) / 16; fillRect(x0, y, x1 - x0, h, pc(i == 0 ? 5 : i)); }
}

void cfgChanged() {
  gfxTheme(THEME_COLOR, lowFx() ? 0 : cfg.glow);
  sysBrightness(uiBrightNow());
  sysApplyRotation();
  sysApplyButtons();
  cfgSave();
  uiDirty();
}

// ------------------------------------------------------------------ carts on the cube
static const char* CART_DIR = "/c/pico8";
static const char* LAST_FILE = "/c/pico8/last.txt";
static bool isCart(const char* n) {
  size_t l = strlen(n);
  return (l > 3 && !strcasecmp(n + l - 3, ".p8")) || (l > 7 && !strcasecmp(n + l - 7, ".p8.png"));
}
// the carts that come with the cube: into C:\pico8 the first time (and again when a new firmware brings a newer one)
static void installCarts() {
  static bool done = false;
  if (done) return;
  done = true;
  fsMkdir(CART_DIR);
  for (int i = 0; i < N_P8CARTS; i++) {
    char p[64]; snprintf(p, sizeof(p), "%s/%s", CART_DIR, P8CARTS[i].name);
    if (fsSize(p) != (long)P8CARTS[i].size) fsWrite(p, (const uint8_t*)P8CARTS[i].data, P8CARTS[i].size);
  }
}
static std::string lastCart() {
  char b[200]; size_t n = 0;
  if (!fsRead(LAST_FILE, (uint8_t*)b, sizeof(b) - 1, n)) return "";
  b[n] = 0;
  return fsSize(b) > 0 ? std::string(b) : std::string();
}
// "pico_crate.p8.png" -> "PICO CRATE"
static std::string niceName(const std::string& file) {
  std::string n = file;
  size_t k = n.rfind(".p8"); if (k != std::string::npos) n = n.substr(0, k);
  for (auto& ch : n) { ch = toupper((unsigned char)ch); if (ch == '_' || ch == '-') ch = ' '; }
  return n;
}
static std::string localPath(const char* file) {
  std::string c = std::string(CART_DIR) + "/" + file;
  if (fsSize(c.c_str()) > 0) return c;
  if (S.sd) { std::string s = std::string("/sd/PICO8/") + file; if (fsSize(s.c_str()) > 0) return s; }
  return "";
}
// the version of a cart got from error.blue (C:\pico8\<file>.v)
static std::string localVer(const char* file) {
  char p[96], b[16]; size_t n = 0; snprintf(p, sizeof(p), "%s/%s.v", CART_DIR, file);
  if (!fsRead(p, (uint8_t*)b, sizeof(b) - 1, n)) return "";
  b[n] = 0; return b;
}

// ------------------------------------------------------------------ tilt -> arrows
static float grav[3] = {0, 0, 9.8f};
static bool gravOk = false;
static float neutral[3] = {0, 0, 1};
static void readGrav() {
  float a[3], g[3];
  if (!imuRead(a, g)) return;
  for (int i = 0; i < 3; i++) grav[i] = gravOk ? grav[i] + (a[i] - grav[i]) * 0.35f : a[i];
  gravOk = true;
}
static void unit(const float* v, float* o) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l < 0.01f) l = 1; for (int i = 0; i < 3; i++) o[i] = v[i] / l; }
static void useCal() {
  if (cfg.p8CalOk) { float c[3] = {cfg.p8Cal[0] / 1000.0f, cfg.p8Cal[1] / 1000.0f, cfg.p8Cal[2] / 1000.0f}; unit(c, neutral); }
  else { gravOk = false; readGrav(); unit(grav, neutral); }   // not calibrated: the way it's held when the cart starts
}
static void p8Calibrate() {
  gravOk = false; readGrav();
  float u[3]; unit(grav, u);
  for (int i = 0; i < 3; i++) cfg.p8Cal[i] = (int16_t)lroundf(u[i] * 1000);
  cfg.p8CalOk = 1; cfgSave();
  for (int i = 0; i < 3; i++) neutral[i] = u[i];
  toast("CALIBRATED: THIS IS STRAIGHT", 1500);
}
static void askCalibrate() {
  ask("CALIBRATE", "Hold the cube the way you want to play, then OK.", {"OK", "CANCEL"}, [](int i) { if (!i) p8Calibrate(); });
}
// degrees left/right and up/down from the calibrated way of holding it
static void tiltDeg(float& dx, float& dy) {
  float u[3]; unit(grav, u);
  float lr = atan2f(u[0], sqrtf(u[1] * u[1] + u[2] * u[2])) - atan2f(neutral[0], sqrtf(neutral[1] * neutral[1] + neutral[2] * neutral[2]));
  float ud = atan2f(u[1], u[2]) - atan2f(neutral[1], neutral[2]);
  while (ud > 3.14159f) ud -= 6.28318f; while (ud < -3.14159f) ud += 6.28318f;
  dx = -lr * 57.2958f;            // the right side down = right
  dy = ud * 57.2958f;             // the top away from you = up
  if (cfg.p8InvX) dx = -dx;
  if (cfg.p8InvY) dy = -dy;
}
static uint8_t arrows = 0;
static uint8_t tiltArrows() {
  readGrav();
  float dx, dy; tiltDeg(dx, dy);
  float on = cfg.p8Dead, off = cfg.p8Dead * 0.6f;   // a little hysteresis: no flicker at the edge
  uint8_t a = 0;
  if (dx > ((arrows & P8_RIGHT) ? off : on)) a |= P8_RIGHT;
  if (dx < -((arrows & P8_LEFT) ? off : on)) a |= P8_LEFT;
  if (dy > ((arrows & P8_UP) ? off : on)) a |= P8_UP;
  if (dy < -((arrows & P8_DOWN) ? off : on)) a |= P8_DOWN;
  arrows = a;
  return a;
}

// ------------------------------------------------------------------ playing
static uint8_t keys = 0;                 // - / + held (as O / X)
static bool menuOpen = false;
static void p8Menu();
static bool hook(int id, bool down) {
  if (menuOpen) return false;            // the cube's menu is up: the buttons work it
  if (id == 2) { if (down) p8Menu(); return true; }
  uint8_t bit = (id == 0) != (cfg.p8Swap != 0) ? P8_O : P8_X;
  if (down) keys |= bit; else keys &= ~bit;
  return true;
}
static bool restartWanted = false, quitWanted = false;
static std::string playingName;
static void p8Menu() {
  if (menuOpen) return;
  menuOpen = true; p8Pause(true); uiWake();
  static char info[80];
  int sl = p8StackLeft();
  if (sl >= 0) snprintf(info, sizeof(info), "%s\n%d FPS \xC2\xB7 STACK %d KB FREE", playingName.c_str(), p8Fps(), sl / 1024);
  else snprintf(info, sizeof(info), "%s\n%d FPS", playingName.c_str(), p8Fps());
  ask("PAUSED", info, {"CONTINUE", "RESTART", "CALIBRATE", "QUIT"}, [](int i) {
    menuOpen = false; keys = 0;
    if (i == 1) restartWanted = true;
    else if (i == 2) p8Calibrate();
    else if (i == 3) quitWanted = true;
    p8Pause(false);
  });
}

struct P8Play : Screen {
  std::string path; uint32_t seen = 0, startAt = 0; bool started = false; int xm[480];
  P8Play(const std::string& p) : path(p) { for (int i = 0; i < 480; i++) xm[i] = i * 128 / 480; }
  const char* name() override { return "PICO-8"; }
  bool fullScreen() override { return true; }
  bool keepAwake() override { return true; }
  bool noSwipeBack() override { return true; }
  bool plain() override { return true; }
  void start() {
    keys = 0; arrows = 0; restartWanted = quitWanted = false;
    cfgFlushNow();                        // settings safe on flash first
    useCal();
    if (!p8Start(path.c_str())) { fail(); return; }
    startAt = msNow();
  }
  void fail() {
    std::string e = p8Error();
#ifdef PICO_SIM
    printf("P8FAIL %s: %s\n", path.c_str(), e.c_str());
#endif
    btnHook = nullptr;
    uiBack();
    if (e.length()) ask("CAN'T PLAY IT", e.c_str(), {"OK"}, nullptr);
  }
  void enter() override {
    btnHook = hook;
    if (!started) {
      started = true;
      fsWrite(LAST_FILE, (const uint8_t*)path.c_str(), path.length());
      playingName = niceName(path.substr(path.rfind('/') + 1));
      start();
    }
  }
  void leave() override { btnHook = nullptr; }
  ~P8Play() {
    btnHook = nullptr; menuOpen = false; p8Stop();
#ifdef PICO_SIM
    simUs = simMax = 0; simN = 0;
#endif
  }
  void tick() override {
    if (quitWanted) { quitWanted = false; uiBack(); return; }
    if (restartWanted) { restartWanted = false; p8Stop(); start(); return; }
    if (!p8Running()) {
      if (p8Error()[0]) { fail(); return; }
      uiBack(); return;                   // the cart's own menu: exit
    }
    if (!menuOpen) p8Buttons(tiltArrows() | keys);
#ifdef PICO_SIM
    auto t0 = std::chrono::steady_clock::now();
    p8Pump();
    double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    if (msNow() - startAt > 1500) { simUs += us; simN++; if (us > simMax) simMax = us; }
    if (simN == 400) printf("P8TIME %s avg %.0f us max %.0f us fps %d\n", path.c_str(), simUs / simN, simMax, p8Fps());
#else
    p8Pump();                             // the simulator (the board runs it in its own task)
#endif
    uint32_t s; p8Frame(&s);
    if (s != seen) { seen = s; uiDirty(); }
    if (msNow() - startAt < 4000) redrawIn(200);
  }
  void draw() override {
    uint32_t s; const uint16_t* f = p8Frame(&s);
    if (f) {
      // 128 -> 480: every PICO-8 pixel is 3 or 4 screen pixels
      for (int y = 0; y < 480; y++) {
        const uint16_t* src = f + (y * 128 / 480) * 128;
        uint16_t* dst = fb + y * SCR_W;
        for (int x = 0; x < 480; x++) dst[x] = src[xm[x]];
      }
    } else gfxClearTo(0);
    if (msNow() - startAt < 4000) {
      blendRect(0, 400, 480, 80, 0, 22);
      drawTextC(FT, 240, 408, "TILT = ARROWS", pc(7), TX_FLAT);
      drawTextC(FT, 240, 440, cfg.p8Swap ? "- = X   + = O   PWR = MENU" : "- = O   + = X   PWR = MENU", pc(14), TX_FLAT);
    }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_LONG) { p8Menu(); return true; }     // a long touch: the menu too
    return true;
  }
};
static void play(const std::string& path) { installCarts(); snd(SND_FLOPPY); uiOpen(new P8Play(path)); }

// ------------------------------------------------------------------ settings rows (as in ERROR-CUBE's SETUP)
static Row heading(const char* n, uint8_t ic = 0) { Row r; r.name = n; r.kind = 4; r.icon = ic; return r; }
static Row info(const char* n, std::function<void(char*)> v) { Row r; r.name = n; r.val = v; r.kind = 3; return r; }
static Row action(const char* n, std::function<void()> f, std::function<void(char*)> v = nullptr) {
  Row r; r.name = n; r.val = v; r.kind = 0; r.act = [f](int) { f(); }; return r;
}
static Row choice(const char* n, uint8_t* v, int count, std::function<const char*(int)> label, std::function<void()> after = nullptr) {
  Row r; r.name = n; r.kind = 1;
  r.val = [v, label](char* o) { snprintf(o, 40, "%s", label(*v)); };
  r.act = [v, count, after](int d) { *v = (*v + (d < 0 ? count - 1 : 1)) % count; if (after) after(); cfgChanged(); };
  return r;
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
static Row number(const char* n, uint8_t* v, int lo, int hi, int step, const char* unit) {
  Row r; r.name = n; r.kind = 1;
  r.val = [v, unit](char* o) { snprintf(o, 40, "%d%s", *v, unit); };
  r.act = [v, lo, hi, step](int d) {
    int x = *v + (d < 0 ? -step : step);
    if (x > hi) x = lo; if (x < lo) x = hi;
    *v = x; cfgChanged();
  };
  return r;
}
static const char* offLabel(int i) {
  static char b[12]; int s = OFF_SECS[i];
  if (!s) return "NEVER";
  if (s < 60) snprintf(b, sizeof(b), "%d SEC", s); else snprintf(b, sizeof(b), "%d MIN", s / 60);
  return b;
}

// ------------------------------------------------------------------ TILT TEST: see what the tilt does
struct TiltTest : Screen {
  uint8_t held = 0;
  const char* name() override { return "TILT TEST"; }
  bool keepAwake() override { return true; }
  void enter() override { useCal(); btnHook = [](int id, bool down) -> bool {
      TiltTest* t = (TiltTest*)uiCurrent();
      if (id == 2) { if (down) uiBack(); return true; }
      uint8_t bit = (id == 0) != (cfg.p8Swap != 0) ? P8_O : P8_X;
      if (down) t->held |= bit; else t->held &= ~bit;
      uiDirty(); return true; }; }
  void leave() override { btnHook = nullptr; }
  ~TiltTest() { btnHook = nullptr; }
  void tick() override { uint8_t a = arrows; tiltArrows(); if (a != arrows) snd(SND_MOVE); uiDirty(); redrawIn(40); }
  void draw() override {
    pathLine("C:\\PICO8\\TILT>");
    float dx, dy; tiltDeg(dx, dy);
    int cx = 170, cy = 250, R = 120;
    // the dead zone (no arrow inside it) and where the cube is now
    ring(cx, cy, R, 2, C_DIM);
    int dz = (int)(cfg.p8Dead * R / 40.0f); if (dz > R) dz = R;
    rect(cx - dz, cy - dz, dz * 2, dz * 2, pc(13));
    int px = cx + (int)(dx * R / 40), py = cy - (int)(dy * R / 40);
    if (px < cx - R) px = cx - R; if (px > cx + R) px = cx + R; if (py < cy - R) py = cy - R; if (py > cy + R) py = cy + R;
    fillCircle(px, py, 12, pc(8));
    // the arrows
    struct A { uint8_t bit; int x, y; const char* s; } AR[4] = {{P8_UP, 390, 150, "\xE2\x86\x91"}, {P8_LEFT, 340, 200, "\xE2\x86\x90"}, {P8_RIGHT, 440, 200, "\xE2\x86\x92"}, {P8_DOWN, 390, 250, "\xE2\x86\x93"}};
    for (auto& a : AR) {
      bool on = arrows & a.bit;
      if (on) fillRect(a.x - 22, a.y - 4, 44, 42, pc(11)); else rect(a.x - 22, a.y - 4, 44, 42, C_DIM);
      drawTextC(FT, a.x, a.y, a.s, on ? pc(0) : C_DIM, TX_FLAT);
    }
    const char* ON[2] = {"O", "X"}; const uint8_t BIT[2] = {P8_O, P8_X};
    for (int i = 0; i < 2; i++) {
      int x = 352 + i * 76, y = 320; bool on = held & BIT[i];
      fillCircle(x, y + 16, 24, on ? pc(i ? 12 : 8) : pc(1)); ring(x, y + 16, 24, 2, C_DIM);
      drawTextC(FT, x, y, ON[i], on ? pc(7) : C_DIM, TX_FLAT);
    }
    char b[40]; snprintf(b, sizeof(b), "%+3d\xC2\xB0 %+3d\xC2\xB0", (int)dx, (int)dy);
    drawTextC(FT, cx, cy + R + 14, b, C_DIM, TX_FLAT);
    drawTextC(FT, 382, 372, cfg.p8Swap ? "- = X  + = O" : "- = O  + = X", C_DIM, TX_FLAT);
    fkeys({{"PWR", "Back", [] { uiBack(); }}, {"C", "Calibrate", [] { askCalibrate(); }}});
  }
};

// ------------------------------------------------------------------ WIFI (as in ERROR-CUBE)
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
    rows.push_back(toggle("PHONE SETUP", (uint8_t*)&S.portal, [] { netPortal(S.portal); }));
    if (S.portal) {
      rows.push_back(info("1. JOIN WIFI", [](char* o) { strcpy(o, S.portalName); }));
      rows.push_back(info("2. OPEN", [](char* o) { strcpy(o, "192.168.4.1"); }));
    } else if (S.net == NET_UP) rows.push_back(info("OR OPEN", [](char* o) { sprintf(o, "%s", S.ip); }));
  }
  void tick() override {
    if (nn < 0) { int k = netScanResults(nets, 16); if (k >= 0) { nn = k; uiDirty(); } }
    ns = netSaved(saved, 6);
    L.tick();
    redrawIn(1000);
  }
  void draw() override { build(); pathLine("C:\\PICO8\\WIFI>"); drawRows(L, rows); fkeysBack(); }
  bool ev(const Ev& e) override { build(); return rowsEv(L, rows, e); }
};

// ------------------------------------------------------------------ FIRMWARE: update PICO-CUBE, or go back to ERROR-CUBE
struct Firmware : Screen {
  bool sw;                                // switching to ERROR-CUBE (else: PICO-CUBE's own update)
  explicit Firmware(bool s) : sw(s) {}
  const char* name() override { return sw ? "SWITCH" : "UPDATE"; }
  bool keepAwake() override { return S.upd == UPD_LOADING || S.upd == UPD_CHECKING; }
  bool noSwipeBack() override { return S.upd == UPD_LOADING; }
  void enter() override {
    if (sw) pnetSwitch();
    else if (S.upd != UPD_LOADING && S.upd != UPD_NEW) netUpdateCheck();
  }
  void tick() override { redrawIn(300); }
  void draw() override {
    pathLine(sw ? "C:\\>ERROR-CUBE" : "C:\\PICO8\\UPDATE>");
    stripe(30, 100, 420, 6);
    char b[96];
    int y = 124;
    if (sw) {
      drawText(FT, 30, y, "PICO-CUBE > ERROR-CUBE", C_FG); y += 40;
      drawText(FT, 30, y, "The cube gets ERROR-CUBE back", C_DIM, TX_FLAT); y += 32;
      drawText(FT, 30, y, "(PICO-8 is not in it). Your", C_DIM, TX_FLAT); y += 32;
      drawText(FT, 30, y, "WiFi, settings and files stay.", C_DIM, TX_FLAT); y += 44;
    } else {
      snprintf(b, sizeof(b), "THIS CUBE: PICO-CUBE %s", CUBE_VERSION); drawText(FT, 30, y, b, C_FG); y += 40;
      if (S.updVer[0] && (S.upd == UPD_NEW || S.upd == UPD_LATEST || S.upd == UPD_LOADING)) {
        snprintf(b, sizeof(b), "NEWEST:    PICO-CUBE %s", S.updVer); drawText(FT, 30, y, b, C_FG); y += 36;
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
      case UPD_LOADING: st = S.updMsg; break;
      case UPD_DONE: st = S.updMsg; break;
      case UPD_FAIL: st = S.updMsg; break;
    }
    if (S.upd == UPD_FAIL) { drawText(FT, 30, y, "DIDN'T WORK:", pc(8)); y += 34; }
    if (st[0]) { drawText(FT, 30, y, st, S.upd == UPD_FAIL ? pc(8) : pc(11)); y += 40; }
    if (S.upd == UPD_LOADING || S.upd == UPD_DONE) {
      meter(30, y, 420, 30, S.updPct, true);
      snprintf(b, sizeof(b), "%d%%  DON'T SWITCH IT OFF", S.updPct); drawText(FT, 30, y + 40, b, C_DIM, TX_FLAT);
    }
    if (S.upd == UPD_LOADING || S.upd == UPD_DONE) return;
    if (sw) {
      if (S.upd == UPD_FAIL) fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Try again", [] { pnetSwitch(); }}});
      else fkeysBack();
    } else if (S.upd == UPD_NEW) fkeys({{"ESC", "Back", [] { uiBack(); }}, {"\xE2\x86\xB5", "Install", [] { netUpdateInstall(); }}});
    else fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Check", [] { netUpdateCheck(); }}});
  }
  bool ev(const Ev& e) override {
    if (S.upd == UPD_LOADING) return true;   // nothing gets in the way of the install
    if (e.t == EV_KEY && e.key == K_OK) { if (!sw && S.upd == UPD_NEW) netUpdateInstall(); else if (sw && S.upd == UPD_FAIL) pnetSwitch(); return true; }
    return false;
  }
};
static void askSwitch() {
  ask("BACK TO ERROR-CUBE?", "The cube downloads ERROR-CUBE from error.blue and restarts in it. Your WiFi, settings and carts stay. To come back: ERROR-CUBE > UPDATE > PICO-CUBE.",
      {"SWITCH", "CANCEL"}, [](int i) {
    if (i) return;
    if (S.net != NET_UP) { ask("NO WIFI", "Join a WiFi network first: SETUP > WIFI.", {"OK"}, nullptr); return; }
    uiOpen(new Firmware(true));
  });
}

// ------------------------------------------------------------------ PICO-CUBE: CARTS | ONLINE | SETUP
static const char* TABS[3] = {"CARTS", "ONLINE", "SETUP"};
struct Cart { std::string path, name; long size; bool sd, last; };
struct Home : Screen {
  int tab = 0, selT[3] = {0, 0, 0};
  ListView L;
  std::vector<Cart> carts;
  PCart* web = (PCart*)malloc(sizeof(PCart) * 96); int nWeb = -1;
  std::vector<Row> rows;
  std::string dlFile, dlName; bool dlPlay = false; int dlSeen = 0;
  const char* name() override { return "PICO-CUBE"; }
  void loadCarts() {
    installCarts();
    carts.clear();
    static FileEnt* e = nullptr;
    if (!e) e = (FileEnt*)malloc(sizeof(FileEnt) * 128);
    if (!e) return;
    sdMount();
    std::string last = lastCart();
    const char* dirs[2] = {"/sd/PICO8", CART_DIR};
    for (int d = 0; d < 2; d++) {
      if (d == 0 && !S.sd) continue;
      int n = fsList(dirs[d], e, 128);
      for (int i = 0; i < n; i++) if (!e[i].dir && isCart(e[i].name)) {
        Cart c; c.path = std::string(dirs[d]) + "/" + e[i].name; c.name = niceName(e[i].name); c.size = e[i].size; c.sd = d == 0;
        c.last = c.path == last;
        carts.push_back(c);
      }
    }
    // the last one played first, then A..Z
    std::sort(carts.begin(), carts.end(), [](const Cart& a, const Cart& b) {
      if (a.last != b.last) return a.last;
      return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });
  }
  void buildSetup() {
    rows.clear();
    rows.push_back(heading("CONTROLS", IC_HAND));
    rows.push_back(number("TILT FOR AN ARROW", &cfg.p8Dead, 4, 30, 2, "\xC2\xB0"));
    rows.push_back(choice("LEFT / RIGHT", &cfg.p8InvX, 2, [](int i) { return i ? "TURNED ROUND" : "NORMAL"; }));
    rows.push_back(choice("UP / DOWN", &cfg.p8InvY, 2, [](int i) { return i ? "TURNED ROUND" : "NORMAL"; }));
    rows.push_back(choice("BUTTONS", &cfg.p8Swap, 2, [](int i) { return i ? "-=X  +=O" : "-=O  +=X"; }));
    rows.push_back(action("CALIBRATE", [] { askCalibrate(); }, [](char* o) { strcpy(o, cfg.p8CalOk ? "DONE" : "NOT YET"); }));
    rows.push_back(action("TEST THE TILT", [] { uiOpen(new TiltTest()); }, [](char* o) { strcpy(o, ">"); }));
    rows.push_back(heading("SOUND + SCREEN", IC_SOUND));
    rows.push_back(slider("VOLUME", &cfg.vol, 0, [] { tone(880, 40); }));
    rows.push_back(slider("BRIGHTNESS", &cfg.bright, 5, [] { sysBrightness(uiBrightNow()); }));
    rows.push_back(choice("ROTATION", &cfg.rot, 4, [](int i) { static const char* R[4] = {"0", "90", "180", "270"}; return R[i]; }));
    rows.push_back(choice("OFF AFTER (MENU)", &cfg.offIdx, N_OFF, [](int i) { return offLabel(i); }));
    rows.push_back(heading("CONNECT", IC_WIFI));
    rows.push_back(action("WIFI", [] { uiOpen(new Wifi()); }, [](char* o) {
      if (S.net == NET_UP) { snprintf(o, 40, "%s", S.ssid); utf8Cut(o, 12); } else strcpy(o, cfg.wifiOn ? "NOT CONNECTED" : "OFF");
    }));
    rows.push_back(heading("FIRMWARE", IC_UPDATE));
    rows.push_back(action("UPDATE PICO-CUBE", [] { uiOpen(new Firmware(false)); }, [](char* o) {
      if (S.upd == UPD_NEW && !pnetSwitching) snprintf(o, 40, "%s IS OUT", S.updVer); else snprintf(o, 40, "%s", CUBE_VERSION);
    }));
    rows.push_back(action("BACK TO ERROR-CUBE", [] { askSwitch(); }, [](char* o) { strcpy(o, ">"); }));
    rows.push_back(heading("THIS CUBE", IC_INFO));
    rows.push_back(info("FREE MEMORY", [](char* o) { snprintf(o, 40, "%uK + %uK", (unsigned)(freeHeap() / 1024), (unsigned)(freePsram() / 1024)); }));
    rows.push_back(info("DRIVE C:", [](char* o) { uint64_t t, u; if (fsInfo("/c", t, u)) snprintf(o, 40, "%uK FREE", (unsigned)((t - u) / 1024)); else strcpy(o, "-"); }));
    rows.push_back(info("BATTERY", [](char* o) { if (S.battery) snprintf(o, 40, "%d%%%s", S.batPct, S.charging ? " CHARGING" : ""); else strcpy(o, S.usb ? "USB" : "-"); }));
    rows.push_back(info("PLAYER", [](char* o) { strcpy(o, "FAKE-08 (MIT)"); }));
  }
  int count() { return tab == 0 ? (int)carts.size() : tab == 1 ? (nWeb > 0 ? nWeb : 0) : (int)rows.size(); }
  void fill() {
    if (tab == 0) loadCarts();
    if (tab == 1) nWeb = pnetCarts(web, 96);
    if (tab == 2) buildSetup();
    L.n = count(); if (L.sel >= L.n) L.sel = L.n ? L.n - 1 : 0;
    if (tab == 2 && L.n && rows[L.sel].kind >= 3) { L.sel = 0; while (L.sel < L.n - 1 && rows[L.sel].kind >= 3) L.sel++; }
  }
  void enter() override { L.setBox(15, 112, 450, 312); L.rowH = 52; fill(); L.showSel(); uiReveal(); }
  void setTab(int t) {
    if (t < 0 || t > 2 || t == tab) return;
    selT[tab] = L.sel; tab = t;
    if (tab == 1 && nWeb == -2) pnetCartsRefresh();
    L.sel = selT[t]; L.scroll = 0; fill(); L.showSel();
    snd(SND_MOVE); uiDirty();
  }
  // ---- a cart from error.blue
  void get(const PCart& c, bool thenPlay) {
    if (S.dlState == 1) { toast("WAIT: STILL GETTING ONE"); return; }
    dlFile = c.file; dlName = c.name; dlPlay = thenPlay; dlSeen = 1;
    fsMkdir(CART_DIR);
    std::string path = std::string("/pico8/carts/") + c.file + "?v=" + c.ver;
    netDownload(path.c_str(), (std::string(CART_DIR) + "/" + c.file).c_str());
    char vf[96]; snprintf(vf, sizeof(vf), "%s/%s.v", CART_DIR, c.file);
    fsWrite(vf, (const uint8_t*)c.ver, strlen(c.ver));
  }
  void webPick(int i) {
    if (i < 0 || i >= nWeb) return;
    PCart c = web[i];
    std::string have = localPath(c.file);
    bool newer = have.length() && localVer(c.file) != c.ver;
    char t[220];
    snprintf(t, sizeof(t), "BY %s\n%s\n%s \xC2\xB7 %uK%s", c.author, c.about, c.lic, (unsigned)((c.size + 1023) / 1024), newer ? " \xC2\xB7 NEW VERSION" : have.length() ? " \xC2\xB7 ON THE CUBE" : "");
    if (have.length()) ask(c.name, t, {"PLAY", newer ? "UPDATE" : "GET AGAIN", "CANCEL"}, [this, c, have](int k) { if (k == 0) play(have); else if (k == 1) get(c, false); });
    else ask(c.name, t, {"GET + PLAY", "GET", "CANCEL"}, [this, c](int k) { if (k < 2) get(c, k == 0); });
  }
  void cartPick(int i) { if (i >= 0 && i < (int)carts.size()) play(carts[i].path); }
  void cartMore(int i) {
    if (i < 0 || i >= (int)carts.size()) return;
    Cart c = carts[i];
    ask(c.name.c_str(), c.sd ? "On the TF card (A:\\PICO8)" : "On the cube (C:\\pico8)", {"PLAY", "DELETE", "CANCEL"}, [this, c](int k) {
      if (k == 0) play(c.path);
      else if (k == 1) ask("DELETE", (c.name + "?").c_str(), {"DELETE", "KEEP"}, [this, c](int j) {
        if (j) return;
        if (fsRemove(c.path.c_str())) { toast("DELETED"); std::string v = c.path + ".v"; fsRemove(v.c_str()); } else toast("CAN'T DELETE IT");
        fill(); uiDirty();
      });
    });
  }
  void pick(int i) { if (tab == 0) cartPick(i); else if (tab == 1) webPick(i); }
  void draw() override {
    for (int t = 0; t < 3; t++) {
      int x = 15 + t * 152, w = 146;
      if (t == tab) invBar(x, 52, w, 38);
      else rect(x, 52, w, 38, C_DIM);
      drawTextC(FT, x + w / 2, 55, TABS[t], t == tab ? C_BG : C_DIM, TX_FLAT);
      hit(x, 46, w, 50, [this, t] { setTab(t); }, nullptr, false);
    }
    stripe(15, 98, 450, 5);
    L.n = count();
    if (tab == 2) {
      buildSetup(); drawRows(L, rows);
    } else if (tab == 0) {
      L.draw([&](int i, int x, int y, int w, int h, bool sel) {
        Cart& c = carts[i];
        int ty = y + (h - LH) / 2;
        if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
        fillRect(x + 10, y + 14, 8, h - 28, pc(8 + i % 7));        // a little colour tab per cart
        char nm[48]; snprintf(nm, sizeof(nm), "%s", c.name.c_str()); utf8Cut(nm, 18);
        drawText(FT, x + 28, ty, nm, sel ? C_BG : C_FG, sel ? TX_FLAT : TX_GLOW);
        char sz[16]; if (c.last) snprintf(sz, sizeof(sz), "LAST"); else snprintf(sz, sizeof(sz), "%s%ldK", c.sd ? "A: " : "", (c.size + 1023) / 1024);
        drawTextR(FT, x + w - 10, ty, sz, sel ? C_BG : c.last ? pc(14) : C_DIM, TX_FLAT);
      }, [this](int i) { cartPick(i); });
      if (carts.empty()) { drawTextC(FT, 240, 200, "NO CARTS YET", C_DIM, TX_FLAT); drawTextC(FT, 240, 240, "GET SOME: ONLINE", C_DIM, TX_FLAT); }
    } else {
      L.draw([&](int i, int x, int y, int w, int h, bool sel) {
        PCart& c = web[i];
        int ty = y + (h - LH) / 2;
        if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
        bool have = localPath(c.file).length() > 0;
        bool newer = have && localVer(c.file) != c.ver;
        if (have) fillCircle(x + 16, y + h / 2, 6, newer ? pc(9) : pc(11)); else ring(x + 16, y + h / 2, 6, 2, sel ? C_BG : C_DIM);
        char nm[40]; snprintf(nm, sizeof(nm), "%s", c.name); utf8Cut(nm, 17);
        drawText(FT, x + 32, ty, nm, sel ? C_BG : C_FG, sel ? TX_FLAT : TX_GLOW);
        char sz[16];
        bool dl = S.dlState == 1 && dlFile == c.file;
        if (dl) snprintf(sz, sizeof(sz), "%d%%", S.dlPct); else if (newer) strcpy(sz, "NEW"); else snprintf(sz, sizeof(sz), "%uK", (unsigned)((c.size + 1023) / 1024));
        drawTextR(FT, x + w - 10, ty, sz, sel ? C_BG : dl ? pc(11) : newer ? pc(9) : C_DIM, TX_FLAT);
      }, [this](int i) { webPick(i); });
      if (nWeb <= 0) {
        const char* m = nWeb == -1 ? "LOADING THE LIST..." : S.net != NET_UP ? "NO WIFI: SETUP > WIFI" : nWeb == 0 ? "NO CARTS THERE YET" : "CAN'T GET THE LIST";
        drawTextC(FT, 240, 200, m, C_DIM, TX_FLAT);
        drawTextC(FT, 240, 240, "error.blue/pico8", C_DIM, TX_FLAT);
      }
    }
    if (tab == 0) fkeys({{"\xE2\x86\xB5", "Play", [this] { cartPick(L.sel); }}, {"\xE2\x86\x90\xE2\x86\x92", "Tab", [this] { setTab((tab + 1) % 3); }}});
    else if (tab == 1) fkeys({{"\xE2\x86\xB5", "Get", [this] { webPick(L.sel); }}, {"R", "Reload", [this] { pnetCartsRefresh(); nWeb = -1; uiDirty(); }},
                              {"\xE2\x86\x90\xE2\x86\x92", "Tab", [this] { setTab((tab + 1) % 3); }}});
    else fkeys({{"\xE2\x86\x90\xE2\x86\x92", "Tab", [this] { setTab((tab + 1) % 3); }}});
  }
  void tick() override {
    L.tick();
    if (tab == 1) { int n = pnetCarts(web, 96); if (n != nWeb) { nWeb = n; L.n = count(); uiDirty(); } if (S.dlState == 1) redrawIn(250); }
    // a download finished
    if (dlSeen && S.dlState != 1) {
      dlSeen = 0;
      if (S.dlState == 2) {
        snd(SND_OK);
        std::string p = std::string(CART_DIR) + "/" + dlFile;
        if (dlPlay) play(p); else toast(("GOT " + dlName).c_str());
      } else { snd(SND_ERR); toast(("COULDN'T GET " + dlName).c_str()); std::string v = std::string(CART_DIR) + "/" + dlFile + ".v"; if (fsSize((std::string(CART_DIR) + "/" + dlFile).c_str()) <= 0) fsRemove(v.c_str()); }
      uiDirty();
    }
    // a new PICO-CUBE: say so once
    static bool told = false;
    if (!told && S.upd == UPD_NEW && !pnetSwitching && uiCurrent() == this) { told = true; toast((std::string("PICO-CUBE ") + S.updVer + " IS OUT: SETUP").c_str(), 4000); }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_SWIPE && e.dir == SW_LEFT) { setTab(tab < 2 ? tab + 1 : 2); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_RIGHT) { setTab(tab > 0 ? tab - 1 : 0); return true; }
    if (e.t == EV_KEY && e.key == K_BACK) { setTab((tab + 1) % 3); return true; }
    if (tab == 2) { buildSetup(); return rowsEv(L, rows, e); }
    if (e.t == EV_LONG && tab == 0 && e.y0 >= L.y && e.y0 < L.y + L.h) {
      int i = (int)((e.y0 - L.y + L.scroll) / L.rowH);
      if (i >= 0 && i < (int)carts.size()) { L.sel = i; cartMore(i); return true; }
    }
    return L.ev(e, [this](int i) { pick(i); });
  }
};
Screen* homeScreen() { return new Home(); }

// ------------------------------------------------------------------ start-up: the 16 colours, then the carts
struct Boot : Screen {
  uint32_t t0 = 0;
  const char* name() override { return "BOOT"; }
  bool fullScreen() override { return true; }
  bool keepAwake() override { return true; }
  void enter() override { t0 = msNow(); snd(SND_ON); }
  void tick() override {
    uint32_t t = msNow() - t0;
    if (t > (cfg.boot == 2 ? 300u : 2200u)) { uiStart(); return; }
    redrawIn(30);
  }
  void draw() override {
    uint32_t t = msNow() - t0;
    gfxClearTo(pc(0));
    // the colours run in, row by row
    int rows = t < 700 ? (int)(t * 24 / 700) : 24;
    for (int r = 0; r < rows; r++) for (int i = 0; i < 16; i++) {
      int x = i * 30, y = r * 20;
      uint32_t k = (i + r + t / 60) % 16;
      if (t > 700) k = (r * 16 + i) % 16 == (int)((t / 50) % 16) ? 7 : 0;
      if (k) fillRect(x, y, 30, 20, pc(k));
    }
    if (t > 700) {
      gfxClearTo(pc(0));
      stripe(140, 170, 200, 10);
      drawTextC(FB, 240, 196, "PICO-CUBE", pc(7), TX_FLAT);
      drawTextC(FT, 240, 250, "VERSION " CUBE_VERSION, pc(6), TX_FLAT);
      drawTextC(FT, 240, 290, "PICO-8 CARTS ON ERROR-CUBE", pc(13), TX_FLAT);
      drawTextC(FT, 240, 410, "(C) 2026 ERROR.BLUE", pc(5), TX_FLAT);
    }
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
    if (cfg.aodBatt && S.battery) { char b[16]; snprintf(b, sizeof(b), "%d%%%s", S.batPct, S.charging ? "+" : ""); drawTextC(FT, 240 + ox, 280 + oy, b, mix(C_BLACK, C_FG, 30), TX_FLAT); }
  }
};
Screen* aodScreen() { return new Aod(); }

// ------------------------------------------------------------------ what ui.cpp and the board ask of the programs
void appsRegister() {}
void appsLoop() {}
void clockTick() {}
uint32_t tmrEnd = 0, tmrLeft = 0; bool tmrRun = false;
Screen* findProg(const char* dos) {
  if (!strcmp(dos, "WIFI")) return new Wifi();
  if (!strcmp(dos, "UPDATE")) return new Firmware(false);
  return nullptr;
}
void openProg(const char* dos) { Screen* s = findProg(dos); if (s) uiOpen(s); }
bool openFile(const char* path) {
  if (isCart(path)) { play(path); return true; }
  if (netInstallFile(path)) { uiOpen(new Firmware(false)); return true; }
  return false;
}
void musicNext(int) {}
void musicToggle() {}
void musicTick() {}
void musicPlayFolder(const char*, const char*) {}
Screen* makeNowPlaying() { return nullptr; }
const char* bootLine(int) { return ""; }
