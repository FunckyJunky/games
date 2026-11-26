// icons3d.cpp: a turning wireframe icon for every program (the 3D menu, LINK...). Unit size: about -1..1.
// Call g3dObj() first (where, how turned, how big), then icon3d().
#include "../ui/ui.h"
#include "../core/g3d.h"
#include <string.h>
#include <math.h>
#include <time.h>

static uint16_t C; static int A;
static void L(float ax, float ay, float az, float bx, float by, float bz) { l3(ax, ay, az, bx, by, bz, C, A); }
static void R(float x, float y, float z, float r, int axis, int n = 20, float a0 = 0, float a1 = 6.2832f) { ring3(v3(x, y, z), r, n, axis, C, A, a0, a1); }
static void B(float x, float y, float z, float hx, float hy, float hz) { box3(v3(x, y, z), v3(hx, hy, hz), C, A); }
// a flat outline pushed out to a solid: front at -d, back at +d, joined at the corners
static void solid(const float* pts, int n, float d, bool closed = true) {
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    if (!closed && j == 0) break;
    L(pts[i * 2], pts[i * 2 + 1], -d, pts[j * 2], pts[j * 2 + 1], -d);
    L(pts[i * 2], pts[i * 2 + 1], d, pts[j * 2], pts[j * 2 + 1], d);
  }
  for (int i = 0; i < n; i++) L(pts[i * 2], pts[i * 2 + 1], -d, pts[i * 2], pts[i * 2 + 1], d);
}

