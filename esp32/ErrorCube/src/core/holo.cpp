// holo.cpp: see holo.h
#include "holo.h"
#include "gfx.h"
#include "fx.h"
#include "sys.h"
#include "img.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

bool uiAwake();

// ------------------------------------------------------------------ motion: how the cube is held
// GYRO+GRAVITY: the gyro turns an orientation (quaternion) right away, gravity pulls its tilt back (no drift);
// "straight" is another orientation that slowly follows (auto centre). GRAVITY: only the up direction.
struct Q { float w, x, y, z; };
static Q qmul(const Q& a, const Q& b) {
  return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
          a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}
static void qnorm(Q& q) { float l = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z); if (l < 1e-9f) { q = {1, 0, 0, 0}; return; } q.w /= l; q.x /= l; q.y /= l; q.z /= l; }
static void upInBody(const Q& q, float v[3]) {   // the world's up, seen from the cube
  v[0] = 2 * (q.x * q.z - q.w * q.y); v[1] = 2 * (q.y * q.z + q.w * q.x); v[2] = 1 - 2 * (q.x * q.x + q.y * q.y);
}
static Q qFromUp(const float a[3]) {              // an orientation whose up is a (any turn round it)
  float c[3] = {a[1], -a[0], 0};                  // a x z
  float d = a[2];
  if (d < -0.9999f) return {0, 1, 0, 0};
  Q q = {1 + d, c[0], c[1], c[2]}; qnorm(q); return q;
}

static Q qA = {1, 0, 0, 0}, qN = {1, 0, 0, 0};   // the cube now, and "straight"
static float gE[3] = {0, 0, 1}, g0[3] = {0, 0, 1};   // GRAVITY mode: up now, up when straight
static float bias[3] = {0, 0, 0};                 // the gyro's zero (learnt while it lies still)
static bool have = false, wantCentre = true;
static float tX = 0, tY = 0;        // the tilt, -1..1
static uint32_t lastMs = 0;
static int sigX = 0, sigY = 0;      // the tilt the last picture was drawn with (in 1/64 steps)
static bool wasActive = false, frameFlat = false;
static float curH = 0;
static uint32_t fpsT0 = 0; static int fpsN = 0, fpsNow = 0;

static const float FOCUS_H[5] = {H_BACK, H_PANEL, H_TEXT, H_CHROME, 2.0f};
static const float CENTRE_S[4] = {0, 8, 3, 1};          // auto centre: OFF, SLOW, MEDIUM, FAST (seconds)

static void norm(float v[3]) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l < 1e-6f) { v[0] = v[1] = 0; v[2] = 1; return; } v[0] /= l; v[1] /= l; v[2] /= l; }

bool holoActive() { return cfg.holo && !cfg.saver && uiAwake(); }
bool holoLayersOn() { return holoActive() && cfg.holoLayers && !frameFlat; }
float holoTX() { return tX; }
float holoTY() { return tY; }
float holoOverH() { return 1.0f + cfg.holoPop / 50.0f; }
void holoRecenter() { wantCentre = true; }
void holoInit() { have = false; wantCentre = true; tX = tY = 0; }
int holoFps() { return fpsNow; }

