// linetext.cpp: see linetext.h. A glyph: strokes split by '|', each a line through points written as two digits
// "xy" (x 0..4, y 0..6, y up from the baseline).
#include "linetext.h"
#include <string.h>
#include <stdint.h>

static const char* const LETTERS[26] = {
  "002640|1333",                       // A
  "00063645443303|3342413000",         // B
  "4536160501103041",                  // C
  "00062644422000",                    // D
  "46060040|0333",                     // E
  "460600|0333",                       // F
  "45361605011030414323",              // G
  "0006|4046|0343",                    // H
  "1636|2620|1030",                    // I
  "4641301001",                        // J
  "0006|4602|1340",                    // K
  "060040",                            // L
  "0006234640",                        // M
  "00064046",                          // N
  "100105163645413010",                // O
  "00063645443303",                    // P
  "100105163645413010|2240",           // Q
  "00063645443303|2340",               // R
  "453616050413334241301001",          // S
  "0646|2620",                         // T
  "060110304146",                      // U
  "062046",                            // V
  "0610243046",                        // W
  "0046|0640",                         // X
  "062346|2320",                       // Y
  "06460040",                          // Z
};
static const char* const DIGITS[10] = {
  "100105163645413010|0145",           // 0
  "152620|1030",                       // 1
  "05163645440040",                    // 2
  "05163645443313|334241301001",       // 3
  "30360242",                          // 4
  "4606033342413000",                  // 5
  "4536160501103041423303",            // 6
  "064610",                            // 7
  "13040516364544331302011030414233",  // 8
  "43130405163645413010",              // 9
};
static const char* NOTE = "1001122110|212645";
static const char* mark(uint32_t c) {
  switch (c) {
    case '.': return "2021";
    case ',': return "2110";
    case '!': return "2622|2021";
    case '?': return "05163645442322|2021";
    case '\'': return "2625";
    case '-': return "1333";
    case ':': return "2425|2021";
    case '+': return "1333|2224";
    case '/': return "0046";
    case '(': return "36252130";
    case ')': return "16252110";
    case 0x266A: case 0x266B: return NOTE;
  }
  return nullptr;
}
// one character: its strokes (nullptr = a space), and how many letters it stands for (ß = SS)
static const char* glyph(uint32_t c, const char** second) {
  *second = nullptr;
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c >= 0xE0 && c <= 0xFE && c != 0xF7) c -= 0x20;      // à..þ -> À..Þ
  if (c >= 'A' && c <= 'Z') return LETTERS[c - 'A'];
  if (c >= '0' && c <= '9') return DIGITS[c - '0'];
  if (c >= 0xC0 && c <= 0xC5) return LETTERS[0];
  if (c == 0xC6) { *second = LETTERS['E' - 'A']; return LETTERS[0]; }
  if (c == 0xC7) return LETTERS['C' - 'A'];
  if (c >= 0xC8 && c <= 0xCB) return LETTERS['E' - 'A'];
  if (c >= 0xCC && c <= 0xCF) return LETTERS['I' - 'A'];
  if (c == 0xD0) return LETTERS['D' - 'A'];
  if (c == 0xD1) return LETTERS['N' - 'A'];
  if ((c >= 0xD2 && c <= 0xD6) || c == 0xD8) return LETTERS['O' - 'A'];
  if (c >= 0xD9 && c <= 0xDC) return LETTERS['U' - 'A'];
  if (c == 0xDD || c == 0x178) return LETTERS['Y' - 'A'];
  if (c == 0xDF) { *second = LETTERS['S' - 'A']; return LETTERS['S' - 'A']; }
  if (c == 0x152 || c == 0x153) { *second = LETTERS['E' - 'A']; return LETTERS['O' - 'A']; }
  return mark(c);
}
static uint32_t next(const char*& s) {
  const uint8_t* p = (const uint8_t*)s;
  uint32_t c = *p++;
  if (c >= 0xF0 && p[0] && p[1] && p[2]) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
  else if (c >= 0xE0 && p[0] && p[1]) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
  else if (c >= 0xC0 && p[0]) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
  s = (const char*)p;
  return c;
}
static const float ADV = 5.5f, SPACE = 4;
float ltWidth(const char* s) {
  float w = 0; int n = 0;
  while (*s) {
    uint32_t c = next(s); const char* g2; const char* g = glyph(c, &g2);
    w += g ? ADV * (g2 ? 2 : 1) : SPACE; n++;
  }
  return n ? w - (ADV - 4) : 0;
}
static int segs(const char* g) {           // lines in one glyph
  int n = 0;
  for (const char* p = g; *p; ) {
    int pts = 0;
    while (*p && *p != '|') { pts++; p += 2; }
    if (pts > 1) n += pts - 1;
    if (*p == '|') p++;
  }
  return n;
}
int ltDraw(const char* s, Vec o, Vec right, Vec up, Vec depth, Col front, Col back, float reveal, int alpha) {
  if (alpha <= 0 || reveal <= 0) return 0;
  // how many lines in all (to draw them on one by one)
  int total = 0;
  for (const char* p = s; *p; ) { uint32_t c = next(p); const char* g2; const char* g = glyph(c, &g2); if (g) total += segs(g); if (g2) total += segs(g2); }
  if (!total) return 0;
  float budget = reveal >= 1 ? 1e9f : reveal * total;
  float x = -ltWidth(s) / 2;
  Vec half = vmul(depth, 0.5f);
  int drawn = 0, done = 0;
  int aB = alpha * 11 / 20, aJ = alpha * 7 / 20;
  for (const char* p = s; *p; ) {
    uint32_t c = next(p); const char* g2; const char* g1 = glyph(c, &g2);
    if (!g1) { x += SPACE; continue; }
    for (int k = 0; k < 2; k++) {
      const char* g = k ? g2 : g1;
      if (!g) break;
      for (const char* q = g; *q; ) {
        Vec prev = vec(0, 0, 0); bool have = false;
        while (*q && *q != '|') {
          if (!have && done >= budget) return drawn;
          float gx = q[0] - '0', gy = q[1] - '0';
          q += 2;
          Vec pt = vadd(o, vadd(vmul(right, x + gx), vmul(up, gy - 3)));
          if (have) {
            if (done >= budget) return drawn;
            float f = budget - done; if (f > 1) f = 1;
            Vec to = f < 1 ? vlerp(prev, pt, f) : pt;
            r3dStreak(vadd(prev, half), vadd(to, half), front, alpha, 1);
            r3dStreak(vsub(prev, half), vsub(to, half), back, aB, 0);
            drawn += 2; done++;
            if (f < 1) return drawn;
          }
          r3dStreak(vadd(pt, half), vsub(pt, half), back, aJ, 0);     // the corner, front to back
          drawn++;
          prev = pt; have = true;
        }
        if (*q == '|') q++;
      }
      x += ADV;
    }
  }
  return drawn;
}
