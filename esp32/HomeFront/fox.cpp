// FOX: the same fox as public/thuisfront/dev/fox.js (keep them in step). The shape comes from fox_mesh.h, made from
// fox.js by tools/hf-foxmesh.mjs.
#include <Arduino.h>
#include <math.h>
#include "screen.h"
#include "fox.h"
#include "fox_mesh.h"

static const float PI2 = 6.2831853f, PIf = 3.14159265f;
static const char* const MOODS[] = { "CHILLING", "CURIOUS", "WALKING", "SITTING", "ZZZ...", "HUH?", "EEK!", "WHOA!", "HOLD ON!", "HELP!", "DIZZY...",
  "OOF!", "HELLO!", "WHEEE!", "UPSIDE DOWN!", "SNIFF SNIFF", "YAAAWN", "HMM?", "STALKING...", "BOING!", "ROLLING!", "FOUND IT!", "POUNCE!",
  "SCRITCH SCRATCH", "ROUND N ROUND", "COMFY", "TAIL! TAIL!", "TA-DA!", "PLAY DEAD", "PLAY WITH ME!", "ZOOMIES!", "MOUSING...", "MISSED!" };
static const int NMOODS = sizeof(MOODS) / sizeof(MOODS[0]);

static inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static inline float easef(float cur, float to, float rate, float dt) { return cur + (to - cur) * (1 - expf(-rate * dt)); }
static inline float wrapf(float a) { return a - PI2 * floorf(a / PI2 + 0.5f); }
static inline float easeA(float cur, float to, float rate, float dt) { return cur + wrapf(to - cur) * (1 - expf(-rate * dt)); }
static inline float headingTo(float dx, float dz) { return atan2f(-dz, dx); }
static inline float hyp2(float a, float b) { return sqrtf(a * a + b * b); }
static inline float hyp3(float a, float b, float c) { return sqrtf(a * a + b * b + c * c); }
float (*foxRnd)() = nullptr;
static inline float rnd() { return foxRnd ? foxRnd() : (esp_random() >> 8) / 16777216.0f; }
static inline int jround(float x) { return (int)floorf(x + 0.5f); }   // as Math.round

// ---------------- what the fox does (as fox.js: keep them in step) ----------------
static const float FOX_CEIL = FOX_BOX_H - 1.24f;     // its height when it hangs from the ceiling (upside down)
static const float XL = FOX_BOX_X - 0.55f, ZB = FOX_BOX_ZB + 0.45f, ZF = FOX_BOX_ZF - 0.45f;

void foxGo(FoxSim& S, float x, float z, int style) {
  S.gx = clampf(x, -XL, XL); S.gz = clampf(z, ZB, ZF); S.gm = 1;
  S.gs = style >= 1 && style <= 4 ? style : 1 + (int)floorf(rnd() * 4); S.gph = 0; S.gt = 0.8f + rnd() * 0.6f; S.gdur = 0;
  S.walking = false; S.sit = 0; S.sniff = 0; S.pause = 0; S.act = 0; S.rest = 0;
  if (S.nap > 0) { S.nap = 0; S.wake = 1.2f; }
}
void foxAct(FoxSim& S, int force) {
  int a = force;
  if (!(a >= 1 && a <= 9)) { a = 1 + (int)floorf(rnd() * 8); if (S.lastAct > 0 && a >= S.lastAct) a++; }
  S.act = S.lastAct = a; S.at = 0; S.aph = 0; S.adir = rnd() < 0.5f ? 1 : -1;
  S.walking = false; S.sit = 0; S.sniff = 0;
  float fx = cosf(S.hd), fz = -sinf(S.hd);
  if (a == 1) S.adir = sinf(S.hd) * (FOX_CAM[0] - S.px) + cosf(S.hd) * (FOX_CAM[2] - S.pz) >= 0 ? 1 : -1;
  if (a == 2) {
    float r = 0.22f, nx = S.adir * sinf(S.hd), nz = S.adir * cosf(S.hd);
    S.acx = clampf(S.px + nx * r, -XL + r, XL - r); S.acz = clampf(S.pz + nz * r, ZB + r, ZF - r);
    S.an = atan2f(S.pz - S.acz, S.px - S.acx); S.ar = r;
  }
  if (a == 7) S.an = 3 + floorf(rnd() * 2);
  if (a == 8) { S.acx = clampf(S.px + fx * 0.6f, -XL, XL); S.acz = clampf(S.pz + fz * 0.6f, ZB, ZF); }
  if (a == 9) S.yawn = 2.5f;
}
static bool actStep(FoxSim& S, float dt) {
  int a = S.act;
  float t = S.at;
  if (a == 1) { S.vx = 0; S.vz = 0; return t < 2.8f; }
  if (a == 2) {
    float w = 2.0f, an = S.an + S.adir * w * dt, r = S.ar;
    S.an = an;
    float qx = S.acx + cosf(an) * r, qz = S.acz + sinf(an) * r;
    S.vx = -sinf(an) * r * w * S.adir + (qx - S.px) * 5; S.vz = cosf(an) * r * w * S.adir + (qz - S.pz) * 5;
    if (t > 3 * PIf / w) { S.rest = 4 + rnd() * 5; S.vx = 0; S.vz = 0; return false; }
    return true;
  }
  if (a == 3) {
    if (t > 0.6f) S.aph = 1;
    float fx = cosf(S.hd), fz = -sinf(S.hd);
    S.vx = S.aph ? fx * 0.15f : 0; S.vz = S.aph ? fz * 0.15f : 0;
    if (t > 3.2f) { S.dizzy = fmaxf(S.dizzy, 1.7f); S.vx = 0; S.vz = 0; return false; }
    return true;
  }
  if (a == 4) {
    S.vx = 0; S.vz = 0;
    if (S.aph == 0 && t > 0.4f) { S.aph = 1; S.vy = 2.8f; S.hop = 0.001f; S.jumping = true; S.a0 = t; }
    else if (S.aph == 1 && t - S.a0 > 0.15f) { S.aph = 2; S.fp = 0; S.tada = 1.4f; return false; }
    return true;
  }
  if (a == 5) {
    S.vx = 0; S.vz = 0;
    S.aph = t < 0.5f ? 0 : t < 4 ? 1 : t < 4.8f ? 2 : 3;
    if (t > 5.2f) { S.tada = 1.3f; return false; }
    return true;
  }
  if (a == 6) {
    S.vx = 0; S.vz = 0;
    if (t > 1.9f) { if (rnd() < 0.5f) { S.act = 7; S.at = 0; S.an = 3; S.aph = 0; return true; } return false; }
    return true;
  }
  if (a == 7) {
    float ex = S.acx - S.px, ez = S.acz - S.pz, d = hyp2(ex, ez);
    if (S.aph == 0 || d < 0.25f) {
      if (S.an <= 0 || t > 7) { S.pause = 2; return false; }
      S.an--; S.aph = 1;
      for (int k = 0; k < 8; k++) { S.acx = (rnd() * 2 - 1) * XL; S.acz = ZB + rnd() * (ZF - ZB); if (hyp2(S.acx - S.px, S.acz - S.pz) >= 1.2f) break; }
      ex = S.acx - S.px; ez = S.acz - S.pz; d = hyp2(ex, ez);
    }
    if (d < 1e-6f) d = 1e-6f;
    S.vx = easef(S.vx, ex / d * 1.5f, 6, dt); S.vz = easef(S.vz, ez / d * 1.5f, 6, dt);
    return true;
  }
  if (a == 8) {
    S.vx = 0; S.vz = 0;
    if (S.aph == 0 && t > 1.4f) {
      float T = 0.56f; S.aph = 1; S.a0 = t;
      S.vx = (S.acx - S.px) / T; S.vz = (S.acz - S.pz) / T; S.vy = 9.8f * T / 2; S.hop = 0.001f; S.jumping = true; S.pounce = 1.4f;
    } else if (S.aph == 1 && t - S.a0 > 0.15f) { S.aph = 2; S.a0 = t; }
    else if (S.aph == 2 && t - S.a0 > 0.7f) { S.aph = 3; S.missed = 1.6f; return false; }
    return true;
  }
  return t < 1.6f;
}
static void goStep(FoxSim& S, float dt) {
  float ex = S.gx - S.px, ez = S.gz - S.pz, d = hyp2(ex, ez), ux = d > 1e-6f ? ex / d : 0, uz = d > 1e-6f ? ez / d : 0;
  S.gdur += dt;
  auto arrive = [&]() { S.gs = 0; S.gm = 0; S.found = 1.6f; S.pause = 2.5f; S.vx = 0; S.vz = 0; };
  if (S.gdur > 20) { S.gs = 0; S.gm = 0; return; }
  if (S.gph == 0) {
    S.vx *= expf(-8 * dt); S.vz *= expf(-8 * dt);
    S.gt -= dt;
    if (S.gt <= 0) { S.gph = 1; S.gt = 0; S.rollDir = rnd() < 0.5f ? 1 : -1; }
    return;
  }
  S.gt += dt;
  if (S.gs == 1) {
    if (d < 0.6f && S.pounce <= 0) {
      float T = 0.45f; S.vx = ex / T; S.vz = ez / T; S.vy = 4.9f * T * 0.95f; S.hop = 0.001f; S.jumping = true; S.pounce = 0.9f;
      S.gs = 0; S.gm = 0; S.found = 1.8f; S.pause = 2.5f;
      return;
    }
    bool moving = fmodf(S.gt, 1.3f) < 0.8f;
    float sp = moving ? 0.28f : 0;
    S.vx = easef(S.vx, ux * sp, 10, dt); S.vz = easef(S.vz, uz * sp, 10, dt);
  } else if (S.gs == 2) {
    if (d < 0.15f) { arrive(); return; }
    S.vx *= expf(-10 * dt); S.vz *= expf(-10 * dt);
    if (S.leap > 0) {
      S.leap -= dt;
      if (S.leap <= 0) {
        float step = fminf(d, 1.0f), T = 0.55f;
        S.vx = ux * step / T; S.vz = uz * step / T; S.vy = 9.8f * T / 2; S.hop = 0.001f; S.jumping = true; S.leap = 0; S.gt = 0;
      }
    } else if (S.gt > 0.25f) S.leap = 0.3f;
  } else if (S.gs == 3) {
    if (d < 0.15f) { arrive(); return; }
    float a = sinf(S.gt * 2.4f) * 0.7f;
    bool stop = fmodf(S.gt, 2.2f) > 1.7f;
    float sp = stop ? 0 : 0.3f, vx = ux * cosf(a) - uz * sinf(a), vz = ux * sinf(a) + uz * cosf(a);
    S.vx = easef(S.vx, vx * sp, 6, dt); S.vz = easef(S.vz, vz * sp, 6, dt);
  } else if (S.gs == 4) {
    if (d < 0.12f || S.gt > 8) { arrive(); S.bump = 0; return; }
    float sp = S.gt < 0.4f ? 0 : 0.8f;
    S.vx = easef(S.vx, ux * sp, 5, dt); S.vz = easef(S.vz, uz * sp, 5, dt);
  }
}

