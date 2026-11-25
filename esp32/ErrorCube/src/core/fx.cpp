// fx.cpp: see fx.h
#include "fx.h"
#include "gfx.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
static void* bigAlloc(size_t n) { void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : malloc(n); }
#else
static void* bigAlloc(size_t n) { return malloc(n); }
#endif

static const int NPX = SCR_W * SCR_H;
static uint32_t* lut = nullptr;           // per screen pixel: where it comes from (bits 0-17) + how dark (bits 18-22)
static int8_t fcol[SCR_W], frow[SCR_H];   // colour fringe: how far red / blue sit apart, by column / row
static uint16_t* spare = nullptr;
static uint16_t* bufs[2] = {nullptr, nullptr};
static int lensMode = 0;

uint16_t* fxBuf(int i) { if (!bufs[i & 1]) bufs[i & 1] = (uint16_t*)bigAlloc(NPX * 2); return bufs[i & 1]; }
uint16_t* fxSpare() { return spare; }
int fxLensMode() { return lensMode; }

static void lensSrc(int mode, float ox, float oy, float& sx, float& sy) {
  float nx = (ox + 0.5f - 240) / 240, ny = (oy + 0.5f - 240) / 240, r = sqrtf(nx * nx + ny * ny), f;
  if (mode == 2) f = (1 + 0.22f * r * r) * 0.8367f;                       // fisheye: the middle bulges out
  else { float e = r > 0.55f ? r - 0.55f : 0; f = 1 + 0.17f * e * e; }   // curved glass: only the edges bend
  sx = 240 + nx * f * 240; sy = 240 + ny * f * 240;
}
void fxLensSetup(int mode) {
  if (mode == lensMode && (lut || !mode)) return;
  lensMode = mode;
  if (!mode) return;
  if (!lut) lut = (uint32_t*)bigAlloc(NPX * 4);
  if (!spare) spare = (uint16_t*)bigAlloc(NPX * 2);
  if (!lut || !spare) { lensMode = 0; return; }
  for (int y = 0; y < SCR_H; y++)
    for (int x = 0; x < SCR_W; x++) {
      float sx, sy;
      lensSrc(mode, x, y, sx, sy);
      // past the glass: black; the last few pixels fade out (a soft tube edge)
      float edge = fminf(fminf(sx, 479.5f - sx), fminf(sy, 479.5f - sy));
      uint32_t v;
      if (edge < 0) v = 0xFFFFFFFFu;
      else {
        int dark = edge < 4 ? (int)((4 - edge) * 7.5f) : 0; if (dark > 31) dark = 31;
        int ix = (int)sx, iy = (int)sy; if (ix > 479) ix = 479; if (iy > 479) iy = 479;
        v = (uint32_t)(iy * SCR_W + ix) | ((uint32_t)dark << 18);
      }
      lut[y * SCR_W + x] = v;
    }
  // the fringe grows toward the edges, up to 3 px
  for (int i = 0; i < SCR_W; i++) { float n = (i - 240) / 240.0f; fcol[i] = (int8_t)roundf(n * n * n * (mode == 2 ? 3.4f : 2.4f)); }
  for (int i = 0; i < SCR_H; i++) { float n = (i - 240) / 240.0f; frow[i] = (int8_t)roundf(n * n * n * (mode == 2 ? 3.0f : 2.0f)); }
}

void fxPost(bool fringe, bool scan) {
  if (!spare) spare = (uint16_t*)bigAlloc(NPX * 2);
  if (!spare) { if (scan) fxScanlines(); return; }
  const uint16_t* src = fb;
  uint16_t* out = spare;
  if (!lensMode || !lut) {
    memcpy(out, src, NPX * 2);
    uint16_t* t = fb; fb = spare; spare = t;
    if (scan) fxScanlines();
    return;
  }
  const uint32_t* L = lut;
  for (int y = 0; y < SCR_H; y++) {
    int fy = fringe ? frow[y] * SCR_W : 0;
    bool sl = scan && y % 3 == 0;
    for (int x = 0; x < SCR_W; x++) {
      uint32_t v = *L++;
      if (v == 0xFFFFFFFFu) { *out++ = 0; continue; }
      int idx = v & 0x3FFFF;
      uint16_t c = src[idx];
      if (fringe) {
        int off = fcol[x] + fy;
        if (off) {
          int a = idx + off, b = idx - off;
          if (a >= 0 && a < NPX && b >= 0 && b < NPX) c = (src[a] & 0xF800) | (c & 0x07E0) | (src[b] & 0x001F);
        }
      }
      int dark = v >> 18;
      if (dark) c = blend(c, 0, dark);
      *out++ = sl ? ((c & 0xF7DE) >> 1) + ((c & 0xE79C) >> 2) : c;
    }
  }
  // swap: fb now holds the picture through the lens, spare the one before it
  uint16_t* t = fb; fb = spare; spare = t;
}

