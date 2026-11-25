// gfx.h: drawing into the cube's 480x480 picture (RGB565, in PSRAM), ERROR-DOS style:
// VT323 letters with glow, double-line boxes, inverse bars, scanlines, glitches.
// Plain C++: the same code runs on the board and in the PC simulator (sim/).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "font_types.h"

#define SCR_W 480
#define SCR_H 480

extern uint16_t* fb;                 // the picture being drawn (SCR_W x SCR_H)

// colours of the current scheme (as on error.blue: GREEN, AMBER, WHITE, DOS BLUE, CYAN)
struct Scheme { const char* name; uint32_t fg, dim, bg; };
extern const Scheme SCHEMES[];
extern const int N_SCHEMES;
extern uint16_t C_FG, C_DIM, C_BG, C_RED, C_WARN, C_BLACK, C_WHITE;

static inline uint16_t rgb(uint32_t c) { return ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F); }
static inline uint16_t rgb3(int r, int g, int b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
// a = 0..32
static inline uint16_t blend(uint16_t d, uint16_t s, uint32_t a) {
  uint32_t dd = (d | ((uint32_t)d << 16)) & 0x07E0F81F, ss = (s | ((uint32_t)s << 16)) & 0x07E0F81F;
  uint32_t r = ((((ss - dd) * a) >> 5) + dd) & 0x07E0F81F;
  return (uint16_t)(r | (r >> 16));
}
uint16_t mix(uint16_t a, uint16_t b, int pct);   // pct of b over a

bool gfxInit();
void gfxTheme(int scheme, int glow);           // glow 0 off, 1 low, 2 high
void gfxClear();                               // background: the scheme's colour with the glass vignette
void gfxClearTo(uint16_t c);

extern int clipX0, clipY0, clipX1, clipY1;     // drawing stays inside (x1/y1 exclusive)
void clipSet(int x, int y, int w, int h);
void clipReset();

void pset(int x, int y, uint16_t c);
void pblend(int x, int y, uint16_t c, int a32);
void fillRect(int x, int y, int w, int h, uint16_t c);
void blendRect(int x, int y, int w, int h, uint16_t c, int a32);
void hline(int x, int y, int w, uint16_t c);
void vline(int x, int y, int h, uint16_t c);
void rect(int x, int y, int w, int h, uint16_t c);
void line(int x0, int y0, int x1, int y1, uint16_t c);
void thickLine(int x0, int y0, int x1, int y1, int t, uint16_t c);
void fillCircle(int cx, int cy, int r, uint16_t c);
void ring(int cx, int cy, int r, int t, uint16_t c);         // anti-aliased circle outline, t px thick
void glowRect(int x, int y, int w, int h, uint16_t c, int spread);   // soft light around a rectangle
void dosBox(int x, int y, int w, int h, uint16_t c);       // ERROR-DOS double line with glow
void invertRect(int x, int y, int w, int h);

// text. Fonts: FT = 15x32 cell (the normal letters), FB = 20x42, FH = 60x96 (clock digits only)
enum { FT = 0, FB = 1, FH = 2 };
enum { TX_GLOW = 0, TX_FLAT = 1 };             // with glow, or the bare letter (inverse bars, dim text)
const CubeFont& font(int f);
int fontW(int f);
int fontH(int f);
uint32_t utf8Next(const char*& s);
int utf8Len(const char* s);                    // number of characters
const char* utf8At(const char* s, int n);      // pointer to character n
void utf8Cut(char* s, int maxChars);
int drawText(int f, int x, int y, const char* s, uint16_t c, int mode = TX_GLOW, int maxChars = 9999);
int drawChar(int f, int x, int y, uint32_t cp, uint16_t c, int mode = TX_GLOW);
int textW(int f, const char* s);
void drawTextC(int f, int cx, int y, const char* s, uint16_t c, int mode = TX_GLOW);   // centred on cx
void drawTextR(int f, int rx, int y, const char* s, uint16_t c, int mode = TX_GLOW);   // ends at rx
extern uint32_t garbleUntil;                   // glitch: random letters until then

// effects over the finished picture
void fxScanlines();
void fxTear(int bands, uint32_t seed);
void fxStatic(int lines, uint32_t seed);
void fxBlock(uint32_t seed);
void fxInvertBand(uint32_t seed);
void fxShift(int dx, int dy);
uint32_t rnd();