void foxStep(FoxSim& S, const float* fin, const float* win, float dt) {
  dt = clampf(dt, 0, 0.1f);
  S.t += dt;
  if (fin) S.sensor = true;
  bool gyro = fin && win;
  float f[3] = { 0, 1, 0 }, w[3] = { 0, 0, 0 };
  if (fin) for (int i = 0; i < 3; i++) { f[i] = fin[i]; w[i] = win ? win[i] : 0; }
  // which way is "up" in the box: the gyroscope turns it along, the accelerometer pulls it back slowly (when ~1 g)
  float* g = S.gl;
  float c[3] = { g[1] * w[2] - g[2] * w[1], g[2] * w[0] - g[0] * w[2], g[0] * w[1] - g[1] * w[0] };
  for (int i = 0; i < 3; i++) g[i] += c[i] * dt;
  float fm = hyp3(f[0], f[1], f[2]), trust = fabsf(fm - 1) < 0.25f ? 1 : 0.1f;
  for (int i = 0; i < 3; i++) g[i] = easef(g[i], f[i] / (fm > 0 ? fm : 1), (gyro ? 1.5f : 5) * trust, dt);
  float gn = hyp3(g[0], g[1], g[2]);
  if (!(gn > 0)) gn = 1;
  for (int i = 0; i < 3; i++) g[i] /= gn;
  float lin[3] = { f[0] - g[0], f[1] - g[1], f[2] - g[2] }, linM = hyp3(lin[0], lin[1], lin[2]), spin = hyp3(w[0], w[1], w[2]);
  float kick = linM > 0.25f ? (linM - 0.25f) * 2.5f / linM : 0;
  float A[3];
  for (int i = 0; i < 3; i++) A[i] = -9.8f * (g[i] + lin[i] * kick);
  // calm, sleep, waking up
  bool busy = linM > 0.06f || spin > 0.35f;
  if (busy) { if (S.nap > 0) { S.wake = 2; S.nap = 0; } S.still = 0; } else S.still += dt;
  if (S.sensor && S.still > 180 && S.nap <= 0 && S.on == 1) S.nap = 60 + rnd() * 40;
  if (S.nap > 0) { S.nap -= dt; if (S.nap <= 0) { S.yawn = 2.5f; S.still = 0; } }
  bool sleeping = S.nap > 0;
  S.dizzy = clampf(S.dizzy + (spin > 3 ? (spin - 3) * dt * 0.45f : 0) + (linM > 1 ? dt * 1.0f : 0) - dt * 0.22f, 0, 3);
  float* timers[] = { &S.startle, &S.wake, &S.hello, &S.bump, &S.yawn, &S.sit, &S.sniff, &S.found, &S.pounce, &S.rest, &S.tada, &S.missed };
  for (float* p : timers) *p = fmaxf(0, *p - dt);
  if (S.act) S.at += dt;
  if (linM > 0.6f && S.startle < 0.6f) { S.startle = 1.4f; if (S.nap > 0) { S.wake = 2; S.nap = 0; } if (S.on == 0) S.tbv += (rnd() - 0.5f) * 8; S.rest = 0; }
  // ---- moving: on the floor (or the ceiling) while pressed against it, else flying
  int surf = S.hop <= 0.0005f ? 1 : S.hop >= FOX_CEIL - 0.0005f ? -1 : 0;
  int wasOn = S.on;
  S.on = surf == 1 && A[1] < -2.5f ? 1 : surf == -1 && A[1] > 2.5f ? -1 : 0;
  float vx0 = S.vx, vy0 = S.vy, vz0 = S.vz;
  bool ownJump = (S.act == 4 || S.act == 8) && S.aph >= 1 && S.jumping;
  if (S.act && ((S.on != 1 && !ownJump) || S.slide || S.startle > 0.8f || S.gs > 0)) { S.act = 0; S.walking = false; }
  float dx = A[0] / 9.8f, dz = A[2] / 9.8f, slope = hyp2(dx, dz);
  float speed = hyp2(S.vx, S.vz);
  if (S.on) {
    float N = fabsf(A[1]), T = hyp2(A[0], A[2]);
    S.slide = T > 0.27f * N || (S.slide && speed > 0.2f && T > 0.1f * N);
    S.vy = 0;
    if (S.slide) {
      float fr = 0.18f * N, Tn = T > 0 ? T : 1;
      float ux = speed > 0.05f ? S.vx / speed : A[0] / Tn, uz = speed > 0.05f ? S.vz / speed : A[2] / Tn;
      S.vx += (A[0] - fr * ux) * dt; S.vz += (A[2] - fr * uz) * dt;
      S.walking = false;
    } else if (S.on == 1 && S.gs > 0 && S.dizzy < 1.5f && S.startle <= 0.8f) {
      goStep(S, dt);
    } else if (S.on == 1) {
      bool canWalk = S.dizzy < 1 && S.startle <= 0 && !sleeping && S.wake <= 0 && S.yawn <= 0 && S.hello <= 0;
      if (S.act) {
        if (S.slide || !actStep(S, dt)) { S.act = 0; if (S.pause < 1) S.pause = 1 + rnd() * 2; }
      } else if (S.walking) {
        float ex = S.tx - S.px, ez = S.tz - S.pz, d = hyp2(ex, ez);
        if (!canWalk || d < 0.12f) {
          S.walking = false; S.pause = 1 + rnd() * 2.5f;
          if (rnd() < 0.4f) S.sniff = 1.2f + rnd(); else if (rnd() < 0.25f) { S.sit = 4 + rnd() * 5; S.pause = S.sit + 0.5f; }
        } else { float sp = d > 0.5f ? 0.6f : 0.35f; S.vx = easef(S.vx, ex / d * sp, 4, dt); S.vz = easef(S.vz, ez / d * sp, 4, dt); }
      } else {
        S.pause -= dt;
        bool free = canWalk && S.pause <= 0 && S.sit <= 0 && S.rest <= 0 && S.tada <= 0 && S.missed <= 0;
        if (free && rnd() < 0.4f) foxAct(S, 0);
        else if (free) {
          S.walking = true;
          for (int k = 0; k < 8; k++) { S.tx = (rnd() * 2 - 1) * (XL - 0.1f); S.tz = ZB + 0.05f + rnd() * (ZF - ZB - 0.1f); if (hyp2(S.tx - S.px, S.tz - S.pz) >= 0.7f) break; }
        }
      }
      if (!S.walking && !S.act) { S.vx *= expf(-6 * dt); S.vz *= expf(-6 * dt); }
    } else { S.vx *= expf(-6 * dt); S.vz *= expf(-6 * dt); S.walking = false; }
  } else {
    S.vx += A[0] * dt; S.vy += A[1] * dt; S.vz += A[2] * dt;
    S.walking = false; S.slide = false;
  }
  S.px += S.vx * dt; S.pz += S.vz * dt; S.hop += S.vy * dt;
  // the box: walls, floor and ceiling; it bounces (and lands when the hit is soft and it is pressed that way)
  float hit = 0;
  auto bounce = [&](float v) { hit = fmaxf(hit, fabsf(v)); return fabsf(v) < 0.3f ? 0.0f : -v * 0.45f; };
  if (S.px < -XL) { S.px = -XL; if (S.vx < 0) S.vx = bounce(S.vx); }
  if (S.px > XL) { S.px = XL; if (S.vx > 0) S.vx = bounce(S.vx); }
  if (S.pz < ZB) { S.pz = ZB; if (S.vz < 0) S.vz = bounce(S.vz); }
  if (S.pz > ZF) { S.pz = ZF; if (S.vz > 0) S.vz = bounce(S.vz); }
  if (S.hop < 0) {
    S.hop = 0;
    if (S.vy < 0) { if ((-S.vy < 1.6f || S.jumping) && A[1] < -2.5f) { hit = fmaxf(hit, S.jumping ? 0 : -S.vy * 0.5f); S.vy = 0; } else S.vy = bounce(S.vy); }
    S.jumping = false;
  }
  if (S.hop > FOX_CEIL) { S.hop = FOX_CEIL; if (S.vy > 0) { if (S.vy < 1.6f && A[1] > 2.5f) { hit = fmaxf(hit, S.vy * 0.5f); S.vy = 0; } else S.vy = bounce(S.vy); } }
  if (hit > 2 && S.bump <= 0) { S.bump = 0.7f; S.dizzy = fminf(3, S.dizzy + hit * 0.1f); S.tbv += (rnd() - 0.5f) * hit * 2.5f; }
  speed = hyp2(S.vx, S.vz);
  // how it is turned: feet on its floor (or ceiling); flying it spins, and turns its feet down like a cat
  float left[3] = { sinf(S.hd), 0, cosf(S.hd) }, am = hyp3(A[0], A[1], A[2]);
  if (!(am > 0)) am = 1;
  if (S.on && S.gs == 4 && S.gph == 1) {
    S.tb = wrapf(S.tb + S.rollDir * (hyp2(S.vx, S.vz) / 0.28f) * dt); S.tbv = 0;
  } else if (S.on) {
    float to = S.on == 1 ? (S.act == 5 && S.aph >= 1 && S.aph <= 2 ? S.adir * PIf * 0.5f : 0) : PIf;
    if (!wasOn && fabsf(wrapf(S.tb - to)) > 1.2f && !S.jumping) S.dizzy = fminf(3, S.dizzy + 0.8f);
    S.tb = easeA(S.tb, to, S.gs == 4 ? 5 : S.act == 5 ? 6 : 8, dt); S.tbv = 0;
  } else {
    float cat = atan2f(-(A[0] * left[0] + A[2] * left[2]) / am, -A[1] / am);
    S.tbv *= expf(-0.7f * dt); S.tb = wrapf(S.tb + S.tbv * dt);
    S.tb = easeA(S.tb, cat, 3 * fmaxf(0, 1 - fabsf(S.tbv) / 5), dt);
  }
  // which way it faces
  float hdTo = S.hd, toGoal = headingTo(S.gx - S.px, S.gz - S.pz);
  if (S.slide) hdTo = headingTo(-dx, -dz);
  else if (S.gs > 0 && S.on == 1) hdTo = S.gs == 4 && S.gph == 1 ? toGoal + (S.rollDir > 0 ? -PIf / 2 : PIf / 2) : S.gph == 0 ? S.hd : toGoal;
  else if ((S.walking || S.act == 2 || S.act == 7) && speed > 0.1f) hdTo = headingTo(S.vx, S.vz);
  else if (S.act == 8 && S.aph == 0) hdTo = headingTo(S.acx - S.px, S.acz - S.pz);
  S.hd -= w[1] * dt * 0.85f;
  if (S.act == 3 && S.aph == 1 && S.on == 1) { S.hd += S.adir * 6.5f * dt; hdTo = S.hd; }
  if (S.on) S.hd = easeA(S.hd, hdTo, S.slide ? 3 : S.act == 7 ? 8 : (S.walking || S.gs > 0 || S.act == 2) ? 5 : 1.5f, dt);
  S.hd = wrapf(S.hd);
  float dtn = dt > 0 ? dt : 1, hdv = wrapf(S.hd - S.hdPrev) / dtn, hda = (hdv - S.hdv) / dtn;
  S.hdPrev = S.hd; S.hdv = hdv;
  // leaning against the slope
  float fwx = cosf(S.hd), fwz = -sinf(S.hd), along = dx * fwx + dz * fwz, side = dx * fwz - dz * fwx;
  S.pt = easef(S.pt, S.on == 1 ? clampf(-along * 1.3f, -0.5f, 0.5f) : 0, 6, dt);
  float rlAct = S.act == 1 ? S.adir * 0.12f : S.act == 5 && S.aph == 0 ? sinf(S.at * 14) * 0.25f : 0;
  S.rl = easef(S.rl, (S.on == 1 ? clampf(side * 1.1f, -0.5f, 0.5f) : 0) + rlAct + (S.dizzy > 1 ? sinf(S.t * 3) * 0.12f : 0), 6, dt);
  // sitting, lying, sleeping, crouching
  bool rolling = S.gs == 4 && S.gph == 1, deadLie = S.act == 5 && S.aph >= 1 && S.aph <= 2;
  S.dd = easef(S.dd, deadLie ? 1 : 0, 7, dt);
  float lfTo = (sleeping || rolling || (S.rest > 0 && S.on == 1)) ? 2 : ((S.sit > 0 || S.act == 1) && S.on == 1 && !S.slide) ? 1 : 0;
  S.lf = easef(S.lf, lfTo, rolling ? 6 : lfTo > S.lf ? 1.5f : 5, dt);
  float crTo = S.gs > 0 && S.on == 1 ? (S.gph == 0 ? 0.35f : S.gs == 1 ? 0.85f : S.gs == 2 && S.leap > 0 ? 1 : S.gs == 3 ? 0.3f : 0)
             : S.act == 4 && S.aph <= 1 ? 1 : S.act == 8 && S.aph == 0 ? 0.5f : 0;
  S.cr = easef(S.cr, crTo, 7, dt);
  // the tricks' own joints
  S.sc = easef(S.sc, S.act == 1 && S.at > 0.5f && S.lf > 0.6f ? (float)S.adir : 0, 6, dt);
  float flyT = S.at - S.a0;
  if (S.act == 4 && S.aph == 1) { float u = clampf(flyT / 0.58f, 0, 1); S.fp = wrapf(PI2 * u * u * (3 - 2 * u)); }
  else if (S.act == 8 && S.aph == 1) S.fp = 0.6f - 2.0f * clampf(flyT / 0.56f, 0, 1);
  else if (S.act == 8 && S.aph == 2) S.fp = easef(S.fp, -1.0f, 12, dt);
  else S.fp = easeA(S.fp, 0, 6, dt);
  S.bw = easef(S.bw, S.act == 6 || (S.act == 9 && S.at < 1.4f) ? 1 : 0, 6, dt);
  // legs
  float waTo = !S.on ? (S.jumping ? (S.act == 4 ? 0 : 0.5f) : 0.9f) : S.slide ? 0.75f : (S.lf > 0.5f || S.bw > 0.3f) ? 0 : S.act == 3 && S.aph == 1 ? 0.6f
             : S.gs == 1 && S.gph == 1 ? clampf(speed * 2.2f, 0, 0.45f) : clampf(speed * 1.1f, 0, 0.6f);
  S.wa = easef(S.wa, waTo, 8, dt);
  S.wp = wrapf(S.wp + dt * ((S.slide || !S.on) ? 15 : S.act == 3 ? 16 : 3 + speed * 12));
  // head
  S.look -= dt;
  if (S.look <= 0) { S.look = 2 + rnd() * 4; S.lookY = rnd() < 0.55f ? 0 : (rnd() - 0.5f) * 1.6f; S.lookP = (rnd() - 0.5f) * 0.4f; }
  float toCam = clampf(wrapf(headingTo(FOX_CAM[0] - S.px, FOX_CAM[2] - S.pz) - S.hd), -1.1f, 1.1f);
  float hyTo = S.walking ? (S.lookY == 0 ? toCam * 0.5f : 0) : S.lookY == 0 ? toCam : S.lookY, hpTo = S.lookY == 0 ? 0.12f : S.lookP;
  if (S.sniff > 0 || (S.gs == 3 && S.gph == 1)) { hyTo = sinf(S.t * 5) * 0.2f; hpTo = -0.55f; }
  if (S.gs > 0 && (S.gph == 0 || S.gs == 1)) { hyTo = clampf(wrapf(toGoal - S.hd), -1.1f, 1.1f); hpTo = S.gph == 0 ? -0.1f : -0.2f; }
  if (S.found > 0 || S.tada > 0 || S.act == 6) { hyTo = toCam; hpTo = 0.25f; }
  if (S.missed > 0) { hyTo = toCam * 0.6f; hpTo = -0.15f; }
  if (S.act == 1) { hyTo = -S.adir * 0.8f; hpTo = -0.35f; }
  if (S.act == 3) { hyTo = S.adir * 1.1f; hpTo = -0.15f; }
  if (S.act == 8 && S.aph == 0) { hyTo = sinf(S.at * 2.2f) * 0.12f; hpTo = -0.4f; }
  if (S.act == 2 || S.act == 7) { hyTo = 0; hpTo = 0.05f; }
  if (S.slide || !S.on) { hyTo = 0; hpTo = 0.25f; }
  if (S.startle > 0 || S.wake > 0 || S.hello > 0) { hyTo = toCam; hpTo = 0.2f; }
  if (S.yawn > 0) { hyTo = toCam * 0.5f; hpTo = 0.45f; }
  if (S.lf > 1.2f) { hyTo = 0.5f; hpTo = -0.45f; }
  if (S.lf > 1.2f && S.rest > 0) { hyTo = toCam * 0.4f; hpTo = -0.35f; }
  if (deadLie) { hyTo = 0; hpTo = 0.1f; }
  if (S.dizzy > 1) hyTo += sinf(S.t * 2.6f) * 0.3f;
  S.hy = easef(S.hy, hyTo, 5, dt); S.hp = easef(S.hp, hpTo, 5, dt);
  // ears
  S.twitch -= dt;
  if (S.twitch <= 0) { S.twitch = 2 + rnd() * 5; S.twitchE = rnd() < 0.5f ? 1 : 2; }
  float earBase = ((S.gs > 0 && S.on == 1 && S.gs != 4) || (S.act == 8 && S.aph == 0) || S.act == 6) ? 0.15f : S.startle > 0 ? -0.15f
                : (S.act == 7 || S.slide || (!S.on && !S.jumping) || S.dizzy > 1 || S.bump > 0) ? -0.7f : (S.lf > 1.2f || deadLie) ? -0.45f : 0;
  float tw = S.twitch < 0.25f ? -0.5f : 0, scE = S.act == 1 ? -0.55f : 0;
  S.el = easef(S.el, earBase + (S.twitchE == 1 ? tw : 0) + (S.adir > 0 ? scE : 0), 14, dt);
  S.er = easef(S.er, earBase + (S.twitchE == 2 ? tw : 0) + (S.adir < 0 ? scE : 0), 14, dt);
  // the tail: a springy joint with its own weight, pulled by what it feels minus the body's own acceleration
  bool wag = S.found > 0 || S.hello > 0 || S.wake > 0 || S.tada > 0 || S.act == 6 || (S.act == 8 && S.aph == 2) || (S.gs == 3 && S.gph == 1);
  float taTo = (rolling || deadLie) ? 0 : S.act == 3 ? -S.adir * 1.4f : S.lf > 1.2f ? 1.5f : wag ? sinf(S.t * 12) * 0.5f
             : (S.gs == 1 || S.act == 8) ? 0 : S.walking ? sinf(S.wp) * 0.25f : sinf(S.t * 1.4f) * 0.3f;
  float tuTo = deadLie ? 0 : (S.act == 6 || (S.act == 8 && S.aph == 2)) ? 0.9f : ((S.gs == 1 && S.gph == 1) || (S.act == 8 && S.aph == 0)) ? -0.15f
             : S.startle > 0 ? 0.85f : S.slide ? 0.6f : (S.found > 0 || S.tada > 0) ? 0.5f : S.lf > 1.2f ? -0.25f : S.lf > 0.5f ? -0.1f : 0.15f;
  float F[3] = { A[0] - (S.vx - vx0) / dtn, A[1] - (S.vy - vy0) / dtn, A[2] - (S.vz - vz0) / dtn };
  float ct = cosf(S.tb), st = sinf(S.tb);
  float upW[3] = { st * left[0], ct, st * left[2] }, leftW[3] = { ct * left[0], -st, ct * left[2] };
  float fUp = F[0] * upW[0] + F[1] * upW[1] + F[2] * upW[2], fLeft = F[0] * leftW[0] + F[1] * leftW[1] + F[2] * leftW[2];
  S.tav += (40 * (taTo - S.ta) - 4 * S.tav + 0.6f * fLeft - 0.35f * clampf(hda, -60, 60)) * dt;
  S.tuv += (40 * (tuTo - S.tu) - 4 * S.tuv + 0.45f * (fUp + 9.8f)) * dt;
  S.ta = clampf(S.ta + S.tav * dt, -2, 2); S.tu = clampf(S.tu + S.tuv * dt, -0.9f, 1.3f);
  // eyes
  S.blink -= dt;
  if (S.blink <= 0) S.blink = 2 + rnd() * 3.5f;
  bool asleep = sleeping && S.lf > 1.5f;
  S.ey = (asleep || S.act == 1) ? 1 : (deadLie && S.aph == 1) ? 2 : S.dizzy > 1 ? 2 : S.startle > 0.6f ? 3 : (S.blink < 0.13f || S.yawn > 1.2f) ? 1 : 0;
  // the words in the top bar, and what floats above it
  bool flying = !S.on && hyp3(S.vx, S.vy, S.vz) > 1.2f;
  static const int GOM[5] = { 0, 18, 19, 15, 20 }, ACTM[10] = { 0, 23, 24, 26, 19, 28, 29, 30, 22, 16 };
  int goMood = S.gph == 0 ? 17 : (S.gs == 1 && S.pounce > 0) ? 22 : GOM[S.gs < 0 || S.gs > 4 ? 0 : S.gs];
  int actMood = S.act == 8 && S.aph == 0 ? 31 : ACTM[S.act < 0 || S.act > 9 ? 0 : S.act];
  S.md = (S.pounce > 0 && !S.on) ? 22 : (S.jumping && !S.on) ? 19 : flying ? 13 : !S.on ? 9 : S.bump > 0 ? 11 : S.startle > 0.4f ? 6 : S.dizzy > 1 ? 10
       : S.on == -1 ? 14 : S.slide ? (slope > 0.45f ? 7 : 8) : S.gs > 0 ? goMood : S.act ? actMood : S.found > 0 ? 21 : S.tada > 0 ? 27 : S.missed > 0 ? 32
       : S.wake > 0 ? 5 : S.yawn > 0 ? 16 : S.hello > 0 ? 12 : asleep ? 4 : (S.lf > 1.5f && S.rest > 0) ? 25 : S.lf > 0.6f ? 3 : S.sniff > 0 ? 15
       : S.walking ? 2 : S.lookY == 0 ? 1 : 0;
  S.ov = asleep ? 1 : (S.startle > 0.6f || S.wake > 0) ? 2 : S.dizzy > 1 ? 3 : (S.gs > 0 && S.gph == 0) ? 4 : 0;
}

