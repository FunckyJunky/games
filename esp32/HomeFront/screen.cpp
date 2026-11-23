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
// UTF-8 in, one '?' for every letter the 8x8 font doesn't have
static int nextChar(const char*& s) {
  uint8_t c = (uint8_t)*s;
  if (!c) return 0;
  s++;
  if (c < 0x80) return (c < 32 || c > 126) ? '?' : c;
  while (((uint8_t)*s & 0xC0) == 0x80) s++;
  return '?';
}
int scrTextW(const char* s, int k) { int n = 0; while (nextChar(s)) n++; return n * 8 * k; }
void scrText(const char* s, int x, int y, int k, uint8_t c) {
  int i = 0, ch;
  while ((ch = nextChar(s))) {
    const uint8_t* g = FONT8 + (ch - 32) * 8;
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
// 5x7 glyphs, one byte per row, bit 4 = the left column; index 10 = '-', 11 = ' '
static const uint8_t GLYPH[12][7] = {
  { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },
  { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E },
  { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E },
  { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
  { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C },
  { 0, 0, 0, 0x1F, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0 } };
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
    const uint8_t* rows = GLYPH[*s >= '0' && *s <= '9' ? *s - '0' : *s == '-' ? 10 : 11];
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
