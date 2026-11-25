// gfx.cpp: see gfx.h
#include "gfx.h"
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
static uint16_t* bgImg = nullptr;               // the empty screen: colour + glass, copied in by gfxClear()

const Scheme SCHEMES[] = {
  {"GREEN", 0x39ff7a, 0x1b7a3c, 0x031208},
  {"AMBER", 0xffb43a, 0x8a5a12, 0x120a02},
  {"WHITE", 0xe9f0ff, 0x6b7080, 0x08090d},
  {"DOS BLUE", 0xf2f2ff, 0x8d98ff, 0x0019a6},
  {"CYAN", 0x45f0ff, 0x157a85, 0x021014},
  {"RED", 0xff5a6a, 0x8a2a34, 0x140306},
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
  if (!bgImg) bgImg = (uint16_t*)bigAlloc(SCR_W * SCR_H * 2);
  return fb && bgImg;
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
  if (scheme == curScheme && glow == curGlow) return;
  curScheme = scheme; curGlow = glow;
  // the glass: a little darker toward the edges, a faint shine at the top left (like #glass on error.blue)
  uint8_t br = (s.bg >> 16) & 255, bgc = (s.bg >> 8) & 255, bb = s.bg & 255;
  uint8_t fr = (s.fg >> 16) & 255, fg = (s.fg >> 8) & 255, fbb = s.fg & 255;
  for (int y = 0; y < SCR_H; y++) {
    for (int x = 0; x < SCR_W; x++) {
      float dx = (x - 240) / 240.0f, dy = (y - 240) / 240.0f;
      float d = sqrtf(dx * dx + dy * dy);
      float v = d < 0.7f ? 1.0f : 1.0f - (d - 0.7f) * 0.9f;
      if (v < 0.35f) v = 0.35f;
      float sx = (x - 140) / 260.0f, sy = (y - 40) / 200.0f;
      float sh = 1.0f - sqrtf(sx * sx + sy * sy); if (sh < 0) sh = 0;
      sh *= 0.035f;
      // tiny bit of the letter colour in the glass, so black isn't dead black (phosphor)
      float r = br * v + fr * (0.012f + sh), g = bgc * v + fg * (0.012f + sh), b = bb * v + fbb * (0.012f + sh);
      // dither so the vignette has no bands
      float dith = (((x * 7 + y * 13) & 7) - 3.5f) * 0.9f;
      int R = (int)(r + dith), G = (int)(g + dith * 0.5f), B = (int)(b + dith);
      if (R < 0) R = 0; if (G < 0) G = 0; if (B < 0) B = 0; if (R > 255) R = 255; if (G > 255) G = 255; if (B > 255) B = 255;
      bgImg[y * SCR_W + x] = rgb3(R, G, B);
    }
  }
}

void gfxClear() { if (bgImg) memcpy(fb, bgImg, SCR_W * SCR_H * 2); }
void gfxClearTo(uint16_t c) { for (int i = 0; i < SCR_W * SCR_H; i++) fb[i] = c; }

void pset(int x, int y, uint16_t c) {
  if (x < clipX0 || y < clipY0 || x >= clipX1 || y >= clipY1) return;
  fb[y * SCR_W + x] = c;
}
void pblend(int x, int y, uint16_t c, int a) {
  if (x < clipX0 || y < clipY0 || x >= clipX1 || y >= clipY1 || a <= 0) return;
  uint16_t* p = &fb[y * SCR_W + x];
  *p = a >= 32 ? c : blend(*p, c, a);
}
void fillRect(int x, int y, int w, int h, uint16_t c) {
  int x0 = x < clipX0 ? clipX0 : x, y0 = y < clipY0 ? clipY0 : y;
  int x1 = x + w > clipX1 ? clipX1 : x + w, y1 = y + h > clipY1 ? clipY1 : y + h;
  for (int yy = y0; yy < y1; yy++) { uint16_t* p = &fb[yy * SCR_W + x0]; for (int xx = x0; xx < x1; xx++) *p++ = c; }
}
void blendRect(int x, int y, int w, int h, uint16_t c, int a) {
  if (a >= 32) { fillRect(x, y, w, h, c); return; }
  if (a <= 0) return;
  int x0 = x < clipX0 ? clipX0 : x, y0 = y < clipY0 ? clipY0 : y;
  int x1 = x + w > clipX1 ? clipX1 : x + w, y1 = y + h > clipY1 ? clipY1 : y + h;
  for (int yy = y0; yy < y1; yy++) { uint16_t* p = &fb[yy * SCR_W + x0]; for (int xx = x0; xx < x1; xx++, p++) *p = blend(*p, c, a); }
}
void hline(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
void vline(int x, int y, int h, uint16_t c) { fillRect(x, y, 1, h, c); }
void rect(int x, int y, int w, int h, uint16_t c) { hline(x, y, w, c); hline(x, y + h - 1, w, c); vline(x, y, h, c); vline(x + w - 1, y, h, c); }
void line(int x0, int y0, int x1, int y1, uint16_t c) {
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
  int x0 = x < clipX0 ? clipX0 : x, y0 = y < clipY0 ? clipY0 : y;
  int x1 = x + w > clipX1 ? clipX1 : x + w, y1 = y + h > clipY1 ? clipY1 : y + h;
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
    int yy = gy + j;
    if (yy < clipY0 || yy >= clipY1) { d += g->w; continue; }
    uint16_t* row = &fb[yy * SCR_W];
    for (int i = 0; i < g->w; i++) {
      uint8_t v = *d++;
      if (!v) continue;
      int xx = gx + i;
      if (xx < clipX0 || xx >= clipX1) continue;
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
    // roll the picture: rows move down by n (with wrap)
    static uint16_t* tmp = nullptr;
    if (!tmp) tmp = (uint16_t*)bigAlloc(SCR_W * SCR_H * 2);
    if (tmp && n) {
      memcpy(tmp, fb, SCR_W * SCR_H * 2);
      memcpy(&fb[n * SCR_W], tmp, (SCR_H - n) * SCR_W * 2);
      memcpy(fb, &tmp[(SCR_H - n) * SCR_W], n * SCR_W * 2);
    }
  }
  if (dx) for (int y = 0; y < SCR_H; y++) {
    uint16_t* p = &fb[y * SCR_W];
    if (dx > 0) memmove(p + dx, p, (SCR_W - dx) * 2); else memmove(p, p - dx, (SCR_W + dx) * 2);
  }
}
