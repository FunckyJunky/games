// ui.cpp: see ui.h
#include "ui.h"
#include "../core/fx.h"
#include "../core/g3d.h"
#include "../core/holo.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

static std::vector<Screen*> stack;
static bool dirty = true;
static uint32_t nextDraw = 0;
struct Hit { int x, y, w, h; Act a, l; bool focus; };
static std::vector<Hit> hits;
static int focusIdx = -1;                 // buttons walk through the hits when the screen doesn't use them
static uint32_t focusShowUntil = 0;
static char toastMsg[64] = "";
static uint32_t toastUntil = 0;
static int lastTapX = 0, lastTapY = 0;
static int ripX = 0, ripY = 0; static uint32_t ripAt = 0;    // the ring where you tapped
extern float holoBarUp;
static uint32_t revealAt = 0;
static int transDir = -1; static uint32_t transAt = 0; static bool transTo = false;
static const uint16_t* lastPre = nullptr;                    // the last frame, before the lens + scanlines
void uiReveal() { revealAt = msNow(); }
uint32_t uiRevealAt() { return revealAt; }

// overlays: drawn over the current screen, they get the input first
struct Overlay { virtual ~Overlay() {} virtual void draw() = 0; virtual bool ev(const Ev& e) = 0; virtual bool dim() { return true; } virtual int kind() { return 0; } };
static std::vector<Overlay*> ovl;
static void ovlClose(Overlay* o) {
  for (size_t i = 0; i < ovl.size(); i++) if (ovl[i] == o) { ovl.erase(ovl.begin() + i); break; }
  delete o; dirty = true;
}

void sndKey() { if (cfg.clicks) snd(SND_KEY); }
void uiDirty() { dirty = true; }
void redrawIn(uint32_t ms) { uint32_t t = msNow() + ms; if (!nextDraw || (int32_t)(t - nextDraw) < 0) nextDraw = t; }
Screen* uiCurrent() { return stack.empty() ? nullptr : stack.back(); }

void uiTransition(int dir) {
  if (cfg.anim < 2 || cfg.saver || !uiAwake() || !lastPre) { if (cfg.anim) uiGlitch(1, 120); return; }
  uint16_t* a = fxBuf(0);
  if (!a || !fxBuf(1)) return;
  memcpy(a, lastPre, SCR_W * SCR_H * 2);
  transDir = dir; transAt = msNow(); transTo = false;
  dirty = true;
}
void uiReplaceRoot(Screen* s) {
  if (!s) return;
  if (stack.empty()) { uiOpen(s); return; }
  bool top = stack.size() == 1;
  if (top) stack[0]->leave();
  delete stack[0];
  stack[0] = s;
  if (top) { s->enter(); uiTransition(2); }
  dirty = true;
}
void uiOpen(Screen* s) {
  if (!s) return;
  if (!stack.empty()) uiTransition(0);
  uiReveal();
  if (!stack.empty()) stack.back()->leave();
  stack.push_back(s);
  focusIdx = -1;
  s->enter();
  dirty = true;
}
void uiBack() {
  if (stack.size() <= 1) return;
  Screen* s = stack.back();
  uiTransition(1);
  uiReveal();
  s->leave();
  stack.pop_back();
  delete s;
  focusIdx = -1;
  if (!stack.empty()) stack.back()->enter();
  sndKey();
  dirty = true;
}
void uiHome() { while (stack.size() > 1) uiBack(); }

// ------------------------------------------------------------------ screen on / off
static bool awake = true, dimmed = false, aodOn = false;
static uint32_t lastAct = 0, sleptAt = 0;
bool crtAnim = true;
int8_t crtWant = 0;
bool uiAwake() { return awake; }
bool uiAod() { return !awake && aodOn; }
void uiActivity() { lastAct = msNow(); if (dimmed) { dimmed = false; dirty = true; } }
void uiWake() {
  uiActivity();
  if (awake) return;
  awake = true; aodOn = false; dimmed = false;
  crtWant = cfg.crt ? 1 : 0;
  dirty = true;
  snd(SND_ON);
}
void uiSleep() {
  if (!awake) return;
  awake = false; dimmed = false; sleptAt = msNow();
  aodOn = cfg.aod;
  crtWant = cfg.crt ? -1 : 0;
  while (!ovl.empty()) { delete ovl.back(); ovl.pop_back(); }
  if (kbIsOpen()) {}
  snd(SND_OFF);
  dirty = true;
}
int uiBrightNow() {
  if (!awake) return aodOn ? (cfg.aodLight < 1 ? 1 : cfg.aodLight > 30 ? 30 : cfg.aodLight) : 0;
  int b = cfg.bright;
  if (cfg.saver && b > 50) b = 50;
  if (dimmed) b = b / 4 < 5 ? 5 : b / 4;
  return b;
}

// ------------------------------------------------------------------ input
static bool tDown = false, tDrag = false, tLongDone = false, tSwallow = false;
static int tx0, ty0, tx, ty;
static uint32_t tAt;
static int pressHit = -1;

static int hitAt(int x, int y) {
  for (int i = (int)hits.size() - 1; i >= 0; i--) {
    const Hit& h = hits[i];
    if (x >= h.x - 4 && x < h.x + h.w + 4 && y >= h.y - 4 && y < h.y + h.h + 4) return i;
  }
  return -1;
}
static bool deliver(const Ev& e) {
  if (!ovl.empty()) { ovl.back()->ev(e); return true; }
  Screen* s = uiCurrent();
  return s && s->ev(e);
}

void uiTouch(bool down, int x, int y) {
  uint32_t now = msNow();
  if (down) { fxLensTouch(x, y); holoUnmap(x, y); }   // the picture is curved (and HOLO moves it): where the finger really is
  if (down && !tDown) {
    tDown = true; tDrag = false; tLongDone = false;
    tx0 = tx = x; ty0 = ty = y; tAt = now;
    tSwallow = !awake;
    if (!awake) { if (cfg.tapWake || aodOn) uiWake(); else tSwallow = true; return; }
    uiActivity();
    pressHit = hitAt(x, y);
    Ev e; e.t = EV_DOWN; e.x = x; e.y = y; e.x0 = x; e.y0 = y;
    deliver(e);
    dirty = true;
    return;
  }
  if (!tDown) return;
  if (tSwallow) { if (!down) tDown = false; return; }
  if (down) {
    tx = x; ty = y;
    if (!tDrag && (abs(x - tx0) > 14 || abs(y - ty0) > 14)) { tDrag = true; pressHit = -1; dirty = true; }
    if (tDrag) { Ev e; e.t = EV_MOVE; e.x = x; e.y = y; e.x0 = tx0; e.y0 = ty0; e.ms = now - tAt; deliver(e); }
    uiActivity();
    return;
  }
  // finger up
  tDown = false;
  uiActivity();
  uint32_t dt = now - tAt;
  int dx = tx - tx0, dy = ty - ty0;
  Ev up; up.t = EV_UP; up.x = tx; up.y = ty; up.x0 = tx0; up.y0 = ty0; up.ms = dt;
  if (!tDrag) {
    if (tLongDone) { deliver(up); return; }
    lastTapX = tx0; lastTapY = ty0;
    if (cfg.anim) { ripX = tx0; ripY = ty0; ripAt = now; }
    int h = hitAt(tx0, ty0);
    Ev e; e.t = EV_TAP; e.x = tx0; e.y = ty0; e.x0 = tx0; e.y0 = ty0;
    bool done = false;
    if (ovl.empty() || h >= 0) {
      if (h >= 0 && hits[h].a) { Act a = hits[h].a; a(); done = true; }
    }
    if (!done) deliver(e);
    deliver(up);
    pressHit = -1;
    dirty = true;
    return;
  }
  deliver(up);
  // a swipe?
  bool fast = dt < 450;
  int dir = 0;
  if (fast && abs(dx) > 70 && abs(dx) > abs(dy) * 1.4) dir = dx > 0 ? SW_RIGHT : SW_LEFT;
  else if (fast && abs(dy) > 70 && abs(dy) > abs(dx) * 1.4) dir = dy > 0 ? SW_DOWN : SW_UP;
  if (dir) {
    if (dir == SW_DOWN && ty0 < 70 && ovl.empty()) { quickOpen(); return; }
    Ev e; e.t = EV_SWIPE; e.dir = dir; e.x = tx; e.y = ty; e.x0 = tx0; e.y0 = ty0; e.ms = dt;
    if (deliver(e)) return;
    Screen* s = uiCurrent();
    if (dir == SW_RIGHT && ovl.empty() && s && !s->noSwipeBack()) uiBack();
  }
  dirty = true;
}

