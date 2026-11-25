// menu3d.cpp: the ERROR-DOS menu in 3D: the programs stand as glass cards on a turning carousel over a
// synthwave floor under the stars, each with its own wireframe icon. The camera follows how you hold the cube.
// Swipe sideways (or - / +) to turn, tap the front card (or hold +) to run it, swipe up / down for the next folder.
#include "../ui/ui.h"
#include "../core/holo.h"
#include "../core/g3d.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <algorithm>

void icon3d(const char* id, float t, uint16_t col, int a32);

static const char* TABS3[4] = {"APPS", "WEB", "ONLINE", "SYSTEM"};
static const int TABF3[4] = {F_APPS, F_WEB, F_ONLINE, F_SYSTEM};
static const float STEP = 0.62f;       // radians between cards
static const float RAD = 470, ZC = 860;  // carousel radius, its middle's distance

struct Home3D : Screen {
  struct Item { std::string name, desc, id; int prog; };
  std::vector<Item> items;
  int tab = 0, webState = -1;
  float pos = 0, vel = 0, target = 0, pos0 = 0;
  bool dragging = false, snapping = true;
  int dragX = 0, lastX = 0; uint32_t lastMoveAt = 0;
  float tx = 0, ty = 0;                // smoothed tilt
  int selT[4] = {0, 0, 0, 0};
  uint32_t t0 = msNow(), lastFrame = 0;
  const char* name() override { return "ERROR-DOS"; }

  void fill() {
    items.clear();
    if (TABF3[tab] == F_WEB) {
      static AppInfo* a = (AppInfo*)malloc(sizeof(AppInfo) * 32);
      int n = a ? netApps(a, 32) : 0;
      webState = n;
      for (int i = 0; i < n; i++) items.push_back({a[i].name, a[i].info, a[i].id, -1});
    } else {
      for (size_t i = 0; i < PROGS.size(); i++) if (PROGS[i].folder == TABF3[tab]) items.push_back({PROGS[i].dos, PROGS[i].desc, "", (int)i});
    }
    if (target > (int)items.size() - 1) target = items.size() ? items.size() - 1 : 0;
  }
  void enter() override { netReportProg("ERROR-DOS"); fill(); target = pos = selT[tab]; if (pos >= (float)items.size()) target = pos = 0; }
  int cur() { int i = (int)roundf(pos); return i < 0 ? 0 : i >= (int)items.size() ? (int)items.size() - 1 : i; }
  void setTab(int t, int dir) {
    if (t < 0 || t > 3 || t == tab) return;
    selT[tab] = (int)target;
    uiTransition(dir);
    tab = t;
    if (TABF3[tab] == F_WEB) netAppsRefresh();
    fill();
    pos = target = selT[t] < (int)items.size() ? selT[t] : 0; vel = 0;
    snd(SND_MOVE);
    uiDirty();
  }
  void go(int d) {
    int n = items.size(); if (!n) return;
    float t = roundf(target) + d;
    if (t < 0 || t > n - 1) { snd(SND_THUD); vel = d * 0.06f; return; }
    target = t; snapping = true; snd(SND_MOVE);
  }
  void run() {
    if (items.empty()) return;
    int i = cur();
    selT[tab] = i;
    if (items[i].prog >= 0) openProg(PROGS[items[i].prog].dos);
    else { snd(SND_FLOPPY); uiOpen(makeWebApp(items[i].id.c_str(), items[i].name.c_str())); }
  }

  void tick() override {
    uint32_t now = msNow();
    float dt = lastFrame ? (now - lastFrame) / 1000.0f : 0.03f; lastFrame = now;
    if (dt > 0.1f) dt = 0.1f;
    int n = items.size();
    if (!dragging) {
      if (fabsf(vel) > 0.004f && !snapping) {
        pos += vel; vel *= 0.9f;
        if (pos < -0.3f) { pos = -0.3f; vel = 0; } if (pos > n - 0.7f) { pos = n - 0.7f; vel = 0; }
        if (fabsf(vel) < 0.02f) { target = roundf(pos); snapping = true; }
      } else {
        if (target < 0) target = 0; if (n && target > n - 1) target = n - 1;
        pos += (target - pos) * (1 - powf(0.0008f, dt));   // ease to the card
        if (fabsf(target - pos) < 0.001f) pos = target;
      }
    }
    // how you hold the cube: the camera leans a little (smoothed, so it doesn't shake)
    if (holoActive()) {
      // HOLO 3D on: the camera follows the same tilt as the rest of the interface (range, centre, invert...)
      float k = cfg.holoDepth / 9.0f;
      tx = -holoTX() * 0.55f * k; ty = -holoTY() * 0.55f * k;
    } else if (cfg.parallax && S.imu && !cfg.saver) {
      float a[3], g[3];
      if (imuRead(a, g)) { tx += (a[0] / 9.81f - tx) * 0.12f; ty += (a[2] / 9.81f - ty) * 0.12f; }
    } else { tx *= 0.9f; ty *= 0.9f; }
    if (TABF3[tab] == F_WEB) {
      static AppInfo* a = (AppInfo*)malloc(sizeof(AppInfo) * 32);
      if (a && netApps(a, 32) != webState) fill();
    }
    redrawIn(30);                                  // about 30 frames a second
  }

