// apps_map.cpp: MAP.EXE: maps made on the phone (error.blue/cube > MAP: a place and a size from OpenStreetMap),
// shown in 3D: buildings at their height, water, forests, grass, fields, roads, railways and trees.
// Drag = move, tilt the cube = turn and look up/down, - / + = zoom, hold = menu.
//
// The file (.map, made by public/cube/map.js), little endian, coordinates in decimetres from the middle (x east, y north):
//   "CMAP" u8 ver(1) u8 0 u16 sizeMetres f32 lat f32 lon, then sections: u8 tag, u32 count:
//   'A' areas:     u8 kind, u16 n, n x (i16 x, i16 y)       kind: 1 water 2 forest 3 grass 4 farmland 5 sand 6 parking 7 pitch
//   'B' buildings: u8 height (m), u8 kind, u16 n, points      kind: 0 other 1 house 2 flats 3 shop 4 industry 5 public 6 church
//   'R' roads:     u8 kind, u16 n, points                     kind: 0 path 1 small 2 street 3 main 4 big 5 rail 6 river
//   'T' trees:     i16 x, i16 y, u8 height (m)
//   'N' name:      u8 len, chars
#include "../ui/ui.h"
#include "../core/g3d.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#include <new>
#ifdef ARDUINO
#include <esp_heap_caps.h>
template <class T> struct PsAlloc {          // big map data in PSRAM
  typedef T value_type;
  PsAlloc() {}
  template <class U> PsAlloc(const PsAlloc<U>&) {}
  T* allocate(size_t n) { void* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); if (!p) p = malloc(n * sizeof(T)); return (T*)p; }
  void deallocate(T* p, size_t) { free(p); }
  template <class U> bool operator==(const PsAlloc<U>&) const { return true; }
  template <class U> bool operator!=(const PsAlloc<U>&) const { return false; }
};
template <class T> using PVec = std::vector<T, PsAlloc<T>>;
#else
template <class T> using PVec = std::vector<T>;
#endif

