// apps_p8.cpp: PICO8.EXE: the carts (A:\PICO8 on the SD card, C:\pico8 on the cube), playing one full screen.
// Controls: tilt the cube = the arrows (from how you hold it: CALIBRATE), - = O, + = X (SWAP in setup),
// PWR = the cube's menu (continue, restart, calibrate, quit). The emulator is FAKE-08 (src/p8).
#include "../ui/ui.h"
#include "../p8/p8.h"
#include "../p8/carts.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>

static bool isCart(const char* n) {
  size_t l = strlen(n);
  return (l > 3 && !strcasecmp(n + l - 3, ".p8")) || (l > 7 && !strcasecmp(n + l - 7, ".p8.png"));
}
// the carts that come with the cube: into C:\pico8 the first time (and again when a new firmware brings a newer one)
static void installCarts() {
  static bool done = false;
  if (done) return;
  done = true;
  fsMkdir("/c/pico8");
  for (int i = 0; i < N_P8CARTS; i++) {
    char p[64]; snprintf(p, sizeof(p), "/c/pico8/%s", P8CARTS[i].name);
    if (fsSize(p) != (long)P8CARTS[i].size) fsWrite(p, (const uint8_t*)P8CARTS[i].data, P8CARTS[i].size);
  }
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
void p8Calibrate() {
  gravOk = false; readGrav();
  float u[3]; unit(grav, u);
  for (int i = 0; i < 3; i++) cfg.p8Cal[i] = (int16_t)lroundf(u[i] * 1000);
  cfg.p8CalOk = 1; cfgSave();
  for (int i = 0; i < 3; i++) neutral[i] = u[i];
  toast("CALIBRATED: THIS IS STRAIGHT", 1500);
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
static bool hook(int id, bool down) {
  if (menuOpen) return false;            // the cube's menu is up: the buttons work it
  if (id == 2) { if (down) { extern void p8Menu(); p8Menu(); } return true; }
  uint8_t bit = (id == 0) != (cfg.p8Swap != 0) ? P8_O : P8_X;
  if (down) keys |= bit; else keys &= ~bit;
  return true;
}
static std::string playing;
static bool restartWanted = false, quitWanted = false;
void p8Menu() {
  if (menuOpen) return;
  menuOpen = true; p8Pause(true); uiWake();
  ask("PICO-8", "", {"CONTINUE", "RESTART", "CALIBRATE", "QUIT"}, [](int i) {
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
    useCal();
    if (!p8Start(path.c_str())) { fail(); return; }
    startAt = msNow();
  }
  void fail() {
    std::string e = p8Error();
    btnHook = nullptr;
    uiBack();
    if (e.length()) ask("PICO-8", e.c_str(), {"OK"}, nullptr);
  }
  void enter() override { btnHook = hook; if (!started) { started = true; playing = path; start(); } }
  void leave() override { btnHook = nullptr; }
  ~P8Play() { btnHook = nullptr; menuOpen = false; p8Stop(); playing.clear(); }
  void tick() override {
    if (quitWanted) { quitWanted = false; uiBack(); return; }
    if (restartWanted) { restartWanted = false; p8Stop(); start(); return; }
    if (!p8Running()) {
      if (p8Error()[0]) { fail(); return; }
      uiBack(); return;                   // the cart's own menu: exit
    }
    if (!menuOpen) p8Buttons(tiltArrows() | keys);
    p8Pump();                             // the simulator (the board runs it in its own task)
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
      drawTextC(FT, 240, 408, "TILT = ARROWS", C_FG, TX_FLAT);
      drawTextC(FT, 240, 440, cfg.p8Swap ? "- = X   + = O   PWR = MENU" : "- = O   + = X   PWR = MENU", C_DIM, TX_FLAT);
    }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_LONG) { p8Menu(); return true; }     // a long touch: the menu too
    return true;
  }
};
void p8Open(const char* path) { installCarts(); uiOpen(new P8Play(path)); }

// ------------------------------------------------------------------ PICO8.EXE: the carts
struct Cart { std::string path, name; long size; bool sd; };
struct Pico8 : Screen {
  std::vector<Cart> carts; ListView L;
  const char* name() override { return "PICO8.EXE"; }
  void load() {
    installCarts();
    carts.clear();
    static FileEnt* e = nullptr;
    if (!e) e = (FileEnt*)malloc(sizeof(FileEnt) * 128);
    if (!e) return;
    sdMount();
    const char* dirs[2] = {"/sd/PICO8", "/c/pico8"};
    for (int d = 0; d < 2; d++) {
      if (d == 0 && !S.sd) continue;
      int n = fsList(dirs[d], e, 128);
      for (int i = 0; i < n; i++) if (!e[i].dir && isCart(e[i].name)) {
        Cart c; c.path = std::string(dirs[d]) + "/" + e[i].name; c.name = e[i].name; c.size = e[i].size; c.sd = d == 0;
        size_t k = c.name.rfind(".p8"); if (k != std::string::npos) c.name = c.name.substr(0, k);
        for (auto& ch : c.name) ch = toupper((unsigned char)ch);
        carts.push_back(c);
      }
    }
    std::sort(carts.begin(), carts.end(), [](const Cart& a, const Cart& b) { return strcasecmp(a.name.c_str(), b.name.c_str()) < 0; });
    L.n = carts.size(); if (L.sel >= L.n) L.sel = L.n ? L.n - 1 : 0;
    uiReveal();
  }
  void enter() override { L.setBox(15, 92, 450, 300); L.rowH = 52; load(); }
  void more(int i) {
    if (i < 0 || i >= (int)carts.size()) return;
    Cart c = carts[i];
    ask(c.name.c_str(), c.sd ? "On the SD card (A:\\PICO8)" : "On the cube (C:\\pico8)", {"PLAY", "DELETE", "CANCEL"}, [this, c](int k) {
      if (k == 0) p8Open(c.path.c_str());
      else if (k == 1) ask("DELETE", (c.name + "?").c_str(), {"DELETE", "KEEP"}, [this, c](int j) {
        if (j == 0) { if (fsRemove(c.path.c_str())) toast("DELETED"); else toast("CAN'T DELETE IT"); load(); }
      });
    });
  }
  void draw() override {
    pathLine("C:\\PICO8>");
    L.n = carts.size();
    L.draw([&](int i, int x, int y, int w, int h, bool sel) {
      Cart& c = carts[i];
      int ty = y + (h - LH) / 2;
      if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
      char nm[48]; snprintf(nm, sizeof(nm), "%s", c.name.c_str()); utf8Cut(nm, 20);
      drawText(FT, x + 10, ty, nm, sel ? C_BG : C_FG, sel ? TX_FLAT : TX_GLOW);
      char sz[16]; snprintf(sz, sizeof(sz), "%s %ldK", c.sd ? "A:" : "C:", (c.size + 1023) / 1024);
      drawTextR(FT, x + w - 10, ty, sz, sel ? C_BG : C_DIM, TX_FLAT);
    }, [this](int i) { if (i >= 0 && i < (int)carts.size()) p8Open(carts[i].path.c_str()); });
    if (carts.empty()) drawTextC(FT, 240, 220, "NO CARTS YET", C_DIM, TX_FLAT);
    drawText(FT, colX(0), rowY(11) + 4, "Add: error.blue/cube > PICO-8", C_DIM, TX_FLAT);
    drawText(FT, colX(0), rowY(12) + 4, "TILT=ARROWS  -=O  +=X", C_DIM, TX_FLAT);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"C", "Calibrate", [] { ask("CALIBRATE", "Hold the cube the way you want to play, then OK.", {"OK", "CANCEL"}, [](int i) { if (!i) p8Calibrate(); }); }},
           {"S", "Setup", [] { Screen* s = makeP8Setup(); if (s) uiOpen(s); }}});
  }
  void tick() override { L.tick(); }
  bool ev(const Ev& e) override {
    if (e.t == EV_LONG && e.y0 >= L.y && e.y0 < L.y + L.h) {
      int i = (int)((e.y0 - L.y + L.scroll) / L.rowH);
      if (i >= 0 && i < (int)carts.size()) { L.sel = i; more(i); return true; }
    }
    return L.ev(e, [this](int i) { if (i >= 0 && i < (int)carts.size()) p8Open(carts[i].path.c_str()); uiDirty(); });
  }
};
Screen* makePico8() { return new Pico8(); }