void uiKey(int key) {
  if (!awake) {
    if (key == K_PWR_LONG) { powerMenu(); }
    uiWake();
    return;
  }
  uiActivity();
  if (key == K_PWR) { uiSleep(); return; }
  if (key == K_PWR_LONG) { powerMenu(); return; }
  Ev e; e.t = EV_KEY; e.key = key;
  if (deliver(e)) { dirty = true; return; }
  // not used by the screen: walk through the buttons on screen
  int n = hits.size();
  if (key == K_BACK) { uiBack(); return; }
  if (!n) return;
  std::vector<int> f;
  for (int i = 0; i < n; i++) if (hits[i].focus) f.push_back(i);
  if (f.empty()) return;
  int k = -1;
  for (size_t i = 0; i < f.size(); i++) if (f[i] == focusIdx) k = i;
  if (key == K_NEXT) k = (k + 1) % f.size();
  else if (key == K_PREV) k = k <= 0 ? f.size() - 1 : k - 1;
  else if (key == K_OK) {
    if (k >= 0 && hits[f[k]].a) { Act a = hits[f[k]].a; sndKey(); a(); }
    dirty = true; return;
  }
  focusIdx = f[k < 0 ? 0 : k];
  focusShowUntil = msNow() + 8000;
  sndKey();
  dirty = true;
}

void hit(int x, int y, int w, int h, Act a, Act longAct, bool focusable) {
  // HOLO: the button may sit on another depth layer than the text; touches are on the text plane
  int tx, ty; holoShift(H_TEXT, tx, ty);
  hits.push_back({x + gOX - tx, y + gOY - ty, w, h, a, longAct, focusable});
}

// ------------------------------------------------------------------ drawing pieces
void putText(int r, int c, const char* s, uint16_t col, int mode) { drawText(FT, colX(c), rowY(r), s, col ? col : C_FG, mode); }

float holoBarUp = 0;
void invBar(int x, int y, int w, int h, bool glow) {
  if (holoBarUp > 0) {                     // HOLO: a raised selection bar: its shadow and sides first
    holoShadow(x, y, w, h, holoBarUp);
    holoWalls(x, y, w, h, C_FG, holoBarUp, true);
    holoBarUp = 0;
  }
  if (glow && cfg.glow) glowRect(x, y, w, h, C_FG, 4);
  fillRect(x, y, w, h, C_FG);
}

void wifiIcon(int x, int y, int rssi, bool on) {
  // four bars, like the setup page
  int n = !on ? 0 : rssi > -60 ? 4 : rssi > -70 ? 3 : rssi > -80 ? 2 : 1;
  for (int i = 0; i < 4; i++) {
    int h = 5 + i * 4;
    uint16_t c = i < n ? C_BG : mix(C_FG, C_BG, 45);
    fillRect(x + i * 6, y + 18 - h, 4, h, c);
  }
}
void battIcon(int x, int y, int pct, bool chg) {
  uint16_t c = C_BG;
  rect(x, y + 3, 26, 14, c); rect(x + 1, y + 4, 24, 12, c);
  fillRect(x + 26, y + 7, 3, 6, c);
  if (pct >= 0) {
    int w = 20 * (pct > 100 ? 100 : pct) / 100;
    fillRect(x + 3, y + 6, w, 8, pct <= 15 && !chg ? C_RED : c);
  }
  if (chg) {   // a little lightning bolt
    static const int8_t B[][2] = {{13, 4}, {9, 11}, {13, 11}, {11, 16}, {17, 8}, {13, 8}, {15, 4}};
    for (int i = 0; i < 6; i++) line(x + B[i][0], y + B[i][1], x + B[i + 1][0], y + B[i + 1][1], C_FG);
  }
}

void icon(int id, int x, int y, uint16_t c) {
  // drawn in a 26 x 26 box, thin lines, ERROR-DOS style
  int cx = x + 13, cy = y + 13;
  switch (id) {
    case IC_SCREEN: rect(x + 1, y + 3, 24, 16, c); rect(x + 2, y + 4, 22, 14, c); fillRect(x + 10, y + 20, 6, 3, c); fillRect(x + 6, y + 23, 14, 2, c); break;
    case IC_LOOK: for (int i = -10; i <= 10; i++) { int h = (int)(6 * sqrtf(1 - i * i / 110.0f)); pset(cx + i, cy - h, c); pset(cx + i, cy + h, c); } fillCircle(cx, cy, 4, c); break;
    case IC_HOLO: rect(x + 2, y + 8, 15, 15, c); rect(x + 9, y + 2, 15, 15, c); line(x + 2, y + 8, x + 9, y + 2, c); line(x + 16, y + 8, x + 23, y + 2, c);
                  line(x + 2, y + 22, x + 9, y + 16, c); line(x + 16, y + 22, x + 23, y + 16, c); break;
    case IC_SOUND: fillRect(x + 2, y + 9, 6, 8, c); for (int i = 0; i < 7; i++) vline(x + 8 + i, y + 9 - i, 8 + 2 * i, c);
                   for (int r = 0; r < 2; r++) for (int a = -40; a <= 40; a += 8) { float t = a * 0.01745f; pset(x + 17 + r * 5 + (int)(cosf(t) * (3 + r * 3)), cy + (int)(sinf(t) * (5 + r * 4)), c); } break;
    case IC_KEY: rect(x + 1, y + 6, 11, 14, c); rect(x + 14, y + 6, 11, 14, c); fillRect(x + 4, y + 12, 5, 2, c); fillRect(x + 17, y + 12, 5, 2, c); vline(x + 19, y + 10, 6, c); break;
    case IC_WIFI: for (int r = 0; r < 3; r++) for (int a = -50; a <= 50; a += 6) { float t = a * 0.01745f; pset(cx + (int)(sinf(t) * (5 + r * 5)), y + 22 - (int)(cosf(t) * (5 + r * 5)), c); } fillCircle(cx, y + 21, 2, c); break;
    case IC_CLOUD: ring(cx, cy, 10, 1, c); vline(cx, y + 3, 21, c); hline(x + 3, cy, 21, c); for (int i = -9; i <= 9; i++) { int w = (int)(5 * sqrtf(1 - i * i / 100.0f)); pset(cx - w, cy + i, c); pset(cx + w, cy + i, c); } break;
    case IC_OFFLINE: fillRect(x + 2, cy - 1, 22, 3, c); for (int i = 0; i < 8; i++) { fillRect(x + 9 + i, cy - 2 - i, 3, 1, c); fillRect(x + 9 + i, cy + 2 + i, 3, 1, c); } fillRect(x + 2, cy - 5, 2, 11, c); break;
    case IC_TIME: ring(cx, cy, 10, 1, c); vline(cx, y + 6, 8, c); hline(cx, cy, 6, c); break;
    case IC_POWER: rect(x + 1, y + 7, 21, 12, c); fillRect(x + 22, y + 10, 3, 6, c); fillRect(x + 3, y + 9, 12, 8, c); break;
    case IC_UPDATE: vline(cx, y + 3, 16, c); vline(cx + 1, y + 3, 16, c); for (int i = 0; i < 7; i++) hline(cx - i, y + 4 + i, 2 + 2 * i, c); hline(x + 3, y + 23, 21, c); break;
    case IC_SYSTEM: rect(x + 5, y + 5, 16, 16, c); rect(x + 9, y + 9, 8, 8, c);
                    for (int i = 0; i < 4; i++) { vline(x + 7 + i * 4, y + 1, 3, c); vline(x + 7 + i * 4, y + 22, 3, c); hline(x + 1, y + 7 + i * 4, 3, c); hline(x + 22, y + 7 + i * 4, 3, c); } break;
    case IC_WALL: rect(x + 1, y + 3, 24, 20, c); line(x + 3, y + 20, x + 10, y + 11, c); line(x + 10, y + 11, x + 15, y + 17, c); line(x + 15, y + 17, x + 18, y + 14, c); line(x + 18, y + 14, x + 23, y + 20, c); fillCircle(x + 19, y + 8, 2, c); break;
    case IC_MOON: for (int i = -10; i <= 10; i++) { int w = (int)sqrtf(100.0f - i * i), w2 = (int)sqrtf(fmaxf(0, 64.0f - (i + 2) * (i + 2))); hline(cx - w, cy + i, w - w2 + (w2 ? 0 : w), c); } break;
    case IC_HAND: for (int i = 0; i < 4; i++) rect(x + 5 + i * 4, y + 3 + (i == 0 || i == 3 ? 4 : 0), 4, 12, c); rect(x + 5, y + 14, 16, 10, c); line(x + 5, y + 15, x + 1, y + 11, c); break;
    case IC_MOTION: rect(x + 7, y + 3, 12, 20, c); for (int a = 200; a <= 340; a += 10) { float t = a * 0.01745f; pset(cx + (int)(cosf(t) * 12), y + 26 + (int)(sinf(t) * 6), c); } break;
    case IC_GLASS: ring(cx, cy, 10, 1, c); line(cx - 5, cy - 5, cx - 1, cy - 8, c); break;
    case IC_LIGHT: fillCircle(cx, cy, 5, c); for (int i = 0; i < 8; i++) { float t = i * 0.785f; line(cx + (int)(cosf(t) * 8), cy + (int)(sinf(t) * 8), cx + (int)(cosf(t) * 11), cy + (int)(sinf(t) * 11), c); } break;
    case IC_DEPTH: for (int i = 0; i < 3; i++) rect(x + 1 + i * 4, y + 1 + i * 4, 24 - i * 8, 24 - i * 8, mix(C_BG, c, 100 - i * 25)); break;
    case IC_FILES: rect(x + 1, y + 6, 24, 17, c); fillRect(x + 1, y + 3, 10, 4, c); hline(x + 1, y + 10, 24, c); break;
    case IC_INFO: ring(cx, cy, 10, 1, c); fillRect(cx - 1, cy - 1, 3, 8, c); fillRect(cx - 1, cy - 6, 3, 3, c); break;
    case IC_TURN: for (int a = 30; a <= 330; a += 6) { float t = a * 0.01745f; pset(cx + (int)(cosf(t) * 10), cy + (int)(sinf(t) * 10), c); } fillRect(cx + 7, cy - 9, 5, 5, c); break;
  }
}

