// gfx.cpp: see gfx.h
#include "gfx.h"
#include "holo.h"
#include "fonts.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
static void* bigAlloc(size_t n) { void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : malloc(n); }
#else
static void* bigAlloc(size_t n) { return malloc(n); }
#endif

uint16_t* fb = nullptr;

const Scheme SCHEMES[] = {
  {"GREEN", 0x39ff7a, 0x1b7a3c, 0x031208},
  {"AMBER", 0xffb43a, 0x8a5a12, 0x120a02},
  {"WHITE", 0xe9f0ff, 0x6b7080, 0x08090d},
  {"DOS BLUE", 0xf2f2ff, 0x8d98ff, 0x0019a6},
  {"CYAN", 0x45f0ff, 0x157a85, 0x021014},
  {"RED", 0xff5a6a, 0x8a2a34, 0x140306},
  {"PICO-8", 0xfff1e8, 0xff77a8, 0x1d2b53},       // PICO-8's own white, pink and dark blue (PICO-CUBE)
};
const int N_SCHEMES = sizeof(SCHEMES) / sizeof(SCHEMES[0]);
uint16_t C_FG, C_DIM, C_BG, C_RED, C_WARN, C_BLACK = 0, C_WHITE = 0xFFFF;
uint32_t garbleUntil = 0;
extern uint32_t msNow();

int clipX0 = 0, clipY0 = 0, clipX1 = SCR_W, clipY1 = SCR_H;
void clipSet(int x, int y, int w, int h) {
  clipX0 = x < 0 ? 0 : x; clipY0 = y < 0 ? 0 : y;
  clipX1 = x + w > SCR_W ? SCR_W : x + w; clipY1 = y + h > SCR_H ? SCR_H : y + h;
}
void clipReset() { clipX0 = 0; clipY0 = 0; clipX1 = SCR_W; clipY1 = SCR_H; }

static uint32_t rs = 0x9E3779B9;
uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

uint16_t mix(uint16_t a, uint16_t b, int pct) { return blend(a, b, pct * 32 / 100); }

// alpha tables: byte (letter << 4 | glow) -> 0..32
static uint8_t lutGlow[256], lutFlat[256];
static int curScheme = -1, curGlow = -1;

bool gfxInit() {
  if (!fb) fb = (uint16_t*)bigAlloc(SCR_W * SCR_H * 2);
  return fb != nullptr;
}

void gfxTheme(int scheme, int glow) {
  if (scheme < 0 || scheme >= N_SCHEMES) scheme = 0;
  if (glow < 0 || glow > 2) glow = 1;
  const Scheme& s = SCHEMES[scheme];
  C_FG = rgb(s.fg); C_DIM = rgb(s.dim); C_BG = rgb(s.bg);
  C_RED = rgb(0xff5a6a); C_WARN = rgb(0xffb43a);
  static const float GAIN[3] = {0.0f, 0.42f, 0.7f};
  for (int v = 0; v < 256; v++) {
    float core = (v >> 4) / 15.0f, gl = (v & 15) / 15.0f * GAIN[glow];
    float a = core + gl * (1 - core);
    lutGlow[v] = (uint8_t)(a * 32 + 0.5f);
    lutFlat[v] = (uint8_t)(core * 32 + 0.5f);
  }
  curScheme = scheme; curGlow = glow;
}

// the empty screen: the background colour (1.5: no glass picture behind, 460 KB less)
void gfxClear() { uint32_t c = C_BG | (uint32_t)C_BG << 16; uint32_t* p = (uint32_t*)fb; for (int i = 0; i < SCR_W * SCR_H / 2; i++) p[i] = c; }
void gfxClearTo(uint16_t c) { for (int i = 0; i < SCR_W * SCR_H; i++) fb[i] = c; }

