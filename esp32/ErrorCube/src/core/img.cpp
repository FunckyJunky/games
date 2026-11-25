// img.cpp: see img.h
#include "img.h"
#include "gfx.h"
#include "sys.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
static void* bigAlloc(size_t n) { void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : malloc(n); }
#else
static void* bigAlloc(size_t n) { return malloc(n); }
#endif

bool imgIsPicture(const char* name) {
  const char* e = strrchr(name, '.');
  return e && (!strcasecmp(e, ".jpg") || !strcasecmp(e, ".jpeg") || !strcasecmp(e, ".bmp"));
}

static bool bmp(const uint8_t* d, size_t n, uint16_t* out, int ow, int oh, int mode, char* err, int* iw, int* ih) {
  if (n < 54 || d[0] != 'B' || d[1] != 'M') { if (err) strcpy(err, "NOT A BMP"); return false; }
  uint32_t off = *(uint32_t*)(d + 10); int32_t W = *(int32_t*)(d + 18), H = *(int32_t*)(d + 22);
  uint16_t bpp = *(uint16_t*)(d + 28); uint32_t comp = *(uint32_t*)(d + 30);
  if ((bpp != 24 && bpp != 32) || (comp != 0 && comp != 3) || W <= 0 || W > 8192) { if (err) strcpy(err, "ONLY 24/32-BIT BMP"); return false; }
  bool flip = H > 0; if (H < 0) H = -H;
  int row = ((W * bpp / 8) + 3) & ~3;
  if (off + (size_t)row * H > n) { if (err) strcpy(err, "FILE IS CUT SHORT"); return false; }
  if (iw) *iw = W; if (ih) *ih = H;
  int dw = ow, dh = oh, ox = 0, oy = 0;
  if (mode != IMG_STRETCH) {
    float k = mode == IMG_FIT ? fminf((float)ow / W, (float)oh / H) : fmaxf((float)ow / W, (float)oh / H);
    dw = (int)(W * k); dh = (int)(H * k); ox = (ow - dw) / 2; oy = (oh - dh) / 2;
  }
  for (int y = 0; y < dh; y++) {
    int Y = y + oy; if (Y < 0 || Y >= oh) continue;
    int sy = y * H / dh; const uint8_t* src = d + off + (size_t)(flip ? H - 1 - sy : sy) * row;
    for (int x = 0; x < dw; x++) {
      int X = x + ox; if (X < 0 || X >= ow) continue;
      const uint8_t* p = src + (x * W / dw) * (bpp / 8);
      out[Y * ow + X] = rgb3(p[2], p[1], p[0]);
    }
  }
  return true;
}

bool imgLoad(const char* path, uint16_t* out, int ow, int oh, int mode, char* err, int* iw, int* ih) {
  long sz = fsSize(path);
  if (sz <= 0) { if (err) strcpy(err, "CAN'T READ IT"); return false; }
  if (sz > 6 * 1024 * 1024) { if (err) strcpy(err, "TOO BIG (6 MB MAX)"); return false; }
  uint8_t* buf = (uint8_t*)bigAlloc(sz + 1);
  if (!buf) { if (err) strcpy(err, "NO MEMORY"); return false; }
  size_t got = 0;
  if (!fsRead(path, buf, sz, got)) { free(buf); if (err) strcpy(err, "CAN'T READ IT"); return false; }
  for (int i = 0; i < ow * oh; i++) out[i] = 0;
  const char* e = strrchr(path, '.');
  bool ok;
  if (e && !strcasecmp(e, ".bmp")) ok = bmp(buf, got, out, ow, oh, mode, err, iw, ih);
  else { ok = jpegDecode(buf, got, out, ow, oh, mode, iw, ih); if (!ok && err) strcpy(err, "CAN'T SHOW THIS JPEG"); }
  free(buf);
  return ok;
}