void fmtTime(char* out, int h, int m, int s, bool secs) {
  int hh = h;
  if (!cfg.h24) { hh = h % 12; if (!hh) hh = 12; }
  if (secs) sprintf(out, "%02d:%02d:%02d", hh, m, s); else sprintf(out, "%02d:%02d", hh, m);
}

void statusBar(const char* title) {
  int y = 16, h = 32;
  HoloZ z(H_CHROME);
  holoShadow(15, y, 450, h, H_CHROME - H_TEXT);
  holoWalls(15, y, 450, h, C_FG, H_CHROME - H_TEXT, true);
  invBar(15, y, 450, h);
  char t[32];
  snprintf(t, sizeof(t), "%s", title ? title : "ERROR-DOS");
  utf8Cut(t, 13);
  drawText(FT, 30, y, t, C_BG, TX_FLAT);
  // right side: clock, battery, WiFi, link
  int x = 450;
  struct tm tm;
  if (sysLocalTime(tm)) {
    char c[12]; fmtTime(c, tm.tm_hour, tm.tm_min, 0, false);
    if (!cfg.h24) strcat(c, tm.tm_hour < 12 ? "a" : "p");
    x -= textW(FT, c); drawText(FT, x, y, c, C_BG, TX_FLAT);
  } else { x -= textW(FT, "--:--"); drawText(FT, x, y, "--:--", C_BG, TX_FLAT); }
  x -= 10;
  if (S.battery || S.batPct >= 0) {
    char p[8]; snprintf(p, sizeof(p), "%d%%", S.batPct < 0 ? 0 : S.batPct);
    if (S.battery) { x -= textW(FT, p); drawText(FT, x, y, p, C_BG, TX_FLAT); x -= 4; }
    x -= 30; battIcon(x, y + 6, S.battery ? S.batPct : 100, S.charging || (!S.battery && S.usb));
    x -= 10;
  }
  if (cfg.offline) {                       // OFFLINE: a little plane
    x -= 24; int px = x, py = y + 16;
    fillRect(px, py - 1, 22, 3, C_BG);                        // body
    for (int i = 0; i < 7; i++) { fillRect(px + 8 + i, py - 2 - i, 3, 1, C_BG); fillRect(px + 8 + i, py + 2 + i, 3, 1, C_BG); }   // wings
    fillRect(px, py - 5, 2, 11, C_BG);                       // tail
    x -= 8;
  } else if (cfg.wifiOn) { x -= 24; wifiIcon(x, y + 7, S.rssi, S.net == NET_UP); x -= 8; }
  if (S.link) { x -= 12; fillCircle(x + 5, y + 16, 5, C_BG); x -= 6; }
  if (musState() == MED_PLAY || musState() == MED_PAUSE) {   // a note: music is on
    x -= 16; fillCircle(x + 4, y + 22, 4, C_BG); fillRect(x + 7, y + 7, 2, 15, C_BG); fillRect(x + 7, y + 7, 7, 3, C_BG); x -= 6;
  }
  if (S.chatUnread) { x -= 22; rect(x, y + 9, 20, 14, C_BG); line(x, y + 9, x + 10, y + 17, C_BG); line(x + 19, y + 9, x + 10, y + 17, C_BG); x -= 6; }
  if (S.upd == UPD_NEW || S.upd == UPD_LOADING) { x -= 15; drawText(FT, x, y, "\xE2\x86\x91", C_BG, TX_FLAT); }
  hit(15, 0, 450, 50, [] { quickOpen(); }, nullptr, false);
}

void pathLine(const char* path) {
  drawText(FT, colX(0), TOP_Y, path, C_FG);
  if ((msNow() / 530) % 2 == 0) fillRect(colX(utf8Len(path)), TOP_Y + 4, CW, LH - 6, C_FG);
  redrawIn(530 - msNow() % 530);
}

void fkeys(std::initializer_list<FKey> keys) {
  int x = colX(0), y = rowY(13);
  HoloZ z(H_CHROME);
  for (const FKey& k : keys) {
    int x0 = x;
    int kw = textW(FT, k.k);
    holoShadow(x, y + 2, kw, LH - 4, H_CHROME - H_TEXT);
    holoWalls(x, y + 2, kw, LH - 4, C_FG, H_CHROME - H_TEXT, true);
    fillRect(x, y + 2, kw, LH - 4, C_FG);
    drawText(FT, x, y, k.k, C_BG, TX_FLAT);
    x += kw;
    drawText(FT, x, y, k.label, C_FG);
    x += textW(FT, k.label) + CW * 2;
    if (k.act) { Act a = k.act; hit(x0, y - 6, x - x0 - CW, LH + 12, [a] { sndKey(); a(); }, nullptr, false); }
  }
}
void fkeysBack() { fkeys({{"ESC", "Back", [] { uiBack(); }}}); }

void toast(const char* msg, uint32_t ms) {
  snprintf(toastMsg, sizeof(toastMsg), "%s", msg);
  toastUntil = msNow() + ms;
  dirty = true;
}

void meter(int x, int y, int w, int h, int pct, bool active) {
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  rect(x, y, w, h, active ? C_FG : C_DIM);
  int iw = (w - 6) * pct / 100;
  fillRect(x + 3, y + 3, iw, h - 6, active ? C_FG : C_DIM);
  // the empty part: light shade dots, like ░
  for (int yy = y + 4; yy < y + h - 4; yy += 3)
    for (int xx = x + 3 + iw + ((yy / 3) & 1) * 2; xx < x + w - 3; xx += 4) pset(xx, yy, C_DIM);
}

int wrapText(const char* s, int width, char lines[][64], int maxLines) {
  int n = 0;
  const char* p = s;
  while (*p && n < maxLines) {
    // take up to width characters, break at the last space
    const char* q = p; const char* lastSpace = nullptr; int k = 0;
    while (*q && *q != '\n' && k < width) { if (*q == ' ') lastSpace = q; utf8Next(q); k++; }
    const char* end = q;
    if (*q && *q != '\n' && k == width && lastSpace && lastSpace > p) end = lastSpace;
    size_t len = end - p; if (len > 63) len = 63;
    memcpy(lines[n], p, len); lines[n][len] = 0;
    n++;
    p = end;
    if (*p == '\n' || *p == ' ') p++;
  }
  return n;
}

void dosButton(int x, int y, int w, int h, const char* label, Act a, bool on, int f) {
  // HOLO: a button stands up from its panel; the chosen one stands higher
  float up = on ? H_RAISE * 1.4f : H_RAISE;
  HoloZ z(holoAt() + up);
  holoShadow(x, y, w, h, up);
  holoWalls(x, y, w, h, C_FG, up, on);
  if (on) { invBar(x, y, w, h); drawTextC(f, x + w / 2, y + (h - fontH(f)) / 2, label, C_BG, TX_FLAT); }
  else {
    rect(x, y, w, h, C_FG); rect(x + 1, y + 1, w - 2, h - 2, C_FG);
    if (cfg.glow) glowRect(x, y, w, h, C_FG, 3);
    drawTextC(f, x + w / 2, y + (h - fontH(f)) / 2, label, C_FG);
  }
  hit(x, y, w, h, [a] { sndKey(); if (a) a(); });
}