void foxZeroOf(const float* z, FoxZero& Z) {
  Z = FoxZero();
  float l = sqrtf(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
  if (!(l > 0.3f)) return;
  float a[3] = { z[0] / l, z[1] / l, z[2] / l }, c = a[1];
  Z.on = true;
  for (int i = 0; i < 3; i++) Z.b[i] = z[3 + i];
  if (c > 0.99999f) return;
  if (c < -0.99999f) { float R[9] = { 1, 0, 0, 0, -1, 0, 0, 0, -1 }; memcpy(Z.R, R, sizeof(R)); return; }
  float v[3] = { -a[2], 0, a[0] }, k = (1 - c) / (1 - c * c);
  float K[9] = { 0, -v[2], v[1], v[2], 0, -v[0], -v[1], v[0], 0 };
  for (int r = 0; r < 3; r++)
    for (int q = 0; q < 3; q++) {
      float k2 = 0;
      for (int m = 0; m < 3; m++) k2 += K[r * 3 + m] * K[m * 3 + q];
      Z.R[r * 3 + q] = (r == q ? 1 : 0) + K[r * 3 + q] + k2 * k;
    }
}
void foxZeroApply(const FoxZero& Z, float* f, float* w) {
  if (!Z.on) return;
  float a[3] = { f[0], f[1], f[2] }, g[3] = { w[0] - Z.b[0], w[1] - Z.b[1], w[2] - Z.b[2] };
  for (int r = 0; r < 3; r++) {
    f[r] = Z.R[r * 3] * a[0] + Z.R[r * 3 + 1] * a[1] + Z.R[r * 3 + 2] * a[2];
    w[r] = Z.R[r * 3] * g[0] + Z.R[r * 3 + 1] * g[1] + Z.R[r * 3 + 2] * g[2];
  }
}

void foxPoseOf(const FoxSim& S, FoxPose& p) {
  p = { S.px, S.pz, S.hop, S.hd, S.rl, S.pt, S.tb, S.lf, S.hy, S.hp, S.el, S.er, S.ta, S.tu, S.wp, S.wa, S.ey, S.md, S.ov,
        S.cr, S.gx, S.gz, S.gm, S.sc, S.fp, S.bw, S.dd };
}

int foxJson(const FoxPose& p, char* out, int n) {
  return snprintf(out, n, "[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%.2f,%.2f]",
                  p.px, p.pz, p.hop, p.hd, p.rl, p.pt, p.tb, p.lf, p.hy, p.hp, p.el, p.er, p.ta, p.tu, p.wp, p.wa, p.ey, p.md, p.ov,
                  p.cr, p.gx, p.gz, p.gm, p.sc, p.fp, p.bw, p.dd);
}

// ---------------- 3x4 matrices ----------------
struct M34 { float m[12]; };
static M34 mT(float x, float y, float z) { return { { 1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z } }; }
static M34 mRx(float a) { float c = cosf(a), s = sinf(a); return { { 1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0 } }; }
static M34 mRy(float a) { float c = cosf(a), s = sinf(a); return { { c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0 } }; }
static M34 mRz(float a) { float c = cosf(a), s = sinf(a); return { { c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0 } }; }
static M34 mMul(const M34& a, const M34& b) {
  M34 o;
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 4; c++)
      o.m[r * 4 + c] = a.m[r * 4] * b.m[c] + a.m[r * 4 + 1] * b.m[4 + c] + a.m[r * 4 + 2] * b.m[8 + c] + (c == 3 ? a.m[r * 4 + 3] : 0);
  return o;
}
static void mAp(const M34& m, const float* p, float* o) {
  o[0] = m.m[0] * p[0] + m.m[1] * p[1] + m.m[2] * p[2] + m.m[3];
  o[1] = m.m[4] * p[0] + m.m[5] * p[1] + m.m[6] * p[2] + m.m[7];
  o[2] = m.m[8] * p[0] + m.m[9] * p[1] + m.m[10] * p[2] + m.m[11];
}