void fxLensTouch(int& x, int& y) {
  if (!lensMode) return;
  float sx, sy;
  lensSrc(lensMode, x, y, sx, sy);
  x = sx < 0 ? 0 : sx > 479 ? 479 : (int)sx;
  y = sy < 0 ? 0 : sy > 479 ? 479 : (int)sy;
}

// ------------------------------------------------------------------ glitches
void fxRgbSplit(int dx) {
  static uint16_t row[SCR_W];
  for (int y = 0; y < SCR_H; y++) {
    uint16_t* p = &fb[y * SCR_W];
    memcpy(row, p, sizeof(row));
    for (int x = 0; x < SCR_W; x++) {
      int a = x - dx, b = x + dx;
      uint16_t r = a >= 0 && a < SCR_W ? row[a] : 0, bl = b >= 0 && b < SCR_W ? row[b] : 0;
      p[x] = (r & 0xF800) | (row[x] & 0x07E0) | (bl & 0x001F);
    }
  }
}
void fxVhs(uint32_t seed, float phase) {
  // a tracking band rolling down: rows pushed sideways, white speckles, a bit washed out
  int y0 = (int)(phase * (SCR_H + 80)) - 60, h = 34 + seed % 30;
  for (int y = y0; y < y0 + h; y++) {
    if (y < 0 || y >= SCR_H) continue;
    uint16_t* p = &fb[y * SCR_W];
    int dx = (int)(sinf(y * 0.7f + seed) * 9) + (int)(rnd() % 7) - 3;
    if (dx > 0) { memmove(p + dx, p, (SCR_W - dx) * 2); for (int x = 0; x < dx; x++) p[x] = 0; }
    else if (dx < 0) { memmove(p, p - dx, (SCR_W + dx) * 2); for (int x = SCR_W + dx; x < SCR_W; x++) p[x] = 0; }
    for (int k = 0; k < 18; k++) { int x = rnd() % SCR_W; p[x] = blend(p[x], 0xFFFF, 10 + rnd() % 22); }
    for (int x = 0; x < SCR_W; x += 2) p[x] = blend(p[x], 0x8410, 6);
  }
}
void fxInterlace(int dx) {
  for (int y = 1; y < SCR_H; y += 2) {
    uint16_t* p = &fb[y * SCR_W];
    if (dx > 0) memmove(p + dx, p, (SCR_W - dx) * 2); else if (dx < 0) memmove(p, p - dx, (SCR_W + dx) * 2);
  }
}
void fxDatamosh(uint32_t seed) {
  // blocks of the picture copied from a little way off, like a broken video stream
  static uint16_t tmp[64 * 64];
  rnd();
  for (int i = 0; i < 7; i++) {
    int w = 16 + (seed >> (i * 3)) % 48, h = 8 + (seed >> (i * 2 + 1)) % 40;
    int x = rnd() % (SCR_W - w), y = rnd() % (SCR_H - h);
    int sx = x + (int)(rnd() % 61) - 30, sy = y + (int)(rnd() % 31) - 15;
    if (sx < 0) sx = 0; if (sy < 0) sy = 0; if (sx > SCR_W - w) sx = SCR_W - w; if (sy > SCR_H - h) sy = SCR_H - h;
    for (int r = 0; r < h; r++) memcpy(&tmp[r * w], &fb[(sy + r) * SCR_W + sx], w * 2);
    for (int r = 0; r < h; r++) memcpy(&fb[(y + r) * SCR_W + x], &tmp[r * w], w * 2);
  }
}
void fxStretch(uint32_t seed) {
  // one row smeared down over the ones under it
  int y = 40 + seed % 380, h = 10 + (seed >> 8) % 50, x0 = (seed >> 4) % 200, w = 120 + (seed >> 12) % 280;
  if (x0 + w > SCR_W) w = SCR_W - x0;
  for (int r = 1; r < h && y + r < SCR_H; r++) memcpy(&fb[(y + r) * SCR_W + x0], &fb[y * SCR_W + x0], w * 2);
}
void fxSignalLoss(uint32_t seed) {
  // the picture drowns in snow, with a few dark sync bars
  int k = 0;
  for (int i = 0; i < NPX; i++) {
    if ((i & 1) == 0) k = rnd();
    int n = (k >> ((i & 1) * 8)) & 255;
    fb[i] = blend(fb[i], n > 128 ? C_FG : 0, n > 128 ? (n - 128) / 6 : 10);
  }
  for (int b = 0; b < 3; b++) { int y = (seed >> (b * 7)) % SCR_H; fillRect(0, y, SCR_W, 6 + b * 3, 0); }
}