// ------------------------------------------------------------------ list
void ListView::clampScroll() {
  float mx = n * rowH - h; if (mx < 0) mx = 0;
  if (scroll < 0) { scroll = 0; vel = 0; }
  if (scroll > mx) { scroll = mx; vel = 0; }
}
void ListView::showSel() {
  if (sel < 0) sel = 0; if (sel >= n) sel = n - 1;
  if (sel < 0) return;
  if (sel * rowH < scroll) scroll = sel * rowH;
  if ((sel + 1) * rowH > scroll + h) scroll = (sel + 1) * rowH - h;
  clampScroll();
}
void ListView::tick() {
  if (!dragging && fabsf(vel) > 0.3f) {
    scroll += vel; vel *= 0.9f; clampScroll(); uiDirty(); redrawIn(30);
  }
}
void ListView::draw(std::function<void(int, int, int, int, int, bool)> row, std::function<void(int)> pick,
                    std::function<void(int, int)>) {
  clampScroll();
  uint32_t now = msNow();
  bool anim = cfg.anim && !cfg.saver;
  // the selection bar slides from row to row
  float target = sel * rowH;
  if (barY < 0 || !anim) barY = target;
  barY += (target - barY) * 0.38f;
  if (fabsf(target - barY) < 1.5f) barY = target;
  bool moving = barY != target;
  clipSet(x, y, w, h);
  if (moving) {
    int by = y + (int)(barY - scroll);
    glowRect(x + 2, by + 3, w - 4, rowH - 6, C_FG, 4);
    blendRect(x + 2, by + 3, w - 4, rowH - 6, C_FG, 20);
    redrawIn(16);
  }
  int first = (int)(scroll / rowH);
  for (int i = first; i < n; i++) {
    int ry = y + i * rowH - (int)scroll;
    if (ry >= y + h) break;
    // rows type themselves in, one after the other, when the list opens
    float p = anim ? ((int32_t)(now - revealAt) - (i - first) * 32) / 120.0f : 1;
    if (p <= 0) { redrawIn(16); continue; }
    if (p < 1) {
      int cw = (int)(w * p);
      int top = ry > y ? ry : y, bot = ry + rowH < y + h ? ry + rowH : y + h;
      clipSet(x, top, cw, bot - top);
      row(i, x, ry, w, rowH, i == sel && !moving);
      clipSet(x, y, w, h);
      fillRect(x + cw, ry + (rowH - LH) / 2 + 4, CW, LH - 8, C_FG);
      redrawIn(16);
      continue;
    }
    if (i == sel && !moving && holoLayersOn()) {
      // HOLO: the chosen row floats above the list
      HoloZ z(holoAt() + H_RAISE);
      holoBarUp = H_RAISE;                 // the row's bar (if it draws one) gets a shadow and sides
      row(i, x, ry, w, rowH, true);
      holoBarUp = 0;
      continue;
    }
    row(i, x, ry, w, rowH, i == sel && !moving);
  }
  clipReset();
  // scroll marks, like ^ and v on error.blue
  if (scroll > 0) drawText(FT, x + w - CW, y - 4, "\xE2\x96\xB2", C_DIM, TX_FLAT);
  if (scroll + h < n * rowH) drawText(FT, x + w - CW, y + h - LH + 4, "\xE2\x96\xBC", C_DIM, TX_FLAT);
  // rows are reached by tapping (handled in ev), not as focus hits
  (void)pick;
}
bool ListView::ev(const Ev& e, std::function<void(int)> pick) {
  if (e.t == EV_KEY) {
    if (e.key == K_NEXT) { if (sel < n - 1) sel++; showSel(); sndKey(); return true; }
    if (e.key == K_PREV) { if (sel > 0) sel--; showSel(); sndKey(); return true; }
    if (e.key == K_OK && sel >= 0 && sel < n) { sndKey(); pick(sel); return true; }
    return false;
  }
  bool inside = e.x0 >= x && e.x0 < x + w && e.y0 >= y && e.y0 < y + h;
  if (e.t == EV_DOWN && inside) { dragging = false; vel = 0; scroll0 = scroll; dragY0 = e.y; lastY = e.y; lastMove = msNow(); return true; }
  if (e.t == EV_MOVE && inside) {
    if (!dragging && abs(e.y - e.y0) < abs(e.x - e.x0)) return false;   // sideways: not mine
    dragging = true;
    uint32_t now = msNow();
    float d = (float)(lastY - e.y);
    if (now != lastMove) vel = vel * 0.5f + d * 0.5f * (16.0f / (now - lastMove + 1));
    lastY = e.y; lastMove = now;
    scroll = scroll0 + (dragY0 - e.y);
    clampScroll();
    uiDirty();
    return true;
  }
  if (e.t == EV_UP && dragging) { dragging = false; if (msNow() - lastMove > 80) vel = 0; redrawIn(20); return true; }
  if (e.t == EV_TAP && inside) {
    int i = (int)((e.y - y + scroll) / rowH);
    if (i >= 0 && i < n) { sel = i; sndKey(); pick(i); return true; }
  }
  if (e.t == EV_SWIPE && (e.dir == SW_UP || e.dir == SW_DOWN) && inside) return true;
  return false;
}

// ------------------------------------------------------------------ settings rows
static int sliderRow = -1;
static void sliderSet(Row& r, int x0, int w, int x) {
  int p = (x - x0) * 100 / w;
  if (p < 0) p = 0; if (p > 100) p = 100;
  if (r.setPct) r.setPct(p);
}
static const int SL_X = 230, SL_W = 150;      // where a slider's bar is (from the row's left edge)
void drawRows(ListView& L, std::vector<Row>& rows) {
  L.n = rows.size();
  L.draw([&](int i, int x, int y, int w, int h, bool sel) {
    Row& r = rows[i];
    int ty = y + (h - LH) / 2;
    if (r.kind == 4) {   // heading (with its icon)
      int hx = x + 6;
      if (r.icon) { icon(r.icon, hx, y + (h - 26) / 2 + 2, C_DIM); hx += 34; }
      drawText(FT, hx, ty, r.name, C_DIM, TX_FLAT);
      int lx = hx + textW(FT, r.name) + 8;
      hline(lx, y + h / 2, x + w - 12 - lx, C_DIM);
      return;
    }
    bool inv = sel && r.kind != 3;
    if (inv) invBar(x + 2, y + 3, w - 4, h - 6);
    uint16_t c = inv ? C_BG : C_FG;
    int mode = inv ? TX_FLAT : TX_GLOW;
    int nx = x + 12;
    if (r.icon) { icon(r.icon, nx, y + (h - 26) / 2, inv ? C_BG : C_FG); nx += 36; }
    drawText(FT, nx, ty, r.name, r.kind == 3 && !inv ? C_DIM : c, r.kind == 3 ? TX_FLAT : mode);
    char v[64] = "";
    if (r.val) r.val(v);
    {   // the value never runs into the name: cut it to the room that's left
      int room = (x + w - 12 - (nx + textW(FT, r.name) + 14)) / CW - (r.kind == 1 ? 4 : 0);
      if (room < 3) room = 3;
      if (r.kind != 2 && utf8Len(v) > room) { utf8Cut(v, room - 1); strcat(v, "\xE2\x80\xA6"); }
    }
    if (r.kind == 2) {
      int p = r.pct ? r.pct() : 0;
      int bx = x + SL_X, by = y + 14;
      if (inv) { rect(bx, by, SL_W, h - 28, C_BG); fillRect(bx + 3, by + 3, (SL_W - 6) * p / 100, h - 34, C_BG); }
      else meter(bx, by, SL_W, h - 28, p, true);
      char pc[8]; snprintf(pc, sizeof(pc), "%d%%", p);
      drawTextR(FT, x + w - 12, ty, pc, c, mode);
    } else if (r.kind == 1) {
      char t[72]; snprintf(t, sizeof(t), "< %s >", v);
      drawTextR(FT, x + w - 12, ty, t, c, mode);
    } else if (v[0]) {
      bool desc = r.icon && r.kind == 0;          // a section's short description: dim
      drawTextR(FT, x + w - 12, ty, v, r.kind == 3 ? c : (inv ? C_BG : desc ? C_DIM : C_FG), desc && !inv ? TX_FLAT : mode);
    }
    if (r.kind == 0 && !v[0]) drawTextR(FT, x + w - 12, ty, "\xE2\x96\xBA", c, mode);
  }, nullptr);
}
bool rowsEv(ListView& L, std::vector<Row>& rows, const Ev& e) {
  // sliders: drag on the bar
  if (e.t == EV_DOWN) {
    int i = (int)((e.y - L.y + L.scroll) / L.rowH);
    sliderRow = -1;
    if (i >= 0 && i < (int)rows.size() && rows[i].kind == 2 && e.x >= L.x + SL_X - 20 && e.x < L.x + SL_X + SL_W + 20 &&
        e.y >= L.y && e.y < L.y + L.h) {
      sliderRow = i; L.sel = i;
      sliderSet(rows[i], L.x + SL_X, SL_W, e.x); uiDirty();
      return true;
    }
  }
  if (sliderRow >= 0 && sliderRow < (int)rows.size()) {
    if (e.t == EV_MOVE) { sliderSet(rows[sliderRow], L.x + SL_X, SL_W, e.x); uiDirty(); return true; }
    if (e.t == EV_UP || e.t == EV_TAP) { if (e.t == EV_UP) { sliderRow = -1; cfgChanged(); } return true; }
  }
  if (e.t == EV_KEY && (e.key == K_NEXT || e.key == K_PREV)) {
    // skip headings and info rows
    int d = e.key == K_NEXT ? 1 : -1, i = L.sel;
    for (int k = 0; k < (int)rows.size(); k++) {
      i += d;
      if (i < 0 || i >= (int)rows.size()) return true;
      if (rows[i].kind != 4 && rows[i].kind != 3) { L.sel = i; L.showSel(); sndKey(); return true; }
    }
    return true;
  }
  if (e.t == EV_TAP && e.x0 >= L.x && e.x0 < L.x + L.w && e.y0 >= L.y && e.y0 < L.y + L.h) {
    int i = (int)((e.y - L.y + L.scroll) / L.rowH);    // a heading or an info line: nothing to pick
    if (i >= 0 && i < (int)rows.size() && (rows[i].kind == 3 || rows[i].kind == 4)) return true;
  }
  return L.ev(e, [&](int i) {
    Row& r = rows[i];
    if (r.kind == 3 || r.kind == 4) return;
    int d = 0;
    if (r.kind == 1) d = (e.t == EV_TAP && lastTapX < L.x + L.w / 2 + 40) ? -1 : 1;
    if (r.kind == 2) {
      int p = r.pct ? r.pct() : 0;
      if (e.t == EV_KEY) { p = p >= 100 ? 0 : p + 10; if (r.setPct) r.setPct(p); cfgChanged(); return; }
    }
    if (r.act) r.act(d);
    uiDirty();
  });
}