// HOLO: everything is drawn moved by (gOX, gOY), the depth layer's shift. Clipping is in layout
// coordinates (before the shift), so a clipped list moves together with its rows.
int gOX = 0, gOY = 0;
void gfxOffset(int dx, int dy) { gOX = dx; gOY = dy; }
// a rectangle: clip in layout coordinates, then shift onto the screen. false = nothing to draw
static inline bool clipShift(int x, int y, int w, int h, int& x0, int& y0, int& x1, int& y1) {
  x0 = x < clipX0 ? clipX0 : x; y0 = y < clipY0 ? clipY0 : y;
  x1 = x + w > clipX1 ? clipX1 : x + w; y1 = y + h > clipY1 ? clipY1 : y + h;
  x0 += gOX; x1 += gOX; y0 += gOY; y1 += gOY;
  if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > SCR_W) x1 = SCR_W; if (y1 > SCR_H) y1 = SCR_H;
  return x0 < x1 && y0 < y1;
}
void pset(int x, int y, uint16_t c) {
  if (x < clipX0 || y < clipY0 || x >= clipX1 || y >= clipY1) return;
  x += gOX; y += gOY;
  if ((unsigned)x >= SCR_W || (unsigned)y >= SCR_H) return;
  fb[y * SCR_W + x] = c;
}
void pblend(int x, int y, uint16_t c, int a) {
  if (x < clipX0 || y < clipY0 || x >= clipX1 || y >= clipY1 || a <= 0) return;
  x += gOX; y += gOY;
  if ((unsigned)x >= SCR_W || (unsigned)y >= SCR_H) return;
  uint16_t* p = &fb[y * SCR_W + x];
  *p = a >= 32 ? c : blend(*p, c, a);
}
void fillRect(int x, int y, int w, int h, uint16_t c) {
  int x0, y0, x1, y1;
  if (!clipShift(x, y, w, h, x0, y0, x1, y1)) return;
  for (int yy = y0; yy < y1; yy++) { uint16_t* p = &fb[yy * SCR_W + x0]; for (int xx = x0; xx < x1; xx++) *p++ = c; }
}
void blendRect(int x, int y, int w, int h, uint16_t c, int a) {
  if (a >= 32) { fillRect(x, y, w, h, c); return; }
  if (a <= 0) return;
  int x0, y0, x1, y1;
  if (!clipShift(x, y, w, h, x0, y0, x1, y1)) return;
  for (int yy = y0; yy < y1; yy++) { uint16_t* p = &fb[yy * SCR_W + x0]; for (int xx = x0; xx < x1; xx++, p++) *p = blend(*p, c, a); }
}
void hline(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
void vline(int x, int y, int h, uint16_t c) { fillRect(x, y, 1, h, c); }
void rect(int x, int y, int w, int h, uint16_t c) { hline(x, y, w, c); hline(x, y + h - 1, w, c); vline(x, y, h, c); vline(x + w - 1, y, h, c); }
// the part of a line inside a box (Liang-Barsky). false = none of it. Lines are clipped before they're drawn:
// a line whose ends are far off the screen (a web app's maths) would otherwise take millions of steps
static bool clipLine(float& ax, float& ay, float& bx, float& by, float x0, float y0, float x1, float y1) {
  float t0 = 0, t1 = 1, dx = bx - ax, dy = by - ay;
  float p[4] = {-dx, dx, -dy, dy}, q[4] = {ax - x0, x1 - ax, ay - y0, y1 - ay};
  for (int i = 0; i < 4; i++) {
    if (p[i] == 0) { if (q[i] < 0) return false; continue; }
    float r = q[i] / p[i];
    if (p[i] < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
    else { if (r < t0) return false; if (r < t1) t1 = r; }
  }
  float nx0 = ax + t0 * dx, ny0 = ay + t0 * dy; bx = ax + t1 * dx; by = ay + t1 * dy; ax = nx0; ay = ny0;
  return true;
}
static bool clipToScreen(int& x0, int& y0, int& x1, int& y1, int m) {
  if (x0 >= -m && x0 < SCR_W + m && y0 >= -m && y0 < SCR_H + m && x1 >= -m && x1 < SCR_W + m && y1 >= -m && y1 < SCR_H + m) return true;
  float ax = x0, ay = y0, bx = x1, by = y1;
  if (!clipLine(ax, ay, bx, by, -m, -m, SCR_W - 1 + m, SCR_H - 1 + m)) return false;
  x0 = (int)lroundf(ax); y0 = (int)lroundf(ay); x1 = (int)lroundf(bx); y1 = (int)lroundf(by);
  return true;
}
void line(int x0, int y0, int x1, int y1, uint16_t c) {
  if (!clipToScreen(x0, y0, x1, y1, 1)) return;
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (;;) {
    pset(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}
void thickLine(int x0, int y0, int x1, int y1, int t, uint16_t c) {
  // round brush along the line
  if (t > 64) t = 64;
  if (!clipToScreen(x0, y0, x1, y1, t)) return;
  int dx = x1 - x0, dy = y1 - y0;
  int n = (int)sqrtf((float)(dx * dx + dy * dy)) + 1;
  for (int i = 0; i <= n; i++) fillCircle(x0 + dx * i / n, y0 + dy * i / n, t / 2, c);
}
void fillCircle(int cx, int cy, int r, uint16_t c) {
  if (r <= 0) { pset(cx, cy, c); return; }
  for (int y = -r; y <= r; y++) {
    int w = (int)sqrtf((float)(r * r - y * y) + 0.5f);
    fillRect(cx - w, cy + y, 2 * w + 1, 1, c);
  }
}
void ring(int cx, int cy, int r, int t, uint16_t c) {
  int R = r + t;
  for (int y = -R - 1; y <= R + 1; y++)
    for (int x = -R - 1; x <= R + 1; x++) {
      float d = sqrtf((float)(x * x + y * y));
      float a = 1.0f - fabsf(d - (r + t * 0.5f)) / (t * 0.5f + 0.5f);
      if (a > 0) pblend(cx + x, cy + y, c, (int)(a * 32));
    }
}
void glowRect(int x, int y, int w, int h, uint16_t c, int spread) {
  for (int i = 1; i <= spread; i++) {
    int a = 9 * (spread + 1 - i) / (spread + 1);
    blendRect(x - i, y - i, w + 2 * i, 1, c, a);
    blendRect(x - i, y + h - 1 + i, w + 2 * i, 1, c, a);
    blendRect(x - i, y - i + 1, 1, h + 2 * i - 2, c, a);
    blendRect(x + w - 1 + i, y - i + 1, 1, h + 2 * i - 2, c, a);
  }
}
void dosBox(int x, int y, int w, int h, uint16_t c) {
  // HOLO: on the text plane a box frame sits one layer back, behind its text, with walls going further back
  float cur = holoAt();
  bool base = fabsf(cur - H_TEXT) < 0.3f;
  HoloZ z(base ? H_PANEL : cur);
  holoWalls(x, y, w, h, c, base ? 0.9f : 0.5f, false);
  // like "border: 3px double" + box-shadow glow (outside and inside)
  glowRect(x, y, w, h, c, 5);
  for (int i = 1; i <= 4; i++) {
    int a = 7 * (5 - i) / 5;
    blendRect(x + 6 + i, y + 6 + i, w - 12 - 2 * i, 1, c, a);
    blendRect(x + 6 + i, y + h - 7 - i, w - 12 - 2 * i, 1, c, a);
    blendRect(x + 6 + i, y + 7 + i, 1, h - 14 - 2 * i, c, a);
    blendRect(x + w - 7 - i, y + 7 + i, 1, h - 14 - 2 * i, c, a);
  }
  for (int k = 0; k < 2; k++) {               // two 2-px lines with a 2-px gap
    int o = k * 4;
    fillRect(x + o, y + o, w - 2 * o, 2, c);
    fillRect(x + o, y + h - 2 - o, w - 2 * o, 2, c);
    fillRect(x + o, y + o, 2, h - 2 * o, c);
    fillRect(x + w - 2 - o, y + o, 2, h - 2 * o, c);
  }
}
void invertRect(int x, int y, int w, int h) {
  int x0, y0, x1, y1;
  if (!clipShift(x, y, w, h, x0, y0, x1, y1)) return;
  for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) fb[yy * SCR_W + xx] ^= 0xFFFF;
}

// ------------------------------------------------------------------ text
static const CubeFont* FONTS[3] = {&FONT_T, &FONT_B, &FONT_H};
const CubeFont& font(int f) { return *FONTS[f < 0 || f > 2 ? 0 : f]; }
int fontW(int f) { return font(f).cw; }
int fontH(int f) { return font(f).ch; }

uint32_t utf8Next(const char*& s) {
  uint8_t c = (uint8_t)*s;
  if (!c) return 0;
  s++;
  if (c < 0x80) return c;
  int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  uint32_t cp = c & (0x3F >> n);
  for (int i = 0; i < n && (*s & 0xC0) == 0x80; i++) cp = (cp << 6) | (*s++ & 0x3F);
  return cp;
}
int utf8Len(const char* s) { int n = 0; while (utf8Next(s)) n++; return n; }
const char* utf8At(const char* s, int n) { while (n-- > 0 && *s) utf8Next(s); return s; }
void utf8Cut(char* s, int maxChars) { char* p = (char*)utf8At(s, maxChars); *p = 0; }

static const CubeGlyph* findGlyph(const CubeFont& F, uint32_t cp) {
  int lo = 0, hi = F.n - 1;
  while (lo <= hi) {
    int m = (lo + hi) >> 1;
    if (F.g[m].cp == cp) return &F.g[m];
    if (F.g[m].cp < cp) lo = m + 1; else hi = m - 1;
  }
  return nullptr;
}
static uint32_t fallback(uint32_t cp) {
  // letters a font doesn't have: something close
  switch (cp) {
    case 0x2018: case 0x2019: return '\'';
    case 0x201C: case 0x201D: return '"';
    case 0x2013: case 0x2014: case 0x2212: return '-';
    case 0x2022: return 0xB7;
    case 0x00A0: return ' ';
  }
  if (cp >= 0x1F300) return '*';             // emoji
  return '?';
}
static const char GARBLE[] = "#$%&@*+=?/\\|~^<>{}[]";

int drawChar(int f, int x, int y, uint32_t cp, uint16_t c, int mode) {
  const CubeFont& F = font(f);
  if (garbleUntil && cp > ' ' && cp < 0x2580 && (rnd() % 100) < 6 && (int32_t)(garbleUntil - msNow()) > 0) cp = GARBLE[rnd() % 21];
  const CubeGlyph* g = findGlyph(F, cp);
  if (!g) { g = findGlyph(F, fallback(cp)); if (!g) g = findGlyph(F, '?'); if (!g) return F.cw; }
  if (!g->w) return F.cw;
  const uint8_t* lut = mode == TX_FLAT ? lutFlat : lutGlow;
  const uint8_t* d = F.data + g->off;
  int gx = x + g->x, gy = y + g->y;
  for (int j = 0; j < g->h; j++) {
    int yy = gy + j, sy = yy + gOY;
    if (yy < clipY0 || yy >= clipY1 || (unsigned)sy >= SCR_H) { d += g->w; continue; }
    uint16_t* row = &fb[sy * SCR_W + gOX];
    for (int i = 0; i < g->w; i++) {
      uint8_t v = *d++;
      if (!v) continue;
      int xx = gx + i;
      if (xx < clipX0 || xx >= clipX1 || (unsigned)(xx + gOX) >= SCR_W) continue;
      int a = lut[v];
      if (a) row[xx] = a >= 32 ? c : blend(row[xx], c, a);
    }
  }
  return F.cw;
}
int drawText(int f, int x, int y, const char* s, uint16_t c, int mode, int maxChars) {
  if (!s) return x;
  int cw = fontW(f);
  for (int n = 0; n < maxChars; n++) {
    uint32_t cp = utf8Next(s);
    if (!cp) break;
    if (cp != ' ') drawChar(f, x, y, cp, c, mode);
    x += cw;
  }
  return x;
}
int textW(int f, const char* s) { return utf8Len(s) * fontW(f); }
void drawTextC(int f, int cx, int y, const char* s, uint16_t c, int mode) { drawText(f, cx - textW(f, s) / 2, y, s, c, mode); }
void drawTextR(int f, int rx, int y, const char* s, uint16_t c, int mode) { drawText(f, rx - textW(f, s), y, s, c, mode); }

// ------------------------------------------------------------------ effects
void fxScanlines() {
  // like #scan on error.blue: one darker line in three
  for (int y = 0; y < SCR_H; y += 3) {
    uint16_t* p = &fb[y * SCR_W];
    for (int x = 0; x < SCR_W; x++) { uint16_t v = p[x]; p[x] = ((v & 0xF7DE) >> 1) + ((v & 0xE79C) >> 2); }
  }
}
void fxTear(int bands, uint32_t seed) {
  rs = seed | 1;
  for (int b = 0; b < bands; b++) {
    int y0 = rnd() % SCR_H, h = 4 + rnd() % 28, dx = (int)(rnd() % 41) - 20;
    for (int y = y0; y < y0 + h && y < SCR_H; y++) {
      uint16_t* p = &fb[y * SCR_W];
      if (dx > 0) { memmove(p + dx, p, (SCR_W - dx) * 2); for (int x = 0; x < dx; x++) p[x] = C_BG; }
      else if (dx < 0) { memmove(p, p - dx, (SCR_W + dx) * 2); for (int x = SCR_W + dx; x < SCR_W; x++) p[x] = C_BG; }
    }
  }
}
void fxStatic(int lines, uint32_t seed) {
  rs = seed | 1;
  for (int i = 0; i < lines; i++) {
    int y = rnd() % SCR_H, x = rnd() % (SCR_W / 2), w = rnd() % SCR_W;
    blendRect(x, y, w, 1 + rnd() % 2, C_FG, rnd() % 16);
  }
}
void fxBlock(uint32_t seed) {
  rs = seed | 1;
  for (int i = 0; i < 2; i++) {
    int w = 30 + rnd() % 160, h = 6 + rnd() % 40, x = rnd() % (SCR_W - w), y = rnd() % (SCR_H - h);
    if (rnd() & 1) invertRect(x, y, w, h); else blendRect(x, y, w, h, C_FG, 10 + rnd() % 12);
  }
}
void fxInvertBand(uint32_t seed) {
  rs = seed | 1;
  int y = rnd() % SCR_H, h = 10 + rnd() % 50;
  invertRect(0, y, SCR_W, h);
}
void fxShift(int dx, int dy) {
  if (dy) {
    int n = (dy % SCR_H + SCR_H) % SCR_H;
    // roll the picture: rows move down by n (with wrap), in place (three reversals, one row of help)
    if (n) {
      uint16_t row[SCR_W];
      auto rev = [&](int a, int b) { for (b--; a < b; a++, b--) { memcpy(row, &fb[a * SCR_W], SCR_W * 2); memcpy(&fb[a * SCR_W], &fb[b * SCR_W], SCR_W * 2); memcpy(&fb[b * SCR_W], row, SCR_W * 2); } };
      rev(0, SCR_H); rev(0, n); rev(n, SCR_H);
    }
  }
  if (dx) for (int y = 0; y < SCR_H; y++) {
    uint16_t* p = &fb[y * SCR_W];
    if (dx > 0) memmove(p + dx, p, (SCR_W - dx) * 2); else memmove(p, p - dx, (SCR_W + dx) * 2);
  }
}