bool holoTick() {
  uint32_t now = msNow();
  float dt = lastMs ? (now - lastMs) / 1000.0f : 0.016f;
  lastMs = now;
  if (dt > 0.2f) dt = 0.2f;
  if (dt <= 0) return false;
  bool act = holoActive();
  if (!act) {
    have = false;
    if (wasActive) { wasActive = false; tX = tY = 0; sigX = sigY = 0; return true; }
    return false;
  }
  if (!wasActive) { wasActive = true; wantCentre = true; }
  float a[3], g[3];
  if (!imuRead(a, g)) return false;
  float an[3] = {a[0], a[1], a[2]};
  float aLen = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]) / 9.81f;
  norm(an);
  if (!have) { memcpy(gE, an, sizeof(gE)); qA = qFromUp(an); have = true; }
  int ci = cfg.holoCentre & 3;
  float bc = ci ? dt / (CENTRE_S[ci] + dt) : 0;
  float rv[3];                                     // how the cube turned from "straight" (radians, in the cube's axes)
  if (cfg.holoSensor == 0) {
    float w[3] = {(g[0] - bias[0]) * 0.017453f, (g[1] - bias[1]) * 0.017453f, (g[2] - bias[2]) * 0.017453f};
    // lying still: learn the gyro's zero
    float wm = fabsf(g[0] - bias[0]) + fabsf(g[1] - bias[1]) + fabsf(g[2] - bias[2]);
    if (wm < 3.0f && fabsf(aLen - 1) < 0.05f) for (int i = 0; i < 3; i++) bias[i] += (g[i] - bias[i]) * 0.02f;
    // gravity pulls the tilt back (less while shaking)
    float v[3]; upInBody(qA, v);
    float e[3] = {an[1] * v[2] - an[2] * v[1], an[2] * v[0] - an[0] * v[2], an[0] * v[1] - an[1] * v[0]};
    float kp = fabsf(aLen - 1) < 0.15f ? 1.2f : 0.2f;
    for (int i = 0; i < 3; i++) w[i] += kp * e[i];
    Q dq = {0, w[0] * 0.5f * dt, w[1] * 0.5f * dt, w[2] * 0.5f * dt};
    Q d = qmul(qA, dq);
    qA.w += d.w; qA.x += d.x; qA.y += d.y; qA.z += d.z; qnorm(qA);
    if (wantCentre) { qN = qA; wantCentre = false; }
    if (bc > 0) {
      float s = qN.w * qA.w + qN.x * qA.x + qN.y * qA.y + qN.z * qA.z, k = s < 0 ? -1.0f : 1.0f;
      qN.w += (k * qA.w - qN.w) * bc; qN.x += (k * qA.x - qN.x) * bc; qN.y += (k * qA.y - qN.y) * bc; qN.z += (k * qA.z - qN.z) * bc;
      qnorm(qN);
    }
    Q dd = qmul({qN.w, -qN.x, -qN.y, -qN.z}, qA);
    if (dd.w < 0) { dd.w = -dd.w; dd.x = -dd.x; dd.y = -dd.y; dd.z = -dd.z; }
    rv[0] = 2 * dd.x; rv[1] = 2 * dd.y; rv[2] = 2 * dd.z;
  } else {
    memcpy(gE, an, sizeof(gE));
    if (wantCentre) { memcpy(g0, gE, sizeof(g0)); wantCentre = false; }
    if (bc > 0) { for (int i = 0; i < 3; i++) g0[i] += (gE[i] - g0[i]) * bc; norm(g0); }
    // up turns the other way than the cube: the cube's turn is -(g0 x gE)
    rv[0] = -(g0[1] * gE[2] - g0[2] * gE[1]); rv[1] = -(g0[2] * gE[0] - g0[0] * gE[2]); rv[2] = 0;
  }
  float ax = -rv[1] * 57.2958f;     // turning round the screen's up-down axis: left / right
  float ay = rv[0] * 57.2958f;      // round the left-right axis: up / down
  float dead = cfg.holoDead, range = cfg.holoRange < 5 ? 5 : cfg.holoRange;
  ax = fabsf(ax) <= dead ? 0 : (ax > 0 ? ax - dead : ax + dead);
  ay = fabsf(ay) <= dead ? 0 : (ay > 0 ? ay - dead : ay + dead);
  float vx = ax / range, vy = ay / range;
  // a soft end: it slows down near the edge instead of stopping hard
  vx = vx / sqrtf(1 + vx * vx * 0.6f) * 1.26f; vy = vy / sqrtf(1 + vy * vy * 0.6f) * 1.26f;
  if (vx > 1) vx = 1; if (vx < -1) vx = -1; if (vy > 1) vy = 1; if (vy < -1) vy = -1;
  if (cfg.holoInvX) vx = -vx;
  if (cfg.holoInvY) vy = -vy;
  float tau = cfg.holoSmooth / 100.0f * 0.4f;
  float k = dt / (tau + dt);
  tX += (vx - tX) * k; tY += (vy - tY) * k;
  int sx = (int)lroundf(tX * 64), sy = (int)lroundf(tY * 64);
  if (sx != sigX || sy != sigY) { sigX = sx; sigY = sy; return true; }
  return false;
}

// ------------------------------------------------------------------ depth layers
void holoShift(float h, int& dx, int& dy) {
  if (!holoLayersOn()) { dx = dy = 0; return; }
  float u = cfg.holoDepth, f = FOCUS_H[cfg.holoFocus > 4 ? 2 : cfg.holoFocus];
  dx = (int)lroundf(tX * (f - h) * u);
  dy = (int)lroundf(tY * (f - h) * u);
}
float holoSet(float h) {
  float p = curH;
  curH = h;
  int dx, dy; holoShift(h, dx, dy);
  gfxOffset(dx, dy);
  return p;
}
float holoAt() { return curH; }