// ------------------------------------------------------------------ keyboard
struct KeyDef { const char* lab; const char* out; float w; };
#define K_SHIFT "\x01"
#define K_BS "\x02"
#define K_SYM "\x03"
#define K_ABC "\x04"
#define K_LANG "\x05"
#define K_DONE "\x06"
#define K_EYE "\x07"
static const KeyDef KB_EN[4][12] = {
  {{"q","q",1},{"w","w",1},{"e","e",1},{"r","r",1},{"t","t",1},{"y","y",1},{"u","u",1},{"i","i",1},{"o","o",1},{"p","p",1},{0,0,0}},
  {{"",0,.5f},{"a","a",1},{"s","s",1},{"d","d",1},{"f","f",1},{"g","g",1},{"h","h",1},{"j","j",1},{"k","k",1},{"l","l",1},{"",0,.5f},{0,0,0}},
  {{"\xE2\x86\x91",K_SHIFT,1.5f},{"z","z",1},{"x","x",1},{"c","c",1},{"v","v",1},{"b","b",1},{"n","n",1},{"m","m",1},{"\xE2\x86\x90",K_BS,1.5f},{0,0,0}},
  {{"123",K_SYM,1.5f},{"UA",K_LANG,1.2f},{",",",",1},{"",  " ",3.1f},{".",".",1},{"-","-",1},{"OK",K_DONE,1.7f},{0,0,0}},
};
static const KeyDef KB_SYM[4][12] = {
  {{"1","1",1},{"2","2",1},{"3","3",1},{"4","4",1},{"5","5",1},{"6","6",1},{"7","7",1},{"8","8",1},{"9","9",1},{"0","0",1},{0,0,0}},
  {{"@","@",1},{"#","#",1},{"$","$",1},{"%","%",1},{"&","&",1},{"*","*",1},{"-","-",1},{"+","+",1},{"(","(",1},{")",")",1},{0,0,0}},
  {{"!","!",1},{"?","?",1},{":",":",1},{";",";",1},{"'","'",1},{"\"","\"",1},{"/","/",1},{"_","_",1},{"=","=",1},{"\xE2\x86\x90",K_BS,1},{0,0,0}},
  {{"ABC",K_ABC,1.5f},{"\\","\\",1},{"<","<",1},{"",  " ",3.3f},{">",">",1},{"~","~",1},{"OK",K_DONE,1.7f},{0,0,0}},
};
static const KeyDef KB_UA[4][13] = {
  {{"й","й",1},{"ц","ц",1},{"у","у",1},{"к","к",1},{"е","е",1},{"н","н",1},{"г","г",1},{"ш","ш",1},{"щ","щ",1},{"з","з",1},{"х","х",1},{"ї","ї",1},{0,0,0}},
  {{"ф","ф",1},{"і","і",1},{"в","в",1},{"а","а",1},{"п","п",1},{"р","р",1},{"о","о",1},{"л","л",1},{"д","д",1},{"ж","ж",1},{"є","є",1},{0,0,0}},
  {{"\xE2\x86\x91",K_SHIFT,1.5f},{"я","я",1},{"ч","ч",1},{"с","с",1},{"м","м",1},{"и","и",1},{"т","т",1},{"ь","ь",1},{"б","б",1},{"ю","ю",1},{"\xE2\x86\x90",K_BS,1.5f},{0,0,0}},
  {{"123",K_SYM,1.5f},{"EN",K_LANG,1.3f},{"ґ","ґ",1},{"'","'",1},{"",  " ",3.2f},{".",".",1},{",",",",1},{"OK",K_DONE,2},{0,0,0}},
};
static uint32_t upperCp(uint32_t c) {
  if (c >= 'a' && c <= 'z') return c - 32;
  if (c >= 0x430 && c <= 0x44F) return c - 0x20;
  if (c >= 0x450 && c <= 0x45F) return c - 0x50;
  if (c == 0x491) return 0x490;
  return c;
}
static int putUtf8(char* o, uint32_t c) {
  if (c < 0x80) { o[0] = c; return 1; }
  if (c < 0x800) { o[0] = 0xC0 | (c >> 6); o[1] = 0x80 | (c & 63); return 2; }
  o[0] = 0xE0 | (c >> 12); o[1] = 0x80 | ((c >> 6) & 63); o[2] = 0x80 | (c & 63); return 3;
}

