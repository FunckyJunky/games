// HOME FRONT drawing. Kept in step with public/thuisfront/dev/render.js (same numbers, same order),
// so the phone's preview matches the board.
#include "screen.h"
#include "font8x8.h"
#include "config.h"
#include <math.h>

#define W LCD_W
#define H LCD_H

uint8_t* FB = nullptr;
bool scrEffects = true;
static uint16_t lut[64], lutDim[64];
static uint8_t* rowTmp = nullptr;

struct Pal { float lo[3], bg[3], fg[3], hi[3], tx[3]; };
static const Pal PALS[4] = {
  { { 2, 12, 5 }, { 3, 18, 8 },   { 57, 255, 122 },  { 190, 255, 210 }, { 57, 255, 122 } },    // DOS GREEN
  { { 12, 8, 1 }, { 20, 13, 2 },  { 255, 196, 48 },  { 255, 240, 190 }, { 255, 200, 52 } },    // DOS YELLOW
  { { 0, 6, 52 }, { 0, 25, 166 }, { 110, 140, 255 }, { 242, 242, 255 }, { 242, 242, 255 } },   // DOS BLUE
  { { 3, 3, 3 },  { 12, 12, 12 }, { 192, 192, 192 }, { 255, 255, 255 }, { 225, 225, 225 } },   // DOS GRAY
};
static const float BLACK3[3] = { 0, 0, 0 };

static inline int rnd(float v) { return (int)floorf(v + 0.5f); }
static void lutSet(int i, const float* c) {
  int r = rnd(c[0]), g = rnd(c[1]), b = rnd(c[2]);
  lut[i] = lcdColor(r, g, b);
  lutDim[i] = lcdColor((int)(r * 0.55f), (int)(g * 0.55f), (int)(b * 0.55f));
}
static void lerp3(const float* a, const float* b, float f, float* o) { for (int k = 0; k < 3; k++) o[k] = a[k] + (b[k] - a[k]) * f; }
static void level(const Pal& p, const float* from, float v, float* o) {
  if (v <= 12) lerp3(from, p.fg, v / 12.0f, o); else lerp3(p.fg, p.hi, (v - 12) / 3.0f, o);
}
static void setTextRamp(int pal, int bg, float k) {
  const Pal& p = PALS[pal & 3];
  float c[3];
  for (int v = 0; v < 16; v++) {
    if (bg == 2) lerp3(p.tx, p.lo, (v / 15.0f) * k, c);
    else lerp3(bg == 1 ? p.bg : BLACK3, p.tx, (v / 15.0f) * k, c);
    lutSet(IX_T + v, c);
  }
}
static int vidPal = -1;
static void setVideoRamp(int pal) {
  if (pal == vidPal) return;
  vidPal = pal;
  const Pal& p = PALS[pal & 3];
  float c[3];
  for (int v = 0; v < 16; v++) { level(p, p.lo, v, c); lutSet(IX_V + v, c); }
}

void scrBegin() {
  FB = (uint8_t*)heap_caps_malloc(W * H, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!FB) FB = (uint8_t*)ps_malloc(W * H);
  rowTmp = (uint8_t*)malloc(W);
  memset(FB, IX_BLACK, W * H);
  float c[3];
  for (int v = 0; v < 16; v++) { level(PALS[0], PALS[0].bg, v, c); lutSet(IX_U + v, c); }
  const float bk[3] = { 0, 0, 0 }, red[3] = { 255, 40, 40 }, wh[3] = { 255, 255, 255 };
  lutSet(IX_BLACK, bk); lutSet(IX_RED, red); lutSet(IX_WHITE, wh);
  static const float SCENE[8][3] = { { 255, 201, 23 }, { 0, 48, 130 }, { 24, 34, 52 }, { 150, 150, 160 }, { 52, 52, 58 }, { 156, 70, 48 }, { 255, 236, 160 }, { 112, 112, 118 } };
  for (int i = 0; i < 8; i++) lutSet(IX_NSY + i, SCENE[i]);
  setTextRamp(0, 0, 1); setVideoRamp(0);
}
void scrFlush(bool scanlines, const uint16_t* plane) { lcdPush(FB, lut, scanlines ? lutDim : nullptr, plane); }
void scrPhotoRamp(int pal, uint16_t* out256) {
  const Pal& p = PALS[pal & 3];
  float c[3];
  for (int y = 0; y < 256; y++) { level(p, p.lo, y * 15.0f / 255.0f, c); out256[y] = lcdColor(rnd(c[0]), rnd(c[1]), rnd(c[2])); }
}

