// holo.cpp: see holo.h
// 1.5: HOLO 3D (tilt, depth layers, shadows, sides, the moving glass) is gone to save memory and time.
// The functions stay (flat: no shift, no shadows), so the rest of the code doesn't change. The wallpapers stay.
#include "holo.h"
#include "gfx.h"
#include "sys.h"
#include "img.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

static bool frameFlat = false;
static float curH = 0;

bool holoActive() { return false; }
bool holoLayersOn() { return false; }
float holoTX() { return 0; }
float holoTY() { return 0; }
float holoOverH() { return 1.0f; }
void holoRecenter() {}
void holoInit() {}
int holoFps() { return 0; }
bool holoTick() { return false; }
void holoShift(float, int& dx, int& dy) { dx = dy = 0; }
float holoSet(float h) { float p = curH; curH = h; gfxOffset(0, 0); return p; }
float holoAt() { return curH; }
void holoFrameStart(bool flat) { frameFlat = flat; holoSet(H_TEXT); }
void holoUnmap(int&, int&) {}
void holoShadow(int, int, int, int, float) {}
void holoWalls(int, int, int, int, uint16_t, float, bool) {}
void holoGlass() {}

// the wallpaper picture: decoded once at screen size (only when a picture is chosen), darkened
static uint16_t* wall = nullptr;
static char wallFor[80] = "\x01"; static int wallLightFor = -1; static bool wallSd = false, wallOk = false;
static void wallFree() { free(wall); wall = nullptr; wallOk = false; wallFor[0] = 1; wallFor[1] = 0; }
static void wallLoad() {
  if (!strcmp(wallFor, cfg.wallPic) && wallLightFor == cfg.wallLight && wallSd == S.sd) return;
  snprintf(wallFor, sizeof(wallFor), "%s", cfg.wallPic); wallLightFor = cfg.wallLight; wallSd = S.sd; wallOk = false;
  if (!cfg.wallPic[0] || (!strncmp(cfg.wallPic, "/sd", 3) && !S.sd)) { free(wall); wall = nullptr; return; }
#ifdef ARDUINO
  if (!wall) wall = (uint16_t*)heap_caps_malloc(SCR_W * SCR_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  if (!wall) wall = (uint16_t*)malloc(SCR_W * SCR_H * 2);
#endif
  if (!wall) return;
  wallOk = imgLoad(cfg.wallPic, wall, SCR_W, SCR_H, IMG_COVER);
  if (wallOk) { int a = cfg.wallLight * 32 / 100; for (int i = 0; i < SCR_W * SCR_H; i++) wall[i] = blend(0, wall[i], a); }
  else { free(wall); wall = nullptr; }
}
bool holoWallOk() { wallLoad(); return wallOk; }

void holoBackdrop() {
  // not behind full-screen programs, not in LOW GRAPHICS
  if (frameFlat || lowFx() || !cfg.holoBack) { if (wall && cfg.holoBack != 5) wallFree(); return; }
  switch (cfg.holoBack) {
    case 5:   // PICTURE
      wallLoad();
      if (wallOk) memcpy(fb, wall, SCR_W * SCR_H * 2);
      break;
    case 1:   // DOTS
      for (int y = -24; y < SCR_H + 24; y += 30)
        for (int x = -24 + ((y / 30) & 1) * 15; x < SCR_W + 24; x += 30) blendRect(x, y, 2, 2, C_FG, 9);
      break;
    case 2:   // GRID
      for (int y = -40; y < SCR_H + 40; y += 40) blendRect(-40, y, SCR_W + 80, 1, C_FG, 4);
      for (int x = -40; x < SCR_W + 40; x += 40) blendRect(x, -40, 1, SCR_H + 80, C_FG, 4);
      break;
    case 3: {  // STARS
      uint32_t s = 2024;
      for (int i = 0; i < 90; i++) {
        s = s * 1103515245 + 12345; int x = (s >> 8) % 540 - 30;
        s = s * 1103515245 + 12345; int y = (s >> 8) % 540 - 30;
        int a = 6 + (s >> 20) % 12;
        blendRect(x, y, 1 + (i % 7 == 0), 1 + (i % 7 == 0), C_FG, a);
      }
      break;
    }
    case 4: {  // ROOM: the walls of a box behind the glass, from the screen's corners to a far frame
      const int I = 34;
      int ox[4] = {0, SCR_W - 1, SCR_W - 1, 0}, oy[4] = {0, 0, SCR_H - 1, SCR_H - 1};
      int ix[4] = {I, SCR_W - 1 - I, SCR_W - 1 - I, I}, iy[4] = {I, I, SCR_H - 1 - I, SCR_H - 1 - I};
      for (int k = 0; k < 4; k++)
        for (int t = 0; t <= 24; t++) {
          int xx = ox[k] + (ix[k] - ox[k]) * t / 24, yy = oy[k] + (iy[k] - oy[k]) * t / 24;
          blendRect(xx - 1, yy - 1, 2, 2, C_FG, 14 - t / 3);
        }
      for (int k = 0; k < 4; k++) {
        int x0 = ix[k], y0 = iy[k], x1 = ix[(k + 1) & 3], y1 = iy[(k + 1) & 3];
        if (y0 == y1) blendRect(x0 < x1 ? x0 : x1, y0, abs(x1 - x0), 1, C_FG, 7);
        else blendRect(x0, y0 < y1 ? y0 : y1, 1, abs(y1 - y0), C_FG, 7);
      }
      for (int i = 1; i < 4; i++) blendRect(I, I + i * (SCR_H - 2 * I) / 4, SCR_W - 2 * I, 1, C_FG, 3);
      for (int i = 1; i < 4; i++) blendRect(I + i * (SCR_W - 2 * I) / 4, I, 1, SCR_H - 2 * I, C_FG, 3);
      break;
    }
  }
}
