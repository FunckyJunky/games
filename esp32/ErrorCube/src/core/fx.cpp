// fx.cpp: see fx.h
// 1.5: the memory-hungry parts are gone (the curved lens table 1.5 MB, the cube turn's two frames,
// the glass buffer, HOLO's perspective and reflection). What's left works on the picture itself.
#include "fx.h"
#include "gfx.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static const int NPX = SCR_W * SCR_H;

void fxPost(bool, bool scan) { if (scan) fxScanlines(); }

// ------------------------------------------------------------------ glitches
void fxRgbSplit(int dx) {
  uint16_t row[SCR_W];                       // on the stack: no RAM kept for it
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