// ---------------- parts ----------------
void scrFill(int x, int y, int w, int h, uint8_t c) {
  int x0 = max(0, x), y0 = max(0, y), x1 = min(W, x + w), y1 = min(H, y + h);
  if (x1 <= x0) return;
  for (int yy = y0; yy < y1; yy++) memset(FB + yy * W + x0, c, x1 - x0);
}
static const uint8_t CYR8[37][8] = {   // А-Я, then Ё Є І Ї Ґ (the same as CYR in render.js)
  { 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00 }, { 0x7F, 0x46, 0x06, 0x3E, 0x66, 0x66, 0x3F, 0x00 },
  { 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00 }, { 0x7F, 0x46, 0x06, 0x06, 0x06, 0x06, 0x0F, 0x00 },
  { 0x7C, 0x66, 0x66, 0x66, 0x66, 0x7F, 0x63, 0x00 }, { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00 },
  { 0x6B, 0x6B, 0x3E, 0x1C, 0x3E, 0x6B, 0x6B, 0x00 }, { 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00 },
  { 0x63, 0x63, 0x73, 0x6B, 0x67, 0x63, 0x63, 0x00 }, { 0x1C, 0x63, 0x73, 0x6B, 0x67, 0x63, 0x63, 0x00 },
  { 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00 }, { 0x78, 0x6C, 0x6C, 0x6C, 0x6C, 0x66, 0x63, 0x00 },
  { 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00 }, { 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00 },
  { 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00 }, { 0x7F, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00 },
  { 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00 }, { 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00 },
  { 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 }, { 0x33, 0x33, 0x33, 0x3E, 0x30, 0x33, 0x1E, 0x00 },
  { 0x08, 0x3E, 0x6B, 0x6B, 0x3E, 0x08, 0x1C, 0x00 }, { 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00 },
  { 0x33, 0x33, 0x33, 0x33, 0x33, 0x7F, 0x60, 0x00 }, { 0x33, 0x33, 0x33, 0x3E, 0x30, 0x30, 0x78, 0x00 },
  { 0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0x7F, 0x00 }, { 0x6B, 0x6B, 0x6B, 0x6B, 0x6B, 0xFF, 0x80, 0x00 },
  { 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3E, 0x00 }, { 0x63, 0x63, 0x63, 0x6F, 0x6B, 0x6B, 0x6F, 0x00 },
  { 0x06, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3E, 0x00 }, { 0x1E, 0x33, 0x30, 0x3C, 0x30, 0x33, 0x1E, 0x00 },
  { 0x33, 0x4B, 0x4B, 0x4F, 0x4B, 0x4B, 0x33, 0x00 }, { 0x7E, 0x63, 0x63, 0x7E, 0x78, 0x6C, 0x67, 0x00 },
  { 0x12, 0x00, 0x7F, 0x46, 0x1E, 0x46, 0x7F, 0x00 }, { 0x3C, 0x66, 0x03, 0x1F, 0x03, 0x66, 0x3C, 0x00 },
  { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 }, { 0x12, 0x00, 0x1E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },
  { 0x60, 0x7F, 0x06, 0x06, 0x06, 0x06, 0x0F, 0x00 } };
