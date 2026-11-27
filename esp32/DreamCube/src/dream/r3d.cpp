// r3d.cpp: see r3d.h. Triangles are lit, fogged and projected when they're added (flat: one colour each), kept
// in a list, then drawn band by band with a 1/z depth buffer. Sky, sun, stars first; see-through triangles after
// the solid ones; then sprites and streaks (depth-tested, not written).
#include "r3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
static void* fastMem(size_t n) { void* p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); return p ? p : malloc(n); }
#else
static void* fastMem(size_t n) { return malloc(n); }
#endif

// ------------------------------------------------------------------ small maths
Vec vnorm(Vec a) { float l = sqrtf(vdot(a, a)); return l > 1e-9f ? vmul(a, 1.0f / l) : vec(0, 1, 0); }
Vec vlerp(Vec a, Vec b, float t) { return vec(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }
Col hexCol(uint32_t c) { return col(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f); }
Col clerp(Col a, Col b, float t) { return col(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t); }
Col cmul(Col a, float s) { return col(a.r * s, a.g * s, a.b * s); }
static inline int q(float v, int m) { int i = (int)(v * m + 0.5f); return i < 0 ? 0 : i > m ? m : i; }
uint16_t c565(Col c) { return (uint16_t)((q(c.r, 31) << 11) | (q(c.g, 63) << 5) | q(c.b, 31)); }

Mat matId() { Mat m = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}}; return m; }
Mat matMul(const Mat& a, const Mat& b) {
  Mat r;
  for (int i = 0; i < 3; i++) {
    const float* A = a.m + i * 4;
    for (int j = 0; j < 3; j++) r.m[i * 4 + j] = A[0] * b.m[j] + A[1] * b.m[4 + j] + A[2] * b.m[8 + j];
    r.m[i * 4 + 3] = A[0] * b.m[3] + A[1] * b.m[7] + A[2] * b.m[11] + A[3];
  }
  return r;
}
Mat matT(float x, float y, float z) { Mat m = matId(); m.m[3] = x; m.m[7] = y; m.m[11] = z; return m; }
Mat matS(float x, float y, float z) { Mat m = {{x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0}}; return m; }
Mat matRx(float a) { float c = cosf(a), s = sinf(a); Mat m = {{1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0}}; return m; }
Mat matRy(float a) { float c = cosf(a), s = sinf(a); Mat m = {{c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0}}; return m; }
Mat matRz(float a) { float c = cosf(a), s = sinf(a); Mat m = {{c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0}}; return m; }
Vec matApply(const Mat& m, Vec v) {
  return vec(m.m[0] * v.x + m.m[1] * v.y + m.m[2] * v.z + m.m[3], m.m[4] * v.x + m.m[5] * v.y + m.m[6] * v.z + m.m[7],
             m.m[8] * v.x + m.m[9] * v.y + m.m[10] * v.z + m.m[11]);
}