static uint16_t c565(int r, int g, int b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static uint16_t shade(uint16_t c, float k) {
  int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  r = (int)(r * k); g = (int)(g * k); b = (int)(b * k);
  if (r > 31) r = 31; if (g > 63) g = 63; if (b > 31) b = 31;
  return (r << 11) | (g << 5) | b;
}
static uint16_t lerp565(uint16_t a, uint16_t b, float t) {      // t 0 = a, 1 = b
  if (t <= 0) return a; if (t >= 1) return b;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31, br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return ((int)(ar + (br - ar) * t) << 11) | ((int)(ag + (bg - ag) * t) << 5) | (int)(ab + (bb - ab) * t);
}

// ------------------------------------------------------------------ the map in memory
struct MShape { uint8_t kind, h; uint16_t n; uint32_t off; float cx, cy, r; };
struct MTree { float x, y; uint8_t h; };
struct MapData {
  char name[40] = "";
  float lat = 0, lon = 0; int size = 0;
  PVec<float> pts;                       // x, y pairs (metres)
  PVec<MShape> areas, builds, roads;
  PVec<MTree> trees;
  void clear() { pts.clear(); areas.clear(); builds.clear(); roads.clear(); trees.clear(); name[0] = 0; }
};
static MapData* M = nullptr;

static bool rd(const uint8_t*& p, const uint8_t* e, void* o, size_t n) { if (p + n > e) return false; memcpy(o, p, n); p += n; return true; }
static bool readShape(const uint8_t*& p, const uint8_t* e, MShape& s, bool closed) {
  uint16_t n; if (!rd(p, e, &n, 2) || n < 2 || p + n * 4 > e) return false;
  s.n = n; s.off = M->pts.size() / 2;
  float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
  for (int i = 0; i < n; i++) {
    int16_t xy[2]; memcpy(xy, p, 4); p += 4;
    float x = xy[0] * 0.1f, y = xy[1] * 0.1f;
    M->pts.push_back(x); M->pts.push_back(y);
    if (x < minx) minx = x; if (x > maxx) maxx = x; if (y < miny) miny = y; if (y > maxy) maxy = y;
  }
  // a closed ring: no repeated last point, and counter-clockwise (so walls know their outside)
  float* q = &M->pts[s.off * 2];
  if (closed && n > 3 && q[0] == q[(n - 1) * 2] && q[1] == q[(n - 1) * 2 + 1]) { s.n = --n; M->pts.resize(M->pts.size() - 2); q = &M->pts[s.off * 2]; }
  if (closed) {
    float a = 0; for (int i = 0; i < n; i++) { int j = (i + 1) % n; a += q[i * 2] * q[j * 2 + 1] - q[j * 2] * q[i * 2 + 1]; }
    if (a < 0) for (int i = 0; i < n / 2; i++) for (int k = 0; k < 2; k++) std::swap(q[i * 2 + k], q[(n - 1 - i) * 2 + k]);
  }
  s.cx = (minx + maxx) / 2; s.cy = (miny + maxy) / 2;
  s.r = sqrtf((maxx - minx) * (maxx - minx) + (maxy - miny) * (maxy - miny)) / 2;
  return true;
}
static bool mapLoad(const char* path, char* err) {
  long n = fsSize(path);
  if (n < 16 || n > 8 * 1024 * 1024) { strcpy(err, "NOT A MAP FILE"); return false; }
  uint8_t* b = (uint8_t*)malloc(n);
  size_t got = 0;
  if (!b || !fsRead(path, b, n, got) || (long)got != n) { free(b); strcpy(err, "CAN'T READ IT"); return false; }
  if (!M) { void* m = malloc(sizeof(MapData)); M = new (m) MapData(); }
  M->clear();
  const uint8_t* p = b; const uint8_t* e = b + n;
  bool ok = !memcmp(p, "CMAP", 4) && p[4] == 1;
  if (ok) {
    p += 6; uint16_t sz; rd(p, e, &sz, 2); M->size = sz; rd(p, e, &M->lat, 4); rd(p, e, &M->lon, 4);
    while (ok && p < e) {
      uint8_t tag; uint32_t cnt;
      if (!rd(p, e, &tag, 1) || !rd(p, e, &cnt, 4)) break;
      if (tag == 'N') { uint8_t l = cnt; if (p + l > e) break; memcpy(M->name, p, l < 39 ? l : 39); M->name[l < 39 ? l : 39] = 0; p += l; continue; }
      for (uint32_t i = 0; i < cnt && ok; i++) {
        MShape s = {0, 0, 0, 0, 0, 0, 0};
        if (tag == 'A') { ok = rd(p, e, &s.kind, 1) && readShape(p, e, s, true); if (ok && s.n >= 3) M->areas.push_back(s); }
        else if (tag == 'B') { ok = rd(p, e, &s.h, 1) && rd(p, e, &s.kind, 1) && readShape(p, e, s, true); if (ok && s.n >= 3) M->builds.push_back(s); }
        else if (tag == 'R') { ok = rd(p, e, &s.kind, 1) && readShape(p, e, s, false); if (ok) M->roads.push_back(s); }
        else if (tag == 'T') { int16_t xy[2]; uint8_t h; ok = rd(p, e, xy, 4) && rd(p, e, &h, 1); if (ok) M->trees.push_back({xy[0] * 0.1f, xy[1] * 0.1f, h}); }
        else ok = false;
      }
    }
  }
  free(b);
  if (!ok && M->builds.empty() && M->areas.empty() && M->roads.empty()) { strcpy(err, "THE MAP FILE IS BROKEN"); return false; }
  // ground areas: the big ones first, water last (it lies on top)
  std::sort(M->areas.begin(), M->areas.end(), [](const MShape& a, const MShape& b) {
    int pa = a.kind == 1 ? 9 : a.kind, pb = b.kind == 1 ? 9 : b.kind;
    return pa != pb ? pa < pb : a.r > b.r;
  });
  std::sort(M->roads.begin(), M->roads.end(), [](const MShape& a, const MShape& b) { return (a.kind == 6 ? -1 : a.kind) < (b.kind == 6 ? -1 : b.kind); });
  return true;
}

// ------------------------------------------------------------------ the camera and drawing
static float camX, camY, camZ, rx, ry, ux, uy, uz, fx, fy, fz;     // position, right, up, forward
static const float F = 400, NEAR = 1.5f;
struct CP { float x, y, z; };
static inline CP toCam(float x, float y, float z) {
  float dx = x - camX, dy = y - camY, dz = z - camZ;
  return {dx * rx + dy * ry, dx * ux + dy * uy + dz * uz, dx * fx + dy * fy + dz * fz};
}
// a filled polygon in camera space: cut at the near plane, projected, scanline filled (opaque)
static float sxy[2 * 260];
static void fillScreen(const float* p, int n, uint16_t c) {
  if (n < 3) return;
  float minY = 1e9f, maxY = -1e9f;
  for (int i = 0; i < n; i++) { if (p[i * 2 + 1] < minY) minY = p[i * 2 + 1]; if (p[i * 2 + 1] > maxY) maxY = p[i * 2 + 1]; }
  int y0 = (int)ceilf(minY - 0.5f), y1 = (int)floorf(maxY - 0.5f);
  if (y0 < 0) y0 = 0; if (y1 > SCR_H - 1) y1 = SCR_H - 1;
  float xs[48];
  for (int y = y0; y <= y1; y++) {
    float sy = y + 0.5f; int k = 0;
    for (int i = 0, j = n - 1; i < n && k < 48; j = i++) {
      float ay = p[j * 2 + 1], by = p[i * 2 + 1];
      if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) { float ax = p[j * 2], bx = p[i * 2]; xs[k++] = ax + (sy - ay) * (bx - ax) / (by - ay); }
    }
    for (int a = 1; a < k; a++) { float t = xs[a]; int b = a - 1; while (b >= 0 && xs[b] > t) { xs[b + 1] = xs[b]; b--; } xs[b + 1] = t; }
    uint16_t* row = fb + y * SCR_W;
    for (int i = 0; i + 1 < k; i += 2) {
      int xa = (int)ceilf(xs[i] - 0.5f), xb = (int)floorf(xs[i + 1] - 0.5f);
      if (xa < 0) xa = 0; if (xb > SCR_W - 1) xb = SCR_W - 1;
      for (int x = xa; x <= xb; x++) row[x] = c;
    }
  }
}
static void fillCam(const CP* v, int n, uint16_t c) {
  static CP cl[260];
  int m = 0;
  for (int i = 0; i < n && m < 258; i++) {
    const CP& a = v[i]; const CP& b = v[(i + 1) % n];
    bool ain = a.z >= NEAR, bin = b.z >= NEAR;
    if (ain) cl[m++] = a;
    if (ain != bin) { float t = (NEAR - a.z) / (b.z - a.z); cl[m++] = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, NEAR}; }
  }
  if (m < 3) return;
  bool any = false;
  for (int i = 0; i < m; i++) {
    sxy[i * 2] = 240 + F * cl[i].x / cl[i].z; sxy[i * 2 + 1] = 240 - F * cl[i].y / cl[i].z;
    if (sxy[i * 2] > -2000 && sxy[i * 2] < 2480) any = true;
  }
  if (any) fillScreen(sxy, m, c);
}
static void mapDot(int x, int y, int r, uint16_t c) {
  for (int dy = -r; dy <= r; dy++) {
    int yy = y + dy; if (yy < 0 || yy >= SCR_H) continue;
    int w = (int)sqrtf((float)(r * r - dy * dy));
    int xa = x - w < 0 ? 0 : x - w, xb = x + w > SCR_W - 1 ? SCR_W - 1 : x + w;
    uint16_t* row = fb + yy * SCR_W;
    for (int xx = xa; xx <= xb; xx++) row[xx] = c;
  }
}

