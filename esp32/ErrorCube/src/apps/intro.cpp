// intro.cpp: the start-up show (BOOT FULL + INTRO on): a starfield warp, ERROR in solid 3D block letters
// spinning in from far away, a lens flare when they land, CUBE typed under it, a loading bar, then ERROR-BIOS.
// Any tap or button skips it.
#include "../ui/ui.h"
#include "../core/g3d.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>

static const char* GLYPH[3][7] = {
  {"11111", "10000", "10000", "11110", "10000", "10000", "11111"},   // E
  {"11110", "10001", "10001", "11110", "10100", "10010", "10001"},   // R
  {"01110", "10001", "10001", "10001", "10001", "10001", "01110"},   // O
};
static const int WORD[5] = {0, 1, 1, 2, 1};    // E R R O R

struct Intro : Screen {
  uint32_t t0 = 0;
  struct Star { float x, y, z; };
  Star st[150];
  bool cell[7][29];
  int typed = 0; bool flared = false, landed = false;
  const char* name() override { return "INTRO"; }
  bool fullScreen() override { return true; }
  bool keepAwake() override { return true; }
  void enter() override {
    t0 = msNow();
    for (auto& s : st) { s.x = (int)(rnd() % 2000) - 1000; s.y = (int)(rnd() % 2000) - 1000; s.z = 100 + rnd() % 1900; }
    memset(cell, 0, sizeof(cell));
    for (int l = 0; l < 5; l++) for (int r = 0; r < 7; r++) for (int c = 0; c < 5; c++) if (GLYPH[WORD[l]][r][c] == '1') cell[r][l * 6 + c] = true;
  }
  void done() { uiReplaceRoot(bootScreen()); }
  void tick() override {
    uint32_t t = msNow() - t0;
    if (t > 4700) { done(); return; }
    int want = t < 2700 ? 0 : (int)((t - 2700) / 140);
    if (want > 4) want = 4;
    if (want > typed) { typed = want; if (cfg.clicks) snd(SND_KEY); }
    if (t > 2350 && !landed) { landed = true; snd(SND_THUD); uiGlitch(8, 180); }
    redrawIn(16);
  }
  bool ev(const Ev& e) override { if (e.t == EV_TAP || e.t == EV_KEY) { done(); return true; } return true; }