// ---------------- the triangles of one frame ----------------
struct WTri { float a[3], b[3], c[3]; uint8_t e; int8_t s; };
static const int MAXT = 360;
static WTri* tris = nullptr;
static int nt = 0;
static void addPart(const float (*v)[3], int nv, const FoxTri* t, int ntr, const M34& m, bool mirror = false) {
  static float wv[64][3];
  for (int i = 0; i < nv && i < 64; i++) mAp(m, v[i], wv[i]);
  for (int i = 0; i < ntr && nt < MAXT; i++) {
    WTri& o = tris[nt++];
    int a = t[i].a, b = t[i].b, c = t[i].c, e = t[i].e;
    if (mirror) { int x = b; b = c; c = x; e = ((e & 1) << 2) | (e & 2) | ((e >> 2) & 1); }
    memcpy(o.a, wv[a], 12); memcpy(o.b, wv[b], 12); memcpy(o.c, wv[c], 12);
    o.e = e; o.s = t[i].s;
  }
}
// the tail: rings along a bent line, made each frame (as tailTris + loft in fox.js)
static void addTail(const M34& mb, float ta, float tu) {
  const int NR = 5;
  float ring[NR][6][3], p[3] = { FOX_TAIL_PIVOT[0], FOX_TAIL_PIVOT[1], FOX_TAIL_PIVOT[2] }, yaw = 0, el = tu;
  for (int i = 0; i < NR; i++) {
    float sx = sinf(yaw), sz = cosf(yaw);
    for (int k = 0; k < 6; k++) {
      float a = 1.5707963f + k * 1.0471976f, hw = FOX_TAIL_R[i][0] * cosf(a), hh = FOX_TAIL_R[i][1] * sinf(a);
      ring[i][k][0] = p[0] + sx * hw; ring[i][k][1] = p[1] + hh; ring[i][k][2] = p[2] + sz * hw;
    }
    if (i < NR - 1) {
      yaw += ta * 0.35f; el += tu * 0.15f - 0.12f;
      float L = FOX_TAIL_L[i];
      p[0] -= cosf(el) * cosf(yaw) * L; p[1] += sinf(el) * L; p[2] += cosf(el) * sinf(yaw) * L;
    }
  }
  float w[NR * 6 + 1][3];
  for (int i = 0; i < NR; i++) for (int k = 0; k < 6; k++) mAp(mb, ring[i][k], w[i * 6 + k]);
  float tip[3] = { 0, 0, 0 };
  for (int k = 0; k < 6; k++) for (int j = 0; j < 3; j++) tip[j] += ring[NR - 1][k][j] / 6;
  mAp(mb, tip, w[NR * 6]);
  auto add = [&](int a, int b, int c, int e, int s, const float* inside) {   // turned to face outwards
    if (nt >= MAXT) return;
    float u[3], v[3], n[3], ctr[3];
    for (int j = 0; j < 3; j++) { u[j] = w[b][j] - w[a][j]; v[j] = w[c][j] - w[a][j]; ctr[j] = (w[a][j] + w[b][j] + w[c][j]) / 3 - inside[j]; }
    n[0] = u[1] * v[2] - u[2] * v[1]; n[1] = u[2] * v[0] - u[0] * v[2]; n[2] = u[0] * v[1] - u[1] * v[0];
    if (n[0] * ctr[0] + n[1] * ctr[1] + n[2] * ctr[2] < 0) { int x = b; b = c; c = x; e = ((e & 1) << 2) | (e & 2) | ((e >> 2) & 1); }
    WTri& o = tris[nt++];
    memcpy(o.a, w[a], 12); memcpy(o.b, w[b], 12); memcpy(o.c, w[c], 12); o.e = e; o.s = s;
  };
  for (int i = 0; i + 1 < NR; i++) {
    float in[3] = { 0, 0, 0 };
    for (int k = 0; k < 6; k++) for (int j = 0; j < 3; j++) in[j] += (w[i * 6 + k][j] + w[(i + 1) * 6 + k][j]) / 12;
    for (int k = 0; k < 6; k++) {
      int a = i * 6 + k, b = i * 6 + (k + 1) % 6, c = (i + 1) * 6 + (k + 1) % 6, d = (i + 1) * 6 + k;
      add(a, b, c, 3, FOX_TAIL_SHADE[i], in);
      add(a, c, d, 6, FOX_TAIL_SHADE[i], in);
    }
  }
  float in[3] = { 0, 0, 0 };
  for (int k = 0; k < 6; k++) for (int j = 0; j < 3; j++) in[j] += w[(NR - 2) * 6 + k][j] / 6;
  for (int k = 0; k < 6; k++) add((NR - 1) * 6 + k, (NR - 1) * 6 + (k + 1) % 6, NR * 6, 1, 4, in);
}