void holoFrameStart(bool flat) {
  frameFlat = flat;
  holoSet(H_TEXT);
  uint32_t now = msNow();
  fpsN++;
  if (now - fpsT0 >= 1000) { fpsNow = fpsN * 1000 / (now - fpsT0 ? now - fpsT0 : 1); fpsN = 0; fpsT0 = now; }
}

void holoUnmap(int& x, int& y) {
  int dx, dy; holoShift(H_TEXT, dx, dy);
  x -= dx; y -= dy;
}

void holoShadow(int x, int y, int w, int h, float above) {
  if (!holoLayersOn() || !cfg.holoShadow || above <= 0) return;
  // light from the top left, above the cube: the shadow falls down-right, and swings when you tilt
  float u = cfg.holoDepth, L = 3 + u * 0.45f;
  int sx = (int)lroundf(above * (L * 0.55f - tX * u * 0.7f)), sy = (int)lroundf(above * (L * 0.75f - tY * u * 0.7f));
  float p = holoSet(curH - above);
  if (cfg.holoShadow == 1) {      // SOFT
    blendRect(x + sx - 3, y + sy - 3, w + 6, h + 6, C_BLACK, 5);
    blendRect(x + sx - 1, y + sy - 1, w + 2, h + 2, C_BLACK, 7);
    blendRect(x + sx + 1, y + sy + 1, w - 2, h - 2, C_BLACK, 9);
  } else blendRect(x + sx, y + sy, w, h, C_BLACK, 20);   // HARD
  holoSet(p);
}

void holoWalls(int x, int y, int w, int h, uint16_t c, float depth, bool filled) {
  int n = cfg.holoWalls;
  if (!holoLayersOn() || n <= 0 || depth == 0) return;
  float h0 = curH;
  int ax, ay, bx, by;
  holoShift(h0, ax, ay); holoShift(h0 - depth, bx, by);
  if (ax == bx && ay == by) return;               // straight on: the sides are hidden behind the front
  int steps = abs(ax - bx) > abs(ay - by) ? abs(ax - bx) : abs(ay - by);
  if (steps > n * 3) steps = n * 3;                // enough slices that the side looks solid
  for (int i = steps; i >= 1; i--) {               // from the back to the front
    holoSet(h0 - depth * i / steps);
    uint16_t cc = mix(C_BG, c, 22 + 30 * (steps - i) / steps);
    if (filled) fillRect(x, y, w, h, cc);
    else { rect(x, y, w, h, cc); rect(x + 1, y + 1, w - 2, h - 2, cc); }
  }
  holoSet(h0);
}

