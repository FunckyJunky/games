// g3d.cpp: see g3d.h
#include "g3d.h"
#include <math.h>
#include <stdlib.h>

static float cpx, cpy, cpz, cyw = 1, syw = 0, cpt = 1, spt = 0, crl = 1, srl = 0, cf = 300, ccx = 240, ccy = 240;
static float opx, opy, opz, oyc = 1, oys = 0, opc = 1, ops = 0, orc = 1, ors = 0, osc = 1;
static float fogN = 0, fogF = 0;
bool g3dGlow = true;

void g3dCam(float x, float y, float z, float yaw, float pitch, float roll, float f, float cx, float cy) {
  cpx = x; cpy = y; cpz = z;
  cyw = cosf(yaw); syw = sinf(yaw); cpt = cosf(pitch); spt = sinf(pitch); crl = cosf(roll); srl = sinf(roll);
  cf = f; ccx = cx; ccy = cy;
}
void g3dObj(V3 p, float yaw, float pitch, float roll, float s) {
  opx = p.x; opy = p.y; opz = p.z; osc = s;
  oyc = cosf(yaw); oys = sinf(yaw); opc = cosf(pitch); ops = sinf(pitch); orc = cosf(roll); ors = sinf(roll);
}
void g3dFog(float n, float f) { fogN = n; fogF = f; }
V3 g3dWorld(V3 p) {
  float x = p.x * osc, y = p.y * osc, z = p.z * osc;
  float x1 = x * oyc + z * oys, z1 = -x * oys + z * oyc;       // yaw
  float y2 = y * opc - z1 * ops, z2 = y * ops + z1 * opc;      // pitch
  float x3 = x1 * orc - y2 * ors, y3 = x1 * ors + y2 * orc;    // roll
  return v3(x3 + opx, y3 + opy, z2 + opz);
}
static V3 toCam(V3 w) {
  float dx = w.x - cpx, dy = w.y - cpy, dz = w.z - cpz;
  float x1 = dx * cyw - dz * syw, z1 = dx * syw + dz * cyw;
  float y2 = dy * cpt - z1 * spt, z2 = dy * spt + z1 * cpt;
  return v3(x1 * crl - y2 * srl, x1 * srl + y2 * crl, z2);
}
bool g3dProj(V3 w, float& sx, float& sy, float& z) {
  V3 c = toCam(w);
  z = c.z;
  if (c.z < 1) return false;
  sx = ccx + cf * c.x / c.z; sy = ccy + cf * c.y / c.z;
  return true;
}
float g3dDepth(V3 p) { return toCam(g3dWorld(p)).z; }

// ------------------------------------------------------------------ 2D
static inline void plot(int x, int y, uint16_t c, float a) { if (a > 0.02f) pblend(x, y, c, (int)(a * 32 + 0.5f)); }
void aaLine(float x0, float y0, float x1, float y1, uint16_t c, int a32) {
  // Xiaolin Wu
  float A = a32 / 32.0f;
  bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
  if (steep) { float t = x0; x0 = y0; y0 = t; t = x1; x1 = y1; y1 = t; }
  if (x0 > x1) { float t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
  if (x1 - x0 > 2000) return;
  float dx = x1 - x0, dy = y1 - y0, g = dx < 0.001f ? 1 : dy / dx;
  int xs = (int)floorf(x0 + 0.5f), xe = (int)floorf(x1 + 0.5f);
  float y = y0 + g * (xs - x0);
  int lo = steep ? clipY0 - 1 : clipX0 - 1, hi = steep ? clipY1 : clipX1;
  if (xs < lo) { y += g * (lo - xs); xs = lo; }
  if (xe > hi) xe = hi;
  for (int x = xs; x <= xe; x++, y += g) {
    int yi = (int)floorf(y); float f = y - yi;
    if (steep) { plot(yi, x, c, (1 - f) * A); plot(yi + 1, x, c, f * A); }
    else { plot(x, yi, c, (1 - f) * A); plot(x, yi + 1, c, f * A); }
  }
}
void glowLine(float x0, float y0, float x1, float y1, uint16_t c, int a32) {
  if (g3dGlow && a32 > 6) {
    float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
    if (l > 0.5f) {
      float nx = -dy / l * 1.6f, ny = dx / l * 1.6f;
      aaLine(x0 + nx, y0 + ny, x1 + nx, y1 + ny, c, a32 / 4);
      aaLine(x0 - nx, y0 - ny, x1 - nx, y1 - ny, c, a32 / 4);
    }
  }
  aaLine(x0, y0, x1, y1, c, a32);
}
void fillPoly(const float* p, int n, uint16_t c, int a32) {
  if (n < 3) return;
  float minY = 1e9f, maxY = -1e9f;
  for (int i = 0; i < n; i++) { if (p[i * 2 + 1] < minY) minY = p[i * 2 + 1]; if (p[i * 2 + 1] > maxY) maxY = p[i * 2 + 1]; }
  int y0 = (int)ceilf(minY - 0.5f), y1 = (int)floorf(maxY - 0.5f);
  if (y0 < clipY0) y0 = clipY0; if (y1 >= clipY1) y1 = clipY1 - 1;
  float xs[16];
  for (int y = y0; y <= y1; y++) {
    float sy = y + 0.5f; int k = 0;
    for (int i = 0; i < n && k < 16; i++) {
      float ax = p[i * 2], ay = p[i * 2 + 1], bx = p[((i + 1) % n) * 2], by = p[((i + 1) % n) * 2 + 1];
      if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) xs[k++] = ax + (sy - ay) * (bx - ax) / (by - ay);
    }
    for (int a = 1; a < k; a++) { float t = xs[a]; int b = a - 1; while (b >= 0 && xs[b] > t) { xs[b + 1] = xs[b]; b--; } xs[b + 1] = t; }
    for (int i = 0; i + 1 < k; i += 2) {
      int xa = (int)ceilf(xs[i] - 0.5f), xb = (int)floorf(xs[i + 1] - 0.5f);
      if (xb >= xa) blendRect(xa, y, xb - xa + 1, 1, c, a32);
    }
  }
}