struct Keyboard : Overlay {
  char title[40], text[220];
  int maxLen; bool pass, show = false, shift = false, caps = false;
  int mode = 0;                       // 0 letters, 1 symbols
  std::function<void(const char*)> done;
  uint32_t flashAt = 0; int flashK = -1;
  Keyboard(const char* t, const char* init, int m, std::function<void(const char*)> d, bool p) : maxLen(m), pass(p), done(d) {
    snprintf(title, sizeof(title), "%s", t); snprintf(text, sizeof(text), "%s", init ? init : "");
  }
  bool dim() override { return false; }
  int kind() override { return 1; }
  void type(const char* out) {
    if (!out) return;
    if (!strcmp(out, K_SHIFT)) { if (shift && !caps) caps = true; else if (caps) { caps = false; shift = false; } else shift = true; return; }
    if (!strcmp(out, K_BS)) { int n = utf8Len(text); if (n) *(char*)utf8At(text, n - 1) = 0; return; }
    if (!strcmp(out, K_SYM)) { mode = 1; return; }
    if (!strcmp(out, K_ABC)) { mode = 0; return; }
    if (!strcmp(out, K_LANG)) { cfg.kbd = !cfg.kbd; cfgSave(); return; }
    if (!strcmp(out, K_EYE)) { show = !show; return; }
    if (!strcmp(out, K_DONE)) { finish(true); return; }
    if (utf8Len(text) >= maxLen) { snd(SND_THUD); return; }
    const char* p = out; uint32_t c = utf8Next(p);
    if (shift || caps) c = upperCp(c);
    char b[5]; int k = putUtf8(b, c); b[k] = 0;
    if (strlen(text) + k < sizeof(text)) strcat(text, b);
    if (shift && !caps) shift = false;
  }
  void finish(bool ok) {
    std::function<void(const char*)> d = done;
    char t[220]; strcpy(t, text);
    ovlClose(this);
    if (ok && d) d(t);
  }
  void draw() override {
    // the whole screen: title, the text, the keys
    gfxClear();
    holoBackdrop();
    statusBar("KEYBOARD");
    char path[48]; snprintf(path, sizeof(path), "%s", title);
    drawText(FT, colX(0), TOP_Y, path, C_FG);
    dosBox(15, 92, 450, 80, C_FG);
    // the text: last 26 characters, cursor after it
    char shown[220];
    if (pass && !show) { int n = utf8Len(text); for (int i = 0; i < n && i < 200; i++) shown[i] = '*'; shown[n < 200 ? n : 200] = 0; }
    else strcpy(shown, text);
    int n = utf8Len(shown);
    const char* from = utf8At(shown, n > 25 ? n - 25 : 0);
    int ex = drawText(FT, 34, 116, from, C_FG);
    if ((msNow() / 530) % 2 == 0) fillRect(ex + 1, 120, CW, LH - 6, C_FG);
    redrawIn(530 - msNow() % 530);
    char cnt[16]; snprintf(cnt, sizeof(cnt), "%d/%d", utf8Len(text), maxLen);
    drawTextR(FT, 465, TOP_Y, cnt, C_DIM, TX_FLAT);
    hit(15, 92, 450, 80, nullptr, [this] { text[0] = 0; uiDirty(); }, false);   // hold on the text = clear it
    // keys
    int top = 186, kh = 50, gap = 6;
    for (int r = 0; r < 4; r++) {
      const KeyDef* row = mode == 1 ? KB_SYM[r] : (cfg.kbd ? KB_UA[r] : KB_EN[r]);
      float tw = 0; int cnt2 = 0;
      for (; row[cnt2].lab; cnt2++) tw += row[cnt2].w;
      float unit = (450.0f - gap * (cnt2 - 1)) / tw;
      float x = 15;
      for (int k = 0; k < cnt2; k++) {
        const KeyDef& K = row[k];
        int w = (int)(K.w * unit);
        int y = top + r * (kh + gap);
        if (K.out) {
          int id = r * 16 + k;
          bool hot = (flashK == id && msNow() - flashAt < 140) ||
                     (!strcmp(K.out, K_SHIFT) && (shift || caps));
          char lab[16];
          if (K.out[0] > 7 && (shift || caps) && mode == 0) {
            const char* p = K.lab; uint32_t c = upperCp(utf8Next(p)); int q = putUtf8(lab, c); lab[q] = 0;
          } else snprintf(lab, sizeof(lab), "%s", K.out[0] == ' ' ? (cfg.kbd && mode == 0 ? "ПРОБІЛ" : "SPACE") : K.lab);
          if (!strcmp(K.out, K_LANG)) snprintf(lab, sizeof(lab), "%s", cfg.kbd ? "EN" : "UA");
          HoloZ kz(hot ? H_TEXT : H_RAISE);   // HOLO: keys stand up, the pressed one goes down
          if (!hot) { holoShadow((int)x, y, w, kh, H_RAISE); holoWalls((int)x, y, w, kh, C_DIM, H_RAISE, false); }
          if (hot) invBar((int)x, y, w, kh, false);
          else { rect((int)x, y, w, kh, C_DIM); rect((int)x + 1, y + 1, w - 2, kh - 2, mix(C_DIM, C_BG, 40)); }
          bool special = K.out[0] <= 7 && K.out[0] != ' ';
          drawTextC(FT, (int)x + w / 2, y + (kh - LH) / 2, lab, hot ? C_BG : (special ? C_DIM : C_FG), hot ? TX_FLAT : TX_GLOW);
          if (caps && !strcmp(K.out, K_SHIFT)) hline((int)x + 8, y + kh - 8, w - 16, C_BG);
          const char* out = K.out;
          hit((int)x, y - gap / 2, w, kh + gap, [this, out, id] { flashK = id; flashAt = msNow(); sndKey(); type(out); uiDirty(); redrawIn(150); },
              !strcmp(out, K_BS) ? Act([this] { text[0] = 0; uiDirty(); }) : Act(nullptr), false);
        }
        x += w + gap;
      }
    }
    if (pass) fkeys({{"ESC", "Cancel", [this] { finish(false); }}, {"\xE2\x86\xB5", "Done", [this] { finish(true); }}, {"S", show ? "Hide" : "Show", [this] { show = !show; }}});
    else fkeys({{"ESC", "Cancel", [this] { finish(false); }}, {"\xE2\x86\xB5", "Done", [this] { finish(true); }}});
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY) {
      if (e.key == K_BACK) finish(false);
      else if (e.key == K_OK) finish(true);
      return true;
    }
    return true;
  }
};
static Keyboard* kb() { for (auto* o : ovl) if (o->kind() == 1) return (Keyboard*)o; return nullptr; }
bool kbIsOpen() { return kb() != nullptr; }
void kbOpen(const char* title, const char* init, int maxLen, std::function<void(const char*)> done, bool password) {
  ovl.push_back(new Keyboard(title, init, maxLen, done, password));
  dirty = true;
}

// ------------------------------------------------------------------ dialog
struct Dialog : Overlay {
  char title[32]; char text[300];
  std::vector<std::string> btn;
  std::function<void(int)> cb;
  int sel = 0;
  uint32_t born = msNow();
  Dialog(const char* t, const char* x, std::vector<const char*> b, std::function<void(int)> c) : cb(c) {
    snprintf(title, sizeof(title), "%s", t); snprintf(text, sizeof(text), "%s", x ? x : "");
    for (auto s : b) btn.push_back(s);
  }
  void pick(int i) {
    std::function<void(int)> c = cb;
    ovlClose(this);
    if (c) c(i);
  }
  void draw() override {
    char lines[8][64];
    int nl = wrapText(text, 24, lines, 8);
    bool vert = btn.size() > 2;
    int bh = 50, bw = 380;
    int h = 72 + nl * LH + (vert ? (int)btn.size() * (bh + 8) : bh + 8) + 10;
    int w = 420, x = (SCR_W - w) / 2, y = (SCR_H - h) / 2;
    if (y < 56) y = 56;
    // a DOS window opening: the box grows out of the middle, with a little static
    float p = cfg.anim ? (msNow() - born) / 170.0f : 1;
    if (p < 1) {
      float e = 1 - (1 - p) * (1 - p);
      int ww = 24 + (int)((w - 24) * e), hh = 12 + (int)((h - 12) * e);
      int xx = SCR_W / 2 - ww / 2, yy = y + h / 2 - hh / 2;
      fillRect(xx, yy, ww, hh, C_BG);
      dosBox(xx, yy, ww, hh, C_FG);
      for (int i = 0; i < 6; i++) blendRect(xx, yy + rnd() % hh, ww, 1, C_FG, 4 + rnd() % 10);
      redrawIn(16);
      return;
    }
    if (holoLayersOn()) { holoShadow(x, y, w, h, holoAt() - H_TEXT); holoWalls(x, y, w, h, C_FG, 0.6f, true); }
    else {
      fillRect(x + 12, y + h, w, 10, C_BLACK);          // the DOS shadow
      fillRect(x + w, y + 12, 12, h, C_BLACK);
    }
    fillRect(x, y, w, h, C_BG);
    dosBox(x, y, w, h, C_FG);
    char t[40]; snprintf(t, sizeof(t), " %s ", title);
    int tw = textW(FT, t);
    fillRect(x + (w - tw) / 2, y, tw, 10, C_BG);
    drawTextC(FT, x + w / 2, y - 10, t, C_FG);
    for (int i = 0; i < nl; i++) drawTextC(FT, SCR_W / 2, y + 30 + i * LH, lines[i], C_FG);
    int by = y + 40 + nl * LH;
    if (vert) {
      for (size_t i = 0; i < btn.size(); i++) {
        int yy = by + i * (bh + 8);
        dosButton(x + (w - bw) / 2, yy, bw, bh, btn[i].c_str(), [this, i] { pick(i); }, (int)i == sel);
      }
    } else {
      int n = btn.size(), bw2 = (w - 60 - (n - 1) * 20) / (n ? n : 1);
      for (int i = 0; i < n; i++)
        dosButton(x + 30 + i * (bw2 + 20), by, bw2, bh, btn[i].c_str(), [this, i] { pick(i); }, i == sel);
    }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY) {
      int n = btn.size();
      if (e.key == K_NEXT) sel = (sel + 1) % n;
      else if (e.key == K_PREV) sel = (sel + n - 1) % n;
      else if (e.key == K_OK) pick(sel);
      else if (e.key == K_BACK) pick(-1);
      sndKey();
    }
    return true;
  }
};
void ask(const char* title, const char* text, std::vector<const char*> buttons, std::function<void(int)> cb) {
  ovl.push_back(new Dialog(title, text, buttons, cb));
  snd(SND_BEEP);
  dirty = true;
}
void powerMenu() {
  ask("POWER", "What now?", {"SCREEN OFF", "RESTART", "POWER OFF", "CANCEL"}, [](int i) {
    if (i == 0) uiSleep();
    else if (i == 1) { toast("RESTARTING..."); sysRestart(); }
    else if (i == 2) sysPowerOff();
  });
}