// the wallpaper picture: decoded once (a bit bigger than the screen, so HOLO can move it), darkened
static uint16_t* wall = nullptr;
static char wallFor[80] = "\x01"; static int wallLightFor = -1; static bool wallSd = false, wallOk = false;
static const int WM = 32, WW = SCR_W + 2 * WM;
static void wallLoad() {
  if (!strcmp(wallFor, cfg.wallPic) && wallLightFor == cfg.wallLight && wallSd == S.sd) return;
  snprintf(wallFor, sizeof(wallFor), "%s", cfg.wallPic); wallLightFor = cfg.wallLight; wallSd = S.sd; wallOk = false;
  if (!cfg.wallPic[0] || (!strncmp(cfg.wallPic, "/sd", 3) && !S.sd)) return;
#ifdef ARDUINO
  if (!wall) wall = (uint16_t*)heap_caps_malloc(WW * WW * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  if (!wall) wall = (uint16_t*)malloc(WW * WW * 2);
#endif
  if (!wall) return;
  wallOk = imgLoad(cfg.wallPic, wall, WW, WW, IMG_COVER);
  if (wallOk) { int a = cfg.wallLight * 32 / 100; for (int i = 0; i < WW * WW; i++) wall[i] = blend(0, wall[i], a); }
}
bool holoWallOk() { wallLoad(); return wallOk; }

void holoBackdrop() {
  // the wallpaper: also without HOLO (then it doesn't move); not behind full-screen programs
  if (frameFlat || cfg.saver || !cfg.holoBack) return;
  float p = holoSet(H_BACK);
  switch (cfg.holoBack) {
    case 5: {  // PICTURE
      wallLoad();
      if (!wallOk) break;
      int dx = gOX < -WM ? -WM : gOX > WM ? WM : gOX, dy = gOY < -WM ? -WM : gOY > WM ? WM : gOY;
      for (int y = 0; y < SCR_H; y++) memcpy(&fb[y * SCR_W], &wall[(y - dy + WM) * WW + (WM - dx)], SCR_W * 2);
      break;
    }
    case 1:   // DOTS
      for (int y = -24; y < SCR_H + 24; y += 30)
        for (int x = -24 + ((y / 30) & 1) * 15; x < SCR_W + 24; x += 30) blendRect(x, y, 2, 2, C_FG, 9);
      break;
    case 2:   // GRID
      for (int y = -40; y < SCR_H + 40; y += 40) blendRect(-40, y, SCR_W + 80, 1, C_FG, 4);
      for (int x = -40; x < SCR_W + 40; x += 40) blendRect(x, -40, 1, SCR_H + 80, C_FG, 4);
      break;
    case 3: {  // STARS: two depths, the far ones move more
      uint32_t s = 2024;
      for (int i = 0; i < 90; i++) {
        s = s * 1103515245 + 12345; int x = (s >> 8) % 540 - 30;
        s = s * 1103515245 + 12345; int y = (s >> 8) % 540 - 30;
        holoSet(i % 3 ? H_BACK : H_BACK * 1.8f);
        int a = 6 + (s >> 20) % 12;
        blendRect(x, y, 1 + (i % 7 == 0), 1 + (i % 7 == 0), C_FG, a);
      }
      break;
    }
    case 4: {  // ROOM: the walls of a box behind the glass, from the screen's corners to a far frame
      int fx, fy, bx, by;
      holoShift(H_CHROME, fx, fy); holoShift(H_BACK, bx, by);
      const int I = 34;     // the far frame, this far in from the edge
      gfxOffset(0, 0);
      int ox[4] = {0, SCR_W - 1, SCR_W - 1, 0}, oy[4] = {0, 0, SCR_H - 1, SCR_H - 1};
      int ix[4] = {I, SCR_W - 1 - I, SCR_W - 1 - I, I}, iy[4] = {I, I, SCR_H - 1 - I, SCR_H - 1 - I};
      for (int k = 0; k < 4; k++) {
        int x0 = ox[k] + fx, y0 = oy[k] + fy, x1 = ix[k] + bx, y1 = iy[k] + by;
        for (int t = 0; t <= 24; t++) {                     // dotted, fading toward the back
          int xx = x0 + (x1 - x0) * t / 24, yy = y0 + (y1 - y0) * t / 24;
          blendRect(xx - 1, yy - 1, 2, 2, C_FG, 14 - t / 3);
        }
      }
      holoSet(H_BACK);
      for (int k = 0; k < 4; k++) {
        int x0 = ix[k], y0 = iy[k], x1 = ix[(k + 1) & 3], y1 = iy[(k + 1) & 3];
        if (y0 == y1) blendRect(x0 < x1 ? x0 : x1, y0, abs(x1 - x0), 1, C_FG, 7);
        else blendRect(x0, y0 < y1 ? y0 : y1, 1, abs(y1 - y0), C_FG, 7);
      }
      // a few lines on the floor of the room
      for (int i = 1; i < 4; i++) blendRect(I, I + i * (SCR_H - 2 * I) / 4, SCR_W - 2 * I, 1, C_FG, 3);
      for (int i = 1; i < 4; i++) blendRect(I + i * (SCR_W - 2 * I) / 4, I, 1, SCR_H - 2 * I, C_FG, 3);
      break;
    }
  }
  holoSet(p);
}

// ------------------------------------------------------------------ the glass in front
void holoGlass() {
  if (!holoActive()) { fxHolo(0, 0, 0, 0, 0, 0, 0, 0, 0); return; }
  int L = cfg.holoLens;
  int lx = (int)lroundf(-tX * L), ly = (int)lroundf(-tY * L);       // the glass is in front: it moves the other way
  float P = cfg.holoPersp / 100.0f * 0.16f;
  int rb = cfg.holoRainbow;
  int rbx = (int)lroundf(tX * rb), rby = (int)lroundf(tY * rb);
  static const int GA[3] = {0, 6, 12};
  int ga = GA[cfg.holoGlare > 2 ? 2 : cfg.holoGlare];
  // the light's reflection slides across the glass against the tilt
  int gx = (int)(170 - tX * 260), gy = (int)(150 - tY * 260);
  fxHolo(lx, ly, -tX * P, -tY * P, rbx, rby, gx, gy, ga);
}