void icon3d(const char* id, float t, uint16_t col, int a32) {
  C = col; A = a32;
  if (!strcmp(id, "CLOCK")) {
    R(0, 0, -0.18f, 1, 2, 28); R(0, 0, 0.18f, 1, 2, 28);
    for (int i = 0; i < 4; i++) { float a = i * 1.5708f; L(cosf(a), sinf(a), -0.18f, cosf(a), sinf(a), 0.18f); }
    for (int i = 0; i < 12; i++) { float a = i * 0.5236f, r0 = i % 3 ? 0.84f : 0.72f; L(cosf(a) * r0, sinf(a) * r0, -0.18f, cosf(a) * 0.95f, sinf(a) * 0.95f, -0.18f); }
    struct tm tm; float hr = 0, mn = t * 0.2f;
    if (sysLocalTime(tm)) { hr = (tm.tm_hour % 12 + tm.tm_min / 60.0f) * 0.5236f; mn = tm.tm_min * 0.1047f + tm.tm_sec * 0.00175f; }
    L(0, 0, -0.2f, sinf(hr) * 0.5f, -cosf(hr) * 0.5f, -0.2f);
    L(0, 0, -0.2f, sinf(mn) * 0.8f, -cosf(mn) * 0.8f, -0.2f);
    float sc = t * 1.0472f;
    L(0, 0, -0.22f, sinf(sc) * 0.88f, -cosf(sc) * 0.88f, -0.22f);
  } else if (!strcmp(id, "MUSIC")) {
    // two notes on a beam, bouncing
    float b = sinf(t * 3) * 0.08f;
    R(-0.55f, 0.55f + b, 0, 0.26f, 2, 14); R(0.45f, 0.4f - b, 0, 0.26f, 2, 14);
    L(-0.3f, 0.55f + b, 0, -0.3f, -0.75f, 0); L(0.7f, 0.4f - b, 0, 0.7f, -0.9f, 0);
    L(-0.3f, -0.75f, 0, 0.7f, -0.9f, 0); L(-0.3f, -0.55f, 0, 0.7f, -0.7f, 0);
    L(-0.3f, -0.75f, -0.1f, 0.7f, -0.9f, -0.1f);
  } else if (!strcmp(id, "VIDEO")) {
    // a film frame with a play triangle that turns
    B(0, 0, 0, 1, 0.7f, 0.12f);
    for (int i = 0; i < 5; i++) { B(-0.8f + i * 0.4f, -0.6f, -0.12f, 0.06f, 0.04f, 0.02f); B(-0.8f + i * 0.4f, 0.6f, -0.12f, 0.06f, 0.04f, 0.02f); }
    float c = cosf(t), s = sinf(t);
    float P[3][2] = {{-0.25f, -0.35f}, {-0.25f, 0.35f}, {0.38f, 0}};
    for (int i = 0; i < 3; i++) { int j = (i + 1) % 3; L(P[i][0] * c, P[i][1], P[i][0] * s - 0.2f, P[j][0] * c, P[j][1], P[j][0] * s - 0.2f); }
  } else if (!strcmp(id, "MAP")) {
    // a folded map with a pin that bobs
    float f[5] = {-0.9f, -0.3f, 0.3f, 0.9f};
    for (int i = 0; i < 3; i++) { float z0 = (i % 2) ? 0.12f : -0.12f, z1 = (i % 2) ? -0.12f : 0.12f; L(f[i], -0.7f, z0, f[i + 1], -0.7f, z1); L(f[i], 0.7f, z0, f[i + 1], 0.7f, z1); L(f[i], -0.7f, z0, f[i], 0.7f, z0); }
    L(0.9f, -0.7f, 0.12f, 0.9f, 0.7f, 0.12f);
    float b = sinf(t * 3) * 0.1f;
    R(0.1f, -0.55f + b, 0, 0.22f, 1, 12); L(0.1f, -0.33f + b, 0, 0.1f, 0.1f, 0);
  } else if (!strcmp(id, "CALC")) {
    B(0, 0, 0, 0.72f, 1, 0.18f);
    B(0, -0.62f, -0.18f, 0.55f, 0.22f, 0.02f);
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) B(-0.38f + c * 0.38f, -0.05f + r * 0.36f, -0.18f, 0.12f, 0.11f, 0.04f);
  } else if (!strcmp(id, "PAINT")) {
    // a brush: handle, metal band, bristles to a point
    R(0, -1, 0, 0.12f, 1, 10); R(0, 0.15f, 0, 0.12f, 1, 10);
    for (int i = 0; i < 4; i++) { float a = i * 1.5708f; L(cosf(a) * 0.12f, -1, sinf(a) * 0.12f, cosf(a) * 0.12f, 0.15f, sinf(a) * 0.12f); }
    B(0, 0.28f, 0, 0.2f, 0.13f, 0.2f);
    R(0, 0.42f, 0, 0.22f, 1, 12);
    for (int i = 0; i < 6; i++) { float a = i * 1.0472f + t; L(cosf(a) * 0.22f, 0.42f, sinf(a) * 0.22f, sinf(t * 2) * 0.1f, 1.05f, 0); }
  } else if (!strcmp(id, "LEVEL")) {
    R(-1, 0, 0, 0.32f, 0, 16); R(1, 0, 0, 0.32f, 0, 16);
    for (int i = 0; i < 4; i++) { float a = i * 1.5708f + 0.785f; L(-1, sinf(a) * 0.32f, cosf(a) * 0.32f, 1, sinf(a) * 0.32f, cosf(a) * 0.32f); }
    L(-0.25f, -0.32f, 0, -0.25f, 0.32f, 0); L(0.25f, -0.32f, 0, 0.25f, 0.32f, 0);
    float bx = sinf(t * 1.3f) * 0.55f;
    R(bx, -0.12f, 0, 0.14f, 2, 12); R(bx, -0.12f, 0, 0.14f, 0, 12);
  } else if (!strcmp(id, "SNAKE")) {
    for (int i = 0; i < 9; i++) {
      float x = 0.95f - i * 0.24f, y = sinf(t * 3 + i * 0.8f) * 0.35f, s = i ? 0.09f : 0.15f;
      B(x, y, 0, s, s, s);
    }
    B(-1.05f + fmodf(t * 0.3f, 0.1f), 0.6f, 0, 0.07f, 0.07f, 0.07f);
  } else if (!strcmp(id, "SOUND")) {
    B(-0.6f, 0, 0, 0.2f, 0.32f, 0.32f);
    R(0.05f, 0, 0, 0.7f, 0, 20);
    for (int i = 0; i < 4; i++) { float a = i * 1.5708f + 0.785f; L(-0.4f, sinf(a) * 0.3f, cosf(a) * 0.3f, 0.05f, sinf(a) * 0.7f, cosf(a) * 0.7f); }
    float p = fmodf(t * 1.2f, 1);
    for (int k = 0; k < 3; k++) { float r = 0.5f + (k + p) * 0.3f; ring3(v3(0.1f, 0, 0), r, 10, 2, C, (int)(A * (1 - (k + p) / 3)), -0.7f, 0.7f); }
  } else if (!strcmp(id, "FILES")) {
    static const float F[] = {-1, -0.55f, -0.35f, -0.55f, -0.2f, -0.4f, 1, -0.4f, 1, 0.7f, -1, 0.7f};
    solid(F, 6, 0.22f);
    float u = sinf(t * 2) * 0.1f;
    L(-0.7f, -0.7f + u, 0, 0.7f, -0.7f + u, 0); L(-0.7f, -0.7f + u, 0, -0.7f, -0.2f, 0); L(0.7f, -0.7f + u, 0, 0.7f, -0.2f, 0);
  } else if (!strcmp(id, "WHO")) {
    // a globe: meridians turning, three latitudes, a little moon going round
    for (int k = 0; k < 4; k++) {
      float a = t * 0.6f + k * 0.785f, ca = cosf(a), sa = sinf(a);
      V3 prev = v3(0, -1, 0);
      for (int i = 1; i <= 16; i++) { float b = i * 0.19635f; V3 p = v3(sinf(b) * ca, -cosf(b), sinf(b) * sa); l3(prev, p, C, A); prev = p; }
      prev = v3(0, -1, 0);
      for (int i = 1; i <= 16; i++) { float b = i * 0.19635f; V3 p = v3(-sinf(b) * ca, -cosf(b), -sinf(b) * sa); l3(prev, p, C, A); prev = p; }
    }
    for (int k = -1; k <= 1; k++) R(0, k * 0.5f, 0, sqrtf(1 - k * k * 0.25f), 1, 22);
    float o = t * 2; R(cosf(o) * 1.35f, -0.2f, sinf(o) * 1.35f, 0.08f, 2, 8);
  } else if (!strcmp(id, "CHAT")) {
    static const float P[] = {-1, -0.7f, 1, -0.7f, 1, 0.35f, -0.1f, 0.35f, -0.55f, 0.85f, -0.5f, 0.35f, -1, 0.35f};
    solid(P, 7, 0.2f);
    for (int k = 0; k < 3; k++) { float y = -0.17f - (fmodf(t * 2 - k * 0.4f, 2) < 0.5f ? 0.12f : 0); B(-0.45f + k * 0.45f, y, -0.2f, 0.07f, 0.07f, 0.03f); }
  } else if (!strcmp(id, "LINK")) {
    B(-0.25f, 0.1f, 0, 0.45f, 0.85f, 0.08f);
    B(-0.25f, 0.0f, -0.08f, 0.35f, 0.6f, 0.01f);
    float p = fmodf(t, 1);
    for (int k = 0; k < 3; k++) ring3(v3(0.35f, -0.8f, 0), 0.25f + (k + p) * 0.22f, 8, 2, C, (int)(A * (1 - (k + p) / 3)), -1.2f, 0.1f);
  } else if (!strcmp(id, "SETUP")) {
    int n = 8; float pts[64];
    for (int i = 0; i < n * 4; i++) {
      float a = (i / 4) * 6.2832f / n + ((i % 4) - 1.5f) * 0.17f + t * 0.8f;
      float r = (i % 4 == 1 || i % 4 == 2) ? 1.0f : 0.78f;
      pts[i * 2] = cosf(a) * r; pts[i * 2 + 1] = sinf(a) * r;
    }
    solid(pts, n * 4, 0.18f);
    R(0, 0, -0.18f, 0.32f, 2, 14); R(0, 0, 0.18f, 0.32f, 2, 14);
  } else if (!strcmp(id, "WIFI")) {
    float p = fmodf(t * 1.5f, 4);
    for (int k = 0; k < 3; k++) {
      int a = k < p ? A : A / 4;
      for (int z = -1; z <= 1; z += 2) ring3(v3(0, 0.75f, z * 0.12f), 0.45f + k * 0.38f, 10, 2, C, a, 3.93f, 5.5f);
    }
    B(0, 0.75f, 0, 0.1f, 0.1f, 0.1f);
  } else if (!strcmp(id, "UPDATE")) {
    float y = sinf(t * 3) * 0.15f;
    static const float P[] = {0, -1, 0.75f, -0.2f, 0.3f, -0.2f, 0.3f, 0.7f, -0.3f, 0.7f, -0.3f, -0.2f, -0.75f, -0.2f};
    float q[14]; for (int i = 0; i < 7; i++) { q[i * 2] = P[i * 2]; q[i * 2 + 1] = P[i * 2 + 1] + y; }
    solid(q, 7, 0.18f);
    L(-0.7f, 1, 0, 0.7f, 1, 0);
  } else if (!strcmp(id, "INFO")) {
    B(0, 0, 0, 0.7f, 0.7f, 0.12f);
    B(0, 0, -0.12f, 0.35f, 0.35f, 0.02f);
    for (int i = 0; i < 5; i++) {
      float u = -0.5f + i * 0.25f;
      L(u, -0.7f, 0, u, -0.95f, 0); L(u, 0.7f, 0, u, 0.95f, 0); L(-0.7f, u, 0, -0.95f, u, 0); L(0.7f, u, 0, 0.95f, u, 0);
    }
  } else if (!strcmp(id, "COMMAND")) {
    B(0, -0.15f, 0, 1, 0.7f, 0.12f);
    L(0, 0.55f, 0, 0, 0.8f, 0); B(0, 0.85f, 0, 0.45f, 0.05f, 0.25f);
    L(-0.7f, -0.45f, -0.13f, -0.45f, -0.25f, -0.13f); L(-0.45f, -0.25f, -0.13f, -0.7f, -0.05f, -0.13f);
    if (fmodf(t, 1) < 0.5f) L(-0.35f, -0.05f, -0.13f, 0.05f, -0.05f, -0.13f);
  } else {
    // web apps: a cube in a cube, turning both ways
    B(0, 0, 0, 0.85f, 0.85f, 0.85f);
    float a = -t * 1.7f, ca = cosf(a), sa = sinf(a), s = 0.42f;
    V3 v[8];
    for (int i = 0; i < 8; i++) { float x = i & 1 ? s : -s, y = i & 2 ? s : -s, z = i & 4 ? s : -s; v[i] = v3(x * ca - z * sa, y, x * sa + z * ca); }
    static const uint8_t E[24] = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 2, 6, 3, 7};
    for (int i = 0; i < 24; i += 2) l3(v[E[i]], v[E[i + 1]], C, A);
  }
}