// ------------------------------------------------------------------ 3D lines
static int fogA(int a32, float z) {
  if (fogF <= fogN) return a32;
  float f = (fogF - z) / (fogF - fogN);
  if (f > 1) f = 1; if (f < 0) return 0;
  return (int)(a32 * f);
}
void l3(V3 a, V3 b, uint16_t c, int a32) {
  V3 A = toCam(g3dWorld(a)), B = toCam(g3dWorld(b));
  if (A.z < 1 && B.z < 1) return;
  if (A.z < 1) { float t = (1 - A.z) / (B.z - A.z); A.x += (B.x - A.x) * t; A.y += (B.y - A.y) * t; A.z = 1; }
  else if (B.z < 1) { float t = (1 - B.z) / (A.z - B.z); B.x += (A.x - B.x) * t; B.y += (A.y - B.y) * t; B.z = 1; }
  int al = fogA(a32, (A.z + B.z) * 0.5f);
  if (al <= 0) return;
  glowLine(ccx + cf * A.x / A.z, ccy + cf * A.y / A.z, ccx + cf * B.x / B.z, ccy + cf * B.y / B.z, c, al);
}
void l3(float ax, float ay, float az, float bx, float by, float bz, uint16_t c, int a32) { l3(v3(ax, ay, az), v3(bx, by, bz), c, a32); }
void ring3(V3 c, float r, int n, int axis, uint16_t col, int a32, float a0, float a1) {
  V3 prev = c;
  for (int i = 0; i <= n; i++) {
    float a = a0 + (a1 - a0) * i / n, s = sinf(a) * r, k = cosf(a) * r;
    V3 p = axis == 0 ? v3(c.x, c.y + s, c.z + k) : axis == 1 ? v3(c.x + k, c.y, c.z + s) : v3(c.x + k, c.y + s, c.z);
    if (i) l3(prev, p, col, a32);
    prev = p;
  }
}
void box3(V3 c, V3 h, uint16_t col, int a32) {
  V3 v[8];
  for (int i = 0; i < 8; i++) v[i] = v3(c.x + (i & 1 ? h.x : -h.x), c.y + (i & 2 ? h.y : -h.y), c.z + (i & 4 ? h.z : -h.z));
  static const uint8_t E[24] = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 2, 6, 3, 7};
  for (int i = 0; i < 24; i += 2) l3(v[E[i]], v[E[i + 1]], col, a32);
}
void quad3(V3 a, V3 b, V3 c, V3 d, uint16_t col, int a32) {
  V3 P[4] = {a, b, c, d};
  float xy[8];
  for (int i = 0; i < 4; i++) { float z; if (!g3dProj(g3dWorld(P[i]), xy[i * 2], xy[i * 2 + 1], z)) return; }
  fillPoly(xy, 4, col, a32);
}
