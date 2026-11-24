// FOX: the same fox as public/thuisfront/dev/fox.js (keep them in step). The shape comes from fox_mesh.h, made from
// fox.js by tools/hf-foxmesh.mjs.
#include <Arduino.h>
#include <math.h>
#include "screen.h"
#include "fox.h"
#include "fox_mesh.h"

static const float PI2 = 6.2831853f;
static const char* const MOODS[] = { "CHILLING", "CURIOUS", "WALKING", "SITTING", "ZZZ...", "HUH?", "EEK!", "WHOA!", "HOLD ON!", "HELP!", "DIZZY...", "OOF!", "HELLO!" };

static inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static inline float easef(float cur, float to, float rate, float dt) { return cur + (to - cur) * (1 - expf(-rate * dt)); }
static inline float wrapf(float a) { return a - PI2 * floorf(a / PI2 + 0.5f); }
static inline float easeA(float cur, float to, float rate, float dt) { return cur + wrapf(to - cur) * (1 - expf(-rate * dt)); }
static inline float headingTo(float dx, float dz) { return atan2f(-dz, dx); }
static inline float rnd() { return (esp_random() >> 8) / 16777216.0f; }
static inline int jround(float x) { return (int)floorf(x + 0.5f); }   // as Math.round

// ---------------- what the fox does ----------------
void foxStep(FoxSim& S, const float* fin, const float* win, float dt) {
  dt = clampf(dt, 0, 0.1f);
  S.t += dt;
  if (fin) S.sensor = true;
  float f[3] = { 0, 1, 0 }, w[3] = { 0, 0, 0 };
  if (fin) for (int i = 0; i < 3; i++) { f[i] = fin[i]; w[i] = win ? win[i] : 0; }
  for (int i = 0; i < 3; i++) S.gl[i] = easef(S.gl[i], f[i], 6, dt);
  float gn = sqrtf(S.gl[0] * S.gl[0] + S.gl[1] * S.gl[1] + S.gl[2] * S.gl[2]);
  if (gn <= 0) gn = 1;
  float up[3] = { S.gl[0] / gn, S.gl[1] / gn, S.gl[2] / gn };
  float j0 = f[0] - S.gl[0], j1 = f[1] - S.gl[1], j2 = f[2] - S.gl[2];
  float jolt = sqrtf(j0 * j0 + j1 * j1 + j2 * j2), spin = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
  float dx = -up[0], dz = -up[2], slope = sqrtf(dx * dx + dz * dz);
  bool busy = jolt > 0.05f || spin > 0.35f;
  if (busy) { if (S.lf > 1.5f && S.still > 2) { S.wake = 2; S.hello = 0; } S.still = 0; } else S.still += dt;
  bool sleepy = S.sensor && S.still > 45;
  S.dizzy = clampf(S.dizzy + (spin > 3 ? (spin - 3) * dt * 0.45f : 0) + (jolt > 0.8f ? dt * 1.3f : 0) - dt * 0.22f, 0, 3);
  S.startle = fmaxf(0, S.startle - dt); S.wake = fmaxf(0, S.wake - dt); S.hello = fmaxf(0, S.hello - dt); S.bump = fmaxf(0, S.bump - dt);
  if (jolt > 0.55f && S.startle < 0.6f) {
    S.startle = 1.6f;
    if (S.hop < 0.05f) S.vy = 1.6f + fminf(1.5f, jolt);
    S.vx -= j0 * 2.2f; S.vz -= j2 * 2.2f;
    if (S.lf > 1.5f) S.wake = 2;
  }
  bool wasFalling = S.falling;
  S.falling = up[1] < (S.falling ? 0.5f : 0.3f);
  if (S.falling) {
    S.tbv = fminf(9, S.tbv + dt * 10); S.tb += S.tbv * dt;
    S.vx += dx * 9.8f * dt * 0.8f; S.vz += dz * 9.8f * dt * 0.8f;
    S.hop = easef(S.hop, 0.35f + 0.15f * sinf(S.t * 7), 8, dt);
  } else if (wasFalling) { S.tbv = 0; S.dizzy = fminf(3, S.dizzy + 1.3f); S.vy = 1.8f; S.bump = 0.6f; }
  if (!S.falling) S.tb = easef(S.tb, PI2 * floorf(S.tb / PI2 + 0.5f), 6, dt);
  bool sliding = !S.falling && slope > 0.24f;
  if (!S.falling) {
    if (sliding) { float k = 9.8f * (slope - 0.2f) / slope; S.vx += dx * k * dt; S.vz += dz * k * dt; }
    S.wander -= dt;
    bool canWalk = !sliding && S.dizzy < 1 && S.startle <= 0 && !sleepy && S.lf < 0.5f && S.wake <= 0 && slope < 0.15f;
    if (S.walking) {
      float ex = S.tx - S.px, ez = S.tz - S.pz, d = sqrtf(ex * ex + ez * ez);
      if (!canWalk || d < 0.12f) S.walking = false;
      else { S.vx = easef(S.vx, ex / d * 0.55f, 4, dt); S.vz = easef(S.vz, ez / d * 0.55f, 4, dt); }
    } else if (canWalk && S.wander <= 0 && S.calm < 8) {
      S.walking = true; S.tx = -1.6f + rnd() * 3.2f; S.tz = -1.0f + rnd() * 1.6f; S.wander = 6 + rnd() * 10;
    }
    if (!S.walking) { float fr = sliding ? 1.2f : 4; S.vx *= expf(-fr * dt); S.vz *= expf(-fr * dt); }
  }
  S.px += S.vx * dt; S.pz += S.vz * dt;
  const float xl = FOX_BOX_X - 0.55f, zb = FOX_BOX_ZB + 0.45f, zf = FOX_BOX_ZF - 0.45f;
  auto hit = [&](float v) { if (fabsf(v) > 1.2f && S.bump <= 0) { S.bump = 0.7f; S.vy = fmaxf(S.vy, 0.9f); } return 0.0f; };
  if (S.px < -xl) { S.px = -xl; if (S.vx < 0) S.vx = hit(S.vx); }
  if (S.px > xl) { S.px = xl; if (S.vx > 0) S.vx = hit(S.vx); }
  if (S.pz < zb) { S.pz = zb; if (S.vz < 0) S.vz = hit(S.vz); }
  if (S.pz > zf) { S.pz = zf; if (S.vz > 0) S.vz = hit(S.vz); }
  if (!S.falling) { S.vy -= 9.8f * dt; S.hop += S.vy * dt; if (S.hop <= 0) { S.hop = 0; S.vy = 0; } }
  float speed = sqrtf(S.vx * S.vx + S.vz * S.vz);
  if (!sliding && !S.walking && speed < 0.15f && S.startle <= 0 && !S.falling) S.calm += dt; else S.calm = 0;
  float lfTo = sleepy ? 2 : (S.calm > 7 && S.dizzy < 1) ? 1 : 0;
  S.lf = easef(S.lf, lfTo, lfTo > S.lf ? 1.2f : 5, dt);
  float hdTo;
  if (sliding) hdTo = headingTo(-dx, -dz);
  else if (S.walking && speed > 0.1f) hdTo = headingTo(S.vx, S.vz);
  else hdTo = S.px > 0.4f ? -2.45f : S.px < -0.4f ? -0.7f : S.hd;
  S.hd -= w[1] * dt * 0.85f;
  if (!S.falling) S.hd = easeA(S.hd, hdTo, sliding ? 3 : S.walking ? 5 : 1.5f, dt);
  S.hd = wrapf(S.hd);
  float fx = cosf(S.hd), fz = -sinf(S.hd);
  float along = dx * fx + dz * fz, side = dx * fz - dz * fx;
  S.pt = easef(S.pt, clampf(-along * 1.3f, -0.5f, 0.5f), 6, dt);
  S.rl = easef(S.rl, clampf(side * 1.1f, -0.5f, 0.5f) + (S.dizzy > 1 ? sinf(S.t * 3) * 0.12f : 0), 6, dt);
  float waTo = S.falling ? 0.9f : sliding ? 0.75f : S.lf > 0.5f ? 0 : clampf(speed * 1.1f, 0, 0.6f);
  S.wa = easef(S.wa, waTo, 8, dt);
  S.wp = wrapf(S.wp + dt * (sliding || S.falling ? 15 : 3 + speed * 12));
  S.look -= dt;
  if (S.look <= 0) { S.look = 2 + rnd() * 4; S.lookY = rnd() < 0.55f ? 0 : (rnd() - 0.5f) * 1.6f; S.lookP = (rnd() - 0.5f) * 0.4f; }
  float toCam = clampf(wrapf(headingTo(FOX_CAM[0] - S.px, FOX_CAM[2] - S.pz) - S.hd), -1.1f, 1.1f);
  float hyTo = S.lookY == 0 ? toCam : S.lookY, hpTo = S.lookY == 0 ? 0.12f : S.lookP;
  if (sliding || S.falling) { hyTo = 0; hpTo = 0.25f; }
  if (S.startle > 0 || S.wake > 0 || S.hello > 0) { hyTo = toCam; hpTo = 0.2f; }
  if (S.lf > 1.2f) { hyTo = 0.5f; hpTo = -0.45f; }
  if (S.dizzy > 1) hyTo += sinf(S.t * 2.6f) * 0.3f;
  S.hy = easef(S.hy, hyTo, 5, dt); S.hp = easef(S.hp, hpTo, 5, dt);
  S.twitch -= dt;
  if (S.twitch <= 0) { S.twitch = 2 + rnd() * 5; S.twitchE = rnd() < 0.5f ? 1 : 2; }
  float earBase = S.startle > 0 ? -0.15f : (sliding || S.falling || S.dizzy > 1 || S.bump > 0) ? -0.7f : S.lf > 1.2f ? -0.45f : 0;
  float tw = S.twitch < 0.25f ? -0.5f : 0;
  S.el = easef(S.el, earBase + (S.twitchE == 1 ? tw : 0), 14, dt); S.er = easef(S.er, earBase + (S.twitchE == 2 ? tw : 0), 14, dt);
  float taTo = S.lf > 1.2f ? 1.5f : S.walking ? sinf(S.wp) * 0.3f : (S.hello > 0 || S.wake > 0) ? sinf(S.t * 12) * 0.5f : sinf(S.t * 1.4f) * 0.35f;
  float tuTo = S.startle > 0 ? 0.85f : (sliding || S.falling) ? 0.6f : S.lf > 1.2f ? -0.25f : S.lf > 0.5f ? -0.1f : 0.15f;
  S.ta = easef(S.ta, taTo, 6, dt); S.tu = easef(S.tu, tuTo, 5, dt);
  S.blink -= dt;
  if (S.blink <= 0) S.blink = 2 + rnd() * 3.5f;
  S.ey = S.lf > 1.5f ? 1 : S.dizzy > 1 ? 2 : S.startle > 0.6f ? 3 : S.blink < 0.13f ? 1 : 0;
  S.md = S.falling ? 9 : S.bump > 0 ? 11 : S.startle > 0.4f ? 6 : S.dizzy > 1 ? 10 : sliding ? (slope > 0.45f ? 7 : 8) : S.wake > 0 ? 5
       : S.hello > 0 ? 12 : S.lf > 1.5f ? 4 : S.lf > 0.6f ? 3 : S.walking ? 2 : S.lookY == 0 ? 1 : 0;
  S.ov = S.lf > 1.5f ? 1 : (S.startle > 0.6f || S.wake > 0) ? 2 : S.dizzy > 1 ? 3 : 0;
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
  p = { S.px, S.pz, S.hop, S.hd, S.rl, S.pt, S.tb, S.lf, S.hy, S.hp, S.el, S.er, S.ta, S.tu, S.wp, S.wa, S.ey, S.md, S.ov };
}