// ------------------------------------------------------------------ quick panel (swipe down from the top)
struct Quick : Overlay {
  int drag = -1;
  uint32_t born = msNow();
  int kind() override { return 2; }
  void setFrom(int which, int x) {
    int p = (x - 150) * 100 / 290; if (p < 0) p = 0; if (p > 100) p = 100;
    if (which == 0) { cfg.bright = p < 5 ? 5 : p; sysBrightness(uiBrightNow()); }
    else cfg.vol = p;
  }
  void draw() override {
    int x = 15, y = 52, w = 450, h = 372;
    // slides down from the status bar
    float p = cfg.anim ? (msNow() - born) / 200.0f : 1;
    if (p < 1) {
      float e = 1 - (1 - p) * (1 - p) * (1 - p);
      int hh = (int)(h * e);
      fillRect(x, y, w, hh, C_BG);
      dosBox(x, y, w, hh < 20 ? 20 : hh, C_FG);
      redrawIn(16);
      return;
    }
    if (holoLayersOn()) { holoShadow(x, y, w, h, holoAt() - H_TEXT); holoWalls(x, y, w, h, C_FG, 0.6f, true); }
    else fillRect(x + 10, y + h, w, 8, C_BLACK);
    fillRect(x, y, w, h, C_BG);
    dosBox(x, y, w, h, C_FG);
    fillRect(x + 160, y, 130, 10, C_BG);
    drawTextC(FT, 240, y - 10, " QUICK ", C_FG);
    // sliders
    const char* names[2] = {"LIGHT", "VOLUME"};
    for (int i = 0; i < 2; i++) {
      int yy = y + 26 + i * 64;
      drawText(FT, x + 20, yy + 8, names[i], C_FG);
      int p = i == 0 ? cfg.bright : cfg.vol;
      meter(150, yy + 6, 290, 36, p, true);
      int which = i;
      hit(140, yy - 4, 320, 56, [this, which] { setFrom(which, lastTapX); cfgChanged(); }, nullptr, false);
    }
    // switches
    char b1[20], b2[20], b3[20];
    snprintf(b1, sizeof(b1), "WIFI %s", cfg.wifiOn ? "ON" : "OFF");
    snprintf(b2, sizeof(b2), "SOUND %s", cfg.sounds || cfg.clicks ? "ON" : "OFF");
    snprintf(b3, sizeof(b3), "OFFLINE");
    int by = y + 160, bw = 132, bh = 50;
    dosButton(x + 18, by, bw, bh, b1, [] { cfg.wifiOn = !cfg.wifiOn; netWifi(cfg.wifiOn); cfgChanged(); }, cfg.wifiOn);
    dosButton(x + 18 + bw + 9, by, bw, bh, b2, [] { bool on = cfg.sounds || cfg.clicks; cfg.sounds = cfg.clicks = !on; cfgChanged(); }, cfg.sounds || cfg.clicks);
    dosButton(x + 18 + 2 * (bw + 9), by, bw, bh, b3, [] { cfg.offline = !cfg.offline; netOffline(cfg.offline); toast(cfg.offline ? "OFFLINE" : "ONLINE", 1200); cfgChanged(); }, cfg.offline);
    by += bh + 12;
    dosButton(x + 18, by, bw, bh, "SLEEP", [this] { ovlClose(this); uiSleep(); });
    dosButton(x + 18 + bw + 9, by, bw, bh, "SETUP", [this] { ovlClose(this); openProg("SETUP"); });
    dosButton(x + 18 + 2 * (bw + 9), by, bw, bh, "POWER", [this] { ovlClose(this); powerMenu(); });
    // state line
    char st[64];
    const char* nw = cfg.offline ? "OFFLINE" : S.net == NET_UP ? S.ip : "NO WIFI";
    if (S.battery) snprintf(st, sizeof(st), "%d%%%s  %s", S.batPct, S.charging ? "+" : "", nw);
    else snprintf(st, sizeof(st), "USB  %s", nw);
    drawTextC(FT, 240, y + h - 50, st, C_DIM, TX_FLAT);
    hit(0, y + h, SCR_W, SCR_H - y - h, [this] { ovlClose(this); }, nullptr, false);
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_DOWN && e.y > 70 && e.y < 210 && e.x > 140) { drag = e.y < 140 ? 0 : 1; setFrom(drag, e.x); uiDirty(); return true; }
    if (e.t == EV_MOVE && drag >= 0) { setFrom(drag, e.x); uiDirty(); return true; }
    if (e.t == EV_UP && drag >= 0) { drag = -1; cfgChanged(); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_UP) { ovlClose(this); return true; }
    if (e.t == EV_KEY && (e.key == K_BACK || e.key == K_OK)) { ovlClose(this); return true; }
    if (e.t == EV_KEY && (e.key == K_NEXT || e.key == K_PREV)) {
      int d = e.key == K_NEXT ? 10 : -10; int b = cfg.bright + d;
      cfg.bright = b < 5 ? 5 : b > 100 ? 100 : b; sysBrightness(uiBrightNow()); cfgChanged(); return true;
    }
    return true;
  }
};
void quickOpen() {
  for (auto* o : ovl) if (o->kind() == 2) return;
  ovl.push_back(new Quick());
  sndKey();
  dirty = true;
}

// ------------------------------------------------------------------ alarm
struct Ring : Overlay {
  char what[32]; uint32_t at, nextBeep = 0;
  Ring(const char* w) { snprintf(what, sizeof(what), "%s", w); at = msNow(); }
  int kind() override { return 3; }
  void draw() override {
    holoShadow(15, 90, 450, 300, holoAt() - H_TEXT);
    holoWalls(15, 90, 450, 300, C_FG, 0.6f, true);
    fillRect(15, 90, 450, 300, C_BG);
    dosBox(15, 90, 450, 300, C_FG);
    bool blink = (msNow() / 400) % 2;
    if (blink) invBar(40, 112, 400, 52);
    drawTextC(FB, 240, 116, what, blink ? C_BG : C_FG, blink ? TX_FLAT : TX_GLOW);
    struct tm t; char c[12] = "--:--";
    if (sysLocalTime(t)) fmtTime(c, t.tm_hour, t.tm_min, 0, false);
    drawTextC(FH, 240, 172, c, C_FG);
    dosButton(40, 300, 190, 60, "STOP", [this] { sndStop(); ovlClose(this); }, true);
    dosButton(250, 300, 190, 60, "+5 MIN", [this] {
      sndStop(); tmrEnd = msNow() + 300000; tmrRun = true; toast("AGAIN IN 5 MIN"); ovlClose(this);
    });
    redrawIn(100);
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY) { sndStop(); ovlClose(this); }
    return true;
  }
  void tick() {
    if ((int32_t)(msNow() - nextBeep) >= 0) { snd(SND_ALARM); nextBeep = msNow() + 1000; }
    if (msNow() - at > 120000) { sndStop(); ovlClose(this); }
  }
};
static Ring* ringing() { for (auto* o : ovl) if (o->kind() == 3) return (Ring*)o; return nullptr; }
void alarmRing(const char* what) {
  if (ringing()) return;
  uiWake();
  ovl.push_back(new Ring(what));
  dirty = true;
}

// ------------------------------------------------------------------ the loop
static uint32_t glitchNext = 0, fxUntil = 0, fxSeed = 0;
static int fxKind = -1;
static bool wasFx = false;
static void scheduleGlitch(uint32_t now) {
  static const uint32_t MEAN[4] = {0, 9000, 4000, 1500};
  int g = cfg.saver ? 0 : cfg.glitch;
  glitchNext = MEAN[g & 3] ? now + MEAN[g & 3] * (40 + rnd() % 120) / 100 : 0;
}

void uiGlitch(int kind, uint32_t ms) {
  if (!awake || cfg.saver) return;
  uint32_t now = msNow();
  if (kind < 0) {
    // which one: tears and colour splits most, the full signal loss now and then
    static const uint8_t K[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 9, 10, 11, 11, 12, 1, 8, 13};
    kind = K[rnd() % sizeof(K)];
  }
  fxKind = kind; fxSeed = rnd();
  fxUntil = now + (ms ? ms : kind == 0 ? 160 : kind == 9 ? 400 : kind == 13 ? 140 : 120 + rnd() % 260);
  if (kind == 7) garbleUntil = now + 160;
  if (cfg.sounds && kind != 7 && rnd() % 10 < 8) snd(SND_CRACKLE);
  dirty = true;
}