  void stars(float t) {
    // warp speed at first, then a slow drift
    float speed = t < 1.0f ? 400 + t * 3800 : fmaxf(40, 4200 - (t - 1.0f) * 6000);
    float f = 300;
    for (auto& s : st) {
      float z0 = s.z;
      s.z -= speed * 0.016f;
      if (s.z < 10) { s.x = (int)(rnd() % 2000) - 1000; s.y = (int)(rnd() % 2000) - 1000; s.z = 2000; z0 = s.z; }
      float x1 = 240 + s.x * f / s.z, y1 = 240 + s.y * f / s.z, x0 = 240 + s.x * f / z0, y0 = 240 + s.y * f / z0;
      int a = (int)(32 * (1 - s.z / 2000)); if (a < 4) continue;
      if (speed > 600) aaLine(x0, y0, x1, y1, C_FG, a); else pblend((int)x1, (int)y1, C_FG, a);
    }
  }
  void letters(float t) {
    // fly in from far away, spinning, and land facing you
    float p = (t - 0.7f) / 1.65f; if (p < 0) return; if (p > 1) p = 1;
    float e = 1 - powf(1 - p, 3);
    // they land turned a little, so you see they are solid, then sway slowly
    float z = 4200 - e * (4200 - 600), yaw = (1 - e) * 6.2832f * 1.25f - 0.3f * e, pitch = (1 - e) * 0.6f + 0.2f * e + (p >= 1 ? sinf(t * 1.6f) * 0.05f : 0);
    float wob = p >= 1 ? sinf(t * 1.1f) * 0.12f : 0;
    g3dCam(0, 0, 0, 0, 0, 0, 430, 240, 200);
    g3dObj(v3(0, 0, z), yaw + wob, pitch, 0, 14.5f);
    struct Face { float d; V3 p[4]; uint16_t c; };
    std::vector<Face> faces;
    faces.reserve(260);
    const float D = 0.8f;
    V3 light = v3(-0.45f, -0.65f, -0.6f);
    for (int r = 0; r < 7; r++)
      for (int c = 0; c < 29; c++) {
        if (!cell[r][c]) continue;
        float x = c - 14, y = r - 3;
        // the six sides; the ones against another block are inside the letter
        for (int f = 0; f < 6; f++) {
          static const int N[6][3] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}};
          int nx = N[f][0], ny = N[f][1], nz = N[f][2];
          if (nz == 0) { int rr = r + ny, cc = c + nx; if (rr >= 0 && rr < 7 && cc >= 0 && cc < 29 && cell[rr][cc]) continue; }
          V3 ctr = v3(x + nx * 0.5f, y + ny * 0.5f, nz * D);
          V3 wc = g3dWorld(ctr), wn = g3dWorld(v3(ctr.x + nx, ctr.y + ny, ctr.z + nz));
          V3 n = v3(wn.x - wc.x, wn.y - wc.y, wn.z - wc.z);
          if (n.x * wc.x + n.y * wc.y + n.z * wc.z >= 0) continue;            // facing away
          float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
          float lit = (n.x * light.x + n.y * light.y + n.z * light.z) / len; if (lit < 0) lit = 0;
          Face fc; fc.d = wc.z; fc.c = mix(C_BG, C_FG, (int)(30 + 70 * lit));
          // its corners
          float ux = nz ? 0.5f : 0, uy = nx ? 0.5f : 0, uz = ny ? D : 0;   // half sizes along the face
          if (nz) { fc.p[0] = v3(x - 0.5f, y - 0.5f, nz * D); fc.p[1] = v3(x + 0.5f, y - 0.5f, nz * D); fc.p[2] = v3(x + 0.5f, y + 0.5f, nz * D); fc.p[3] = v3(x - 0.5f, y + 0.5f, nz * D); }
          else if (nx) { float xx = x + nx * 0.5f; fc.p[0] = v3(xx, y - 0.5f, -D); fc.p[1] = v3(xx, y - 0.5f, D); fc.p[2] = v3(xx, y + 0.5f, D); fc.p[3] = v3(xx, y + 0.5f, -D); }
          else { float yy = y + ny * 0.5f; fc.p[0] = v3(x - 0.5f, yy, -D); fc.p[1] = v3(x + 0.5f, yy, -D); fc.p[2] = v3(x + 0.5f, yy, D); fc.p[3] = v3(x - 0.5f, yy, D); }
          (void)ux; (void)uy; (void)uz;
          faces.push_back(fc);
        }
      }
    std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.d > b.d; });
    for (auto& f : faces) quad3(f.p[0], f.p[1], f.p[2], f.p[3], f.c, 32);
  }
  void draw() override {
    float t = (msNow() - t0) / 1000.0f;
    gfxClearTo(C_BLACK);
    stars(t);
    letters(t);
    // the lens flare when the letters land: a streak across, a ring, little rings along a line
    float ft = t - 2.35f;
    if (ft > 0 && ft < 0.9f) {
      float k = 1 - ft / 0.9f, w = 60 + ft * 900;
      for (int i = -2; i <= 2; i++) blendRect(240 - (int)w / 2, 200 + i, (int)w, 1, C_WHITE, (int)(k * (24 - abs(i) * 8)));
      blendRect(240 - (int)w / 4, 196, (int)w / 2, 9, C_FG, (int)(k * 10));
      ring(240, 200, (int)(30 + ft * 160), 2, mix(C_BLACK, C_FG, (int)(k * 60)));
      for (int i = 1; i <= 3; i++) ring(240 + i * 55, 200 + i * 35, 6 + i * 5, 2, mix(C_BLACK, C_FG, (int)(k * 35)));
    }
    if (typed > 0) {
      char c[8] = "CUBE"; c[typed > 4 ? 4 : typed] = 0;
      drawTextC(FB, 240, 300, c, C_FG);
      if (typed >= 4) drawTextC(FT, 240, 350, "error.blue", C_DIM, TX_FLAT);
    }
    if (t > 3.3f) {
      float p = (t - 3.3f) / 1.2f; if (p > 1) p = 1;
      drawText(FT, 90, 400, "LOADING ERROR-DOS", C_DIM, TX_FLAT);
      meter(90, 434, 300, 18, (int)(p * 100), true);
    }
  }
};
Screen* introScreen() { return new Intro(); }