// ------------------------------------------------------------------ the unit shapes
// Triangles wind counter-clockwise seen from outside (right-handed, y up). Sizes: -1..1 on every axis.
struct Build { float v[3 * 200]; uint16_t t[3 * 260]; int nv, nt; };
static Mesh meshes[P_N];
static int vtx(Build& b, float x, float y, float z) { b.v[b.nv * 3] = x; b.v[b.nv * 3 + 1] = y; b.v[b.nv * 3 + 2] = z; return b.nv++; }
static void tri(Build& b, int i, int j, int k) { b.t[b.nt * 3] = i; b.t[b.nt * 3 + 1] = j; b.t[b.nt * 3 + 2] = k; b.nt++; }
static void quad(Build& b, int i, int j, int k, int l) { tri(b, i, j, k); tri(b, i, k, l); }
static void keep(int p, Build& b) {
  float* v = (float*)malloc(b.nv * 12); uint16_t* t = (uint16_t*)malloc(b.nt * 6);
  memcpy(v, b.v, b.nv * 12); memcpy(t, b.t, b.nt * 6);
  meshes[p].v = v; meshes[p].nv = b.nv; meshes[p].t = t; meshes[p].nt = b.nt;
}
// a shape turned around y: rings of (radius, height), n sides; caps where the radius is 0 (a point) or closed
static void lathe(Build& b, const float* rh, int rings, int n, bool capTop, bool capBottom, float a0 = 0, float a1 = 6.2831853f, bool full = true) {
  int base = b.nv;
  int cols = full ? n : n + 1;
  for (int r = 0; r < rings; r++)
    for (int i = 0; i < cols; i++) { float a = a0 + (a1 - a0) * i / n; vtx(b, rh[r * 2] * cosf(a), rh[r * 2 + 1], -rh[r * 2] * sinf(a)); }
  for (int r = 0; r + 1 < rings; r++)
    for (int i = 0; i < n; i++) {
      int i2 = full ? (i + 1) % n : i + 1;
      int a = base + r * cols + i, bb = base + r * cols + i2, c = base + (r + 1) * cols + i2, d = base + (r + 1) * cols + i;
      // ring r is below ring r+1 when its height is lower
      quad(b, a, bb, c, d);
    }
  if (capBottom) { int c = vtx(b, 0, rh[1], 0); for (int i = 0; i < n; i++) tri(b, c, base + (full ? (i + 1) % n : i + 1), base + i); }
  if (capTop) { int c = vtx(b, 0, rh[(rings - 1) * 2 + 1], 0); int o = base + (rings - 1) * cols; for (int i = 0; i < n; i++) tri(b, c, o + i, o + (full ? (i + 1) % n : i + 1)); }
}
static void makeShapes() {
  static Build b;
  // box
  b.nv = b.nt = 0;
  for (int i = 0; i < 8; i++) vtx(b, i & 1 ? 1 : -1, i & 2 ? 1 : -1, i & 4 ? 1 : -1);
  quad(b, 0, 2, 3, 1); quad(b, 4, 5, 7, 6);   // -z, +z
  quad(b, 0, 1, 5, 4); quad(b, 2, 6, 7, 3);   // -y, +y
  quad(b, 0, 4, 6, 2); quad(b, 1, 3, 7, 5);   // -x, +x
  keep(P_BOX, b);
  // ball: 8 sides, 5 bands
  { b.nv = b.nt = 0; float rh[12]; for (int r = 0; r < 6; r++) { float a = -1.5708f + 3.14159f * r / 5; rh[r * 2] = cosf(a); rh[r * 2 + 1] = sinf(a); }
    rh[0] = 0; rh[10] = 0;
    // poles: lathe with radius 0 rings makes degenerate quads; build it by hand instead
    int s = vtx(b, 0, -1, 0);
    for (int r = 1; r < 5; r++) for (int i = 0; i < 8; i++) { float a = 6.2831853f * (i + (r & 1) * 0.5f) / 8; vtx(b, rh[r * 2] * cosf(a), rh[r * 2 + 1], -rh[r * 2] * sinf(a)); }
    int n = vtx(b, 0, 1, 0);
    for (int i = 0; i < 8; i++) tri(b, s, 1 + (i + 1) % 8, 1 + i);
    for (int r = 1; r < 4; r++) for (int i = 0; i < 8; i++) {
      int a = 1 + (r - 1) * 8, c = 1 + r * 8;
      if (r & 1) { tri(b, a + i, a + (i + 1) % 8, c + i); tri(b, a + (i + 1) % 8, c + (i + 1) % 8, c + i); }
      else { tri(b, a + i, c + (i + 1) % 8, c + i); tri(b, a + i, a + (i + 1) % 8, c + (i + 1) % 8); }
    }
    for (int i = 0; i < 8; i++) tri(b, n, 1 + 24 + i, 1 + 24 + (i + 1) % 8);
    keep(P_BALL, b); }
  // cylinder (8 sides), cone, pyramid (4 sides), dome, disc (a very flat cylinder), tube (open, both sides)
  { b.nv = b.nt = 0; float rh[] = {1, -1, 1, 1}; lathe(b, rh, 2, 8, true, true); keep(P_CYL, b); }
  { b.nv = b.nt = 0; int base = 0; for (int i = 0; i < 8; i++) { float a = 6.2831853f * i / 8; vtx(b, cosf(a), -1, -sinf(a)); }
    int tip = vtx(b, 0, 1, 0), c = vtx(b, 0, -1, 0);
    for (int i = 0; i < 8; i++) { tri(b, base + i, base + (i + 1) % 8, tip); tri(b, c, base + (i + 1) % 8, base + i); }
    keep(P_CONE, b); }
  { b.nv = b.nt = 0; for (int i = 0; i < 4; i++) { float a = 6.2831853f * (i + 0.5f) / 4; vtx(b, cosf(a) * 1.41421f, -1, -sinf(a) * 1.41421f); }
    int tip = vtx(b, 0, 1, 0);
    for (int i = 0; i < 4; i++) tri(b, i, (i + 1) % 4, tip);
    tri(b, 0, 2, 1); tri(b, 0, 3, 2);
    keep(P_PYR, b); }
  { b.nv = b.nt = 0; float rh[10]; for (int r = 0; r < 4; r++) { float a = 1.5708f * r / 3; rh[r * 2] = cosf(a); rh[r * 2 + 1] = sinf(a) * 2 - 1; }
    rh[6] = 0.0f;
    int base = 0;
    for (int r = 0; r < 3; r++) for (int i = 0; i < 8; i++) { float a = 6.2831853f * i / 8; vtx(b, rh[r * 2] * cosf(a), rh[r * 2 + 1], -rh[r * 2] * sinf(a)); }
    int top = vtx(b, 0, 1, 0), c = vtx(b, 0, -1, 0);
    for (int r = 0; r < 2; r++) for (int i = 0; i < 8; i++) quad(b, base + r * 8 + i, base + r * 8 + (i + 1) % 8, base + (r + 1) * 8 + (i + 1) % 8, base + (r + 1) * 8 + i);
    for (int i = 0; i < 8; i++) { tri(b, top, base + 16 + i, base + 16 + (i + 1) % 8); tri(b, c, base + (i + 1) % 8, base + i); }
    keep(P_DOME, b); }
  { b.nv = b.nt = 0; float rh[] = {1, -1, 1, 1}; lathe(b, rh, 2, 16, true, true); keep(P_DISC, b); }
  { b.nv = b.nt = 0; float rh[] = {1, -1, 1, 1}; lathe(b, rh, 2, 10, false, false); keep(P_TUBE, b); }
  // wedge: a roof, the ridge along x at the top
  { b.nv = b.nt = 0;
    int a = vtx(b, -1, -1, -1), c = vtx(b, -1, -1, 1), d = vtx(b, -1, 1, 0), e = vtx(b, 1, -1, -1), f = vtx(b, 1, -1, 1), g = vtx(b, 1, 1, 0);
    tri(b, a, c, d); tri(b, e, g, f);
    quad(b, a, d, g, e); quad(b, c, f, g, d); quad(b, a, e, f, c);
    keep(P_WEDGE, b); }
  // torus (around y, tube 0.3) and half a torus (arc, upright in the x-y plane: a rainbow, a handle)
  for (int k = 0; k < 2; k++) {
    b.nv = b.nt = 0;
    int N = k ? 9 : 12, M = 5; bool arc = k == 1;
    int cols = arc ? N + 1 : N;
    for (int i = 0; i < cols; i++) {
      float a = arc ? 3.14159f * i / N : 6.2831853f * i / N;
      for (int j = 0; j < M; j++) {
        float t = 6.2831853f * j / M, r = 1 + 0.3f * cosf(t), h = 0.3f * sinf(t);
        if (arc) vtx(b, r * cosf(a), r * sinf(a), h); else vtx(b, r * cosf(a), h, -r * sinf(a));
      }
    }
    for (int i = 0; i < N; i++) for (int j = 0; j < M; j++) {
      int i2 = arc ? i + 1 : (i + 1) % N, j2 = (j + 1) % M;
      if (arc) quad(b, i * M + j, i * M + j2, i2 * M + j2, i2 * M + j);
      else quad(b, i * M + j, i2 * M + j, i2 * M + j2, i * M + j2);
    }
    keep(arc ? P_ARC : P_TORUS, b);
  }
  // star and heart: flat outlines in the x-y plane, 0.4 thick (z -0.2..0.2), fans from the middle
  for (int k = 0; k < 2; k++) {
    b.nv = b.nt = 0;
    float px[24], py[24]; int n = 0;
    if (!k) for (int i = 0; i < 10; i++) { float a = 1.5708f + 6.2831853f * i / 10, r = i & 1 ? 0.42f : 1.0f; px[n] = r * cosf(a); py[n] = r * sinf(a); n++; }
    else for (int i = 0; i < 20; i++) {
      float t = 6.2831853f * i / 20;
      float x = 16 * powf(sinf(t), 3), y = 13 * cosf(t) - 5 * cosf(2 * t) - 2 * cosf(3 * t) - cosf(4 * t);
      px[n] = x / 17.0f; py[n] = (y + 2) / 17.0f; n++;
    }
    int cf = vtx(b, 0, 0, 0.2f), cb = vtx(b, 0, 0, -0.2f);
    for (int i = 0; i < n; i++) { vtx(b, px[i], py[i], 0.2f); vtx(b, px[i], py[i], -0.2f); }
    for (int i = 0; i < n; i++) {
      int i2 = (i + 1) % n;
      int f0 = 2 + i * 2, f1 = 2 + i2 * 2;
      // the outline goes counter-clockwise (seen from +z)
      tri(b, cf, f0, f1); tri(b, cb, f1 + 1, f0 + 1);
      quad(b, f0, f0 + 1, f1 + 1, f1);
    }
    keep(k ? P_HEART : P_STAR, b);
  }
  // rock: an icosahedron, a little uneven
  { b.nv = b.nt = 0; float t = 1.618034f;
    float P[12][3] = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    for (int i = 0; i < 12; i++) { float l = sqrtf(1 + t * t) * (0.86f + 0.14f * ((i * 7) % 5) / 4.0f); vtx(b, P[i][0] / l, P[i][1] / l, P[i][2] / l); }
    int F[20][3] = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                    {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
    for (int i = 0; i < 20; i++) tri(b, F[i][0], F[i][1], F[i][2]);
    keep(P_ROCK, b); }
  // ring: a flat band (planet rings, halos), both sides
  { b.nv = b.nt = 0; int N = 16;
    for (int i = 0; i < N; i++) { float a = 6.2831853f * i / N; vtx(b, cosf(a), 0, -sinf(a)); vtx(b, 0.7f * cosf(a), 0, -0.7f * sinf(a)); }
    for (int i = 0; i < N; i++) { int i2 = (i + 1) % N; quad(b, i * 2, i2 * 2, i2 * 2 + 1, i * 2 + 1); }
    keep(P_RING, b); }
}
const Mesh& primMesh(int p) { return meshes[p < 0 || p >= P_N ? 0 : p]; }

// ------------------------------------------------------------------ the frame
#define MAXT 7000
#define MAXS 700
#define MAXL 500
#define MAXD 360
#define BAND 40
struct STri { float x[3], y[3]; float A, B, C; int16_t ymin, ymax; uint16_t c; uint8_t fl; };
struct SSpr { float x, y, w, r; uint16_t c; uint8_t shape, a; };
struct SLin { float x0, y0, x1, y1, w0, w1; uint16_t c; uint8_t a; };
struct SDot { int16_t x, y; uint16_t c; uint8_t a; };
struct SDisc { float x, y, r, glow; Col c; };
static STri* tris; static int nTri, nTriA, lastTris;   // solid from the front, see-through from the back
static SSpr* sprs; static int nSpr;
static SLin* lins; static int nLin;
static SDot* dots; static int nDot;
static SDisc discs[4]; static int nDisc;
static uint16_t* bandC; static float* bandZ;
static uint16_t skyRow[RH][4];
float r3dFlash = 0;

static Vec eye, fwd, rgt, upv;
static float F, CX = RW / 2.0f, CY = RH / 2.0f;
static const float NEAR = 0.12f;
static Vec sunDir = {0.4f, 0.8f, 0.3f};
static Col sunC = {0.9f, 0.85f, 0.75f}, ambS = {0.35f, 0.4f, 0.5f}, ambG = {0.2f, 0.18f, 0.15f};
static float fogN = 20, fogF = 60; static Col fogC = {0.6f, 0.7f, 0.8f};
static Col fillC = {0, 0, 0};

bool r3dInit() {
  static bool done = false;
  if (done) return tris != nullptr;
  done = true;
  makeShapes();
  tris = (STri*)malloc(sizeof(STri) * MAXT);
  sprs = (SSpr*)malloc(sizeof(SSpr) * MAXS);
  lins = (SLin*)malloc(sizeof(SLin) * MAXL);
  dots = (SDot*)malloc(sizeof(SDot) * MAXD);
  bandC = (uint16_t*)fastMem(RW * BAND * 2);
  bandZ = (float*)fastMem(RW * BAND * 4);
  if (!tris || !sprs || !lins || !dots || !bandC || !bandZ) { free(tris); tris = nullptr; return false; }
  return true;
}
int r3dTris() { return lastTris; }

void r3dBegin(Vec e, Vec target, float fov, float roll) {
  eye = e;
  fwd = vnorm(vsub(target, e));
  rgt = vnorm(vcross(fwd, vec(0, 1, 0)));
  upv = vcross(rgt, fwd);
  if (roll) { float c = cosf(roll), s = sinf(roll); Vec r2 = vadd(vmul(rgt, c), vmul(upv, s)); upv = vsub(vmul(upv, c), vmul(rgt, s)); rgt = r2; }
  F = (RW / 2.0f) / tanf(fov * 3.14159f / 360.0f);
  nTri = 0; nTriA = 0; nSpr = 0; nLin = 0; nDot = 0; nDisc = 0;
}
void r3dLight(Vec toSun, Col sun, Col skyAmb, Col groundAmb) { sunDir = vnorm(toSun); sunC = sun; ambS = skyAmb; ambG = groundAmb; }
void r3dFill(Col c) { fillC = c; }
void r3dFog(float n, float f, Col c) { fogN = n; fogF = f > n ? f : n + 1; fogC = c; }

static inline Vec toView(Vec p) { Vec d = vsub(p, eye); return vec(vdot(d, rgt), vdot(d, upv), vdot(d, fwd)); }
bool r3dProject(Vec w, float& sx, float& sy, float& depth) {
  Vec v = toView(w);
  depth = v.z;
  if (v.z < NEAR) return false;
  sx = CX + F * v.x / v.z; sy = CY - F * v.y / v.z;
  return true;
}

void r3dSkyRows(Col top, Col hor, Col below) {
  static const float BAYER[4] = {-0.375f, 0.125f, -0.125f, 0.375f};
  for (int y = 0; y < RH; y++) {
    // the direction through this row's middle, how high above the horizon
    Vec d = vadd(fwd, vmul(upv, (CY - (y + 0.5f)) / F));
    float e = d.y / sqrtf(vdot(d, d));
    Col c;
    if (e >= 0) { float t = e / 0.85f; t = t > 1 ? 1 : sqrtf(t); c = clerp(hor, top, t); }
    else { float t = -e / 0.25f; c = clerp(hor, below, t > 1 ? 1 : t); }
    if (r3dFlash > 0) c = clerp(c, col(0.9f, 0.9f, 1), r3dFlash * 0.7f);
    for (int k = 0; k < 4; k++) {
      float o = BAYER[(k + (y & 1) * 2) & 3] / 31.0f;
      skyRow[y][k] = c565(col(c.r + o, c.g + o * 0.5f, c.b + o));
    }
  }
}
void r3dSkyDisc(Vec dir, float r, Col c, float glow) {
  if (nDisc >= 4) return;
  Vec v = vec(vdot(dir, rgt), vdot(dir, upv), vdot(dir, fwd));
  if (v.z <= 0.05f) return;
  SDisc& d = discs[nDisc++];
  d.x = CX + F * v.x / v.z; d.y = CY - F * v.y / v.z; d.r = r; d.glow = glow; d.c = c;
}
void r3dSkyDot(Vec dir, Col c, int a) {
  if (nDot >= MAXD) return;
  Vec v = vec(vdot(dir, rgt), vdot(dir, upv), vdot(dir, fwd));
  if (v.z <= 0.05f) return;
  float x = CX + F * v.x / v.z, y = CY - F * v.y / v.z;
  if (x < 0 || y < 0 || x >= RW || y >= RH) return;
  SDot& d = dots[nDot++]; d.x = (int16_t)x; d.y = (int16_t)y; d.c = c565(c); d.a = a;
}

// a lit, fogged triangle in view space -> the list (clipped at the near plane first)
static void addView(Vec* v, int n, uint16_t c, int fl);
static Col shade(Vec n, Col base, float depth, int fl) {
  Col o = base;
  if (!(fl & R_EMIT)) {
    float d = vdot(n, sunDir);
    if (fl & R_SOFT) d = (d + 0.6f) / 1.6f;
    if (d < 0) d = 0;
    float h = n.y * 0.5f + 0.5f;
    Col amb = clerp(ambG, ambS, h);
    float fd = -vdot(n, fwd); if (fd < 0) fd = 0;
    o = col(base.r * (amb.r + sunC.r * d + fillC.r * fd), base.g * (amb.g + sunC.g * d + fillC.g * fd), base.b * (amb.b + sunC.b * d + fillC.b * fd));
  }
  if (!(fl & R_NOFOG)) {
    float f = (depth - fogN) / (fogF - fogN);
    if (f > 0) o = clerp(o, fogC, f > 1 ? 1 : f);
  }
  if (r3dFlash > 0) o = clerp(o, col(1, 1, 1), r3dFlash * 0.45f);
  return o;
}
void r3dTri(Vec a, Vec b, Vec c, Col color, int fl) {
  Vec n = vcross(vsub(b, a), vsub(c, a));
  float len = sqrtf(vdot(n, n));
  if (len < 1e-12f) return;
  n = vmul(n, 1.0f / len);
  bool back = vdot(n, vsub(eye, a)) <= 0;
  if (back) { if (!(fl & R_NOCULL)) return; n = vmul(n, -1); }
  Vec v[3] = {toView(a), toView(b), toView(c)};
  if (v[0].z < NEAR && v[1].z < NEAR && v[2].z < NEAR) return;
  float depth = (v[0].z + v[1].z + v[2].z) * (1.0f / 3);
  if (depth > fogF + 4 && !(fl & R_NOFOG) && !(fl & R_EMIT)) return;   // lost in the fog
  addView(v, 3, c565(shade(n, color, depth, fl)), fl);
}
static void addOne(const Vec& a, const Vec& b, const Vec& c, uint16_t col, int fl) {
  float X[3], Y[3], W[3];
  const Vec* p[3] = {&a, &b, &c};
  for (int i = 0; i < 3; i++) { W[i] = 1.0f / p[i]->z; X[i] = CX + F * p[i]->x * W[i]; Y[i] = CY - F * p[i]->y * W[i]; }
  float mnx = fminf(X[0], fminf(X[1], X[2])), mxx = fmaxf(X[0], fmaxf(X[1], X[2]));
  float mny = fminf(Y[0], fminf(Y[1], Y[2])), mxy = fmaxf(Y[0], fmaxf(Y[1], Y[2]));
  if (mxx < 0 || mny >= RH || mxy < 0 || mnx >= RW) return;
  float det = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
  if (fabsf(det) < 1e-6f) return;
  bool alpha = fl & R_ALPHA;
  if (nTri + nTriA >= MAXT) return;
  // solid ones grow from the front of the list, see-through ones from the back
  STri& t = alpha ? tris[MAXT - 1 - nTriA++] : tris[nTri++];
  float id = 1.0f / det;
  t.A = ((W[1] - W[0]) * (Y[2] - Y[0]) - (W[2] - W[0]) * (Y[1] - Y[0])) * id;
  t.B = ((X[1] - X[0]) * (W[2] - W[0]) - (X[2] - X[0]) * (W[1] - W[0])) * id;
  t.C = W[0] - t.A * X[0] - t.B * Y[0];
  // sorted top to bottom
  int o[3] = {0, 1, 2};
  if (Y[o[1]] < Y[o[0]]) { int s = o[0]; o[0] = o[1]; o[1] = s; }
  if (Y[o[2]] < Y[o[1]]) { int s = o[1]; o[1] = o[2]; o[2] = s; }
  if (Y[o[1]] < Y[o[0]]) { int s = o[0]; o[0] = o[1]; o[1] = s; }
  for (int i = 0; i < 3; i++) { t.x[i] = X[o[i]]; t.y[i] = Y[o[i]]; }
  float y0 = ceilf(t.y[0] - 0.5f), y1 = ceilf(t.y[2] - 0.5f) - 1;
  t.ymin = (int16_t)(y0 < 0 ? 0 : y0 > RH ? RH : y0);
  t.ymax = (int16_t)(y1 < -1 ? -1 : y1 >= RH ? RH - 1 : y1);
  t.c = col; t.fl = (uint8_t)fl;
}
static void addView(Vec* v, int n, uint16_t c, int fl) {
  // clip at z = NEAR (Sutherland-Hodgman), then a fan
  Vec o[4]; int m = 0;
  for (int i = 0; i < n; i++) {
    const Vec& a = v[i]; const Vec& b = v[(i + 1) % n];
    bool ina = a.z >= NEAR, inb = b.z >= NEAR;
    if (ina) o[m++] = a;
    if (ina != inb) { float t = (NEAR - a.z) / (b.z - a.z); o[m++] = vlerp(a, b, t); }
  }
  for (int i = 1; i + 1 < m; i++) addOne(o[0], o[i], o[i + 1], c, fl);
}
void r3dMesh(int p, const Mat& w, Col c, int fl) {
  const Mesh& m = primMesh(p);
  Vec tv[200];
  int nv = m.nv > 200 ? 200 : m.nv;
  for (int i = 0; i < nv; i++) tv[i] = matApply(w, vec(m.v[i * 3], m.v[i * 3 + 1], m.v[i * 3 + 2]));
  // the whole thing behind the camera or far in the fog: skip
  Vec ctr = matApply(w, vec(0, 0, 0));
  float rad = fabsf(w.m[0]) + fabsf(w.m[1]) + fabsf(w.m[2]) + fabsf(w.m[4]) + fabsf(w.m[5]) + fabsf(w.m[6]) + fabsf(w.m[8]) + fabsf(w.m[9]) + fabsf(w.m[10]);
  Vec cv = toView(ctr);
  if (cv.z + rad < NEAR) return;
  if (!(fl & (R_NOFOG | R_EMIT)) && cv.z - rad > fogF + 4) return;
  for (int i = 0; i < m.nt; i++) r3dTri(tv[m.t[i * 3]], tv[m.t[i * 3 + 1]], tv[m.t[i * 3 + 2]], c, fl);
}
void r3dSprite(Vec p, float size, Col c, int shape, int a) {
  if (nSpr >= MAXS) return;
  Vec v = toView(p);
  if (v.z < 0.3f) return;
  SSpr& s = sprs[nSpr];
  s.w = 1.0f / v.z; s.x = CX + F * v.x * s.w; s.y = CY - F * v.y * s.w; s.r = size * F * s.w;
  if (s.r < 0.6f) s.r = 0.6f;
  if (s.r > 40) s.r = 40;
  if (s.x + s.r < 0 || s.x - s.r >= RW || s.y + s.r < 0 || s.y - s.r >= RH) return;
  float f = (v.z - fogN) / (fogF - fogN);
  if (f > 0) c = clerp(c, fogC, f > 1 ? 1 : f);
  s.c = c565(c); s.shape = shape; s.a = a; nSpr++;
}
void r3dStreak(Vec a, Vec b, Col c, int al) {
  if (nLin >= MAXL) return;
  Vec va = toView(a), vb = toView(b);
  const float NZ = 0.3f;
  if (va.z < NZ && vb.z < NZ) return;
  if (va.z < NZ) va = vlerp(va, vb, (NZ - va.z) / (vb.z - va.z));
  else if (vb.z < NZ) vb = vlerp(vb, va, (NZ - vb.z) / (va.z - vb.z));
  SLin& l = lins[nLin];
  l.w0 = 1.0f / va.z; l.w1 = 1.0f / vb.z;
  l.x0 = CX + F * va.x * l.w0; l.y0 = CY - F * va.y * l.w0; l.x1 = CX + F * vb.x * l.w1; l.y1 = CY - F * vb.y * l.w1;
  if (fmaxf(l.x0, l.x1) < 0 || fminf(l.x0, l.x1) >= RW || fmaxf(l.y0, l.y1) < 0 || fminf(l.y0, l.y1) >= RH) return;
  float f = ((va.z + vb.z) * 0.5f - fogN) / (fogF - fogN);
  if (f > 0) c = clerp(c, fogC, f > 1 ? 1 : f);
  l.c = c565(c); l.a = al; nLin++;
}

// ------------------------------------------------------------------ drawing a band
static inline uint16_t bl(uint16_t d, uint16_t s, uint32_t a) {
  uint32_t dd = (d | ((uint32_t)d << 16)) & 0x07E0F81F, ss = (s | ((uint32_t)s << 16)) & 0x07E0F81F;
  uint32_t r = ((((ss - dd) * a) >> 5) + dd) & 0x07E0F81F;
  return (uint16_t)(r | (r >> 16));
}
static void rasterTri(const STri& t, int b0, int b1) {
  int ya = t.ymin > b0 ? t.ymin : b0, yb = t.ymax < b1 - 1 ? t.ymax : b1 - 1;
  if (ya > yb) return;
  float x0 = t.x[0], y0 = t.y[0], x1 = t.x[1], y1 = t.y[1], x2 = t.x[2], y2 = t.y[2];
  float d02 = y2 - y0, d01 = y1 - y0, d12 = y2 - y1;
  float s02 = d02 > 1e-6f ? (x2 - x0) / d02 : 0, s01 = d01 > 1e-6f ? (x1 - x0) / d01 : 0, s12 = d12 > 1e-6f ? (x2 - x1) / d12 : 0;
  bool alpha = t.fl & R_ALPHA;
  uint16_t c = t.c;
  for (int py = ya; py <= yb; py++) {
    float yc = py + 0.5f;
    float xa = x0 + s02 * (yc - y0);
    float xb = yc < y1 ? x0 + s01 * (yc - y0) : x1 + s12 * (yc - y1);
    if (xa > xb) { float s = xa; xa = xb; xb = s; }
    int ix0 = (int)ceilf(xa - 0.5f), ix1 = (int)ceilf(xb - 0.5f);
    if (ix0 < 0) ix0 = 0;
    if (ix1 > RW) ix1 = RW;
    if (ix0 >= ix1) continue;
    float w = t.A * (ix0 + 0.5f) + t.B * yc + t.C;
    uint16_t* pc = bandC + (py - b0) * RW;
    float* pz = bandZ + (py - b0) * RW;
    if (!alpha) {
      for (int x = ix0; x < ix1; x++, w += t.A) if (w > pz[x]) { pz[x] = w; pc[x] = c; }
    } else {
      for (int x = ix0; x < ix1; x++, w += t.A) if (w > pz[x]) pc[x] = bl(pc[x], c, 15);
    }
  }
}
// sprites: 8x8 pictures (bit 7 = left), drawn with depth
static const uint8_t SPR_HEART[8] = {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};
static const uint8_t SPR_NOTE[8] = {0x0C, 0x0E, 0x0B, 0x09, 0x08, 0x78, 0xF8, 0x70};
static const uint8_t SPR_Z[8] = {0x00, 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00};
static const uint8_t SPR_Q[8] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00};
static const uint8_t SPR_STAR[8] = {0x18, 0x18, 0xFF, 0x7E, 0x3C, 0x7E, 0x66, 0x00};
static const uint8_t SPR_DROP[8] = {0x10, 0x10, 0x38, 0x38, 0x7C, 0x7C, 0x7C, 0x38};
static void drawSprite(const SSpr& s, int b0, int b1) {
  int r = (int)(s.r + 0.5f); if (r < 1) r = 1;
  int cx = (int)s.x, cy = (int)s.y;
  int ya = cy - r > b0 ? cy - r : b0, yb = cy + r < b1 - 1 ? cy + r : b1 - 1;
  const uint8_t* bm = s.shape == SP_HEART ? SPR_HEART : s.shape == SP_NOTE ? SPR_NOTE : s.shape == SP_Z ? SPR_Z : s.shape == SP_QUEST ? SPR_Q :
                      s.shape == SP_STAR ? SPR_STAR : s.shape == SP_DROP ? SPR_DROP : nullptr;
  for (int y = ya; y <= yb; y++) {
    uint16_t* pc = bandC + (y - b0) * RW; float* pz = bandZ + (y - b0) * RW;
    for (int x = cx - r; x <= cx + r; x++) {
      if (x < 0 || x >= RW || pz[x] > s.w) continue;
      int dx = x - cx, dy = y - cy;
      bool on;
      int a = s.a;
      switch (s.shape) {
        case SP_SQUARE: on = true; break;
        case SP_RING: { int d = dx * dx + dy * dy; on = d <= r * r && d >= (r - 1) * (r - 1) - 1; break; }
        case SP_SPARK: on = dx == 0 || dy == 0; if (on && (abs(dx) + abs(dy)) * 2 > r) a = a / 2; break;
        case SP_DISC: { int d = dx * dx + dy * dy; on = d <= r * r; if (on && r >= 3 && d > (r - 1) * (r - 1)) a = a / 2; break; }
        default: {
          int bx = (dx + r) * 8 / (2 * r + 1), by = (dy + r) * 8 / (2 * r + 1);
          on = bm && (bm[by & 7] >> (7 - (bx & 7))) & 1;
        }
      }
      if (on) pc[x] = a >= 32 ? s.c : bl(pc[x], s.c, a);
    }
  }
}
static void drawLine(const SLin& l, int b0, int b1) {
  // the part of the line inside this band
  float x0 = l.x0, y0 = l.y0, x1 = l.x1, y1 = l.y1, w0 = l.w0, w1 = l.w1;
  if (y0 > y1) { float s; s = x0; x0 = x1; x1 = s; s = y0; y0 = y1; y1 = s; s = w0; w0 = w1; w1 = s; }
  if (y1 < b0 || y0 >= b1) return;
  float dx = x1 - x0, dy = y1 - y0, dw = w1 - w0;
  float ta = 0, tb = 1;
  if (dy > 1e-4f) { ta = fmaxf(0, (b0 - y0) / dy); tb = fminf(1, (b1 - y0) / dy); }
  if (dx > 1e-4f) { ta = fmaxf(ta, (0 - x0) / dx); tb = fminf(tb, (RW - x0) / dx); }
  else if (dx < -1e-4f) { ta = fmaxf(ta, (RW - x0) / dx); tb = fminf(tb, (0 - x0) / dx); }
  if (ta > tb) return;
  float len = fmaxf(fabsf(dx), fabsf(dy)) * (tb - ta);
  int n = (int)len + 1;
  if (n > 600) n = 600;
  for (int i = 0; i <= n; i++) {
    float t = ta + (tb - ta) * i / n;
    int x = (int)(x0 + dx * t), y = (int)(y0 + dy * t);
    if (y < b0 || y >= b1 || x < 0 || x >= RW) continue;
    int o = (y - b0) * RW + x;
    if (bandZ[o] > (w0 + dw * t) * 1.002f) continue;
    bandC[o] = bl(bandC[o], l.c, l.a);
  }
}
static void drawDisc(const SDisc& d, int b0, int b1) {
  float R = d.r * (1 + d.glow * 2.5f);
  int ya = (int)(d.y - R), yb = (int)(d.y + R);
  if (ya < b0) ya = b0;
  if (yb > b1 - 1) yb = b1 - 1;
  uint16_t core = c565(d.c);
  for (int y = ya; y <= yb; y++) {
    uint16_t* pc = bandC + (y - b0) * RW;
    float fy = y + 0.5f - d.y;
    for (int x = (int)(d.x - R); x <= (int)(d.x + R); x++) {
      if (x < 0 || x >= RW) continue;
      float fx = x + 0.5f - d.x, r = sqrtf(fx * fx + fy * fy);
      if (r <= d.r) pc[x] = r > d.r - 1 ? bl(pc[x], core, (int)((d.r - r) * 32)) : core;
      else if (d.glow > 0 && r < R) { float g = 1 - (r - d.r) / (R - d.r); pc[x] = bl(pc[x], core, (int)(g * g * 14 * d.glow)); }
    }
  }
}
static void renderBand(int b0, int b1) {
  for (int y = b0; y < b1; y++) {
    uint32_t* p = (uint32_t*)(bandC + (y - b0) * RW);
    uint32_t a = skyRow[y][0] | ((uint32_t)skyRow[y][1] << 16), b = skyRow[y][2] | ((uint32_t)skyRow[y][3] << 16);
    for (int x = 0; x < RW / 2; x += 2) { p[x] = a; p[x + 1] = b; }
  }
  memset(bandZ, 0, RW * (b1 - b0) * 4);
  for (int i = 0; i < nDisc; i++) drawDisc(discs[i], b0, b1);
  for (int i = 0; i < nDot; i++) {
    const SDot& d = dots[i];
    if (d.y >= b0 && d.y < b1) { uint16_t* p = bandC + (d.y - b0) * RW + d.x; *p = bl(*p, d.c, d.a); }
  }
  for (int i = 0; i < nTri; i++) rasterTri(tris[i], b0, b1);
  for (int i = 0; i < nTriA; i++) rasterTri(tris[MAXT - 1 - i], b0, b1);
  for (int i = 0; i < nLin; i++) drawLine(lins[i], b0, b1);
  for (int i = 0; i < nSpr; i++) drawSprite(sprs[i], b0, b1);
}