static uint16_t areaCol(int k) {
  switch (k) {
    case 1: return c565(40, 90, 170);    // water
    case 2: return c565(30, 92, 40);     // forest
    case 3: return c565(78, 140, 62);    // grass, park
    case 4: return c565(150, 150, 84);   // farmland
    case 5: return c565(200, 186, 130);  // sand
    case 6: return c565(110, 110, 116);  // parking
    case 7: return c565(60, 150, 90);    // pitch
    default: return c565(90, 110, 80);
  }
}
static uint16_t buildCol(int k, int h) {
  switch (k) {
    case 1: return c565(214, 190, 160);  // house
    case 2: return c565(190, 180, 176);  // flats
    case 3: return c565(200, 160, 120);  // shop
    case 4: return c565(160, 160, 168);  // industry
    case 5: return c565(196, 170, 150);  // public
    case 6: return c565(220, 210, 190);  // church
    default: return h > 20 ? c565(170, 176, 190) : c565(200, 186, 170);
  }
}

// ------------------------------------------------------------------ the view
struct MapView : Screen {
  std::string path; char err[48] = "";
  float tx = 0, ty = 0, yaw = 0, pitch = 0.75f, dist = 320;     // looking at (tx, ty) from dist metres, pitch radians down
  float tiltYaw = 0, tiltPitch = 0; bool tiltOn = true, top = false;
  float g0[3] = {0, 0, 1}; bool g0ok = false;
  uint32_t lastT = 0, helpUntil = 0, frames = 0, fpsAt = 0; int fps = 0;
  int dragX = -1, dragY = -1;
  MapView(const char* p) : path(p) {}
  const char* name() override { return "MAP.EXE"; }
  bool fullScreen() override { return true; }
  bool plain() override { return true; }
  bool keepAwake() override { return true; }
  bool noSwipeBack() override { return true; }
  void enter() override {
    if (!err[0] && (!M || path.size())) {
      if (!mapLoad(path.c_str(), err)) { std::string e = err; uiBack(); ask("MAP", e.c_str(), {"OK"}, nullptr); return; }
      path.clear();
      dist = M->size > 0 ? M->size * 0.45f : 320; if (dist < 120) dist = 120; if (dist > 900) dist = 900;
      helpUntil = msNow() + 4500;
      g0ok = false;
    }
    lastT = msNow();
  }
  void reset() { tx = ty = 0; yaw = 0; pitch = 0.75f; dist = M && M->size ? M->size * 0.45f : 320; g0ok = false; top = false; }
  void tick() override {
    uint32_t now = msNow(); float dt = (now - lastT) / 1000.0f; if (dt > 0.2f) dt = 0.2f; lastT = now;
    // tilt: left/right turns, forward/back looks down/up (from how it was held when the map opened)
    float a[3], g[3];
    if (tiltOn && imuRead(a, g)) {
      float l = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); if (l < 1) l = 1;
      float u[3] = {a[0] / l, a[1] / l, a[2] / l};
      if (!g0ok) { for (int i = 0; i < 3; i++) g0[i] = u[i]; g0ok = true; }
      float lr = atan2f(u[0], sqrtf(u[1] * u[1] + u[2] * u[2])) - atan2f(g0[0], sqrtf(g0[1] * g0[1] + g0[2] * g0[2]));
      float ud = atan2f(u[1], u[2]) - atan2f(g0[1], g0[2]);
      while (ud > 3.14159f) ud -= 6.28318f; while (ud < -3.14159f) ud += 6.28318f;
      auto dz = [](float v) { const float D = 0.12f; return v > D ? v - D : v < -D ? v + D : 0.0f; };
      float ty_ = -dz(lr) * 1.6f, tp = dz(ud) * 1.2f;
      if (ty_ != 0 || tp != 0) { yaw += ty_ * dt * 2; if (!top) pitch += tp * dt * 1.2f; uiDirty(); }
    }
    if (pitch < 0.25f) pitch = 0.25f; if (pitch > 1.5f) pitch = 1.5f;
    if (now < helpUntil) redrawIn(250);
  }
  void cameraSet() {
    float p = top ? 1.52f : pitch;
    float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(p), cpp = cosf(p);
    fx = sy * cpp; fy = cy * cpp; fz = -sp;        // forward
    rx = cy; ry = -sy;                            // right (on the ground)
    ux = sy * sp; uy = cy * sp; uz = cpp;         // up
    camX = tx - fx * dist; camY = ty - fy * dist; camZ = -fz * dist;
  }
  void draw() override {
    if (!M) { gfxClearTo(0); return; }
    cameraSet();
    uint16_t sky = c565(120, 170, 220), haze = c565(150, 176, 190), ground = c565(70, 92, 62);
    // the horizon: where the ground goes to the distance
    float p = top ? 1.52f : pitch;
    int hy = (int)(240 - F * tanf(p));
    for (int y = 0; y < SCR_H; y++) {
      uint16_t c;
      if (y < hy) c = lerp565(sky, haze, 1 - (hy - y) / 160.0f);
      else c = lerp565(haze, ground, (y - hy) / 90.0f);
      uint16_t* row = fb + y * SCR_W; for (int x = 0; x < SCR_W; x++) row[x] = c;
    }
    float maxD = dist * 3.2f + 200;              // how far things are drawn
    float maxD2 = maxD * maxD;
    auto far = [&](float x, float y, float r) { float dx = x - tx, dy = y - ty; return dx * dx + dy * dy > (maxD + r) * (maxD + r); };
    auto fogT = [&](float z) { float t = (z - maxD * 0.45f) / (maxD * 0.55f); return t < 0 ? 0 : t > 1 ? 1 : t; };
    static CP v[260];
    // 1: the ground: fields, grass, forest, sand, water
    for (const MShape& s : M->areas) {
      if (far(s.cx, s.cy, s.r)) continue;
      int n = s.n < 256 ? s.n : 256;
      const float* q = &M->pts[s.off * 2];
      for (int i = 0; i < n; i++) v[i] = toCam(q[i * 2], q[i * 2 + 1], 0);
      CP c = toCam(s.cx, s.cy, 0);
      fillCam(v, n, lerp565(areaCol(s.kind), haze, fogT(c.z) * 0.8f));
    }
    // 2: roads (ribbons on the ground), rivers, railways
    for (const MShape& s : M->roads) {
      if (far(s.cx, s.cy, s.r)) continue;
      static const float W[7] = {1.2f, 2.5f, 3.5f, 5.5f, 8.0f, 2.2f, 6.0f};
      float w = W[s.kind % 7] * 0.5f;
      uint16_t col = s.kind == 6 ? areaCol(1) : s.kind == 5 ? c565(90, 80, 80) : s.kind == 0 ? c565(196, 180, 150) : s.kind >= 3 ? c565(236, 226, 200) : c565(206, 206, 206);
      const float* q = &M->pts[s.off * 2];
      for (int i = 0; i + 1 < s.n; i++) {
        float ax = q[i * 2], ay = q[i * 2 + 1], bx = q[i * 2 + 2], by = q[i * 2 + 3];
        float dx = bx - ax, dy = by - ay, l = sqrtf(dx * dx + dy * dy); if (l < 0.01f) continue;
        float nx = -dy / l * w, ny = dx / l * w;
        CP m = toCam((ax + bx) / 2, (ay + by) / 2, 0);
        if (m.z > maxD) continue;
        uint16_t cc = lerp565(col, haze, fogT(m.z) * 0.8f);
        if (m.z > 250 && s.kind < 3) {           // far small roads: a line is enough
          CP A = toCam(ax, ay, 0), B = toCam(bx, by, 0);
          if (A.z > NEAR && B.z > NEAR) aaLine(240 + F * A.x / A.z, 240 - F * A.y / A.z, 240 + F * B.x / B.z, 240 - F * B.y / B.z, cc, 24);
          continue;
        }
        v[0] = toCam(ax + nx, ay + ny, 0.05f); v[1] = toCam(bx + nx, by + ny, 0.05f);
        v[2] = toCam(bx - nx, by - ny, 0.05f); v[3] = toCam(ax - nx, ay - ny, 0.05f);
        fillCam(v, 4, cc);
        if (s.kind == 5 && (i & 1)) { v[0] = toCam(ax + nx * 0.3f, ay + ny * 0.3f, 0.08f); v[1] = toCam(bx + nx * 0.3f, by + ny * 0.3f, 0.08f); v[2] = toCam(bx - nx * 0.3f, by - ny * 0.3f, 0.08f); v[3] = toCam(ax - nx * 0.3f, ay - ny * 0.3f, 0.08f); fillCam(v, 4, c565(170, 170, 170)); }
      }
    }
    // 3: buildings and trees, the far ones first
    struct Obj { float d; int i; };
    static PVec<Obj> objs;
    objs.clear();
    for (int i = 0; i < (int)M->builds.size(); i++) {
      const MShape& s = M->builds[i];
      float dx = s.cx - camX, dy = s.cy - camY, d2 = dx * dx + dy * dy;
      if (far(s.cx, s.cy, s.r)) continue;
      CP c = toCam(s.cx, s.cy, s.h * 0.5f);
      if (c.z < -s.r) continue;                     // behind the camera
      if (c.z > s.r && fabsf(c.x) > c.z * 1.3f + s.r) continue;   // well off to the side
      objs.push_back({d2, i});
    }
    for (int i = 0; i < (int)M->trees.size(); i++) {
      const MTree& t = M->trees[i];
      float dx = t.x - camX, dy = t.y - camY, d2 = dx * dx + dy * dy;
      if (d2 > maxD2 * 0.6f) continue;
      CP c = toCam(t.x, t.y, 2);
      if (c.z < NEAR || fabsf(c.x) > c.z * 1.2f + 5) continue;
      objs.push_back({d2, -1 - i});
    }
    std::sort(objs.begin(), objs.end(), [](const Obj& a, const Obj& b) { return a.d > b.d; });
    const float LX = -0.55f, LY = 0.83f;           // light from the north-west
    for (const Obj& o : objs) {
      if (o.i < 0) {                                // a tree: trunk + round top
        const MTree& t = M->trees[-1 - o.i];
        float h = t.h ? t.h : 8;
        CP b = toCam(t.x, t.y, 0), m = toCam(t.x, t.y, h * 0.62f), tp = toCam(t.x, t.y, h);
        if (b.z < NEAR || tp.z < NEAR) continue;
        float fog = fogT(m.z);
        int bx = (int)(240 + F * b.x / b.z), by = (int)(240 - F * b.y / b.z), mx = (int)(240 + F * m.x / m.z), my = (int)(240 - F * m.y / m.z);
        int r = (int)(F * h * 0.32f / m.z); if (r < 1) r = 1; if (r > 90) r = 90;
        aaLine(bx, by, mx, my, lerp565(c565(96, 70, 40), haze, fog), 32);
        mapDot(mx, my, r, lerp565(c565(36, 100, 44), haze, fog * 0.8f));
        if (r > 2) mapDot(mx - r / 3, my - r / 3, r / 2, lerp565(c565(62, 140, 60), haze, fog * 0.8f));
        continue;
      }
      const MShape& s = M->builds[o.i];
      const float* q = &M->pts[s.off * 2];
      float h = s.h ? s.h : 7;
      CP c = toCam(s.cx, s.cy, h);
      float fog = fogT(c.z) * 0.85f;
      uint16_t base = buildCol(s.kind, s.h);
      int n = s.n < 250 ? s.n : 250;
      // walls that face the camera (the outline is counter-clockwise: the outside is to the right of each edge)
      if (c.z < 600 || F * s.r / (c.z + 1) > 3) {
        for (int i = 0; i < n; i++) {
          int j = (i + 1) % n;
          float ax = q[i * 2], ay = q[i * 2 + 1], bx = q[j * 2], by = q[j * 2 + 1];
          float ex = bx - ax, ey = by - ay, nx = ey, ny = -ex;   // outward normal
          if (nx * (camX - ax) + ny * (camY - ay) <= 0) continue;
          float l = sqrtf(nx * nx + ny * ny); if (l < 0.01f) continue;
          float lit = 0.62f + 0.38f * ((nx * LX + ny * LY) / l);
          v[0] = toCam(ax, ay, 0); v[1] = toCam(bx, by, 0); v[2] = toCam(bx, by, h); v[3] = toCam(ax, ay, h);
          fillCam(v, 4, lerp565(shade(base, lit), haze, fog));
        }
      }
      // the roof
      for (int i = 0; i < n; i++) v[i] = toCam(q[i * 2], q[i * 2 + 1], h);
      uint16_t roof = s.kind == 1 ? c565(176, 90, 70) : s.kind == 6 ? c565(120, 120, 130) : shade(base, 0.82f);
      fillCam(v, n, lerp565(roof, haze, fog));
    }
    // the overlay: compass, scale, name
    frames++; if (msNow() - fpsAt >= 1000) { fps = frames; frames = 0; fpsAt = msNow(); }
    int cxp = 440, cyp = 40;
    mapDot(cxp, cyp, 22, c565(0, 0, 0));
    float nxp = -sinf(yaw) * 16, nyp = -cosf(yaw) * 16;
    aaLine(cxp, cyp, cxp + nxp, cyp + nyp, c565(255, 80, 80), 32); aaLine(cxp, cyp, cxp - nxp, cyp - nyp, c565(220, 220, 220), 32);
    drawTextC(FT, cxp + (int)(nxp * 1.6f), cyp + (int)(nyp * 1.6f) - 12, "N", c565(255, 255, 255), TX_FLAT);
    char t[48]; snprintf(t, sizeof(t), "%s", M->name[0] ? M->name : "MAP"); utf8Cut(t, 24);
    blendRect(0, 0, 300, 34, 0, 18);
    drawText(FT, 8, 4, t, c565(255, 255, 255), TX_FLAT);
    int wm = (int)(dist * 1.2f);                  // metres across the screen at the middle
    if (wm >= 100) wm = wm / 10 * 10;
    snprintf(t, sizeof(t), "%dM", wm);
    blendRect(0, 446, 150, 34, 0, 18);
    drawText(FT, 8, 450, t, c565(255, 255, 255), TX_FLAT);
    if (msNow() < helpUntil) {
      blendRect(60, 330, 360, 106, 0, 22);
      drawTextC(FT, 240, 338, "DRAG = MOVE", c565(255, 255, 255), TX_FLAT);
      drawTextC(FT, 240, 368, "TILT = TURN / LOOK", c565(255, 255, 255), TX_FLAT);
      drawTextC(FT, 240, 398, "- + ZOOM  HOLD = MENU", c565(255, 255, 255), TX_FLAT);
    }
  }
  void menu() {
    ask("MAP", M && M->name[0] ? M->name : "", {top ? "3D VIEW" : "FROM ABOVE", "BACK TO THE MIDDLE", tiltOn ? "TILT: OFF" : "TILT: ON", "CLOSE THE MAP"},
        [this](int k) {
          if (k == 0) top = !top;
          else if (k == 1) reset();
          else if (k == 2) { tiltOn = !tiltOn; g0ok = false; }
          else if (k == 3) { uiBack(); return; }
          uiDirty();
        });
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY) {
      if (e.key == K_NEXT) { dist *= 0.75f; if (dist < 17) dist = 17; uiDirty(); return true; }
      if (e.key == K_PREV) { dist *= 1.33f; if (dist > 2500) dist = 2500; uiDirty(); return true; }
      if (e.key == K_OK) { menu(); return true; }
      if (e.key == K_BACK) { uiBack(); return true; }
      return false;
    }
    if (e.t == EV_LONG) { dragX = -1; menu(); return true; }
    if (e.t == EV_DOWN) { dragX = e.x; dragY = e.y; return true; }
    if (e.t == EV_MOVE && dragX >= 0) {
      // the ground follows the finger: metres per pixel at the middle of the view
      float k = dist / F;
      float ddx = (e.x - dragX) * k, ddy = (e.y - dragY) * k / sinf(top ? 1.52f : pitch);
      float sy = sinf(yaw), cy = cosf(yaw);
      // right on the ground = (cos, -sin), forward = (sin, cos): the ground goes with the finger
      tx += -cy * ddx + sy * ddy; ty += sy * ddx + cy * ddy;
      float lim = M && M->size ? M->size * 0.6f : 2000;
      if (tx < -lim) tx = -lim; if (tx > lim) tx = lim; if (ty < -lim) ty = -lim; if (ty > lim) ty = lim;
      dragX = e.x; dragY = e.y; uiDirty(); return true;
    }
    if (e.t == EV_UP || e.t == EV_TAP) { dragX = -1; return true; }
    return true;
  }
};
void mapOpen(const char* path) { uiOpen(new MapView(path)); }