// ---------------- drawing ----------------
static void proj(const float* p, float* o) {
  float d[3] = { p[0] - FOX_CAM[0], p[1] - FOX_CAM[1], p[2] - FOX_CAM[2] };
  float z = d[0] * FOX_CF[0] + d[1] * FOX_CF[1] + d[2] * FOX_CF[2];
  o[0] = FOX_CX + FOX_FOC * (d[0] * FOX_CR[0] + d[1] * FOX_CR[1] + d[2] * FOX_CR[2]) / z;
  o[1] = FOX_CY - FOX_FOC * (d[0] * FOX_CU[0] + d[1] * FOX_CU[1] + d[2] * FOX_CU[2]) / z;
  o[2] = z;
}
static void tri(const float* a, const float* b, const float* c, uint8_t col) {
  float ymin = fminf(a[1], fminf(b[1], c[1])), ymax = fmaxf(a[1], fmaxf(b[1], c[1]));
  int y0 = max(0, (int)ceilf(ymin - 0.5f)), y1 = min(LCD_H - 1, (int)floorf(ymax - 0.5f));
  const float* E[3][2] = { { a, b }, { b, c }, { c, a } };
  for (int y = y0; y <= y1; y++) {
    float yc = y + 0.5f, xl = 1e9f, xr = -1e9f;
    for (int i = 0; i < 3; i++) {
      const float *p = E[i][0], *q = E[i][1];
      if ((p[1] <= yc && q[1] > yc) || (q[1] <= yc && p[1] > yc)) {
        float x = p[0] + (yc - p[1]) * (q[0] - p[0]) / (q[1] - p[1]);
        if (x < xl) xl = x;
        if (x > xr) xr = x;
      }
    }
    if (xl > xr) continue;
    int x0 = max(0, (int)ceilf(xl - 0.5f)), x1 = min(LCD_W - 1, (int)floorf(xr - 0.5f));
    if (x1 >= x0) memset(FB + y * LCD_W + x0, IX_T + col, x1 - x0 + 1);
  }
}
static void seg(const float* p, const float* q, uint8_t c) { scrLine(jround(p[0]), jround(p[1]), jround(q[0]), jround(q[1]), c); }
static void G(float x, float y, float z, float* o) { float p[3] = { x, y, z }; proj(p, o); }