// ------------------------------------------------------------------ 2x up into the screen
static inline uint16_t avg(uint16_t a, uint16_t b) { return ((a & 0xF7DE) >> 1) + ((b & 0xF7DE) >> 1); }
static void outRowSharp(uint16_t* out, const uint16_t* p, int y) {
  uint32_t* o = (uint32_t*)(out + (2 * y) * (RW * 2));
  for (int x = 0; x < RW; x++) o[x] = p[x] | ((uint32_t)p[x] << 16);
  memcpy(out + (2 * y + 1) * (RW * 2), o, RW * 4);
}
static void outRowSoft(uint16_t* out, const uint16_t* p, const uint16_t* n, int y) {
  uint16_t* o0 = out + (2 * y) * (RW * 2), *o1 = o0 + RW * 2;
  for (int x = 0; x < RW; x++) {
    uint16_t a = p[x], b = p[x + 1 < RW ? x + 1 : x], c = n[x], d = n[x + 1 < RW ? x + 1 : x];
    uint16_t ab = avg(a, b), cd = avg(c, d);
    o0[2 * x] = a; o0[2 * x + 1] = ab; o1[2 * x] = avg(a, c); o1[2 * x + 1] = avg(ab, cd);
  }
}
void r3dRender(uint16_t* out, bool soft) {
  lastTris = nTri + nTriA;
  static uint16_t pend[RW];
  for (int b0 = 0; b0 < RH; b0 += BAND) {
    int b1 = b0 + BAND < RH ? b0 + BAND : RH;
    renderBand(b0, b1);
    if (!soft) { for (int y = b0; y < b1; y++) outRowSharp(out, bandC + (y - b0) * RW, y); continue; }
    if (b0) outRowSoft(out, pend, bandC, b0 - 1);
    for (int y = b0; y + 1 < b1; y++) outRowSoft(out, bandC + (y - b0) * RW, bandC + (y + 1 - b0) * RW, y);
    memcpy(pend, bandC + (b1 - 1 - b0) * RW, RW * 2);
    if (b1 == RH) outRowSoft(out, pend, pend, RH - 1);
  }
}