int foxJson(const FoxPose& p, char* out, int n) {
  return snprintf(out, n, "[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%d]",
                  p.px, p.pz, p.hop, p.hd, p.rl, p.pt, p.tb, p.lf, p.hy, p.hp, p.el, p.er, p.ta, p.tu, p.wp, p.wa, p.ey, p.md, p.ov);
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
  // the box: a grid on the floor, the back wall's edges
  for (int i = 0; i <= 12; i++) { float x = -FOX_BOX_X + i * FOX_BOX_X / 6; G(x, 0, FOX_BOX_ZB, A); G(x, 0, FOX_BOX_ZF, B); seg(A, B, IX_T + 3); }
  for (int i = 0; i <= 6; i++) { float z = FOX_BOX_ZB + i * (FOX_BOX_ZF - FOX_BOX_ZB) / 6; G(-FOX_BOX_X, 0, z, A); G(FOX_BOX_X, 0, z, B); seg(A, B, IX_T + 3); }
  for (int sd = -1; sd <= 1; sd += 2) {
    float x = sd * FOX_BOX_X;
    G(x, 0, FOX_BOX_ZB, A); G(x, FOX_BOX_H, FOX_BOX_ZB, B); seg(A, B, IX_T + 5);
    G(x, 0, FOX_BOX_ZF, A); G(x, 0, FOX_BOX_ZB, B); seg(A, B, IX_T + 6);
  }
  G(-FOX_BOX_X, FOX_BOX_H, FOX_BOX_ZB, A); G(FOX_BOX_X, FOX_BOX_H, FOX_BOX_ZB, B); seg(A, B, IX_T + 5);
  G(-FOX_BOX_X, 0, FOX_BOX_ZF, A); G(FOX_BOX_X, 0, FOX_BOX_ZF, B); seg(A, B, IX_T + 6);
  G(-FOX_BOX_X, 0, FOX_BOX_ZB, A); G(FOX_BOX_X, 0, FOX_BOX_ZB, B); seg(A, B, IX_T + 5);
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
  float sit = fminf(1, P.lf), lie = fmaxf(0, P.lf - 1);
  float drop = sit * 0.13f + lie * 0.3f, pitch = P.pt + sit * 0.42f * (1 - lie) - lie * 0.05f;
  M34 mb = mT(P.px, P.hop - drop, P.pz);
  mb = mMul(mb, mRy(P.hd));
  mb = mMul(mb, mT(0, 0.62f, 0)); mb = mMul(mb, mRx(P.rl)); mb = mMul(mb, mRz(pitch)); mb = mMul(mb, mRx(P.tb)); mb = mMul(mb, mT(0, -0.62f, 0));
  addPart(FOX_BODY_V, FOX_BODY_NV, FOX_BODY_T, FOX_BODY_NT, mb);
  M34 mh = mMul(mb, mT(FOX_HEAD_PIVOT[0], FOX_HEAD_PIVOT[1], FOX_HEAD_PIVOT[2]));
  mh = mMul(mh, mRy(P.hy)); mh = mMul(mh, mRz(P.hp - pitch * 0.6f));
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
    float sw = sinf(P.wp + FOX_LEG_PH[i]) * P.wa;
    if (front) sw += lie * 1.35f - sit * (1 - lie) * 0.42f; else sw += sit * 1.25f + lie * 0.25f;
    M34 ml = mMul(mb, mT(FOX_HIPS[i][0], FOX_HIPS[i][1], FOX_HIPS[i][2]));
    ml = mMul(ml, mRz(sw));
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
  else if (P.ov == 3) {
    for (int i = 0; i < 3; i++) {
      float a = (scrEffects ? t * 4 : 0) + i * PI2 / 3;
      scrText("*", hx - 4 + jround(cosf(a) * 16), hy - 10 + jround(sinf(a) * 5), 1, IX_T + 14);
    }
  }
  // the top bar
  scrFill(0, 0, LCD_W, 11, IX_T + 12);
  scrText("FOX", 3, 2, 1, IX_T);
  const char* m = (P.md >= 0 && P.md < 13) ? MOODS[P.md] : "";
  scrText(m, LCD_W - 3 - scrTextW(m, 1), 2, 1, IX_T);
}