struct DTri { float z, a[2], b[2], c[2]; uint8_t e, lvl; };

void scrFox(const FoxPose& P, float t, int pal) {
  if (!tris) tris = (WTri*)ps_malloc(sizeof(WTri) * MAXT);
  static DTri* list = nullptr;
  if (!list) list = (DTri*)ps_malloc(sizeof(DTri) * MAXT);
  scrTextRamp(pal);
  memset(FB, IX_T, LCD_W * LCD_H);
  if (!tris || !list) return;
  float A[3], B[3];
  // the box: a grid on the floor, a thin one on the ceiling, its edges
  for (int i = 0; i <= 12; i++) { float x = -FOX_BOX_X + i * FOX_BOX_X / 6; G(x, 0, FOX_BOX_ZB, A); G(x, 0, FOX_BOX_ZF, B); seg(A, B, IX_T + 3); }
  for (int i = 0; i <= 6; i++) { float z = FOX_BOX_ZB + i * (FOX_BOX_ZF - FOX_BOX_ZB) / 6; G(-FOX_BOX_X, 0, z, A); G(FOX_BOX_X, 0, z, B); seg(A, B, IX_T + 3); }
  for (int i = 1; i < 6; i++) { float x = -FOX_BOX_X + i * FOX_BOX_X / 3; G(x, FOX_BOX_H, FOX_BOX_ZB, A); G(x, FOX_BOX_H, FOX_BOX_ZF, B); seg(A, B, IX_T + 2); }
  for (int i = 1; i < 3; i++) { float z = FOX_BOX_ZB + i * (FOX_BOX_ZF - FOX_BOX_ZB) / 3; G(-FOX_BOX_X, FOX_BOX_H, z, A); G(FOX_BOX_X, FOX_BOX_H, z, B); seg(A, B, IX_T + 2); }
  for (int sd = -1; sd <= 1; sd += 2) {
    float x = sd * FOX_BOX_X;
    G(x, 0, FOX_BOX_ZB, A); G(x, FOX_BOX_H, FOX_BOX_ZB, B); seg(A, B, IX_T + 5);
    G(x, 0, FOX_BOX_ZF, A); G(x, 0, FOX_BOX_ZB, B); seg(A, B, IX_T + 6);
    G(x, FOX_BOX_H, FOX_BOX_ZF, A); G(x, FOX_BOX_H, FOX_BOX_ZB, B); seg(A, B, IX_T + 5);
    G(x, 0, FOX_BOX_ZF, A); G(x, FOX_BOX_H, FOX_BOX_ZF, B); seg(A, B, IX_T + 6);
  }
  {
    const float E[4][3] = { { FOX_BOX_H, FOX_BOX_ZB, 5 }, { 0, FOX_BOX_ZF, 6 }, { 0, FOX_BOX_ZB, 5 }, { FOX_BOX_H, FOX_BOX_ZF, 6 } };
    for (int i = 0; i < 4; i++) { G(-FOX_BOX_X, E[i][0], E[i][1], A); G(FOX_BOX_X, E[i][0], E[i][1], B); seg(A, B, IX_T + (int)E[i][2]); }
  }
  if (P.gm) {                                      // the tapped spot: a ring and a cross, pulsing
    float r = 0.13f + (scrEffects ? 0.04f * sinf(t * 8) : 0);
    G(P.gx + r, 0, P.gz, A);
    for (int i = 1; i <= 12; i++) { float a = i * PI2 / 12; G(P.gx + cosf(a) * r, 0, P.gz + sinf(a) * r, B); seg(A, B, IX_T + 14); memcpy(A, B, sizeof(A)); }
    G(P.gx - 0.07f, 0, P.gz, A); G(P.gx + 0.07f, 0, P.gz, B); seg(A, B, IX_T + 14);
    G(P.gx, 0, P.gz - 0.07f, A); G(P.gx, 0, P.gz + 0.07f, B); seg(A, B, IX_T + 14);
  }
  // its shadow
  {
    float sh = 1 / (1 + P.hop * 1.6f), pts[14][3];
    for (int i = 0; i < 14; i++) {
      float a = i * PI2 / 14, lx = cosf(a) * 0.78f * sh, lz = sinf(a) * 0.3f * sh;
      G(P.px + lx * cosf(P.hd) + lz * sinf(P.hd), 0, P.pz - lx * sinf(P.hd) + lz * cosf(P.hd), pts[i]);
    }
    for (int i = 1; i + 1 < 14; i++) tri(pts[0], pts[i], pts[i + 1], 1);
  }
  // the fox (as foxTris in fox.js)
  nt = 0;
  float sit = fminf(1, P.lf), lie = fmaxf(0, P.lf - 1), cr = P.cr, bw = P.bw, sc = P.sc, dd = P.dd, tq = scrEffects ? t : 0;
  float drop = sit * 0.13f + lie * 0.3f + cr * 0.13f + bw * 0.09f + dd * 0.47f;
  float pose = P.pt + sit * 0.42f * (1 - lie) - lie * 0.05f - bw * 0.25f, pitch = pose + P.fp;
  M34 mb = mT(P.px, P.hop - drop, P.pz);
  mb = mMul(mb, mRy(P.hd));
  mb = mMul(mb, mT(0, 0.62f, 0)); mb = mMul(mb, mRx(P.rl)); mb = mMul(mb, mRz(pitch)); mb = mMul(mb, mRx(P.tb)); mb = mMul(mb, mT(0, -0.62f, 0));
  addPart(FOX_BODY_V, FOX_BODY_NV, FOX_BODY_T, FOX_BODY_NT, mb);
  M34 mh = mMul(mb, mT(FOX_HEAD_PIVOT[0], FOX_HEAD_PIVOT[1], FOX_HEAD_PIVOT[2]));
  mh = mMul(mh, mRy(P.hy)); mh = mMul(mh, mRz(P.hp - pose * 0.6f));
  addPart(FOX_HEAD_V, FOX_HEAD_NV, FOX_HEAD_T, FOX_HEAD_NT, mh);
  for (int sd = 1; sd >= -1; sd -= 2) {
    float e = sd > 0 ? P.el : P.er;
    M34 me = mMul(mh, mT(FOX_EAR_PIVOT[0], FOX_EAR_PIVOT[1], FOX_EAR_PIVOT[2] * sd));
    me = mMul(me, mRz(e)); me = mMul(me, mRx(-0.15f * sd));
    if (sd < 0) { M34 mir = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0 } }; me = mMul(me, mir); }
    addPart(FOX_EAR_V, FOX_EAR_NV, FOX_EAR_T, FOX_EAR_NT, me, sd < 0);
  }
  for (int i = 0; i < 4; i++) {
    bool front = i < 2;
    int sd = FOX_HIPS[i][2] > 0 ? 1 : -1;
    float sw = sinf(P.wp + FOX_LEG_PH[i]) * P.wa, spl = 0;
    if (front) sw += lie * 1.35f - sit * (1 - lie) * 0.42f - cr * 0.35f + bw * 1.1f + dd * 0.4f;
    else sw += sit * 1.25f + lie * 0.25f + cr * 0.55f + bw * 0.25f - dd * 0.4f;
    if (!front && sc * sd > 0) { float a = fabsf(sc); spl = -0.5f * a * sd; sw += a * (1.05f + 0.3f * sinf(tq * 26)); }   // scratching
    M34 ml = mMul(mb, mT(FOX_HIPS[i][0], FOX_HIPS[i][1], FOX_HIPS[i][2]));
    ml = mMul(ml, mRz(sw));
    if (spl != 0) ml = mMul(ml, mRx(spl));
    addPart(FOX_LEG_V, FOX_LEG_NV, FOX_LEG_T, FOX_LEG_NT, ml);
  }
  addTail(mb, P.ta, P.tu);
  // far triangles first, back faces left out, lit from the top left
  int nl = 0;
  for (int i = 0; i < nt; i++) {
    const WTri& T = tris[i];
    float pa[3], pb[3], pc[3];
    proj(T.a, pa); proj(T.b, pb); proj(T.c, pc);
    float area = (pb[0] - pa[0]) * (pc[1] - pa[1]) - (pb[1] - pa[1]) * (pc[0] - pa[0]);
    if (area >= 0) continue;
    float u[3] = { T.b[0] - T.a[0], T.b[1] - T.a[1], T.b[2] - T.a[2] }, v[3] = { T.c[0] - T.a[0], T.c[1] - T.a[1], T.c[2] - T.a[2] };
    float n[3] = { u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
    float nl2 = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (nl2 <= 0) nl2 = 1;
    float lit = fmaxf(0, (n[0] * FOX_LIGHT[0] + n[1] * FOX_LIGHT[1] + n[2] * FOX_LIGHT[2]) / nl2);
    int lvl = jround(3 + 9 * lit) + T.s;
    lvl = lvl < 1 ? 1 : lvl > 13 ? 13 : lvl;
    DTri& d = list[nl];
    d.z = (pa[2] + pb[2] + pc[2]) / 3;
    d.a[0] = pa[0]; d.a[1] = pa[1]; d.b[0] = pb[0]; d.b[1] = pb[1]; d.c[0] = pc[0]; d.c[1] = pc[1];
    d.e = T.e; d.lvl = lvl;
    int j = nl++;                                  // insertion sort, far first, equal ones keep their order (as JS sort)
    DTri x = list[j];
    while (j > 0 && list[j - 1].z < x.z) { list[j] = list[j - 1]; j--; }
    list[j] = x;
  }
  for (int i = 0; i < nl; i++) {
    const DTri& d = list[i];
    float a[2] = { d.a[0], d.a[1] }, b[2] = { d.b[0], d.b[1] }, c[2] = { d.c[0], d.c[1] };
    tri(a, b, c, d.lvl);
    uint8_t ec = IX_T + min(15, d.lvl + 3);
    if (d.e & 1) seg(a, b, ec);
    if (d.e & 2) seg(b, c, ec);
    if (d.e & 4) seg(c, a, ec);
  }
  // eyes
  float o0[3], zero[3] = { 0, 0, 0 };
  mAp(mh, zero, o0);
  for (int sd = 1; sd >= -1; sd -= 2) {
    float pl[3] = { 0.15f * FOX_HS, 0.11f * FOX_HS, 0.085f * FOX_HS * sd }, nlc[3] = { 0.75f, 0.25f, 0.62f * sd }, pw[3], nw[3], q[3];
    mAp(mh, pl, pw); mAp(mh, nlc, nw);
    for (int j = 0; j < 3; j++) nw[j] -= o0[j];
    if (nw[0] * (FOX_CAM[0] - pw[0]) + nw[1] * (FOX_CAM[1] - pw[1]) + nw[2] * (FOX_CAM[2] - pw[2]) <= 0) continue;
    proj(pw, q);
    int x = jround(q[0]), y = jround(q[1]);
    if (P.ey == 1) scrFill(x - 1, y, 3, 1, IX_T);
    else if (P.ey == 2) { scrFill(x - 1, y - 1, 1, 1, IX_T); scrFill(x + 1, y - 1, 1, 1, IX_T); scrFill(x, y, 1, 1, IX_T); scrFill(x - 1, y + 1, 1, 1, IX_T); scrFill(x + 1, y + 1, 1, 1, IX_T); }
    else if (P.ey == 3) { scrFill(x - 1, y - 1, 3, 3, IX_T); scrFill(x - 1, y - 1, 1, 1, IX_T + 15); }
    else { scrFill(x - 1, y - 1, 2, 2, IX_T); scrFill(x - 1, y - 1, 1, 1, IX_T + 15); }
  }
  // above its head: zzz, !, or stars going round
  float tl[3] = { 0.05f, 0.52f, 0 }, tw[3], top[3];
  mAp(mh, tl, tw); proj(tw, top);
  int hx = jround(top[0]), hy = jround(top[1]);
  if (P.ov == 1) {
    float u = scrEffects ? fmodf(t * 0.6f, 1.0f) : 0.5f;
    scrText("z", hx + 6 + jround(u * 10), hy - 4 - jround(u * 14), 1, IX_T + 10);
    scrText("Z", hx + 14 + jround(u * 10), hy - 18 - jround(u * 14), 2, IX_T + 13);
  } else if (P.ov == 2) scrText("!", hx - 7, hy - 22, 2, IX_T + 15);
  else if (P.ov == 4) scrText("?", hx - 7, hy - 22, 2, IX_T + 15);
  else if (P.ov == 3) {
    for (int i = 0; i < 3; i++) {
      float a = (scrEffects ? t * 4 : 0) + i * PI2 / 3;
      scrText("*", hx - 4 + jround(cosf(a) * 16), hy - 10 + jround(sinf(a) * 5), 1, IX_T + 14);
    }
  }
  // the top bar
  scrFill(0, 0, LCD_W, 11, IX_T + 12);
  scrText("FOX", 3, 2, 1, IX_T);
  const char* m = (P.md >= 0 && P.md < NMOODS) ? MOODS[P.md] : "";
  scrText(m, LCD_W - 3 - scrTextW(m, 1), 2, 1, IX_T);
}