// ------------------------------------------------------------------ the cube turn
void fxCube(const uint16_t* A, const uint16_t* B, float t, int dir, uint16_t* out) {
  const float F = 700, PI2 = 1.5707963f;
  float th = t * PI2;
  float Zc = F + 240 + 230 * sinf(t * 3.14159f);      // the cube backs away a little halfway, so you see it is a cube
  bool vert = dir < 2;
  // the two faces: angle of each (0 = facing you)
  float ang[2];
  if (dir == 0 || dir == 2) { ang[0] = -th; ang[1] = PI2 - th; } else { ang[0] = th; ang[1] = th - PI2; }
  const uint16_t* src[2] = {A, B};
  float cs[2], sn[2], shade[2]; bool vis[2];
  for (int f = 0; f < 2; f++) {
    cs[f] = cosf(ang[f]); sn[f] = sinf(ang[f]);
    vis[f] = Zc * cs[f] > 240;
    float s = (Zc * cs[f] - 240) / (Zc - 240); if (s < 0) s = 0; if (s > 1) s = 1;
    shade[f] = (1 - s) * 26;
  }
  // along the turning axis: for each line across it, which face and where on it
  static int16_t pos[SCR_W]; static uint8_t face[SCR_W]; static int32_t zf[SCR_W];
  for (int i = 0; i < SCR_W; i++) {
    float k = i + 0.5f - 240; pos[i] = -1;
    for (int f = 0; f < 2; f++) {
      if (!vis[f]) continue;
      float den = k * sn[f] - F * cs[f];
      if (fabsf(den) < 1e-3f) continue;
      float v = (F * 240 * sn[f] - k * (Zc - 240 * cs[f])) / den;
      if (v < -240 || v >= 240) continue;
      float z = v * sn[f] + Zc - 240 * cs[f];
      if (z <= 1) continue;
      pos[i] = (int16_t)(v + 240); face[i] = f; zf[i] = (int32_t)(z / F * 65536);
      break;
    }
  }
  for (int y = 0; y < SCR_H; y++) {
    uint16_t* o = out + y * SCR_W;
    if (vert) {
      // turning up / down: row y shows row pos[y] of one face, squeezed sideways by its distance
      if (pos[y] < 0) { memset(o, 0, SCR_W * 2); continue; }
      int f = face[y]; const uint16_t* r = src[f] + pos[y] * SCR_W; int z = zf[y], sh = (int)shade[f];
      for (int x = 0; x < SCR_W; x++) {
        int u = (((x - 240) * z) >> 16) + 240;
        o[x] = u < 0 || u >= SCR_W ? 0 : (sh ? blend(r[u], 0, sh) : r[u]);
      }
    } else {
      // turning sideways: column x shows column pos[x], squeezed up/down by its distance
      for (int x = 0; x < SCR_W; x++) {
        if (pos[x] < 0) { o[x] = 0; continue; }
        int v = (((y - 240) * zf[x]) >> 16) + 240;
        if (v < 0 || v >= SCR_H) { o[x] = 0; continue; }
        uint16_t c = src[face[x]][v * SCR_W + pos[x]];
        int sh = (int)shade[face[x]];
        o[x] = sh ? blend(c, 0, sh) : c;
      }
    }
  }
}
