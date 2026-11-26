// p8png.cpp (ERROR-CUBE): see p8png.h
#include "p8png.h"
#include <string.h>
#include <stdlib.h>
#ifdef ARDUINO
#include "miniz.h"                      // the ROM's tinfl (esp_rom)
static bool inflateZ(const unsigned char* in, size_t n, unsigned char* out, size_t outN) {
  size_t r = tinfl_decompress_mem_to_mem(out, outN, in, n, TINFL_FLAG_PARSE_ZLIB_HEADER);
  return r == outN;
}
#else
#include <zlib.h>
static bool inflateZ(const unsigned char* in, size_t n, unsigned char* out, size_t outN) {
  uLongf o = outN;
  int r = uncompress(out, &o, in, n);
  return (r == Z_OK || r == Z_BUF_ERROR) && o == outN;
}
#endif
static unsigned be32(const unsigned char* p) { return (unsigned)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static int paeth(int a, int b, int c) { int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c); return pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }

const char* p8png_error_text(unsigned e) {
  switch (e) {
    case 1: return "not a PNG"; case 2: return "broken PNG"; case 3: return "PNG kind not supported";
    case 4: return "out of memory"; case 5: return "PNG data broken"; default: return "PNG error";
  }
}
unsigned p8png_decode(std::vector<unsigned char>& rgba, unsigned& w, unsigned& h, const unsigned char* d, size_t n) {
  static const unsigned char SIG[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (n < 33 || memcmp(d, SIG, 8)) return 1;
  size_t p = 8; int depth = 0, ctype = 0, inter = 0; w = h = 0;
  std::vector<unsigned char> z;
  while (p + 12 <= n) {
    unsigned len = be32(d + p); const unsigned char* t = d + p + 4;
    if (p + 12 + len > n) return 2;
    const unsigned char* c = d + p + 8;
    if (!memcmp(t, "IHDR", 4) && len >= 13) { w = be32(c); h = be32(c + 4); depth = c[8]; ctype = c[9]; inter = c[12]; }
    else if (!memcmp(t, "IDAT", 4)) z.insert(z.end(), c, c + len);
    else if (!memcmp(t, "IEND", 4)) break;
    p += 12 + len;
  }
  if (!w || !h || w > 4096 || h > 4096) return 2;
  if (depth != 8 || (ctype != 6 && ctype != 2) || inter) return 3;
  int bpp = ctype == 6 ? 4 : 3;
  size_t stride = (size_t)w * bpp, rawN = (stride + 1) * h;
  unsigned char* raw = (unsigned char*)malloc(rawN);
  if (!raw) return 4;
  if (!inflateZ(z.data(), z.size(), raw, rawN)) { free(raw); return 5; }
  rgba.assign((size_t)w * h * 4, 255);
  std::vector<unsigned char> prev(stride, 0), cur(stride);
  for (unsigned y = 0; y < h; y++) {
    const unsigned char* r = raw + y * (stride + 1);
    int f = r[0]; r++;
    for (size_t i = 0; i < stride; i++) {
      int a = i >= (size_t)bpp ? cur[i - bpp] : 0, b = prev[i], cc = i >= (size_t)bpp ? prev[i - bpp] : 0, x = r[i];
      switch (f) { case 1: x += a; break; case 2: x += b; break; case 3: x += (a + b) >> 1; break; case 4: x += paeth(a, b, cc); break; }
      cur[i] = (unsigned char)x;
    }
    unsigned char* o = &rgba[(size_t)y * w * 4];
    for (unsigned x = 0; x < w; x++) { o[x * 4] = cur[x * bpp]; o[x * 4 + 1] = cur[x * bpp + 1]; o[x * 4 + 2] = cur[x * bpp + 2]; if (bpp == 4) o[x * 4 + 3] = cur[x * bpp + 3]; }
    prev.swap(cur);
  }
  free(raw);
  return 0;
}