void uiInit() {
  gfxInit();
  gfxTheme(cfg.color, cfg.saver ? 0 : cfg.glow);
  lastAct = msNow();
  scheduleGlitch(msNow());
}
void uiStart() {
  uiTransition(0);
  while (!stack.empty()) { delete stack.back(); stack.pop_back(); }
  uiOpen(homeScreen());
}

void uiLoop() {
  uint32_t now = msNow();
  if (holoTick()) dirty = true;               // HOLO: the cube moved, the picture moves with it
  if (Screen* s0 = uiCurrent()) s0->tick();
  if (Ring* r = ringing()) r->tick();
  clockTick();
  appsLoop();
  Screen* s = uiCurrent();
  // tapping or a button: touch keeps it awake. Screen off after OFF_SECS (not with the keyboard or a dialog open)
  uint32_t offMs = OFF_SECS[cfg.offIdx] * 1000u;
  bool keep = (s && s->keepAwake()) || !ovl.empty();
  if (awake && offMs && !keep && !tDown) {
    uint32_t idle = now - lastAct;
    if (idle > offMs) uiSleep();
    else if (cfg.dim && offMs > 15000 && idle > offMs - 10000 && !dimmed) { dimmed = true; dirty = true; }
  }
  // the always-on clock: once a minute
  if (!awake && aodOn) { static uint32_t lastMin = 0; if (now - lastMin > 20000) { lastMin = now; dirty = true; } }
  // auto power off when asleep on battery
  if (!awake && cfg.autoOffIdx && S.battery && !S.usb && now - sleptAt > AUTO_OFF_MIN[cfg.autoOffIdx] * 60000u) sysPowerOff();
  // glitches, like ERROR-DOS
  if (awake && glitchNext && (int32_t)(now - glitchNext) >= 0) {
    scheduleGlitch(now);
    Screen* c = uiCurrent();
    bool calm = (c && (c->fullScreen() || c->noSwipeBack())) || kbIsOpen() || dimmed;
    if (!calm) uiGlitch(-1, 0);
  }
  if (!glitchNext && cfg.glitch && !cfg.saver) scheduleGlitch(now);
  // a message from the control page
  static uint32_t seenMsg = 0;
  if (S.msgAt && S.msgAt != seenMsg) {
    seenMsg = S.msgAt;
    uiWake();
    ask("MESSAGE", S.msgText, {"OK"}, nullptr);
    snd(SND_MSG);
  }
}

bool uiFrame() {
  uint32_t now = msNow();
  bool fx = fxKind >= 0 && (int32_t)(fxUntil - now) > 0;
  if (!fx && fxKind >= 0) { fxKind = -1; dirty = true; }
  if (nextDraw && (int32_t)(now - nextDraw) >= 0) { nextDraw = 0; dirty = true; }
  if (toastUntil && (int32_t)(now - toastUntil) >= 0) { toastUntil = 0; dirty = true; }
  bool inTrans = transDir >= 0;
  if (!dirty && !fx && !inTrans) return false;
  dirty = false;
  if (!awake) {
    if (!aodOn) return false;
    Screen* a = aodScreen();
    gfxClearTo(C_BLACK);
    a->draw();
    delete a;
    return true;
  }
  bool calm = cfg.saver;
  gfxTheme(cfg.color, calm ? 0 : cfg.glow);
  fxLensSetup(calm ? 0 : cfg.lens);
  { Screen* s0 = uiCurrent(); holoFrameStart(s0 && s0->fullScreen()); }
  // a transition: the cube turns from the last picture to the new one
  float tt = inTrans ? (now - transAt) / 440.0f : 0;
  if (inTrans && tt >= 1) { transDir = -1; inTrans = false; }
  if (!inTrans || !transTo) {
    hits.clear();
    gfxClear();
    Screen* s = uiCurrent();
    Keyboard* k = kb();
    if (k) { holoSet(H_TEXT); k->draw(); }
    else {
      holoBackdrop();                                  // HOLO: the far wall
      if (s) {
        if (!s->fullScreen()) statusBar(s->name());
        holoSet(H_TEXT);
        s->draw();
      }
      for (Overlay* o : ovl) {
        if (o->dim()) {
          // shade what's under the dialog, so it stands out (like the dark layer on error.blue)
          for (int i = 0; i < SCR_W * SCR_H; i++) { uint16_t v = fb[i]; fb[i] = ((v & 0xE79C) >> 2); }
          hits.clear();
        }
        holoSet(holoOverH());                          // HOLO: dialogs float in front
        o->draw();
      }
    }
    holoSet(H_TEXT);
    // the focused button (when the hardware buttons are used to move around)
    if (focusIdx >= 0 && focusIdx < (int)hits.size() && (int32_t)(focusShowUntil - now) > 0) {
      const Hit& h = hits[focusIdx];
      if ((now / 300) % 3) { rect(h.x - 4, h.y - 4, h.w + 8, h.h + 8, C_FG); rect(h.x - 3, h.y - 3, h.w + 6, h.h + 6, C_FG); }
      redrawIn(300);
    }
    if (pressHit >= 0 && pressHit < (int)hits.size() && tDown) {
      const Hit& h = hits[pressHit];
      blendRect(h.x, h.y, h.w, h.h, C_FG, 8);
    }
    if (toastUntil) {
      // slides up from the bottom
      uint32_t age = 2600 - ((int32_t)(toastUntil - now) > 2600 ? 2600 : toastUntil - now);
      int w = textW(FT, toastMsg) + 30, x = (SCR_W - w) / 2, y = 382 + (age < 160 && cfg.anim ? (160 - age) / 3 : 0);
      HoloZ tz(holoOverH());
      if (holoLayersOn()) { holoShadow(x, y, w, 40, holoAt() - H_TEXT); holoWalls(x, y, w, 40, C_FG, 0.5f, true); }
      else fillRect(x + 6, y + 6, w, 40, C_BLACK);
      invBar(x, y, w, 40);
      drawTextC(FT, SCR_W / 2, y + 4, toastMsg, C_BG, TX_FLAT);
      if (age < 160) redrawIn(16);
    }
    // a ring where you tapped
    if (ripAt && now - ripAt < 340) {
      float p = (now - ripAt) / 340.0f, r = 10 + 70 * p;
      int a = (int)(26 * (1 - p)), n = 40;
      for (int i = 0; i < n; i++) {
        float a0 = i * 6.2832f / n, a1 = (i + 1) * 6.2832f / n;
        glowLine(ripX + cosf(a0) * r, ripY + sinf(a0) * r, ripX + cosf(a1) * r, ripY + sinf(a1) * r, C_FG, a);
      }
      redrawIn(16);
    }
    if (inTrans) { memcpy(fxBuf(1), fb, SCR_W * SCR_H * 2); transTo = true; }
  }
  gfxOffset(0, 0);
  if (inTrans) {
    float e = tt < 0.5f ? 4 * tt * tt * tt : 1 - powf(-2 * tt + 2, 3) / 2;    // ease in and out
    fxCube(fxBuf(0), fxBuf(1), e, transDir, fb);
    redrawIn(1);
  } else if (fx) {
    uint32_t t = now - (fxUntil - 400);
    switch (fxKind) {
      case 0: fxShift(0, (int)((now - (fxUntil - 160)) * 3) % 200 - 100); break;   // roll
      case 1: case 2: fxTear(1 + fxSeed % 3, fxSeed + (now / 60)); break;
      case 3: fxShift((now / 35) % 2 ? 2 : -2, 0); break;                          // jitter
      case 4: fxBlock(fxSeed); break;
      case 5: fxStatic(26, fxSeed + now / 50); break;
      case 6: fxInvertBand(fxSeed); break;
      case 8: fxRgbSplit(3 + (int)(fxSeed % 5)); fxTear(1, fxSeed + now / 50); break;   // colour split
      case 9: fxVhs(fxSeed, (t % 400) / 400.0f); break;                                // VHS tracking band
      case 10: fxInterlace((now / 40) % 2 ? 4 : -4); break;
      case 11: fxDatamosh(fxSeed + now / 70); break;
      case 12: fxStretch(fxSeed + (now / 90) * 7919); break;
      case 13: fxSignalLoss(fxSeed + now); break;
    }
    redrawIn(40);
    wasFx = true;
  }
  holoGlass();                                         // HOLO: the glass moves the other way
  fxPost(cfg.fringe && !calm, cfg.scan);
  lastPre = fxSpare();
  return true;
}