  void draw() override {
    uint32_t now = msNow();
    float t = (now - t0) / 1000.0f;
    bool calm = cfg.saver;
    // ------------------------------------------------ the sky: stars, a striped sun on the horizon
    float yaw = tx * 0.22f, pitch = 0.1f - ty * 0.15f;
    g3dCam(0, -30, 0, yaw, pitch, -tx * 0.05f, 430, 240, 244);
    float hx, hy, hz;
    g3dProj(v3(sinf(yaw) * 6000, 170, cosf(yaw) * 6000), hx, hy, hz);
    int horizon = (int)hy;
    uint32_t sr = 12345;
    for (int i = 0; i < 70; i++) {
      sr = sr * 1103515245 + 12345; int x = (sr >> 8) % 480;
      sr = sr * 1103515245 + 12345; int y = 50 + (sr >> 8) % 200;
      int sx = x - (int)(tx * 30), sy = y + (int)(ty * 20);
      if (sy >= horizon - 4) continue;
      int tw = (int)(10 + 9 * sinf(t * (1 + (i % 5) * 0.4f) + i));
      pblend(((sx % 480) + 480) % 480, sy, C_FG, tw);
    }
    // the sun: a half disc of stripes, thinner toward the bottom
    int sunR = 120, scx = 240 - (int)(tx * 60);
    for (int y = horizon - sunR; y < horizon; y++) {
      int dy = horizon - y, band = (dy * 10) / sunR;
      if ((y + (int)(t * 12)) % 12 < 11 - band) {
        int w = (int)sqrtf((float)(sunR * sunR - dy * dy));
        blendRect(scx - w, y, 2 * w, 1, C_FG, 5 + dy * 9 / sunR);
      }
    }
    glowLine(0, horizon, 480, horizon, C_FG, 18);
    // ------------------------------------------------ the floor: a grid rolling toward you
    g3dObj(v3(0, 0, 0), 0, 0, 0, 1);
    g3dFog(150, 2300);
    float sc = fmodf(t * 90, 140);
    for (int i = 0; i < 16; i++) { float z = 2340 - i * 140 + sc; if (z > 150) l3(-1500, 170, z, 1500, 170, z, C_DIM, 26); }
    for (int i = -10; i <= 10; i++) l3(i * 150, 170, 150, i * 150, 170, 2300, C_DIM, 22);
    // ------------------------------------------------ the cards, the far ones first
    int n = items.size();
    std::vector<int> order;
    for (int i = 0; i < n; i++) if (fabsf((i - pos) * STEP) < 1.7f) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return fabsf(a - pos) > fabsf(b - pos); });
    g3dFog(300, 1400);
    float frontL = 1e9, frontR = -1e9, frontT = 1e9, frontB = -1e9;
    for (int i : order) {
      float a = (i - pos) * STEP, d = fabsf(i - pos);
      V3 c = v3(RAD * sinf(a), 0, ZC - RAD * cosf(a));
      bool front = d < 0.5f;
      float bob = calm ? 0 : sinf(t * 1.3f + i) * 6;
      g3dObj(v3(c.x, c.y + bob, c.z), a, 0, 0, 1);
      const float W = 98, H = 118;
      uint16_t col = front ? C_FG : C_DIM;
      int al = front ? 32 : 26;
      quad3(v3(-W, -H, 0), v3(W, -H, 0), v3(W, H, 0), v3(-W, H, 0), C_BG, front ? 28 : 22);
      // double border, like the ERROR-DOS boxes
      for (int k = 0; k < 2; k++) {
        float w = W - k * 9, h = H - k * 9;
        l3(-w, -h, 0, w, -h, 0, col, al); l3(w, -h, 0, w, h, 0, col, al); l3(w, h, 0, -w, h, 0, col, al); l3(-w, h, 0, -w, -h, 0, col, al);
      }
      // the front card: corner marks that pulse
      if (front) {
        float m = 14 + 4 * sinf(t * 5), w = W + 10, h = H + 10;
        l3(-w, -h, 0, -w + m, -h, 0, C_FG, 32); l3(-w, -h, 0, -w, -h + m, 0, C_FG, 32);
        l3(w, -h, 0, w - m, -h, 0, C_FG, 32); l3(w, -h, 0, w, -h + m, 0, C_FG, 32);
        l3(-w, h, 0, -w + m, h, 0, C_FG, 32); l3(-w, h, 0, -w, h - m, 0, C_FG, 32);
        l3(w, h, 0, w - m, h, 0, C_FG, 32); l3(w, h, 0, w, h - m, 0, C_FG, 32);
        for (int k = 0; k < 4; k++) {
          float sx, sy, z; V3 p = g3dWorld(v3(k & 1 ? W : -W, k & 2 ? H : -H, 0));
          if (g3dProj(p, sx, sy, z)) { frontL = fminf(frontL, sx); frontR = fmaxf(frontR, sx); frontT = fminf(frontT, sy); frontB = fmaxf(frontB, sy); }
        }
      }
      // the program's icon, standing out in front of the card
      const Item& it = items[i];
      const char* id = it.prog >= 0 ? PROGS[it.prog].dos : "WEB";
      float spin = front ? sinf(t * 0.9f) * 0.6f : t * 0.4f + i;
      V3 ip = g3dWorld(v3(0, -18, -46));
      g3dObj(ip, a + spin, front ? 0.25f * sinf(t * 0.7f) : 0.2f, 0, front ? 62 : 54);
      icon3d(id, t, col, front ? 32 : 24);
    }
    g3dFog(0, 0);
    // ------------------------------------------------ folder tabs, the name of the front card
    for (int k = 0; k < 4; k++) {
      int x = 15 + k * 114, w = 108;
      if (k == tab) invBar(x, 56, w, 38); else { blendRect(x, 56, w, 38, C_BG, 20); rect(x, 56, w, 38, C_DIM); }
      drawTextC(FT, x + w / 2, 59, TABS3[k], k == tab ? C_BG : C_DIM, TX_FLAT);
      hit(x, 50, w, 48, [this, k] { setTab(k, k > tab ? 0 : 1); }, nullptr, false);
    }
    if (!n) {
      const char* m = TABF3[tab] == F_WEB ? (webState == -1 ? "LOADING THE LIST..." : S.net != NET_UP ? "NO WIFI" : "NO WEB APPS YET") : "EMPTY";
      drawTextC(FT, 240, 230, m, C_DIM, TX_FLAT);
      return;
    }
    const Item& it = items[cur()];
    float nearness = 1 - fminf(1, fabsf(pos - cur()) * 2.5f);
    if (nearness > 0.05f) {
      char nm[32]; snprintf(nm, sizeof(nm), "%s", it.name.c_str()); utf8Cut(nm, 20);
      int w = textW(FB, nm) + 28;
      blendRect(240 - w / 2, 374, w, 44, C_BG, (int)(28 * nearness));
      drawTextC(FB, 240, 375, nm, C_FG, TX_GLOW);
      char d[48]; snprintf(d, sizeof(d), "%s", it.desc.c_str()); utf8Cut(d, 28);
      drawTextC(FT, 240, 420, d, mix(C_BG, C_DIM, (int)(100 * nearness)), TX_FLAT);
    }
    // where we are: dots
    for (int k = 0; k < n && n < 24; k++) {
      int x = 240 + (k - (n - 1) / 2.0f) * 12;
      if (k == cur()) fillRect(x - 3, 456, 7, 7, C_FG); else fillRect(x - 1, 458, 3, 3, C_DIM);
    }
    // touching the cards
    if (frontR > frontL) hit((int)frontL, (int)frontT, (int)(frontR - frontL), (int)(frontB - frontT), [this] { run(); }, nullptr, true);
  }

  bool ev(const Ev& e) override {
    uint32_t now = msNow();
    if (e.t == EV_DOWN && e.y > 100) { dragging = false; pos0 = pos; dragX = e.x; lastX = e.x; lastMoveAt = now; vel = 0; return true; }
    if (e.t == EV_MOVE && e.y0 > 100 && abs(e.x - e.x0) > abs(e.y - e.y0)) {
      dragging = true; snapping = false;
      float d = (e.x - lastX) / 190.0f;
      if (now != lastMoveAt) vel = vel * 0.4f - d * 0.6f;
      lastX = e.x; lastMoveAt = now;
      pos = pos0 - (e.x - dragX) / 190.0f;
      return true;
    }
    if (e.t == EV_UP && dragging) {
      dragging = false;
      if (now - lastMoveAt > 90) vel = 0;
      if (fabsf(vel) < 0.02f) { target = roundf(pos); snapping = true; }
      int c = cur(); if (c != (int)selT[tab]) { selT[tab] = c; snd(SND_MOVE); }
      return true;
    }
    if (e.t == EV_SWIPE) {
      if (e.dir == SW_UP) { setTab((tab + 1) % 4, 0); return true; }
      if (e.dir == SW_DOWN) { setTab((tab + 3) % 4, 1); return true; }
      return true;                                   // sideways swipes turn the carousel (handled above)
    }
    if (e.t == EV_TAP && e.y > 100) {
      if (e.x < 120) { go(-1); return true; }
      if (e.x > 360) { go(1); return true; }
      if (e.y > 360) { run(); return true; }
      return true;
    }
    if (e.t == EV_KEY) {
      if (e.key == K_NEXT) go(1);
      else if (e.key == K_PREV) go(-1);
      else if (e.key == K_OK) run();
      else if (e.key == K_BACK) setTab((tab + 1) % 4, 0);
      return true;
    }
    return false;
  }
};
Screen* makeHome3D() { return new Home3D(); }