// ------------------------------------------------------------------ MAP.EXE: the maps
struct MapFile { std::string path, name; long size; bool sd; const char* net = nullptr; };
// maps that come from error.blue (downloaded the first time)
struct NetMap { const char* name; const char* path; const char* file; };
static const NetMap NETMAPS[] = {{"HOME", "/cube/maps/home.map", "home.map"}};
struct Maps : Screen {
  std::vector<MapFile> maps; ListView L;
  const char* name() override { return "MAP.EXE"; }
  void load() {
    maps.clear();
    static FileEnt* e = nullptr;
    if (!e) e = (FileEnt*)malloc(sizeof(FileEnt) * 96);
    if (!e) return;
    sdMount();
    const char* dirs[2] = {"/sd/MAPS", "/c/maps"};
    for (int d = 0; d < 2; d++) {
      if (d == 0 && !S.sd) continue;
      int n = fsList(dirs[d], e, 96);
      for (int i = 0; i < n; i++) {
        size_t l = strlen(e[i].name);
        if (e[i].dir || l < 5 || strcasecmp(e[i].name + l - 4, ".map")) continue;
        MapFile m; m.path = std::string(dirs[d]) + "/" + e[i].name; m.name = std::string(e[i].name, l - 4); m.size = e[i].size; m.sd = d == 0;
        for (auto& ch : m.name) ch = toupper((unsigned char)ch);
        maps.push_back(m);
      }
    }
    std::sort(maps.begin(), maps.end(), [](const MapFile& a, const MapFile& b) { return strcasecmp(a.name.c_str(), b.name.c_str()) < 0; });
    for (const NetMap& nm : NETMAPS) {
      bool have = false;
      for (auto& m : maps) if (!strcasecmp((m.path.substr(m.path.rfind('/') + 1)).c_str(), nm.file)) have = true;
      if (!have) { MapFile m; m.name = nm.name; m.size = 0; m.sd = S.sd; m.net = nm.path; m.path = std::string(S.sd ? "/sd/MAPS/" : "/c/maps/") + nm.file; maps.push_back(m); }
    }
    L.n = maps.size(); if (L.sel >= L.n) L.sel = L.n ? L.n - 1 : 0;
    uiReveal();
  }
  void enter() override { L.setBox(15, 92, 450, 300); L.rowH = 52; load(); }
  void draw() override {
    pathLine("C:\\MAP>");
    L.n = maps.size();
    L.draw([&](int i, int x, int y, int w, int h, bool sel) {
      MapFile& m = maps[i];
      int ty = y + (h - LH) / 2;
      if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
      char nm[48]; snprintf(nm, sizeof(nm), "%s", m.name.c_str()); utf8Cut(nm, 20);
      drawText(FT, x + 10, ty, nm, sel ? C_BG : C_FG, sel ? TX_FLAT : TX_GLOW);
      char sz[24];
      if (m.net) snprintf(sz, sizeof(sz), "%s", getting == i ? (S.dlState == 1 ? "LOADING" : "...") : "\xE2\x86\x93 GET");
      else snprintf(sz, sizeof(sz), "%s %ldK", m.sd ? "A:" : "C:", (m.size + 1023) / 1024);
      drawTextR(FT, x + w - 10, ty, sz, sel ? C_BG : m.net ? C_WARN : C_DIM, TX_FLAT);
    }, [this](int i) { pick(i); });
    if (maps.empty()) { drawTextC(FT, 240, 200, "NO MAPS YET", C_DIM, TX_FLAT); drawTextC(FT, 240, 236, "Make one on your phone:", C_DIM, TX_FLAT); drawTextC(FT, 240, 268, "error.blue/cube > MAP", C_FG, TX_FLAT); }
    else drawText(FT, colX(0), rowY(12) + 4, "New: error.blue/cube > MAP", C_DIM, TX_FLAT);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Refresh", [this] { load(); }}});
  }
  int getting = -1;
  void pick(int i) {
    if (i < 0 || i >= (int)maps.size()) return;
    MapFile& m = maps[i];
    if (!m.net) { mapOpen(m.path.c_str()); return; }
    if (S.net != NET_UP || cfg.offline) { toast("NEEDS WIFI: IT COMES FROM ERROR.BLUE", 2000); return; }
    if (getting >= 0) return;
    getting = i; netDownload(m.net, m.path.c_str()); toast("GETTING THE MAP...", 1500);
  }
  void tick() override {
    L.tick();
    if (getting >= 0 && S.dlState != 1) {         // the download ended
      int i = getting; getting = -1;
      std::string p = i < (int)maps.size() ? maps[i].path : "";
      if (S.dlState == 2) { load(); mapOpen(p.c_str()); } else toast("COULD NOT GET THE MAP", 2000);
      S.dlState = 0;
    }
    if (getting >= 0) redrawIn(300);
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_LONG && e.y0 >= L.y && e.y0 < L.y + L.h) {
      int i = (int)((e.y0 - L.y + L.scroll) / L.rowH);
      if (i >= 0 && i < (int)maps.size() && !maps[i].net) {
        MapFile m = maps[i];
        ask(m.name.c_str(), "", {"OPEN", "DELETE", "CANCEL"}, [this, m](int k) {
          if (k == 0) mapOpen(m.path.c_str());
          else if (k == 1) { if (fsRemove(m.path.c_str())) toast("DELETED"); load(); }
        });
        return true;
      }
    }
    return L.ev(e, [this](int i) { pick(i); uiDirty(); });
  }
};
Screen* makeMaps() { return new Maps(); }