// a character (code point) -> its glyph: 32-126 the 8x8 font, 0x100 + n Cyrillic glyph n, else '?' (as glyphOf in render.js)
static int glyphOf(uint32_t cp) {
  if (cp >= 32 && cp <= 126) return cp;
  if (cp >= 0x410 && cp <= 0x42F) return 0x100 + cp - 0x410;
  if (cp >= 0x430 && cp <= 0x44F) return 0x100 + cp - 0x430;
  switch (cp) {
    case 0x401: case 0x451: return 0x100 + 32;   // Ё
    case 0x404: case 0x454: return 0x100 + 33;   // Є
    case 0x406: case 0x456: return 0x100 + 34;   // І
    case 0x407: case 0x457: return 0x100 + 35;   // Ї
    case 0x490: case 0x491: return 0x100 + 36;   // Ґ
    case 0x2018: case 0x2019: case 0x2BC: return '\'';
    case 0xAB: case 0xBB: case 0x201C: case 0x201D: case 0x201E: return '"';
    case 0x2013: case 0x2014: return '-';
  }
  return '?';
}
static const uint8_t* glyphBits(int g) { return g >= 0x100 ? CYR8[g - 0x100] : FONT8 + (g - 32) * 8; }
// UTF-8 in, one glyph out per character (0 at the end)
static int nextChar(const char*& s) {
  uint8_t c = (uint8_t)*s;
  if (!c) return 0;
  s++;
  if (c < 0x80) return (c < 32 || c > 126) ? '?' : c;
  uint32_t cp;
  int more;
  if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; more = 1; }
  else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; more = 2; }
  else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; more = 3; }
  else return '?';
  for (; more && ((uint8_t)*s & 0xC0) == 0x80; more--, s++) cp = cp << 6 | ((uint8_t)*s & 0x3F);
  return more ? '?' : glyphOf(cp);
}
static void drawGlyph(int g, int x, int y, int k, uint8_t c) {
  const uint8_t* bits = glyphBits(g);
  for (int r = 0; r < 8; r++) {
    if (!bits[r]) continue;
    for (int b = 0; b < 8; b++) if (bits[r] & (1 << b)) scrFill(x + b * k, y + r * k, k, k, c);
  }
}
int scrTextW(const char* s, int k) { int n = 0; while (nextChar(s)) n++; return n * 8 * k; }
void scrText(const char* s, int x, int y, int k, uint8_t c) {
  int i = 0, ch;
  while ((ch = nextChar(s))) {
    const uint8_t* g = glyphBits(ch);
    for (int r = 0; r < 8; r++) {
      uint8_t bits = g[r];
      if (!bits) continue;
      for (int b = 0; b < 8; b++) if (bits & (1 << b)) scrFill(x + (i * 8 + b) * k, y + r * k, k, k, c);
    }
    i++;
  }
}
void scrCenter(const char* s, int y, int k, uint8_t c) { scrText(s, (W - scrTextW(s, k)) / 2, y, k, c); }
void scrLine(int x0, int y0, int x1, int y1, uint8_t c) {
  int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, n = 0;
  for (;;) {
    if (x0 >= 0 && y0 >= 0 && x0 < W && y0 < H) FB[y0 * W + x0] = c;
    if ((x0 == x1 && y0 == y1) || ++n > 2000) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
struct Rng {                      // mulberry32, the same numbers as the web page
  uint32_t a;
  explicit Rng(uint32_t s) : a(s) {}
  double next() {
    a += 0x6D2B79F5u;
    uint32_t t = a;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return (double)(t ^ (t >> 14)) / 4294967296.0;
  }
};

// ---------------- text pictures ----------------
TextPic* textDecode(const uint8_t* d, size_t len) {
  if (len < 16 || memcmp(d, "HFT1", 4)) return nullptr;
  auto u16 = [&](size_t o) { return (uint16_t)(d[o] | (d[o + 1] << 8)); };
  TextPic* t = new TextPic();
  t->w = u16(4); t->h = u16(6); t->x = (int16_t)u16(8); t->y = (int16_t)u16(10); t->n = u16(12);
  if (!t->w || !t->h || (uint32_t)t->w * t->h > 1600000) { delete t; return nullptr; }
  size_t o = 16;
  if (t->n) {
    if (o + (size_t)t->n * 8 > len) t->n = (len - o) / 8;
    t->boxes = (uint16_t*)ps_malloc(t->n * 8);
    for (int i = 0; i < t->n * 4; i++) t->boxes[i] = u16(o + i * 2);
    o += t->n * 8;
  }
  size_t total = (size_t)t->w * t->h, p = 0;
  t->px = (uint8_t*)ps_malloc(total);
  if (!t->px) { textFree(t); return nullptr; }
  while (o < len && p < total) {
    uint8_t b = d[o++], v = b & 15;
    size_t run = (b >> 4) + 1;
    if (run == 16) run = 16 + (o < len ? d[o++] : 0);
    if (p + run > total) run = total - p;
    memset(t->px + p, v, run);
    p += run;
  }
  if (p < total) memset(t->px + p, 0, total - p);
  return t;
}
void textFree(TextPic* t) {
  if (!t) return;
  free(t->boxes); free(t->px);
  delete t;
}
static void blit(const TextPic* t, int dx, int dy, int sx = 0, int sy = 0, int sw = -1, int sh = -1) {
  if (sw < 0) sw = t->w;
  if (sh < 0) sh = t->h;
  int ya = max(0, -dy), yb = min(sh, H - dy), xa = max(0, -dx), xb = min(sw, W - dx);
  for (int yy = ya; yy < yb; yy++) {
    const uint8_t* row = t->px + (size_t)(sy + yy) * t->w + sx;
    uint8_t* o = FB + (dy + yy) * W + dx;
    for (int xx = xa; xx < xb; xx++) { uint8_t v = row[xx]; if (v) o[xx] = IX_T + v; }
  }
}
static void glitchRows(uint32_t seed, double chance, double maxShift) {
  Rng r(seed);
  if (r.next() < chance) {
    int n = 1 + (int)floor(r.next() * 3);
    for (int s = 0; s < n; s++) {
      int sy = (int)floor(r.next() * H), sh = 2 + (int)floor(r.next() * 10), dx = (int)floor((r.next() - 0.5) * 2 * maxShift + 0.5);
      for (int y = sy; y < min(H, sy + sh); y++) {
        memcpy(rowTmp, FB + y * W, W);
        for (int x = 0; x < W; x++) { int sx = x - dx; FB[y * W + x] = (sx >= 0 && sx < W) ? rowTmp[sx] : rowTmp[0]; }
      }
    }
    if (r.next() < 0.5) {
      int y = (int)floor(r.next() * H);
      for (int x = 0; x < W; x++) if (r.next() < 0.3) FB[y * W + x] = IX_T + 3 + (int)floor(r.next() * 6);
    }
  }
}
// returns the text brightness (pulse), 0-1
static float drawText(const TextPic* t, uint8_t anim, int sp, float tA) {
  switch (anim) {
    case A_SCROLL: {
      float pps = 15 + sp * 15, total = t->w + W;
      blit(t, rnd(W - fmodf(tA * pps, total)), t->y);
      return 1;
    }
    case A_RISE: {
      float pps = 8 + sp * 7, total = t->h + H;
      blit(t, t->x, rnd(H - fmodf(tA * pps, total)));
      return 1;
    }
    case A_TYPE: {
      int n = t->n;
      if (!n) { blit(t, t->x, t->y); return 1; }
      float cps = 2 + sp * 2;
      int steps = n + (int)ceilf(cps * 3);
      int k = (int)floorf(tA * cps) % steps, shown = min(k, n);
      for (int i = 0; i < shown; i++) { const uint16_t* b = t->boxes + i * 4; blit(t, t->x + b[0], t->y + b[1], b[0], b[1], b[2], b[3]); }
      const uint16_t* b = t->boxes + max(0, shown - 1) * 4;
      if (((int)floorf(tA * 2.5f)) % 2 == 0 || shown < n) {
        int cx = shown ? b[0] + b[2] : b[0], ch = max(4, rnd(b[3] * 0.55f));
        scrFill(t->x + cx + 1, t->y + b[1] + rnd((b[3] - ch) / 2.0f), max(2, rnd(b[3] * 0.3f)), ch, IX_T + 15);
      }
      return 1;
    }
    case A_BLINK: {
      float hz = 0.5f + sp * 0.25f;
      if (((int)floorf(tA * hz * 2)) % 2 == 0) blit(t, t->x, t->y);
      return 1;
    }
    case A_WAVE: {
      float w = 1.5f + sp * 0.5f;
      for (int i = 0; i < t->w; i++) {
        int x = t->x + i;
        if (x < 0 || x >= W) continue;
        int dy = rnd(7 * sinf((t->x + i) * 0.055f - tA * w));
        blit(t, x, t->y + dy, i, 0, 1, t->h);
      }
      return 1;
    }
    case A_GLITCH:
      blit(t, t->x, t->y);
      glitchRows((uint32_t)floorf(tA * 12) + 1, 0.12 + sp * 0.045, 6 + sp * 4);
      return 1;
    case A_PULSE:
      blit(t, t->x, t->y);
      return 0.3f + 0.7f * (0.5f + 0.5f * sinf(tA * (1.2f + sp * 0.45f)));
    default:
      blit(t, t->x, t->y);
      return 1;
  }
}

// ---------------- video ----------------
static inline uint8_t nib(const uint8_t* p, int i) { uint8_t b = p[i >> 1]; return (i & 1) ? (b & 15) : (b >> 4); }
static void drawVideo(const uint8_t* f, int fw, int fh) {
  if (fw == W && fh == H) {
    for (int i = 0; i < W * H; i += 2) { uint8_t b = f[i >> 1]; FB[i] = IX_V + (b >> 4); FB[i + 1] = IX_V + (b & 15); }
    return;
  }
  if (fw * 2 == W && fh * 2 == H) {
    for (int y = 0; y < fh; y++) {
      uint8_t* o = FB + y * 2 * W;
      for (int x = 0; x < fw; x++) { uint8_t v = IX_V + nib(f, y * fw + x); o[x * 2] = o[x * 2 + 1] = o[W + x * 2] = o[W + x * 2 + 1] = v; }
    }
    return;
  }
  for (int y = 0; y < H; y++) { int sy = y * fh / H; for (int x = 0; x < W; x++) FB[y * W + x] = IX_V + nib(f, sy * fw + x * fw / W); }
}
static void drawNoSignal(uint32_t now) {
  Rng r(now / 50);
  for (int y = 0; y < H; y += 2) for (int x = 0; x < W; x += 2) {
    uint8_t v = IX_V + (int)floor(r.next() * 7);
    int o = y * W + x;
    FB[o] = FB[o + 1] = FB[o + W] = FB[o + W + 1] = v;
  }
  scrFill(80, 70, 160, 32, IX_BLACK);
  scrCenter("NO SIGNAL", 78, 2, IX_U + 15);
}

const uint16_t* scrComm(const CommSet& c, const TextPic* t, const Media& m, float tA, uint32_t nowMs) {
  setVideoRamp(c.vpal == NOFILTER ? 0 : c.vpal);
  const uint16_t* plane = nullptr;
  if (c.mode == M_PHOTO) {
    if (m.photo) { memset(FB, IX_PLANE, W * H); plane = m.photo; }
    else { memset(FB, IX_T, W * H); scrText("NO PHOTO YET", 4, 4, 1, IX_U + 6); }
  } else if (c.mode == M_VIDEO || c.mode == M_BOTH) {
    uint32_t grey = m.gray && m.grayAt && nowMs - m.grayAt < 3000 ? m.grayAt : 0;
    uint32_t col = m.color && m.colorAt && nowMs - m.colorAt < 3000 ? m.colorAt : 0;
    if (col && col >= grey) { memset(FB, IX_PLANE, W * H); plane = m.color; }
    else if (grey) drawVideo(m.gray, m.fw, m.fh);
    else if (c.mode == M_VIDEO) drawNoSignal(scrEffects ? nowMs : 0);
    else { memset(FB, IX_T, W * H); scrText("NO VIDEO", 4, 4, 1, IX_U + 6); }
  } else memset(FB, IX_T, W * H);
  float k = 1;
  if (c.mode != M_VIDEO) {
    if (t) k = drawText(t, scrEffects ? c.anim : (uint8_t)A_STATIC, c.speed, tA);
    else if (c.mode == M_TEXT) scrCenter("NO TEXT YET", 82, 1, IX_U + 5);
  }
  setTextRamp(c.tpal, c.mode == M_TEXT ? c.bg : 0, k);
  return plane;
}

// ---------------- CLOCK: big dot-matrix digits (the same as drawClock in render.js) ----------------
// 5x7 glyphs, one byte per row, bit 4 = the left column; index 10 = '-', 11 = ' ', 12 = '+'
static const uint8_t GLYPH[13][7] = {
  { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },
  { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E },
  { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E },
  { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
  { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C },
  { 0, 0, 0, 0x1F, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0 }, { 0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0 } };
static const uint8_t COLON[7] = { 0, 0, 1, 0, 1, 0, 0 };
static const char* const WDAY[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
static const char* const MONTH[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
static void dot(int x, int y, int d, uint8_t c) {
  for (int r = 0; r < d; r++) {
    bool e = r == 0 || r == d - 1, n = r == 1 || r == d - 2;
    int inset = d >= 6 ? (e ? 2 : n ? 1 : 0) : d >= 4 ? (e ? 1 : 0) : 0;
    scrFill(x + inset, y + r, d - 2 * inset, 1, c);
  }
}
static void matrix(const char* s, int x, int y, int p, int d, uint8_t lit, uint8_t ghost, bool colonOn) {
  for (; *s; s++) {
    bool colon = *s == ':';
    int w = colon ? 1 : 5;
    const uint8_t* rows = GLYPH[*s >= '0' && *s <= '9' ? *s - '0' : *s == '-' ? 10 : *s == '+' ? 12 : 11];
    for (int r = 0; r < 7; r++) for (int c = 0; c < w; c++) {
      bool on = colon ? colonOn && COLON[r] : (rows[r] >> (4 - c)) & 1;
      dot(x + c * p, y + r * p, d, on ? lit : ghost);
    }
    x += (w + 1) * p;
  }
}
void scrClock(const ClockSet& k, const struct tm* t, int ms) {
  memset(FB, IX_T, W * H);
  bool big = k.sec == 2, h24 = k.h24;
  char str[12], line[40] = "";
  if (!t) { strcpy(str, big ? "--:--:--" : "--:--"); strcpy(line, "NO TIME YET"); }
  else {
    int h = t->tm_hour;
    if (!h24) { h %= 12; if (!h) h = 12; }
    if (h24) snprintf(str, sizeof(str), "%02d:%02d", h, t->tm_min); else snprintf(str, sizeof(str), "%2d:%02d", h, t->tm_min);
    if (big) { size_t n = strlen(str); snprintf(str + n, sizeof(str) - n, ":%02d", t->tm_sec); }
    if (k.date) snprintf(line, sizeof(line), "%s %d %s %d", WDAY[t->tm_wday % 7], t->tm_mday, MONTH[t->tm_mon % 12], t->tm_year + 1900);
    if (!h24) { size_t n = strlen(line); snprintf(line + n, sizeof(line) - n, "%s%s", n ? "  " : "", t->tm_hour < 12 ? "AM" : "PM"); }
  }
  int p = big ? 8 : 12, d = p - 2, cols = big ? 39 : 25;
  int tw = cols * p - (p - d), th = 7 * p - (p - d);
  bool row = !big && k.sec != 0;
  int total = th + (row ? 13 : 0) + (line[0] ? 28 : 0);
  int y = rnd((H - total) / 2.0f), x = rnd((W - tw) / 2.0f);
  matrix(str, x, y, p, d, IX_T + 15, IX_T + 2, !scrEffects || !t || ms < 500);
  y += th;
  if (row) {
    y += 10;
    for (int i = 0; i < 60; i++) dot(x + i * 5, y, 3, t && i <= t->tm_sec ? IX_T + 15 : IX_T + 2);
    y += 3;
  }
  if (line[0]) { y += 12; scrText(line, (W - scrTextW(line, 2)) / 2, y, 2, IX_T + 11); }
  setTextRamp(k.pal, k.bg ? 1 : 0, 1);
}

// ---------------- ETA: when I arrive, with a description and a progress bar (the same as drawEta in render.js) ----------------
// word wrap for the 8x8 font, on glyphs (t, len): lines of at most max letters (a longer word is cut);
// out/outLen get the first `keep` lines; returns how many lines it takes
struct GLine { uint16_t g[40]; int n; };
static int wrapWords(const uint16_t* t, int len, int max, GLine* out, int keep) {
  int n = 0, ll = 0, i = 0;
  uint16_t line[40];
  auto push = [&](const uint16_t* s, int l) { if (n < keep) { memcpy(out[n].g, s, l * 2); out[n].n = l; } n++; };
  while (i < len) {
    while (i < len && t[i] == ' ') i++;
    if (i >= len) break;
    const uint16_t* w = t + i;
    int wl = 0;
    while (i < len && t[i] != ' ') { i++; wl++; }
    while (wl > max) { if (ll) { push(line, ll); ll = 0; } push(w, max); w += max; wl -= max; }
    if (!wl) continue;
    if (!ll) { memcpy(line, w, wl * 2); ll = wl; }
    else if (ll + 1 + wl <= max) { line[ll++] = ' '; memcpy(line + ll, w, wl * 2); ll += wl; }
    else { push(line, ll); memcpy(line, w, wl * 2); ll = wl; }
  }
  if (ll) push(line, ll);
  return n;
}
// a description about a train -> the progress bar is a train going to the station (as isTrain in render.js)
static const char* const TRAIN_WORDS[] = { "TRAIN", "TREIN", "SPRINTER", "INTERCITY", "ПОТЯГ", "ПОТЯЗ", "ПОЇЗД", "ПОЕЗД", "ЕЛЕКТРИЧК", "ЭЛЕКТРИЧК" };
static uint16_t upper(uint16_t g) { return g >= 'a' && g <= 'z' ? g - 32 : g; }   // Cyrillic glyphs are capitals already
static bool isTrain(const uint16_t* t, int len) {
  for (const char* w : TRAIN_WORDS) {
    uint16_t k[16]; int kn = 0;
    for (const char* p = w; kn < 16;) { int g = nextChar(p); if (!g) break; k[kn++] = g; }
    for (int i = 0; i + kn <= len; i++) {
      int j = 0;
      while (j < kn && upper(t[i + j]) == k[j]) j++;
      if (j == kn) return true;
    }
  }
  return false;
}
static void trainCar(int x, int y, int len, bool cab, int wheel) {
  scrFill(x + 1, y + 8, len - 2, 1, IX_DARK);
  scrFill(x, y + 9, len, 12, IX_NSY);
  for (int wx = x + 3; wx + 5 <= x + len - (cab ? 10 : 3); wx += 7) scrFill(wx, y + 11, 5, 4, IX_GLASS);
  scrFill(x, y + 17, len, 2, IX_NSB);
  int door = x + (cab ? 12 : (len >> 1) - 2);
  scrFill(door, y + 11, 4, 9, IX_NSB);
  scrFill(door + 1, y + 12, 2, 3, IX_GLASS);
  scrFill(x + 2, y + 21, len - 4, 1, IX_DARK);
  const int ws[4] = { x + 4, x + 9, x + len - 10, x + len - 5 };
  for (int wx : ws) { scrFill(wx - 1, y + 21, 3, 3, IX_BLACK); scrFill(wx - 1 + wheel, y + 22, 1, 1, IX_RAIL); }
  if (cab) {
    for (int r = 0; r < 4; r++) scrFill(x + len - 4 + r, y + 9 + r, 4 - r, 1, IX_T);
    scrFill(x + len - 9, y + 11, 5, 4, IX_GLASS);
    scrFill(x + len - 3, y + 15, 2, 2, IX_LAMP);
    scrFill(x + 10, y + 7, 8, 1, IX_RAIL);
    scrFill(x + 12, y + 6, 1, 1, IX_RAIL); scrFill(x + 13, y + 5, 1, 1, IX_RAIL); scrFill(x + 14, y + 4, 1, 1, IX_RAIL);
    scrFill(x + 13, y + 3, 1, 1, IX_RAIL); scrFill(x + 12, y + 2, 1, 1, IX_RAIL);
    scrFill(x + 9, y + 1, 7, 1, IX_RAIL);
  }
}
static void trainScene(int y, double p, int wheel) {
  const int sx = 282;
  scrFill(262, y + 21, W - 262, 2, IX_PLAT);
  scrFill(sx, y + 7, 34, 14, IX_BRICK);
  scrFill(sx - 2, y + 5, 38, 2, IX_DARK); scrFill(sx + 2, y + 3, 30, 2, IX_DARK); scrFill(sx + 6, y + 1, 22, 2, IX_DARK);
  scrFill(sx + 4, y + 11, 6, 5, IX_LAMP); scrFill(sx + 24, y + 11, 6, 5, IX_LAMP);
  scrFill(sx + 14, y + 12, 6, 9, IX_NSB);
  scrFill(sx + 11, y + 7, 12, 3, IX_NSB); scrFill(sx + 13, y + 8, 8, 1, IX_WHITE);
  scrFill(0, y + 24, W, 1, IX_RAIL);
  for (int x = 2; x < W; x += 6) scrFill(x, y + 25, 3, 2, IX_DARK);
  const int L = 66, span = sx - 8 - L, front = 4 + L + (int)floor(p * span);
  trainCar(front - L, y, 30, false, wheel);
  scrFill(front - L + 30, y + 15, 2, 2, IX_DARK);
  trainCar(front - 34, y, 34, true, wheel);
}
// the words on the ETA screen: 0 = English, 1 = Ukrainian (the same as ETA_TXT in render.js)
static const char* const ETA_TXT[2][11] = {
  { "ETA", "ARRIVES ", "ARRIVED ", "ARRIVED", "ON MY WAY SINCE ", "ON MY WAY", "ON THE WAY SINCE ", "ARRIVING NOW", "LATE", "NO ARRIVAL SET", "WAITING FOR THE TIME" },
  { "ПРИЇЗД", "БУДУ О ", "НА МІСЦІ О ", "НА МІСЦІ", "В ДОРОЗІ З ", "В ДОРОЗІ", "В ДОРОЗІ З ", "ОСЬ-ОСЬ БУДУ", "ЗАПІЗНЮЮСЬ", "ЧАС НЕ ЗАДАНО", "ЧЕКАЮ НА ЧАС" } };
static void hmStr(char* b, const struct tm* t) { snprintf(b, 6, "%02d:%02d", t->tm_hour, t->tm_min); }
void scrEta(const EtaSet& e, int64_t now, bool known, const struct tm* toT, const struct tm* fromT) {
  memset(FB, IX_T, W * H);
  int ms = (int)(((now % 1000) + 1000) % 1000);
  bool blink = !scrEffects || ms < 500;
  scrFill(0, 0, W, 12, IX_T + 12);
  const char* const* L = ETA_TXT[e.lang == 1 ? 1 : 0];
  scrText(L[0], 4, 2, 1, IX_T + 0);
  char top[48] = "", hm[8];
  if (e.to && known && toT) { hmStr(hm, toT); snprintf(top, sizeof(top), "%s%s", e.done ? L[2] : L[1], hm); scrText(top, W - 4 - scrTextW(top, 1), 2, 1, IX_T + 0); }
  uint16_t txt[64]; int tl = 0;
  for (const char* q = e.text; tl < 64;) { int g = nextChar(q); if (!g) break; txt[tl++] = g; }
  GLine lines[3];
  int k = 2, n = wrapWords(txt, tl, 19, lines, 3);
  if (n > 2) { k = 1; n = wrapWords(txt, tl, 38, lines, 3); if (n > 3) n = 3; }
  int lh = k == 2 ? 18 : 11, descH = n * lh, gap = n ? 6 : 0;
  bool train = isTrain(txt, tl);
  int barH = train ? 28 : 12;
  int total = descH + gap + 54 + 10 + barH + 12 + 8;
  int y = 12 + max(4, rnd((160 - total) / 2.0f));
  for (int i = 0; i < n; i++) {
    int x0 = (W - lines[i].n * 8 * k) / 2;
    for (int j = 0; j < lines[i].n; j++) drawGlyph(lines[i].g[j], x0 + j * 8 * k, y + i * lh, k, IX_T + 15);
  }
  y += descH + gap;
  char str[16] = "--:--", bottom[64];
  strlcpy(bottom, L[9], sizeof(bottom));
  bool colon = false, running = false;
  double p = 0;
  auto fmt = [](char* b, size_t sz, const char* pre, int64_t secs) {
    int h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
    if (h) snprintf(b, sz, "%s%d:%02d:%02d", pre, h, m, s); else snprintf(b, sz, "%s%02d:%02d", pre, m, s);
  };
  if (e.to && !known) strlcpy(bottom, L[10], sizeof(bottom));
  else if (e.to && e.done) {
    str[0] = 0; p = 1;
    if (fromT) { hmStr(hm, fromT); snprintf(bottom, sizeof(bottom), "%s%s", L[6], hm); } else bottom[0] = 0;
  } else if (e.to) {
    int64_t span = ((int64_t)e.to - (int64_t)e.from) * 1000, left = (int64_t)e.to * 1000 - now;
    p = span > 0 ? (double)(now - (int64_t)e.from * 1000) / (double)span : 1;
    p = p < 0 ? 0 : p > 1 ? 1 : p;
    if (left > 0) {
      fmt(str, sizeof(str), "", (left + 999) / 1000); colon = true; running = true;
      if (fromT) { hmStr(hm, fromT); snprintf(bottom, sizeof(bottom), "%s%s", L[4], hm); } else strlcpy(bottom, L[5], sizeof(bottom));
    } else {
      int64_t late = (-left) / 1000;
      if (late < 60) { strcpy(str, "00:00"); strlcpy(bottom, L[7], sizeof(bottom)); } else { fmt(str, sizeof(str), "+", late); strlcpy(bottom, L[8], sizeof(bottom)); }
    }
  }
  if (str[0]) {
    int cols = -1;
    for (const char* c = str; *c; c++) cols += (*c == ':' ? 1 : 5) + 1;
    matrix(str, rnd((W - (cols * 8 - 2)) / 2.0f), y, 8, 6, IX_T + 15, IX_T + 2, colon ? blink : true);
  } else scrText(L[3], (W - scrTextW(L[3], 4)) / 2, y + 11, 4, IX_T + 15);
  y += 54 + 10;
  if (train) trainScene(y, e.to ? p : 0, running && scrEffects ? (ms / 250) % 2 : 0);
  else {
    int filled = (int)floor(p * 50);
    for (int i = 0; i < 50; i++) {
      uint8_t c = i < filled ? IX_T + 15 : (i == filled && running && scrEffects) ? (ms < 500 ? IX_T + 15 : IX_T + 6) : IX_T + 2;
      scrFill(10 + i * 6, y, 5, 12, c);
    }
  }
  y += barH + 12;
  if (bottom[0]) scrText(bottom, 10, y, 1, IX_T + 11);
  if (e.to && known) { char pc[8]; snprintf(pc, sizeof(pc), "%d%%", (int)floor(p * 100)); scrText(pc, W - 10 - scrTextW(pc, 1), y, 1, IX_T + 11); }
  setTextRamp(e.pal, 0, 1);
}

// ---------------- menu screens ----------------
static void bars(const char* left, const char* right) {
  scrFill(0, 0, W, 12, IX_U + 12);
  scrText(left, 4, 2, 1, IX_U + 0);
  if (right && *right) scrText(right, W - 4 - scrTextW(right, 1), 2, 1, IX_U + 0);
}
static const float HV[20][3] = { { -1, 0, -0.8f }, { 1, 0, -0.8f }, { 1, 1.2f, -0.8f }, { -1, 1.2f, -0.8f }, { -1, 0, 0.8f }, { 1, 0, 0.8f }, { 1, 1.2f, 0.8f }, { -1, 1.2f, 0.8f },
  { -1, 2.05f, 0 }, { 1, 2.05f, 0 }, { -0.25f, 0, -0.8f }, { -0.25f, 0.7f, -0.8f }, { 0.25f, 0.7f, -0.8f }, { 0.25f, 0, -0.8f },
  { 0.42f, 0.5f, -0.8f }, { 0.8f, 0.5f, -0.8f }, { 0.8f, 0.88f, -0.8f }, { 0.42f, 0.88f, -0.8f }, { 0.55f, 1.62f, -0.25f }, { 0.55f, 2.35f, -0.25f } };
static const uint8_t HE[25][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
  { 3, 8 }, { 7, 8 }, { 2, 9 }, { 6, 9 }, { 8, 9 }, { 10, 11 }, { 11, 12 }, { 12, 13 }, { 14, 15 }, { 15, 16 }, { 16, 17 }, { 17, 14 }, { 18, 19 } };
static void drawHouse(float t, float cx, float cy, float f, uint8_t c) {
  float yaw = t * 0.7f, pitch = 0.12f, cy0 = cosf(yaw), sy0 = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
  float P[20][2];
  for (int i = 0; i < 20; i++) {
    float x = HV[i][0], y = HV[i][1] - 1.0f, z = HV[i][2];
    float x1 = x * cy0 + z * sy0, z1 = -x * sy0 + z * cy0;
    float y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp + 5.5f;
    P[i][0] = cx + (x1 * f) / z2; P[i][1] = cy - (y2 * f) / z2;
  }
  for (auto& e : HE) scrLine((int)P[e[0]][0], (int)P[e[0]][1], (int)P[e[1]][0], (int)P[e[1]][1], c);
  float ph = fmodf(t * 1.2f, 1);
  for (int k = 0; k < 2; k++) {
    float r = 3 + fmodf(ph + k * 0.5f, 1) * 12, ax = P[19][0], ay = P[19][1];
    for (float a = -0.9f; a < 0.9f; a += 0.15f) {
      int x = (int)(ax + cosf(a - (float)M_PI / 2) * r), y = (int)(ay + sinf(a - (float)M_PI / 2) * r);
      if (x >= 0 && y >= 0 && x < W && y < H) FB[y * W + x] = IX_U + 9;
    }
  }
}
void scrIdle(float t, const ScreenInfo& in) {
  memset(FB, IX_U, W * H);
  bars("HOME FRONT", in.name[0] ? in.name : in.id);
  drawHouse(t, 66, 92, 150, IX_U + 12);
  scrText("HOME FRONT", 128, 40, 2, IX_U + 15);
  scrText("WAITING FOR A WEB APP", 128, 66, 1, IX_U + 12);
  if (((int)floorf(t * 1.9f)) % 2 == 0) scrFill(128 + scrTextW("WAITING FOR A WEB APP", 1) + 2, 66, 7, 8, IX_U + 12);
  scrText("CONTROL IT FROM", 128, 92, 1, IX_U + 6);
  scrText("error.blue/thuisfront", 128, 104, 1, IX_U + 13);
  scrFill(0, 158, W, 1, IX_U + 4);
  char left[64];
  if (in.wifi[0]) snprintf(left, sizeof(left), "%s %s", in.wifi, in.ip); else snprintf(left, sizeof(left), "%s", in.ip);
  left[26] = 0;
  scrText(left, 4, 162, 1, IX_U + 7);
  const char* st = in.online ? "ONLINE" : "OFFLINE";
  scrText(st, W - 4 - scrTextW(st, 1), 162, 1, in.online ? IX_U + 11 : IX_RED);
  if (((int)floorf(t / 4)) % 3 == 0 && fmodf(t, 4) < 0.12f) glitchRows((uint32_t)floorf(t * 30), 1, 14);
}
void scrUpdate(float t, int pct, const char* msg) {
  memset(FB, IX_U, W * H);
  bars("HOME FRONT", "UPDATE");
  drawHouse(t, 268, 100, 90, IX_U + 9);
  scrText("NEW FIRMWARE", 14, 34, 2, IX_U + 15);
  scrText(msg, 14, 60, 1, pct == -2 ? IX_RED : IX_U + 12);
  if (pct >= 0) {
    scrFill(14, 82, 200, 14, IX_U + 6); scrFill(16, 84, 196, 10, IX_U + 0);
    scrFill(16, 84, 196 * pct / 100, 10, IX_U + 13);
    char b[8]; snprintf(b, sizeof(b), "%d%%", pct); scrText(b, 220, 85, 1, IX_U + 13);
    scrText("OVER WIFI, NO CABLE NEEDED", 14, 112, 1, IX_U + 7);
    scrText("DON'T TURN IT OFF", 14, 126, 1, IX_U + 10);
  } else scrText("IT GOES ON WITH THE OLD ONE", 14, 112, 1, IX_U + 7);
}
void scrConnected(float t, const ScreenInfo& in) {
  memset(FB, IX_U, W * H);
  bars("HOME FRONT", in.name);
  const int k = 3;
  int w = scrTextW("CONNECTED", k) + 34, x = (W - w) / 2;
  scrFill(x, 44, 26, 26, IX_U + 12); scrFill(x + 3, 47, 20, 20, IX_U + 0);
  scrLine(x + 7, 57, x + 11, 62, IX_U + 15); scrLine(x + 8, 57, x + 12, 62, IX_U + 15);
  scrLine(x + 11, 62, x + 19, 50, IX_U + 15); scrLine(x + 12, 62, x + 20, 50, IX_U + 15);
  scrText("CONNECTED", x + 34, 45, k, IX_U + 15);
  char b[64];
  if (in.wifi[0]) { snprintf(b, sizeof(b), "WIFI %s", in.wifi); scrCenter(b, 90, 1, IX_U + 12); }
  if (in.ip[0]) { snprintf(b, sizeof(b), "IP %s", in.ip); scrCenter(b, 104, 1, IX_U + 8); }
  scrCenter("LOADING error.blue/thuisfront/dev", 124, 1, IX_U + 7);
  float left = max(0.0f, 1 - t / 3);
  scrFill(40, 142, 240, 6, IX_U + 3); scrFill(40, 142, rnd(240 * left), 6, IX_U + 12);
}
void scrConnecting(float t, const ScreenInfo& in) {
  memset(FB, IX_U, W * H);
  bars("HOME FRONT", in.name);
  drawHouse(t, 160, 70, 120, IX_U + 10);
  char b[24];
  snprintf(b, sizeof(b), "CONNECTING%.*s", ((int)floorf(t * 3)) % 4, "...");
  scrCenter(b, 120, 2, IX_U + 14);
  scrCenter("error.blue", 146, 1, IX_U + 7);
}
void scrBoot(float t, const char* const* lines, int n) {
  memset(FB, IX_U, W * H);
  bars("HOME FRONT BIOS", "v" HF_VERSION);
  int shown = min(n, (int)(t / 0.09f) + 1);
  for (int i = 0; i < shown; i++) scrText(lines[i], 6, 20 + i * 12, 1, i == shown - 1 ? IX_U + 14 : IX_U + 9);
  if (((int)floorf(t * 3)) % 2 == 0) scrFill(6 + scrTextW(lines[shown - 1], 1) + 2, 20 + (shown - 1) * 12, 7, 8, IX_U + 12);
  drawHouse(t, 268, 130, 90, IX_U + 8);
}
void scrJoining(float t, const ScreenInfo& in, const char* ssid, bool lowPower) {
  memset(FB, IX_U, W * H);
  bars("HOME FRONT", in.name);
  drawHouse(t, 160, 66, 110, IX_U + 10);
  char b[24];
  snprintf(b, sizeof(b), "JOINING%.*s", ((int)floorf(t * 3)) % 4, "...");
  scrCenter(b, 112, 2, IX_U + 14);
  scrCenter(ssid, 134, 1, IX_U + 12);
  if (lowPower) scrCenter("(LOW POWER)", 148, 1, IX_U + 6);
}
void scrSetup(float t, const ScreenInfo& in, const char* status) {
  memset(FB, IX_U, W * H);
  bars("HOME FRONT", "WIFI SETUP");
  scrText("1. ON YOUR PHONE, JOIN WIFI", 8, 22, 1, IX_U + 7);
  scrText(in.ap, 8, 34, 2, IX_U + 15);
  scrText("2. OPEN THIS PAGE", 8, 60, 1, IX_U + 7);
  scrText("192.168.4.1", 8, 72, 2, IX_U + 15);
  scrText("3. PICK YOUR WIFI", 8, 98, 1, IX_U + 7);
  scrText("   AND TYPE ITS PASSWORD", 8, 110, 1, IX_U + 7);
  drawHouse(t, 272, 96, 80, IX_U + 9);
  scrFill(0, 158, W, 1, IX_U + 4);
  scrText(status, 4, 162, 1, IX_U + 10);
}
void scrInfoBox(const ScreenInfo& in, const char* extra) {
  int x = 26, y = 22, w = 268, h = 128;
  scrFill(x, y, w, h, IX_U + 12); scrFill(x + 2, y + 2, w - 4, h - 4, IX_U + 0);
  scrFill(x + 2, y + 2, w - 4, 12, IX_U + 12);
  scrText("HOME FRONT", x + 6, y + 4, 1, IX_U + 0);
  char b[64];
  int yy = y + 22;
  auto ln = [&](const char* k, const char* v) { snprintf(b, sizeof(b), "%-6s%s", k, v); b[32] = 0; scrText(b, x + 8, yy, 1, IX_U + 13); yy += 12; };
  ln("NAME", in.name); ln("ID", in.id); ln("WIFI", in.wifi[0] ? in.wifi : "-"); ln("IP", in.ip[0] ? in.ip : "-");
  ln("VER", HF_VERSION); ln("NET", in.online ? "ONLINE" : "OFFLINE");
  scrText(extra, x + 8, yy + 4, 1, IX_U + 7);
}
void scrBanner(const char* s) {
  scrFill(0, H - 12, W, 12, IX_U + 12);
  scrText(s, 4, H - 10, 1, IX_U + 0);
}
void scrOfflineDot(uint32_t nowMs) { if (!scrEffects || (nowMs / 600) % 2 == 0) scrFill(W - 7, 2, 5, 5, IX_RED); }
