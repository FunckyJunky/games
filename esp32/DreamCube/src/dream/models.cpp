// models.cpp: DREAM-CUBE's things, built from the unit shapes. Model space: y = 0 is the ground, the front faces +z,
// about one unit = one metre. S() = a fixed part (centre, full sizes, turn in degrees); J() = a moving part (pivot,
// the shape's centre from the pivot, full sizes); the movement names are in world.h (A_*), scene.cpp moves them.
#include "world.h"
#include <string.h>
#include <strings.h>
#include <math.h>

#define MAXP 2400
#define MAXM 140
static Part* pool = nullptr; static int np = 0;   // in PSRAM on the cube (big)
static Model table[MAXM]; static int nm = 0;
static uint8_t special[MAXM];
const Model* MODELS = table;
int N_MODELS = 0;
static const float D = 0.0174533f;

static void add(int anim, int prim, int c, float px, float py, float pz, float ox, float oy, float oz, float w, float h, float d,
                float rx, float ry, float rz, float ph, int fl) {
  if (!pool || np >= MAXP) return;
  Part& p = pool[np++];
  p.prim = prim; p.col = c; p.anim = anim; p.fl = fl;
  p.px = px; p.py = py; p.pz = pz; p.ox = ox; p.oy = oy; p.oz = oz;
  p.sx = w * 0.5f; p.sy = h * 0.5f; p.sz = d * 0.5f;
  p.rx = rx * D; p.ry = ry * D; p.rz = rz * D; p.ph = ph;
  table[nm - 1].np++;
}
static void S(int prim, int c, float x, float y, float z, float w, float h, float d, float rx = 0, float ry = 0, float rz = 0, int fl = 0) {
  add(A_NONE, prim, c, x, y, z, 0, 0, 0, w, h, d, rx, ry, rz, 0, fl);
}
static void J(int anim, int prim, int c, float px, float py, float pz, float ox, float oy, float oz, float w, float h, float d,
              float rx = 0, float ry = 0, float rz = 0, float ph = 0, int fl = 0) {
  add(anim, prim, c, px, py, pz, ox, oy, oz, w, h, d, rx, ry, rz, ph, fl);
}
static void M(const char* name, uint32_t c1, uint32_t c2, float rad, float h, int kind, int anim = B_IDLE, int sp = 0) {
  if (nm >= MAXM) return;
  Model& m = table[nm]; special[nm] = sp; nm++;
  m.name = name; m.c1 = c1; m.c2 = c2; m.rad = rad; m.h = h; m.kind = kind; m.anim = anim; m.p0 = np; m.np = 0;
}
// two eyes on a head (pivot hx, hy, hz with the head's movement), facing +z
static void eyes(int anim, float hx, float hy, float hz, float dx, float y, float z, float s, int c = CL_BLACK) {
  J(anim, P_BOX, c, hx, hy, hz, -dx, y, z, s, s * 1.2f, s * 0.5f);
  J(anim, P_BOX, c, hx, hy, hz, dx, y, z, s, s * 1.2f, s * 0.5f);
}
static void legs4(int prim, int c, float x, float y, float z, float w, float len) {
  J(A_LEGL, prim, c, -x, y, z, 0, -len / 2, 0, w, len, w);
  J(A_LEGR, prim, c, x, y, z, 0, -len / 2, 0, w, len, w);
  J(A_LEGR, prim, c, -x, y, -z, 0, -len / 2, 0, w, len, w);
  J(A_LEGL, prim, c, x, y, -z, 0, -len / 2, 0, w, len, w);
}
static void wheels(int c, float x, float y, float z1, float z2, float d, float w) {
  float zs[2] = {z1, z2};
  for (int i = 0; i < 2; i++) for (int s = -1; s <= 1; s += 2) J(A_SPINX, P_CYL, c, s * x, y, zs[i], 0, 0, 0, d, w, d, 0, 0, 90);
}

// ------------------------------------------------------------------ people and creatures
static void person(bool kid) {
  float s = kid ? 0.62f : 1, r = (kid ? 0.25f : 0.2f) * s / (kid ? 0.62f : 1) * (kid ? 0.62f : 1);
  if (kid) r = 0.16f;
  for (int k = -1; k <= 1; k += 2) {
    int a = k < 0 ? A_LEGL : A_LEGR, b = k < 0 ? A_ARML : A_ARMR;
    J(a, P_BOX, CL_SEC, k * 0.13f * s, 0.9f * s, 0, 0, -0.42f * s, 0, 0.21f * s, 0.84f * s, 0.24f * s);
    J(a, P_BOX, CL_DARK, k * 0.13f * s, 0.9f * s, 0, 0, -0.85f * s, 0.05f * s, 0.23f * s, 0.1f * s, 0.34f * s);
    J(b, P_BOX, CL_MAIN, k * 0.36f * s, 1.5f * s, 0, 0, -0.29f * s, 0, 0.15f * s, 0.6f * s, 0.17f * s);
    J(b, P_BOX, CL_SKIN, k * 0.36f * s, 1.5f * s, 0, 0, -0.65f * s, 0, 0.13f * s, 0.14f * s, 0.14f * s);
  }
  S(P_BOX, CL_MAIN, 0, 1.22f * s, 0, 0.56f * s, 0.7f * s, 0.32f * s);
  float hy = 1.57f * s;
  J(A_HEAD, P_BALL, CL_SKIN, 0, hy, 0, 0, r * 1.05f, 0, r * 2, r * 2.1f, r * 2);
  J(A_HEAD, P_DOME, CL_HAIR, 0, hy, 0, 0, r * 1.72f, -r * 0.22f, r * 2.2f, r * 0.95f, r * 2.1f);
  J(A_HEAD, P_BOX, CL_HAIR, 0, hy, 0, 0, r * 1.25f, -r * 0.62f, r * 1.9f, r * 1.1f, r * 0.8f);
  eyes(A_HEAD, 0, hy, 0, r * 0.36f, r * 1.1f, r * 0.93f, r * 0.2f);
  J(A_HEAD, P_BOX, CL_RED, 0, hy, 0, 0, r * 0.62f, r * 0.9f, r * 0.45f, r * 0.1f, r * 0.12f);
}
static void robot() {
  for (int k = -1; k <= 1; k += 2) {
    J(k < 0 ? A_LEGL : A_LEGR, P_BOX, CL_SILVER, k * 0.18f, 0.78f, 0, 0, -0.38f, 0, 0.22f, 0.76f, 0.26f);
    J(k < 0 ? A_LEGL : A_LEGR, P_BOX, CL_DARK, k * 0.18f, 0.78f, 0, 0, -0.74f, 0.04f, 0.28f, 0.1f, 0.36f);
    J(k < 0 ? A_ARML : A_ARMR, P_BOX, CL_SILVER, k * 0.47f, 1.46f, 0, 0, -0.3f, 0, 0.16f, 0.6f, 0.16f);
    J(k < 0 ? A_ARML : A_ARMR, P_BOX, CL_DARK, k * 0.47f, 1.46f, 0, 0, -0.66f, 0, 0.2f, 0.14f, 0.2f);
  }
  S(P_BOX, CL_MAIN, 0, 1.15f, 0, 0.76f, 0.76f, 0.5f);
  S(P_BOX, CL_SCREEN, 0, 1.2f, 0.255f, 0.4f, 0.24f, 0.02f);
  J(A_HEAD, P_BOX, CL_MAIN, 0, 1.55f, 0, 0, 0.24f, 0, 0.56f, 0.44f, 0.46f);
  J(A_HEAD, P_BOX, CL_EYE, 0, 1.55f, 0, -0.13f, 0.27f, 0.235f, 0.13f, 0.08f, 0.02f);
  J(A_HEAD, P_BOX, CL_EYE, 0, 1.55f, 0, 0.13f, 0.27f, 0.235f, 0.13f, 0.08f, 0.02f);
  J(A_HEAD, P_CYL, CL_DARK, 0, 1.55f, 0, 0, 0.56f, 0, 0.04f, 0.22f, 0.04f);
  J(A_HEAD, P_BALL, CL_FIRE, 0, 1.55f, 0, 0, 0.7f, 0, 0.12f, 0.12f, 0.12f);
}
static void ghost() {
  J(A_BOB, P_DOME, CL_MAIN, 0, 0, 0, 0, 1.0f, 0, 1.0f, 1.6f, 1.0f, 0, 0, 0, 0, R_ALPHA);
  J(A_BOB, P_BOX, CL_BLACK, 0, 0, 0, -0.17f, 1.35f, 0.44f, 0.12f, 0.2f, 0.06f);
  J(A_BOB, P_BOX, CL_BLACK, 0, 0, 0, 0.17f, 1.35f, 0.44f, 0.12f, 0.2f, 0.06f);
  J(A_BOB, P_BALL, CL_BLACK, 0, 0, 0, 0, 1.0f, 0.46f, 0.18f, 0.2f, 0.06f);
  J(A_ARML, P_BALL, CL_MAIN, -0.5f, 1.0f, 0, 0, -0.1f, 0, 0.22f, 0.32f, 0.22f, 0, 0, 0, 0, R_ALPHA);
  J(A_ARMR, P_BALL, CL_MAIN, 0.5f, 1.0f, 0, 0, -0.1f, 0, 0.22f, 0.32f, 0.22f, 0, 0, 0, 0, R_ALPHA);
}
static void alien() {
  for (int k = -1; k <= 1; k += 2) {
    J(k < 0 ? A_LEGL : A_LEGR, P_BOX, CL_MAIN, k * 0.1f, 0.6f, 0, 0, -0.3f, 0, 0.12f, 0.6f, 0.12f);
    J(k < 0 ? A_ARML : A_ARMR, P_BOX, CL_MAIN, k * 0.22f, 1.0f, 0, 0, -0.25f, 0, 0.08f, 0.5f, 0.08f);
  }
  S(P_BOX, CL_SEC, 0, 0.82f, 0, 0.34f, 0.46f, 0.24f);
  J(A_HEAD, P_BALL, CL_MAIN, 0, 1.06f, 0, 0, 0.36f, 0, 0.7f, 0.8f, 0.66f);
  J(A_HEAD, P_BALL, CL_BLACK, 0, 1.06f, 0, -0.15f, 0.36f, 0.27f, 0.22f, 0.3f, 0.1f, 0, 0, -25);
  J(A_HEAD, P_BALL, CL_BLACK, 0, 1.06f, 0, 0.15f, 0.36f, 0.27f, 0.22f, 0.3f, 0.1f, 0, 0, 25);
  for (int k = -1; k <= 1; k += 2) {
    J(A_HEAD, P_CYL, CL_MAIN, 0, 1.06f, 0, k * 0.16f, 0.82f, 0, 0.03f, 0.3f, 0.03f, 0, 0, -k * 20.0f);
    J(A_HEAD, P_BALL, CL_GLOWM, 0, 1.06f, 0, k * 0.22f, 0.98f, 0, 0.09f, 0.09f, 0.09f);
  }
}
static void monster() {
  S(P_BOX, CL_MAIN, 0, 1.15f, 0, 1.3f, 1.4f, 1.0f);
  S(P_BALL, CL_WHITE, 0, 1.5f, 0.45f, 0.6f, 0.6f, 0.3f);
  J(A_HEAD, P_BALL, CL_BLACK, 0, 1.5f, 0.6f, 0, 0, 0, 0.26f, 0.26f, 0.1f);
  S(P_BOX, CL_DARK, 0, 0.95f, 0.5f, 0.8f, 0.24f, 0.04f);
  for (int i = -1; i <= 1; i++) S(P_PYR, CL_WHITE, i * 0.25f, 1.0f, 0.53f, 0.16f, 0.16f, 0.06f, 0, 0, 180);
  for (int k = -1; k <= 1; k += 2) {
    S(P_CONE, CL_WHITE, k * 0.45f, 2.05f, 0, 0.22f, 0.5f, 0.22f, 0, 0, -k * 20.0f);
    J(k < 0 ? A_LEGL : A_LEGR, P_BOX, CL_DEEP, k * 0.36f, 0.5f, 0, 0, -0.25f, 0.05f, 0.36f, 0.5f, 0.44f);
    J(k < 0 ? A_ARML : A_ARMR, P_BOX, CL_DEEP, k * 0.76f, 1.55f, 0, 0, -0.42f, 0, 0.22f, 0.85f, 0.26f);
  }
}
static void dog(bool cat) {
  float bw = cat ? 0.32f : 0.42f, bh = cat ? 0.3f : 0.36f, bl = cat ? 0.72f : 0.85f, by = cat ? 0.46f : 0.55f;
  S(P_BOX, CL_MAIN, 0, by, 0, bw, bh, bl);
  legs4(P_BOX, CL_MAIN, bw * 0.33f, by - bh * 0.35f, bl * 0.36f, cat ? 0.09f : 0.12f, by - bh * 0.35f);
  float hy = by + bh * 0.4f, hz = bl * 0.46f, hs = cat ? 0.3f : 0.34f;
  J(A_HEAD, P_BOX, CL_MAIN, 0, hy, hz, 0, hs * 0.38f, hs * 0.36f, hs, hs * 0.92f, hs);
  J(A_HEAD, P_BOX, cat ? CL_WHITE : CL_LITE, 0, hy, hz, 0, hs * 0.15f, hs * 0.95f, hs * 0.55f, hs * 0.42f, hs * 0.4f);
  J(A_HEAD, P_BOX, cat ? CL_PINK : CL_BLACK, 0, hy, hz, 0, hs * 0.3f, hs * 1.17f, hs * 0.22f, hs * 0.16f, hs * 0.08f);
  eyes(A_HEAD, 0, hy, hz, hs * 0.25f, hs * 0.6f, hs * 0.87f, hs * 0.14f, cat ? CL_GREEN : CL_BLACK);
  for (int k = -1; k <= 1; k += 2) {
    if (cat) J(A_HEAD, P_PYR, CL_MAIN, 0, hy, hz, k * hs * 0.3f, hs * 0.98f, hs * 0.3f, hs * 0.3f, hs * 0.36f, hs * 0.18f);
    else J(A_HEAD, P_BOX, CL_SEC, 0, hy, hz, k * hs * 0.56f, hs * 0.5f, hs * 0.25f, hs * 0.16f, hs * 0.65f, hs * 0.4f, 0, 0, k * 10.0f);
  }
  J(A_TAIL, P_BOX, CL_MAIN, 0, by + bh * 0.25f, -bl * 0.5f, 0, cat ? 0.28f : 0.16f, -0.02f, 0.07f, cat ? 0.56f : 0.32f, 0.07f, cat ? -18.0f : -42.0f);
}
static void bird() {
  S(P_BALL, CL_MAIN, 0, 0.36f, 0, 0.36f, 0.32f, 0.5f);
  S(P_BALL, CL_SEC, 0, 0.32f, 0.08f, 0.28f, 0.24f, 0.36f);
  J(A_HEAD, P_BALL, CL_MAIN, 0, 0.46f, 0.17f, 0, 0.1f, 0.06f, 0.26f, 0.26f, 0.26f);
  J(A_HEAD, P_CONE, CL_ORANGE, 0, 0.46f, 0.17f, 0, 0.08f, 0.26f, 0.08f, 0.18f, 0.08f, 90);
  eyes(A_HEAD, 0, 0.46f, 0.17f, 0.07f, 0.14f, 0.17f, 0.045f);
  J(A_WINGL, P_BOX, CL_DEEP, -0.14f, 0.42f, 0, -0.26f, 0, -0.02f, 0.52f, 0.04f, 0.3f);
  J(A_WINGR, P_BOX, CL_DEEP, 0.14f, 0.42f, 0, 0.26f, 0, -0.02f, 0.52f, 0.04f, 0.3f);
  S(P_BOX, CL_DEEP, 0, 0.4f, -0.32f, 0.18f, 0.03f, 0.24f, -15);
  S(P_BOX, CL_ORANGE, -0.06f, 0.1f, 0, 0.03f, 0.2f, 0.03f);
  S(P_BOX, CL_ORANGE, 0.06f, 0.1f, 0, 0.03f, 0.2f, 0.03f);
}
static void fish(bool whale) {
  float k = whale ? 4 : 1;
  S(P_BALL, CL_MAIN, 0, 0.4f * k, 0, 0.32f * k * (whale ? 1.5f : 1), 0.5f * k, 0.9f * k);
  if (whale) S(P_BALL, CL_SEC, 0, 0.27f * k, 0.05f * k, 0.36f * k, 0.25f * k, 0.75f * k);
  J(A_FINS, P_BOX, CL_SEC, 0, 0.4f * k, -0.42f * k, 0, 0, -0.16f * k, 0.04f * k * (whale ? 6 : 1), whale ? 0.04f * k : 0.42f * k, 0.3f * k);
  if (!whale) S(P_BOX, CL_SEC, 0, 0.68f, -0.05f, 0.03f, 0.18f, 0.32f, -20);
  for (int s = -1; s <= 1; s += 2) {
    S(P_BALL, CL_WHITE, s * 0.14f * k * (whale ? 1.4f : 1), 0.48f * k, 0.25f * k, 0.1f * k, 0.1f * k, 0.1f * k);
    S(P_BALL, CL_BLACK, s * 0.17f * k * (whale ? 1.3f : 1), 0.48f * k, 0.28f * k, 0.06f * k, 0.06f * k, 0.06f * k);
  }
}
static void horse(bool cow) {
  float by = cow ? 1.15f : 1.3f, bw = cow ? 0.85f : 0.6f, bh = cow ? 0.8f : 0.6f, bl = cow ? 1.7f : 1.5f, ll = by - bh * 0.4f;
  S(P_BOX, CL_MAIN, 0, by, 0, bw, bh, bl);
  legs4(P_BOX, CL_MAIN, bw * 0.33f, ll, bl * 0.38f, 0.17f, ll);
  J(A_LEGL, P_BOX, CL_DARK, -bw * 0.33f, ll, bl * 0.38f, 0, -ll + 0.06f, 0, 0.19f, 0.12f, 0.19f);
  J(A_LEGR, P_BOX, CL_DARK, bw * 0.33f, ll, bl * 0.38f, 0, -ll + 0.06f, 0, 0.19f, 0.12f, 0.19f);
  J(A_LEGR, P_BOX, CL_DARK, -bw * 0.33f, ll, -bl * 0.38f, 0, -ll + 0.06f, 0, 0.19f, 0.12f, 0.19f);
  J(A_LEGL, P_BOX, CL_DARK, bw * 0.33f, ll, -bl * 0.38f, 0, -ll + 0.06f, 0, 0.19f, 0.12f, 0.19f);
  if (cow) {
    S(P_BOX, CL_SEC, bw * 0.5f, by + 0.1f, 0.3f, 0.04f, 0.45f, 0.5f);
    S(P_BOX, CL_SEC, -bw * 0.5f, by - 0.1f, -0.35f, 0.04f, 0.4f, 0.6f);
    S(P_BOX, CL_SEC, 0, by + bh * 0.5f, -0.2f, 0.4f, 0.04f, 0.5f);
    J(A_HEAD, P_BOX, CL_MAIN, 0, by + 0.15f, bl * 0.5f, 0, 0.05f, 0.25f, 0.5f, 0.5f, 0.55f);
    J(A_HEAD, P_BOX, CL_PINK, 0, by + 0.15f, bl * 0.5f, 0, -0.12f, 0.56f, 0.44f, 0.26f, 0.12f);
    eyes(A_HEAD, 0, by + 0.15f, bl * 0.5f, 0.14f, 0.15f, 0.53f, 0.07f);
    for (int k = -1; k <= 1; k += 2) {
      J(A_HEAD, P_CONE, CL_WHITE, 0, by + 0.15f, bl * 0.5f, k * 0.3f, 0.38f, 0.2f, 0.08f, 0.24f, 0.08f, 0, 0, -k * 50.0f);
      J(A_HEAD, P_BOX, CL_MAIN, 0, by + 0.15f, bl * 0.5f, k * 0.32f, 0.18f, 0.12f, 0.18f, 0.08f, 0.12f);
    }
    J(A_TAIL, P_BOX, CL_MAIN, 0, by + 0.3f, -bl * 0.5f, 0, -0.35f, -0.03f, 0.06f, 0.7f, 0.06f, 15);
  } else {
    S(P_BOX, CL_MAIN, 0, by + 0.45f, bl * 0.47f, 0.32f, 0.8f, 0.38f, 35);
    S(P_BOX, CL_SEC, 0, by + 0.56f, bl * 0.4f, 0.1f, 0.8f, 0.22f, 35);
    J(A_HEAD, P_BOX, CL_MAIN, 0, by + 0.78f, bl * 0.62f, 0, 0, 0.2f, 0.3f, 0.32f, 0.66f, 25);
    eyes(A_HEAD, 0, by + 0.78f, bl * 0.62f, 0.16f, 0.06f, 0.12f, 0.06f);
    for (int k = -1; k <= 1; k += 2) J(A_HEAD, P_PYR, CL_MAIN, 0, by + 0.78f, bl * 0.62f, k * 0.09f, 0.22f, -0.06f, 0.08f, 0.16f, 0.08f);
    J(A_TAIL, P_BOX, CL_SEC, 0, by + 0.2f, -bl * 0.5f, 0, -0.38f, -0.06f, 0.13f, 0.8f, 0.13f, 20);
  }
}
static void pig() {
  S(P_BALL, CL_MAIN, 0, 0.62f, 0, 1.0f, 0.85f, 1.3f);
  legs4(P_BOX, CL_DEEP, 0.28f, 0.32f, 0.4f, 0.16f, 0.32f);
  J(A_HEAD, P_BALL, CL_MAIN, 0, 0.75f, 0.55f, 0, 0.05f, 0.12f, 0.62f, 0.6f, 0.58f);
  J(A_HEAD, P_CYL, CL_DEEP, 0, 0.75f, 0.55f, 0, 0.0f, 0.46f, 0.26f, 0.12f, 0.22f, 90);
  eyes(A_HEAD, 0, 0.75f, 0.55f, 0.13f, 0.17f, 0.37f, 0.06f);
  for (int k = -1; k <= 1; k += 2) J(A_HEAD, P_PYR, CL_DEEP, 0, 0.75f, 0.55f, k * 0.2f, 0.36f, 0.05f, 0.16f, 0.2f, 0.08f, -20);
  J(A_TAIL, P_TORUS, CL_DEEP, 0, 0.72f, -0.64f, 0, 0.04f, -0.04f, 0.14f, 0.14f, 0.14f, 90);
}
static void sheep() {
  S(P_BALL, CL_MAIN, 0, 0.75f, 0, 0.9f, 0.7f, 1.2f);
  S(P_BALL, CL_MAIN, -0.25f, 0.9f, 0.25f, 0.55f, 0.55f, 0.55f);
  S(P_BALL, CL_MAIN, 0.25f, 0.92f, -0.25f, 0.55f, 0.55f, 0.55f);
  S(P_BALL, CL_MAIN, 0.2f, 0.88f, 0.3f, 0.5f, 0.5f, 0.5f);
  S(P_BALL, CL_MAIN, -0.22f, 0.86f, -0.32f, 0.5f, 0.5f, 0.5f);
  legs4(P_BOX, CL_SEC, 0.24f, 0.45f, 0.35f, 0.1f, 0.45f);
  J(A_HEAD, P_BOX, CL_SEC, 0, 0.9f, 0.55f, 0, 0, 0.16f, 0.3f, 0.36f, 0.36f, 15);
  eyes(A_HEAD, 0, 0.9f, 0.55f, 0.1f, 0.06f, 0.34f, 0.05f, CL_WHITE);
  for (int k = -1; k <= 1; k += 2) J(A_HEAD, P_BOX, CL_SEC, 0, 0.9f, 0.55f, k * 0.2f, 0.08f, 0.1f, 0.18f, 0.07f, 0.1f);
}
static void rabbit() {
  S(P_BALL, CL_MAIN, 0, 0.3f, 0, 0.5f, 0.5f, 0.62f);
  J(A_HEAD, P_BALL, CL_MAIN, 0, 0.5f, 0.2f, 0, 0.12f, 0.05f, 0.36f, 0.34f, 0.34f);
  eyes(A_HEAD, 0, 0.5f, 0.2f, 0.08f, 0.16f, 0.2f, 0.05f);
  J(A_HEAD, P_BOX, CL_PINK, 0, 0.5f, 0.2f, 0, 0.09f, 0.22f, 0.06f, 0.04f, 0.03f);
  for (int k = -1; k <= 1; k += 2) {
    J(A_HEAD, P_BOX, CL_MAIN, 0, 0.5f, 0.2f, k * 0.08f, 0.48f, 0, 0.09f, 0.44f, 0.05f, 0, 0, -k * 8.0f);
    J(A_HEAD, P_BOX, CL_PINK, 0, 0.5f, 0.2f, k * 0.08f, 0.48f, 0.026f, 0.05f, 0.34f, 0.01f, 0, 0, -k * 8.0f);
    S(P_BOX, CL_MAIN, k * 0.14f, 0.05f, 0.12f, 0.12f, 0.1f, 0.3f);
  }
  S(P_BALL, CL_WHITE, 0, 0.36f, -0.32f, 0.16f, 0.16f, 0.16f);
}
static void bear(bool panda) {
  S(P_BALL, CL_MAIN, 0, 0.78f, 0, 1.1f, 1.15f, 1.0f);
  if (panda) S(P_BALL, CL_SEC, 0, 0.98f, 0, 1.12f, 0.45f, 1.02f);
  S(P_BALL, CL_LITE, 0, 0.72f, 0.32f, 0.6f, 0.7f, 0.4f);
  J(A_HEAD, P_BALL, CL_MAIN, 0, 1.25f, 0.15f, 0, 0.25f, 0.1f, 0.72f, 0.66f, 0.66f);
  J(A_HEAD, P_BALL, CL_LITE, 0, 1.25f, 0.15f, 0, 0.16f, 0.42f, 0.3f, 0.24f, 0.2f);
  J(A_HEAD, P_BOX, CL_BLACK, 0, 1.25f, 0.15f, 0, 0.22f, 0.52f, 0.1f, 0.07f, 0.04f);
  if (panda) for (int k = -1; k <= 1; k += 2) J(A_HEAD, P_BALL, CL_SEC, 0, 1.25f, 0.15f, k * 0.14f, 0.32f, 0.36f, 0.16f, 0.2f, 0.06f, 0, 0, k * 20.0f);
  eyes(A_HEAD, 0, 1.25f, 0.15f, 0.14f, 0.33f, 0.4f, 0.06f, panda ? CL_WHITE : CL_BLACK);
  for (int k = -1; k <= 1; k += 2) {
    J(A_HEAD, P_BALL, panda ? CL_SEC : CL_MAIN, 0, 1.25f, 0.15f, k * 0.28f, 0.56f, 0, 0.24f, 0.24f, 0.14f);
    J(k < 0 ? A_LEGL : A_LEGR, P_CYL, panda ? CL_SEC : CL_MAIN, k * 0.3f, 0.35f, 0.1f, 0, -0.17f, 0, 0.3f, 0.36f, 0.3f);
    J(k < 0 ? A_ARML : A_ARMR, P_BALL, panda ? CL_SEC : CL_MAIN, k * 0.5f, 1.05f, 0.15f, 0, -0.25f, 0.05f, 0.26f, 0.55f, 0.26f);
  }
}
static void elephant() {
  S(P_BALL, CL_MAIN, 0, 1.5f, 0, 1.7f, 1.5f, 2.2f);
  legs4(P_CYL, CL_MAIN, 0.45f, 1.0f, 0.6f, 0.45f, 1.0f);
  J(A_HEAD, P_BALL, CL_MAIN, 0, 1.8f, 1.0f, 0, 0.1f, 0.2f, 1.0f, 1.0f, 0.9f);
  for (int k = -1; k <= 1; k += 2) {
    J(A_HEAD, P_DISC, CL_LITE, 0, 1.8f, 1.0f, k * 0.56f, 0.1f, 0.05f, 1.0f, 0.06f, 0.9f, 0, 0, 90);
    J(A_HEAD, P_CONE, CL_WHITE, 0, 1.8f, 1.0f, k * 0.22f, -0.3f, 0.56f, 0.08f, 0.4f, 0.08f, 70);
  }
  eyes(A_HEAD, 0, 1.8f, 1.0f, 0.26f, 0.25f, 0.6f, 0.07f);
  J(A_TRUNK, P_CYL, CL_MAIN, 0, 1.75f, 1.6f, 0, -0.35f, 0.02f, 0.26f, 0.75f, 0.26f, 8);
  J(A_TRUNK, P_CYL, CL_MAIN, 0, 1.75f, 1.6f, 0, -0.85f, 0.1f, 0.18f, 0.4f, 0.18f, 14, 0, 0, 0.4f);
  J(A_TAIL, P_BOX, CL_MAIN, 0, 1.6f, -1.08f, 0, -0.3f, -0.03f, 0.06f, 0.6f, 0.06f, 15);
}
static void snake() {
  for (int i = 0; i < 9; i++) {
    float z = 0.9f - i * 0.25f, r = i == 0 ? 0.3f : 0.26f - i * 0.015f;
    J(A_SNAKE, P_BALL, i & 1 ? CL_SEC : CL_MAIN, 0, r * 0.5f, z, 0, 0, 0, r, r * 0.9f, 0.34f, 0, 0, 0, i * 0.7f);
  }
  J(A_SNAKE, P_BOX, CL_BLACK, 0, 0.15f, 0.9f, -0.08f, 0.06f, 0.1f, 0.05f, 0.06f, 0.03f);
  J(A_SNAKE, P_BOX, CL_BLACK, 0, 0.15f, 0.9f, 0.08f, 0.06f, 0.1f, 0.05f, 0.06f, 0.03f);
  J(A_SNAKE, P_BOX, CL_RED, 0, 0.1f, 0.9f, 0, 0, 0.2f, 0.04f, 0.01f, 0.16f);
}
static void butterfly() {
  S(P_CYL, CL_DARK, 0, 0.3f, 0, 0.06f, 0.5f, 0.06f, 90);
  for (int k = -1; k <= 1; k += 2) {
    int a = k < 0 ? A_WINGL : A_WINGR;
    J(a, P_BOX, CL_MAIN, k * 0.03f, 0.3f, 0, k * 0.28f, 0, 0.1f, 0.54f, 0.02f, 0.42f, 0, k * 10.0f);
    J(a, P_BOX, CL_SEC, k * 0.03f, 0.3f, 0, k * 0.22f, -0.005f, -0.15f, 0.4f, 0.02f, 0.3f, 0, -k * 15.0f);
    S(P_CYL, CL_DARK, k * 0.06f, 0.42f, 0.28f, 0.015f, 0.26f, 0.015f, 50, 0, -k * 20.0f);
  }
}
static void spider() {
  S(P_BALL, CL_MAIN, 0, 0.4f, -0.15f, 0.55f, 0.48f, 0.6f);
  S(P_BALL, CL_MAIN, 0, 0.36f, 0.25f, 0.32f, 0.28f, 0.32f);
  eyes(A_NONE, 0, 0.36f, 0.25f, 0.06f, 0.06f, 0.15f, 0.05f, CL_FIRE);
  for (int i = 0; i < 4; i++) for (int k = -1; k <= 1; k += 2) {
    float z = 0.35f - i * 0.18f;
    J((i + (k > 0)) & 1 ? A_LEGL : A_LEGR, P_BOX, CL_MAIN, k * 0.12f, 0.4f, z, k * 0.3f, -0.12f, 0, 0.6f, 0.05f, 0.05f, 0, (1.5f - i) * 18.0f * k, -k * 32.0f);
  }
}
static void dino(bool dragon) {
  S(P_BALL, CL_MAIN, 0, 1.6f, 0, 1.1f, 1.2f, 1.9f, -15);
  for (int k = -1; k <= 1; k += 2) {
    J(k < 0 ? A_LEGL : A_LEGR, P_BOX, CL_MAIN, k * 0.38f, 1.3f, -0.15f, 0, -0.62f, 0, 0.34f, 1.25f, 0.45f);
    J(k < 0 ? A_LEGL : A_LEGR, P_BOX, CL_DEEP, k * 0.38f, 1.3f, -0.15f, 0, -1.24f, 0.12f, 0.4f, 0.12f, 0.6f);
    S(P_BOX, CL_MAIN, k * 0.4f, 1.75f, 0.78f, 0.12f, 0.36f, 0.12f, 40);
  }
  S(P_CONE, CL_MAIN, 0, 1.5f, -1.45f, 0.7f, 1.8f, 0.7f, -100);
  S(P_BOX, CL_MAIN, 0, 2.15f, 0.8f, 0.55f, 0.75f, 0.6f, 30);
  J(A_HEAD, P_BOX, CL_MAIN, 0, 2.25f, 0.7f, 0, 0.25f, 0.35f, 0.72f, 0.6f, 1.0f);
  J(A_HEAD, P_BOX, CL_DEEP, 0, 2.25f, 0.7f, 0, -0.08f, 0.32f, 0.6f, 0.2f, 0.85f);
  for (int i = 0; i < 3; i++) for (int k = -1; k <= 1; k += 2)
    J(A_HEAD, P_PYR, CL_WHITE, 0, 2.25f, 0.7f, k * 0.22f, 0.02f, 0.55f + i * 0.12f, 0.07f, 0.12f, 0.07f, 0, 0, 180);
  eyes(A_HEAD, 0, 2.25f, 0.7f, 0.2f, 0.42f, 0.52f, 0.1f, dragon ? CL_FIRE : CL_BLACK);
  for (int i = 0; i < 4; i++) S(P_PYR, CL_SEC, 0, 2.2f - i * 0.12f, 0.2f - i * 0.45f, 0.12f, 0.36f, 0.3f);
  if (dragon) {
    J(A_WINGL, P_BOX, CL_SEC, -0.4f, 2.1f, -0.1f, -0.9f, 0.15f, 0, 1.8f, 0.05f, 1.2f, 0, -10);
    J(A_WINGR, P_BOX, CL_SEC, 0.4f, 2.1f, -0.1f, 0.9f, 0.15f, 0, 1.8f, 0.05f, 1.2f, 0, 10);
    for (int k = -1; k <= 1; k += 2) J(A_HEAD, P_CONE, CL_GOLD, 0, 2.25f, 0.7f, k * 0.25f, 0.68f, 0.05f, 0.1f, 0.35f, 0.1f, -30);
  }
}
static void snowman() {
  S(P_BALL, CL_WHITE, 0, 0.5f, 0, 1.0f, 1.0f, 1.0f, 0, 0, 0, R_SOFT);
  S(P_BALL, CL_WHITE, 0, 1.25f, 0, 0.72f, 0.72f, 0.72f, 0, 0, 0, R_SOFT);
  J(A_HEAD, P_BALL, CL_WHITE, 0, 1.6f, 0, 0, 0.25f, 0, 0.5f, 0.5f, 0.5f, 0, 0, 0, 0, R_SOFT);
  J(A_HEAD, P_CONE, CL_ORANGE, 0, 1.6f, 0, 0, 0.25f, 0.33f, 0.08f, 0.3f, 0.08f, 90);
  eyes(A_HEAD, 0, 1.6f, 0, 0.09f, 0.33f, 0.22f, 0.06f);
  J(A_HEAD, P_CYL, CL_DARK, 0, 1.6f, 0, 0, 0.52f, 0, 0.56f, 0.04f, 0.56f);
  J(A_HEAD, P_CYL, CL_DARK, 0, 1.6f, 0, 0, 0.7f, 0, 0.36f, 0.34f, 0.36f);
  S(P_TORUS, CL_MAIN, 0, 1.5f, 0, 0.5f, 0.3f, 0.5f);
  for (int i = 0; i < 3; i++) S(P_BOX, CL_DARK, 0, 1.12f + i * 0.14f, 0.35f, 0.06f, 0.06f, 0.04f);
  for (int k = -1; k <= 1; k += 2) J(k < 0 ? A_ARML : A_ARMR, P_BOX, CL_WOOD, k * 0.3f, 1.35f, 0, k * 0.35f, 0.12f, 0, 0.65f, 0.04f, 0.04f, 0, 0, k * 25.0f);
}

// ------------------------------------------------------------------ nature
static void tree() {
  S(P_CYL, CL_WOOD, 0, 0.8f, 0, 0.32f, 1.6f, 0.32f);
  J(A_SWAY, P_ROCK, CL_MAIN, 0, 1.2f, 0, 0, 1.15f, 0, 1.9f, 1.6f, 1.9f);
  J(A_SWAY, P_ROCK, CL_MAIN, 0, 1.2f, 0, 0.45f, 1.55f, 0.25f, 1.2f, 1.1f, 1.2f, 0, 40);
  J(A_SWAY, P_ROCK, CL_DEEP, 0, 1.2f, 0, -0.45f, 1.35f, -0.3f, 1.25f, 1.15f, 1.25f, 0, 80);
}
static void pine() {
  S(P_CYL, CL_WOOD, 0, 0.45f, 0, 0.3f, 0.9f, 0.3f);
  for (int i = 0; i < 3; i++) J(A_SWAY, P_CONE, i == 1 ? CL_DEEP : CL_MAIN, 0, 0.6f, 0, 0, 0.75f + i * 0.75f, 0, 1.9f - i * 0.45f, 1.3f, 1.9f - i * 0.45f, 0, i * 22.5f);
}
static void palm() {
  for (int i = 0; i < 5; i++) S(P_CYL, i & 1 ? CL_WOOD : CL_BROWN, i * i * 0.025f, 0.3f + i * 0.58f, 0, 0.28f - i * 0.02f, 0.62f, 0.28f - i * 0.02f, 0, 0, -i * 4.0f);
  float tx = 0.4f, ty = 3.05f;
  for (int i = 0; i < 6; i++) J(A_SWAY, P_BOX, CL_MAIN, tx, ty, 0, 0, 0, 0.7f, 0.42f, 0.05f, 1.4f, 28, i * 60.0f, 0, i * 0.3f);
  for (int i = 0; i < 3; i++) S(P_BALL, CL_BROWN, tx + 0.15f * (i - 1), ty - 0.18f, 0.12f * (i & 1), 0.2f, 0.2f, 0.2f);
}
static void flower() {
  J(A_SWAY, P_CYL, CL_GREEN, 0, 0, 0, 0, 0.32f, 0, 0.05f, 0.64f, 0.05f);
  J(A_SWAY, P_BOX, CL_GREEN, 0, 0, 0, 0.1f, 0.25f, 0, 0.2f, 0.02f, 0.1f, 0, 30, 20);
  J(A_SWAY, P_BALL, CL_YELLOW, 0, 0, 0, 0, 0.7f, 0.04f, 0.14f, 0.14f, 0.1f, 70);
  for (int i = 0; i < 6; i++) {
    float a = i * 1.0472f;
    J(A_SWAY, P_BALL, CL_MAIN, 0, 0, 0, cosf(a) * 0.13f, 0.7f + sinf(a) * 0.13f, 0.02f, 0.16f, 0.16f, 0.05f);
  }
}
static void bush() {
  S(P_ROCK, CL_MAIN, 0, 0.45f, 0, 1.2f, 0.9f, 1.1f);
  S(P_ROCK, CL_DEEP, 0.45f, 0.35f, 0.2f, 0.8f, 0.7f, 0.8f, 0, 50);
  S(P_ROCK, CL_LITE, -0.4f, 0.35f, -0.15f, 0.75f, 0.65f, 0.75f, 0, 100);
}
static void rock() {
  S(P_ROCK, CL_MAIN, 0, 0.45f, 0, 1.3f, 0.95f, 1.1f, 0, 20);
  S(P_ROCK, CL_DEEP, 0.7f, 0.22f, 0.3f, 0.6f, 0.45f, 0.55f, 30, 70);
}
static void mountain() {
  S(P_CONE, CL_MAIN, 0, 3.0f, 0, 8, 6, 8);
  S(P_CONE, CL_WHITE, 0, 5.1f, 0, 2.5f, 1.8f, 2.5f);
  S(P_CONE, CL_DEEP, 3.2f, 1.8f, 1.0f, 5, 3.6f, 5, 0, 20);
}
static void cloud() {
  S(P_BALL, CL_MAIN, 0, 0, 0, 2.0f, 1.2f, 1.4f, 0, 0, 0, R_SOFT);
  S(P_BALL, CL_MAIN, 0.9f, -0.15f, 0.1f, 1.4f, 0.9f, 1.1f, 0, 0, 0, R_SOFT);
  S(P_BALL, CL_MAIN, -0.9f, -0.2f, -0.1f, 1.3f, 0.85f, 1.0f, 0, 0, 0, R_SOFT);
  S(P_BALL, CL_MAIN, 0.3f, 0.45f, -0.1f, 1.2f, 0.9f, 1.0f, 0, 0, 0, R_SOFT);
}
static void sun() {
  S(P_BALL, CL_GLOWM, 0, 0, 0, 2, 2, 2);
  for (int i = 0; i < 8; i++) J(A_SPINZ, P_CONE, CL_GLOWM, 0, 0, 0, 0, 1.5f, -0.1f, 0.3f, 0.6f, 0.15f, 0, 0, i * 45.0f, 0.3f);
  S(P_BOX, CL_DEEP, -0.32f, 0.2f, 0.98f, 0.14f, 0.2f, 0.05f, 0, 0, 0, R_EMIT);
  S(P_BOX, CL_DEEP, 0.32f, 0.2f, 0.98f, 0.14f, 0.2f, 0.05f, 0, 0, 0, R_EMIT);
  S(P_ARC, CL_DEEP, 0, -0.1f, 0.95f, 0.36f, 0.3f, 0.1f, 0, 0, 180, R_EMIT);
}
static void moon() {
  S(P_BALL, CL_MAIN, 0, 0, 0, 2, 2, 2, 0, 0, 0, R_SOFT);
  S(P_DISC, CL_DEEP, 0.4f, 0.3f, 0.88f, 0.5f, 0.06f, 0.5f, 70);
  S(P_DISC, CL_DEEP, -0.45f, -0.25f, 0.85f, 0.35f, 0.06f, 0.35f, 110, 0, 20);
  S(P_DISC, CL_DEEP, 0.05f, -0.55f, 0.8f, 0.28f, 0.06f, 0.28f, 125);
}
static void planet() {
  S(P_BALL, CL_MAIN, 0, 0, 0, 2, 2, 2);
  S(P_BALL, CL_DEEP, 0, 0.3f, 0, 2.03f, 0.35f, 2.03f);
  J(A_SPINY, P_RING, CL_SEC, 0, 0, 0, 0, 0, 0, 4.2f, 1, 4.2f, 18, 0, 8, 0, R_NOCULL);
}
static void star() { J(A_SPINY, P_STAR, CL_GLOWM, 0, 0, 0, 0, 0, 0, 1.2f, 1.2f, 1.2f); }
static void fire() {
  for (int i = 0; i < 3; i++) S(P_CYL, CL_WOOD, 0, 0.12f, 0, 0.18f, 1.1f, 0.18f, 90, i * 60.0f);
  J(A_FLAME, P_CONE, CL_FIRE, 0, 0.15f, 0, 0, 0.4f, 0, 0.6f, 0.8f, 0.6f, 0, 0, 0, 0);
  J(A_FLAME, P_CONE, CL_YELLOW, 0, 0.15f, 0, 0.12f, 0.3f, 0.05f, 0.3f, 0.5f, 0.3f, 0, 0, 0, 1.7f, R_EMIT);
  J(A_FLAME, P_CONE, CL_FIRE, 0, 0.15f, 0, -0.15f, 0.3f, -0.08f, 0.35f, 0.55f, 0.35f, 0, 0, 0, 3.1f);
}
static void volcano() {
  S(P_CONE, CL_MAIN, 0, 2.2f, 0, 7, 4.4f, 7);
  S(P_CYL, CL_MAIN, 0, 4.3f, 0, 2.1f, 0.2f, 2.1f);
  S(P_DISC, CL_FIRE, 0, 4.42f, 0, 1.7f, 0.05f, 1.7f);
  for (int i = 0; i < 3; i++) S(P_BOX, CL_FIRE, 0, 3.4f - i * 0.1f, 0, 0.25f, 2.0f, 0.08f, -35 + i * 5.0f, i * 120.0f + 20, 0);
}
static void mushroom() {
  S(P_CYL, CL_WHITE, 0, 0.35f, 0, 0.32f, 0.7f, 0.32f);
  S(P_DOME, CL_MAIN, 0, 0.85f, 0, 1.1f, 0.6f, 1.1f);
  for (int i = 0; i < 5; i++) S(P_BALL, CL_WHITE, sinf(i * 1.256f) * 0.3f, 1.05f, cosf(i * 1.256f) * 0.3f, 0.16f, 0.08f, 0.16f);
  S(P_BALL, CL_WHITE, 0, 1.15f, 0, 0.16f, 0.08f, 0.16f);
}
static void cactus() {
  S(P_CYL, CL_MAIN, 0, 1.0f, 0, 0.5f, 2.0f, 0.5f);
  S(P_DOME, CL_MAIN, 0, 2.1f, 0, 0.5f, 0.3f, 0.5f);
  S(P_CYL, CL_MAIN, 0.42f, 1.1f, 0, 0.5f, 0.26f, 0.26f, 0, 0, 90);
  S(P_CYL, CL_MAIN, 0.6f, 1.45f, 0, 0.3f, 0.8f, 0.3f);
  S(P_CYL, CL_MAIN, -0.38f, 0.8f, 0, 0.4f, 0.24f, 0.24f, 0, 0, 90);
  S(P_CYL, CL_MAIN, -0.56f, 1.05f, 0, 0.26f, 0.6f, 0.26f);
  S(P_BALL, CL_PINK, 0, 2.28f, 0, 0.2f, 0.14f, 0.2f);
}
static void grass() {
  for (int i = 0; i < 7; i++) J(A_SWAY, P_CONE, i & 1 ? CL_MAIN : CL_DEEP, 0, 0, 0, sinf(i * 2.4f) * 0.3f, 0.25f, cosf(i * 2.4f) * 0.3f, 0.1f, 0.5f + (i % 3) * 0.12f, 0.06f, 0, 0, sinf(i * 1.7f) * 15);
}
// the rainbow is drawn in code (scene.cpp): one band so the list knows its size
static void rainbow() { S(P_BOX, CL_RED, 0, 0, 0, 0.01f, 0.01f, 0.01f); }

// ------------------------------------------------------------------ buildings
static void house() {
  S(P_BOX, CL_MAIN, 0, 1.0f, 0, 3, 2, 2.6f);
  S(P_WEDGE, CL_SEC, 0, 2.6f, 0, 3.4f, 1.2f, 3.0f);
  S(P_BOX, CL_DARK, 0.9f, 2.9f, -0.5f, 0.35f, 0.9f, 0.35f);
  S(P_BOX, CL_WOOD, 0, 0.55f, 1.31f, 0.6f, 1.1f, 0.05f);
  for (int k = -1; k <= 1; k += 2) {
    S(P_BOX, CL_WIN, k * 0.95f, 1.2f, 1.31f, 0.55f, 0.5f, 0.04f);
    S(P_BOX, CL_WIN, 1.51f * k, 1.2f, 0, 0.04f, 0.5f, 0.6f);
  }
}
static void tower() {
  S(P_BOX, CL_MAIN, 0, 4, 0, 2, 8, 2);
  for (int i = 0; i < 7; i++) S(P_BOX, CL_WIN, 0, 0.9f + i * 1.0f, 0, 2.04f, 0.4f, 2.04f);
  S(P_BOX, CL_DEEP, 0, 8.2f, 0, 1.4f, 0.4f, 1.4f);
  S(P_CYL, CL_GRAY, 0.3f, 8.9f, 0.2f, 0.06f, 1.2f, 0.06f);
  J(A_BLINK, P_BALL, CL_FIRE, 0.3f, 9.5f, 0.2f, 0, 0, 0, 0.15f, 0.15f, 0.15f);
}
static void castle() {
  S(P_BOX, CL_MAIN, 0, 1.2f, 0, 3.6f, 2.4f, 3.6f);
  S(P_BOX, CL_MAIN, 0, 2.2f, 0, 1.6f, 4.4f, 1.6f);
  S(P_PYR, CL_SEC, 0, 5.0f, 0, 1.3f, 1.2f, 1.3f);
  S(P_BOX, CL_DARK, 0, 0.6f, 1.81f, 0.8f, 1.2f, 0.04f);
  for (int x = -1; x <= 1; x += 2) for (int z = -1; z <= 1; z += 2) {
    S(P_CYL, CL_LITE, x * 1.8f, 1.6f, z * 1.8f, 0.9f, 3.2f, 0.9f);
    S(P_CONE, CL_SEC, x * 1.8f, 3.65f, z * 1.8f, 1.1f, 0.9f, 1.1f);
  }
  for (int i = -2; i <= 2; i++) S(P_BOX, CL_MAIN, i * 0.62f, 2.55f, 1.75f, 0.3f, 0.3f, 0.12f);
  S(P_CYL, CL_GRAY, 0, 6.0f, 0, 0.04f, 0.9f, 0.04f);
  J(A_FLAG, P_BOX, CL_RED, 0, 6.3f, 0, 0.25f, 0, 0, 0.5f, 0.3f, 0.02f);
  S(P_BOX, CL_WIN, 0, 3.4f, 0.81f, 0.3f, 0.5f, 0.04f);
}
static void tent() {
  S(P_WEDGE, CL_MAIN, 0, 0.8f, 0, 2.4f, 1.6f, 2.0f, 0, 90);
  S(P_PYR, CL_DARK, 0, 0.45f, 1.18f, 0.5f, 0.9f, 0.05f);
}
static void lighthouse() {
  for (int i = 0; i < 4; i++) S(P_CYL, i & 1 ? CL_WHITE : CL_MAIN, 0, 0.6f + i * 1.2f, 0, 1.7f - i * 0.18f, 1.2f, 1.7f - i * 0.18f);
  S(P_CYL, CL_DARK, 0, 4.85f, 0, 1.5f, 0.1f, 1.5f);
  S(P_CYL, CL_LIGHT, 0, 5.3f, 0, 0.8f, 0.8f, 0.8f);
  S(P_CONE, CL_MAIN, 0, 6.0f, 0, 1.1f, 0.6f, 1.1f);
  J(A_SPINY, P_CONE, CL_LIGHT, 0, 5.3f, 0, 0, 0, 2.2f, 1.4f, 4.0f, 0.5f, -90, 0, 0, 0, R_ALPHA | R_NOCULL);
  S(P_BOX, CL_WOOD, 0, 0.45f, 0.86f, 0.4f, 0.9f, 0.05f);
}
static void windmill() {
  S(P_CYL, CL_MAIN, 0, 1.6f, 0, 1.6f, 3.2f, 1.6f);
  S(P_DOME, CL_SEC, 0, 3.4f, 0, 1.7f, 0.8f, 1.7f);
  S(P_CYL, CL_DARK, 0, 3.3f, 0.9f, 0.2f, 0.4f, 0.2f, 90);
  for (int i = 0; i < 4; i++) J(A_SPINZ, P_BOX, CL_WHITE, 0, 3.3f, 1.05f, 0, 1.2f, 0, 0.45f, 2.3f, 0.04f, 0, 0, i * 90.0f, 0.8f);
  S(P_BOX, CL_WOOD, 0, 0.5f, 0.8f, 0.5f, 1.0f, 0.05f);
}
static void factory() {
  S(P_BOX, CL_MAIN, 0, 1.0f, 0, 4, 2, 2.6f);
  for (int i = 0; i < 3; i++) S(P_WEDGE, CL_SEC, -1.33f + i * 1.33f, 2.35f, 0, 1.3f, 0.7f, 2.6f, 0, 90);
  S(P_CYL, CL_DEEP, 1.4f, 2.6f, -0.8f, 0.5f, 3.2f, 0.5f);
  S(P_CYL, CL_DEEP, 0.6f, 2.3f, -0.8f, 0.4f, 2.6f, 0.4f);
  for (int i = 0; i < 4; i++) S(P_BOX, CL_WIN, -1.5f + i * 1.0f, 1.2f, 1.31f, 0.5f, 0.5f, 0.04f);
}
static void bridge() {
  S(P_BOX, CL_MAIN, 0, 1.4f, 0, 1.6f, 0.2f, 7);
  for (int k = -1; k <= 1; k += 2) {
    S(P_ARC, CL_SEC, k * 0.75f, 0, 0, 3.5f, 2.6f, 0.4f, 0, 90);
    S(P_BOX, CL_SEC, k * 0.75f, 1.8f, 0, 0.08f, 0.6f, 7);
  }
}

// ------------------------------------------------------------------ things that go
static void car() {
  S(P_BOX, CL_MAIN, 0, 0.55f, 0, 1.6f, 0.55f, 3.4f);
  S(P_BOX, CL_WIN, 0, 1.05f, -0.25f, 1.36f, 0.5f, 1.7f);
  S(P_BOX, CL_MAIN, 0, 1.33f, -0.25f, 1.42f, 0.08f, 1.74f);
  wheels(CL_BLACK, 0.78f, 0.32f, 1.1f, -1.1f, 0.64f, 0.3f);
  for (int k = -1; k <= 1; k += 2) { S(P_BOX, CL_LIGHT, k * 0.55f, 0.62f, 1.71f, 0.3f, 0.14f, 0.03f); S(P_BOX, CL_RED, k * 0.6f, 0.65f, -1.71f, 0.25f, 0.12f, 0.03f, 0, 0, 0, R_EMIT); }
}
static void bus() {
  S(P_BOX, CL_MAIN, 0, 1.25f, 0, 2.1f, 1.8f, 5.6f);
  S(P_BOX, CL_WIN, 0, 1.6f, 0.1f, 2.14f, 0.6f, 4.8f);
  S(P_BOX, CL_WIN, 0, 1.4f, 2.81f, 1.8f, 0.9f, 0.04f);
  wheels(CL_BLACK, 0.95f, 0.38f, 1.8f, -1.8f, 0.76f, 0.3f);
  for (int k = -1; k <= 1; k += 2) S(P_BOX, CL_LIGHT, k * 0.75f, 0.7f, 2.81f, 0.3f, 0.16f, 0.03f);
}
static void truck() {
  S(P_BOX, CL_MAIN, 0, 1.0f, 1.9f, 2, 1.4f, 1.4f);
  S(P_BOX, CL_WIN, 0, 1.35f, 2.61f, 1.7f, 0.55f, 0.04f);
  S(P_BOX, CL_SEC, 0, 1.45f, -0.9f, 2.1f, 2.1f, 4);
  S(P_BOX, CL_DARK, 0, 0.42f, 0, 1.8f, 0.2f, 6);
  wheels(CL_BLACK, 0.92f, 0.42f, 1.9f, -1.6f, 0.84f, 0.34f);
  for (int k = -1; k <= 1; k += 2) J(A_SPINX, P_CYL, CL_BLACK, k * 0.92f, 0.42f, -0.4f, 0, 0, 0, 0.84f, 0.34f, 0.84f, 0, 0, 90);
}
static void bike() {
  for (int z = -1; z <= 1; z += 2) J(A_SPINX, P_TORUS, CL_DARK, 0, 0.42f, z * 0.55f, 0, 0, 0, 0.66f, 0.66f, 0.66f, 0, 0, 90);
  S(P_BOX, CL_MAIN, 0, 0.62f, 0, 0.06f, 0.06f, 0.8f, 20);
  S(P_BOX, CL_MAIN, 0, 0.58f, -0.3f, 0.06f, 0.45f, 0.06f, -15);
  S(P_BOX, CL_MAIN, 0, 0.62f, 0.45f, 0.06f, 0.5f, 0.06f, 15);
  S(P_BOX, CL_DARK, 0, 0.86f, -0.34f, 0.16f, 0.06f, 0.26f);
  S(P_BOX, CL_DARK, 0, 0.9f, 0.52f, 0.5f, 0.05f, 0.05f);
}
static void train() {
  S(P_BOX, CL_MAIN, 0, 0.9f, 0.4f, 1.5f, 1.0f, 3.6f);
  S(P_CYL, CL_MAIN, 0, 1.6f, 0.9f, 1.2f, 2.2f, 1.2f, 90);
  S(P_BOX, CL_SEC, 0, 1.85f, -0.85f, 1.5f, 1.3f, 1.1f);
  S(P_BOX, CL_WIN, 0, 2.0f, -0.85f, 1.54f, 0.45f, 0.7f);
  S(P_CYL, CL_DARK, 0, 2.6f, 1.5f, 0.35f, 0.7f, 0.35f);
  S(P_CONE, CL_RED, 0, 1.0f, 2.35f, 1.1f, 0.6f, 0.4f, 70);
  S(P_DISC, CL_LIGHT, 0, 1.6f, 2.0f, 0.4f, 0.05f, 0.4f, 90);
  for (int i = 0; i < 3; i++) for (int k = -1; k <= 1; k += 2) J(A_SPINX, P_CYL, CL_DARK, k * 0.7f, 0.38f, 1.6f - i * 1.1f, 0, 0, 0, 0.7f, 0.15f, 0.7f, 0, 0, 90);
  S(P_BOX, CL_SEC, 0, 1.0f, -3.4f, 1.5f, 1.2f, 3.2f);
  S(P_BOX, CL_WIN, 0, 1.2f, -3.4f, 1.54f, 0.4f, 2.6f);
  for (int i = 0; i < 2; i++) for (int k = -1; k <= 1; k += 2) J(A_SPINX, P_CYL, CL_DARK, k * 0.7f, 0.32f, -2.5f - i * 1.8f, 0, 0, 0, 0.6f, 0.15f, 0.6f, 0, 0, 90);
}
static void plane() {
  S(P_CYL, CL_MAIN, 0, 0, 0, 0.8f, 4.4f, 0.8f, 90);
  S(P_CONE, CL_MAIN, 0, 0, 2.6f, 0.8f, 0.8f, 0.8f, 90);
  S(P_CONE, CL_MAIN, 0, 0.1f, -2.6f, 0.8f, 0.8f, 0.6f, -90);
  S(P_BOX, CL_SEC, 0, -0.1f, 0.2f, 5.6f, 0.1f, 1.1f);
  S(P_BOX, CL_SEC, 0, 0.1f, -2.4f, 2.0f, 0.08f, 0.6f);
  S(P_BOX, CL_SEC, 0, 0.55f, -2.5f, 0.08f, 1.0f, 0.7f, -20);
  S(P_BOX, CL_WIN, 0, 0.2f, 0, 0.82f, 0.18f, 3.2f);
  J(A_SPINZ, P_BOX, CL_DARK, 0, 0, 3.02f, 0, 0, 0, 1.6f, 0.12f, 0.04f, 0, 0, 0, 13);
}
static void boat(bool ship) {
  float k = ship ? 2.2f : 1;
  S(P_WEDGE, CL_MAIN, 0, 0.3f * k, 0, 1.4f * k, 0.8f * k, 3.2f * k, 0, 90, 180);
  S(P_BOX, CL_WOOD, 0, 0.55f * k, 0, 1.3f * k, 0.1f, 2.9f * k);
  if (ship) {
    S(P_BOX, CL_WHITE, 0, 1.6f, -0.4f, 2.2f, 1.6f, 3.0f);
    S(P_BOX, CL_WIN, 0, 1.9f, -0.4f, 2.24f, 0.4f, 2.6f);
    S(P_BOX, CL_WHITE, 0, 2.7f, -0.8f, 1.6f, 0.7f, 1.6f);
    S(P_CYL, CL_SEC, 0, 3.4f, -1.0f, 0.7f, 1.6f, 0.7f);
  } else {
    S(P_CYL, CL_WOOD, 0, 1.8f, 0.2f, 0.08f, 2.6f, 0.08f);
    J(A_FLAG, P_WEDGE, CL_SEC, 0, 1.9f, 0.2f, 0, 0, -0.55f, 0.04f, 2.0f, 1.1f, 0, 90);
  }
}
static void rocket() {
  S(P_CYL, CL_MAIN, 0, 2.0f, 0, 0.9f, 3, 0.9f);
  S(P_CONE, CL_SEC, 0, 4.1f, 0, 0.9f, 1.2f, 0.9f);
  S(P_BALL, CL_GLASS, 0, 2.7f, 0.42f, 0.38f, 0.38f, 0.1f);
  for (int i = 0; i < 4; i++) S(P_WEDGE, CL_SEC, sinf(i * 1.5708f) * 0.55f, 0.85f, cosf(i * 1.5708f) * 0.55f, 0.06f, 1.0f, 0.5f, 0, i * 90.0f + 90);
  S(P_CONE, CL_DARK, 0, 0.35f, 0, 0.7f, 0.4f, 0.7f, 180);
  J(A_FLAME, P_CONE, CL_FIRE, 0, 0.1f, 0, 0, -0.5f, 0, 0.55f, 1.0f, 0.55f, 180);
  J(A_FLAME, P_CONE, CL_YELLOW, 0, 0.1f, 0, 0, -0.3f, 0, 0.3f, 0.6f, 0.3f, 180, 0, 0, 1.3f, R_EMIT);
}
static void helicopter() {
  S(P_BALL, CL_MAIN, 0, 1.0f, 0.2f, 1.4f, 1.3f, 2.0f);
  S(P_BALL, CL_WIN, 0, 1.1f, 0.75f, 1.0f, 0.8f, 0.8f);
  S(P_CYL, CL_MAIN, 0, 1.15f, -1.4f, 0.24f, 2.2f, 0.24f, 90);
  S(P_BOX, CL_MAIN, 0, 1.5f, -2.4f, 0.06f, 0.7f, 0.4f);
  J(A_SPINX, P_BOX, CL_DARK, 0.1f, 1.5f, -2.45f, 0, 0, 0, 0.03f, 0.8f, 0.1f, 0, 0, 0, 11);
  S(P_CYL, CL_DARK, 0, 1.75f, 0.2f, 0.12f, 0.3f, 0.12f);
  J(A_SPINY, P_BOX, CL_DARK, 0, 1.92f, 0.2f, 0, 0, 0, 4.2f, 0.04f, 0.22f, 0, 0, 0, 7);
  J(A_SPINY, P_BOX, CL_DARK, 0, 1.92f, 0.2f, 0, 0, 0, 0.22f, 0.04f, 4.2f, 0, 0, 0, 7);
  for (int k = -1; k <= 1; k += 2) S(P_BOX, CL_DARK, k * 0.55f, 0.15f, 0.2f, 0.08f, 0.08f, 2.0f);
}
static void ufo() {
  S(P_DISC, CL_SILVER, 0, 0, 0, 3.2f, 0.3f, 3.2f);
  S(P_DOME, CL_SILVER, 0, -0.2f, 0, 2.4f, 0.4f, 2.4f, 180);
  S(P_DOME, CL_GLASS, 0, 0.45f, 0, 1.4f, 0.9f, 1.4f, 0, 0, 0, R_ALPHA);
  S(P_BALL, CL_GREEN, 0, 0.35f, 0, 0.45f, 0.5f, 0.45f);
  for (int i = 0; i < 8; i++) J(A_BLINK, P_BALL, CL_GLOWM, 0, 0, 0, sinf(i * 0.785f) * 1.45f, 0, cosf(i * 0.785f) * 1.45f, 0.2f, 0.2f, 0.2f, 0, 0, 0, i * 0.785f);
  J(A_SPINY, P_CONE, CL_GLOWM, 0, -0.3f, 0, 0, -1.8f, 0, 1.8f, 3.4f, 1.8f, 0, 0, 0, 0, R_ALPHA | R_NOCULL);
}

// ------------------------------------------------------------------ things
static void ball() {
  J(A_SPINX, P_BALL, CL_MAIN, 0, 0.5f, 0, 0, 0, 0, 1, 1, 1);
  J(A_SPINX, P_TORUS, CL_SEC, 0, 0.5f, 0, 0, 0, 0, 0.94f, 0.94f, 0.94f, 0, 0, 90);
}
static void book() {
  for (int k = -1; k <= 1; k += 2) {
    S(P_BOX, CL_MAIN, k * 0.52f, 0.12f, 0, 1.0f, 0.05f, 1.4f, 0, 0, -k * 10.0f);
    S(P_BOX, CL_WHITE, k * 0.49f, 0.18f, 0, 0.92f, 0.08f, 1.3f, 0, 0, -k * 10.0f);
    for (int i = 0; i < 4; i++) S(P_BOX, CL_DARK, k * 0.5f, 0.225f + i * 0.0f, 0.4f - i * 0.25f, 0.6f, 0.005f, 0.04f, 0, 0, -k * 10.0f);
  }
  J(A_PAGE, P_BOX, CL_WHITE, 0, 0.22f, 0, 0.45f, 0, 0, 0.9f, 0.01f, 1.28f);
}
static void phone() {
  S(P_BOX, CL_DARK, 0, 0.8f, 0, 0.75f, 1.5f, 0.1f, -12);
  S(P_BOX, CL_SCREEN, 0, 0.8f, 0.04f, 0.66f, 1.3f, 0.03f, -12);
  for (int i = 0; i < 3; i++) S(P_BOX, CL_WHITE, -0.08f + i * 0.0f, 1.12f - i * 0.25f, 0.08f - i * 0.05f, 0.4f - i * 0.1f, 0.12f, 0.02f, -12, 0, 0, R_EMIT);
}
static void computer() {
  S(P_BOX, CL_DARK, 0, 0.05f, 0.2f, 1.8f, 0.08f, 1.2f);
  S(P_BOX, CL_GRAY, 0, 0.1f, 0.25f, 1.5f, 0.02f, 0.7f);
  S(P_BOX, CL_DARK, 0, 0.62f, -0.45f, 1.8f, 1.1f, 0.06f, -12);
  S(P_BOX, CL_SCREEN, 0, 0.64f, -0.41f, 1.62f, 0.94f, 0.02f, -12);
  for (int i = 0; i < 4; i++) S(P_BOX, CL_WHITE, -0.3f + (i & 1) * 0.2f, 0.9f - i * 0.17f, -0.37f + i * 0.035f, 0.8f - (i & 1) * 0.3f, 0.06f, 0.01f, -12, 0, 0, R_EMIT);
}
static void tv() {
  S(P_BOX, CL_DARK, 0, 1.3f, 0, 2.6f, 1.5f, 0.16f);
  S(P_BOX, CL_SCREEN, 0, 1.3f, 0.09f, 2.4f, 1.3f, 0.02f);
  S(P_BOX, CL_DARK, 0, 0.3f, 0, 0.2f, 0.6f, 0.1f);
  S(P_BOX, CL_DARK, 0, 0.03f, 0, 1.0f, 0.06f, 0.5f);
}
static void chair() {
  S(P_BOX, CL_MAIN, 0, 0.5f, 0, 0.9f, 0.1f, 0.9f);
  S(P_BOX, CL_MAIN, 0, 1.0f, -0.42f, 0.9f, 0.9f, 0.08f);
  for (int x = -1; x <= 1; x += 2) for (int z = -1; z <= 1; z += 2) S(P_BOX, CL_SEC, x * 0.38f, 0.25f, z * 0.38f, 0.08f, 0.5f, 0.08f);
}
static void tableM() {
  S(P_BOX, CL_MAIN, 0, 0.78f, 0, 2.2f, 0.1f, 1.3f);
  for (int x = -1; x <= 1; x += 2) for (int z = -1; z <= 1; z += 2) S(P_BOX, CL_SEC, x * 0.98f, 0.38f, z * 0.54f, 0.1f, 0.76f, 0.1f);
}
static void bed() {
  S(P_BOX, CL_WOOD, 0, 0.25f, 0, 1.7f, 0.4f, 2.4f);
  S(P_BOX, CL_WHITE, 0, 0.55f, 0, 1.6f, 0.25f, 2.3f);
  S(P_BOX, CL_MAIN, 0, 0.62f, -0.25f, 1.66f, 0.2f, 1.6f);
  S(P_BOX, CL_WHITE, 0, 0.75f, 0.85f, 1.0f, 0.2f, 0.45f);
  S(P_BOX, CL_WOOD, 0, 0.7f, 1.2f, 1.7f, 1.0f, 0.1f);
}
static void cup() {
  S(P_CYL, CL_MAIN, 0, 0.3f, 0, 0.55f, 0.6f, 0.55f);
  S(P_DISC, CL_BROWN, 0, 0.58f, 0, 0.46f, 0.02f, 0.46f);
  S(P_TORUS, CL_MAIN, 0.32f, 0.32f, 0, 0.3f, 0.3f, 0.3f, 90, 90);
  S(P_DISC, CL_WHITE, 0, 0.01f, 0, 0.9f, 0.02f, 0.9f);
}
static void bottle() {
  S(P_CYL, CL_MAIN, 0, 0.45f, 0, 0.45f, 0.9f, 0.45f);
  S(P_CONE, CL_MAIN, 0, 1.0f, 0, 0.45f, 0.22f, 0.45f);
  S(P_CYL, CL_MAIN, 0, 1.25f, 0, 0.16f, 0.4f, 0.16f);
  S(P_CYL, CL_SEC, 0, 1.48f, 0, 0.18f, 0.08f, 0.18f);
  S(P_CYL, CL_WHITE, 0, 0.45f, 0, 0.47f, 0.35f, 0.47f);
}
static void gift() {
  S(P_BOX, CL_MAIN, 0, 0.45f, 0, 0.9f, 0.9f, 0.9f);
  S(P_BOX, CL_SEC, 0, 0.455f, 0, 0.2f, 0.92f, 0.92f);
  S(P_BOX, CL_SEC, 0, 0.455f, 0, 0.92f, 0.92f, 0.2f);
  for (int k = -1; k <= 1; k += 2) S(P_TORUS, CL_SEC, k * 0.14f, 1.0f, 0, 0.28f, 0.28f, 0.28f, 90, 0, k * 30.0f);
}
static void clock() {
  S(P_DISC, CL_MAIN, 0, 1.1f, 0, 1.6f, 0.2f, 1.6f, 90);
  S(P_DISC, CL_WHITE, 0, 1.1f, 0.06f, 1.4f, 0.12f, 1.4f, 90);
  J(A_HANDH, P_BOX, CL_BLACK, 0, 1.1f, 0.14f, 0, 0.22f, 0, 0.06f, 0.44f, 0.02f);
  J(A_HANDM, P_BOX, CL_BLACK, 0, 1.1f, 0.15f, 0, 0.3f, 0, 0.04f, 0.6f, 0.02f);
  for (int i = 0; i < 4; i++) S(P_BOX, CL_DARK, sinf(i * 1.5708f) * 0.58f, 1.1f + cosf(i * 1.5708f) * 0.58f, 0.13f, 0.06f, 0.06f, 0.02f);
  S(P_BOX, CL_SEC, 0, 0.15f, 0, 0.5f, 0.3f, 0.3f);
}
static void bulb() {
  J(A_BEAT, P_BALL, CL_LIGHT, 0, 0.9f, 0, 0, 0, 0, 0.9f, 1.0f, 0.9f);
  S(P_CYL, CL_GRAY, 0, 0.3f, 0, 0.4f, 0.35f, 0.4f);
  S(P_CYL, CL_DARK, 0, 0.1f, 0, 0.22f, 0.1f, 0.22f);
  S(P_TORUS, CL_GRAY, 0, 0.32f, 0, 0.42f, 0.4f, 0.42f);
}
static void guitar() {
  S(P_BALL, CL_MAIN, 0, 0.5f, 0, 0.9f, 0.85f, 0.3f);
  S(P_BALL, CL_MAIN, 0, 1.05f, 0, 0.7f, 0.65f, 0.28f);
  S(P_DISC, CL_DARK, 0, 0.82f, 0.13f, 0.24f, 0.02f, 0.24f, 90);
  S(P_BOX, CL_WOOD, 0, 1.7f, 0.06f, 0.14f, 1.4f, 0.08f);
  S(P_BOX, CL_DARK, 0, 2.45f, 0.06f, 0.2f, 0.3f, 0.08f);
  S(P_BOX, CL_SEC, 0, 0.45f, 0.14f, 0.3f, 0.06f, 0.03f);
}
static void drum() {
  S(P_CYL, CL_MAIN, 0, 0.45f, 0, 1.2f, 0.9f, 1.2f);
  S(P_CYL, CL_WHITE, 0, 0.92f, 0, 1.22f, 0.04f, 1.22f);
  S(P_TORUS, CL_GOLD, 0, 0.92f, 0, 0.6f, 0.4f, 0.6f);
  for (int k = -1; k <= 1; k += 2) J(A_ARML + (k > 0), P_CYL, CL_WOOD, k * 0.3f, 1.3f, 0.2f, 0, 0, 0.1f, 0.04f, 0.8f, 0.04f, 40, 0, k * 15.0f);
}
static void piano() {
  S(P_BOX, CL_MAIN, 0, 0.9f, -0.2f, 2.4f, 1.8f, 0.7f);
  S(P_BOX, CL_WHITE, 0, 0.82f, 0.25f, 2.2f, 0.08f, 0.3f);
  for (int i = 0; i < 7; i++) if (i != 2 && i != 6) S(P_BOX, CL_BLACK, -0.9f + i * 0.3f, 0.88f, 0.2f, 0.12f, 0.06f, 0.18f);
  S(P_BOX, CL_MAIN, 0, 0.3f, 0.25f, 0.2f, 0.6f, 0.2f);
}
static void umbrella() {
  S(P_CONE, CL_MAIN, 0, 2.0f, 0, 2.2f, 0.5f, 2.2f);
  S(P_CYL, CL_DARK, 0, 1.1f, 0, 0.05f, 2.0f, 0.05f);
  S(P_ARC, CL_DARK, 0.1f, 0.12f, 0, 0.12f, 0.1f, 0.3f, 0, 0, 180);
  S(P_BALL, CL_SEC, 0, 2.28f, 0, 0.1f, 0.1f, 0.1f);
}
static void key() {
  S(P_TORUS, CL_GOLD, 0, 0, 0.45f, 0.5f, 0.5f, 0.5f, 90);
  S(P_BOX, CL_GOLD, 0, 0, -0.3f, 0.12f, 0.12f, 1.1f);
  S(P_BOX, CL_GOLD, 0, -0.12f, -0.65f, 0.1f, 0.24f, 0.1f);
  S(P_BOX, CL_GOLD, 0, -0.1f, -0.45f, 0.1f, 0.18f, 0.1f);
}
static void coins() {
  for (int i = 0; i < 4; i++) S(P_DISC, CL_GOLD, 0.02f * (i & 1), 0.07f + i * 0.13f, -0.03f * (i & 2), 0.8f, 0.12f, 0.8f);
  J(A_SPINY, P_DISC, CL_GOLD, 0, 1.2f, 0.6f, 0, 0, 0, 0.8f, 0.1f, 0.8f, 90, 0, 0, 2.5f);
  S(P_DISC, CL_GOLD, 0.8f, 0.07f, 0.3f, 0.8f, 0.12f, 0.8f);
  S(P_DISC, CL_GOLD, 0.8f, 0.2f, 0.32f, 0.8f, 0.12f, 0.8f);
}
static void crown() {
  S(P_TUBE, CL_GOLD, 0, 0.3f, 0, 1.0f, 0.6f, 1.0f, 0, 0, 0, R_NOCULL);
  for (int i = 0; i < 5; i++) {
    float a = i * 1.2566f;
    S(P_CONE, CL_GOLD, sinf(a) * 0.46f, 0.78f, cosf(a) * 0.46f, 0.22f, 0.36f, 0.22f);
    S(P_BALL, i & 1 ? CL_BLUE : CL_RED, sinf(a) * 0.51f, 0.3f, cosf(a) * 0.51f, 0.14f, 0.14f, 0.08f, 0, a / D);
  }
}
static void heart() { J(A_BEAT, P_HEART, CL_MAIN, 0, 0, 0, 0, 0, 0, 1.2f, 1.2f, 1.2f); }
static void letter() {
  S(P_BOX, CL_MAIN, 0, 0.6f, 0, 1.6f, 1.0f, 0.06f);
  for (int k = -1; k <= 1; k += 2) S(P_BOX, CL_SEC, k * 0.39f, 0.8f, 0.035f, 0.92f, 0.04f, 0.01f, 0, 0, k * 32.0f);
  S(P_HEART, CL_RED, 0, 0.62f, 0.04f, 0.18f, 0.18f, 0.1f);
}
static void pencil() {
  S(P_CYL, CL_MAIN, 0, 0.0f, 0, 0.18f, 1.6f, 0.18f, 0, 0, 0);
  S(P_CONE, CL_WOOD, 0, 0.95f, 0, 0.18f, 0.3f, 0.18f);
  S(P_CONE, CL_DARK, 0, 1.06f, 0, 0.06f, 0.1f, 0.06f);
  S(P_CYL, CL_PINK, 0, -0.88f, 0, 0.18f, 0.18f, 0.18f);
  S(P_CYL, CL_SILVER, 0, -0.75f, 0, 0.19f, 0.1f, 0.19f);
}
static void camera() {
  S(P_BOX, CL_MAIN, 0, 0.5f, 0, 1.2f, 0.75f, 0.45f);
  S(P_CYL, CL_DARK, 0, 0.5f, 0.3f, 0.5f, 0.3f, 0.5f, 90);
  S(P_DISC, CL_GLASS, 0, 0.5f, 0.46f, 0.36f, 0.02f, 0.36f, 90);
  S(P_BOX, CL_DARK, -0.35f, 0.92f, 0, 0.3f, 0.12f, 0.3f);
  J(A_BLINK, P_BOX, CL_LIGHT, 0.38f, 0.75f, 0.23f, 0, 0, 0, 0.2f, 0.1f, 0.02f);
}
static void hat() {
  S(P_CYL, CL_MAIN, 0, 0.04f, 0, 1.3f, 0.06f, 1.3f);
  S(P_CYL, CL_MAIN, 0, 0.45f, 0, 0.8f, 0.8f, 0.8f);
  S(P_CYL, CL_SEC, 0, 0.15f, 0, 0.82f, 0.14f, 0.82f);
}
static void trophy() {
  S(P_CONE, CL_GOLD, 0, 1.15f, 0, 0.9f, 0.8f, 0.9f, 180);
  S(P_CYL, CL_GOLD, 0, 1.55f, 0, 0.9f, 0.04f, 0.9f);
  for (int k = -1; k <= 1; k += 2) S(P_ARC, CL_GOLD, k * 0.45f, 1.2f, 0, 0.24f, 0.28f, 0.5f, 0, 0, k * -90.0f);
  S(P_CYL, CL_GOLD, 0, 0.55f, 0, 0.14f, 0.5f, 0.14f);
  S(P_BOX, CL_DARK, 0, 0.15f, 0, 0.7f, 0.3f, 0.7f);
  S(P_STAR, CL_SEC, 0, 1.2f, 0.32f, 0.3f, 0.3f, 0.2f, 0, 0, 0, R_EMIT);
}
static void flag() {
  S(P_CYL, CL_GRAY, 0, 1.25f, 0, 0.08f, 2.5f, 0.08f);
  for (int i = 0; i < 4; i++) J(A_FLAG, P_BOX, i == 1 || i == 2 ? CL_SEC : CL_MAIN, 0.04f + i * 0.3f, 2.1f, 0, 0.15f, 0, 0, 0.3f, 0.7f, 0.03f, 0, 0, 0, i * 0.6f);
  S(P_BALL, CL_GOLD, 0, 2.55f, 0, 0.12f, 0.12f, 0.12f);
}
static void bag() {
  S(P_BOX, CL_MAIN, 0, 0.5f, 0, 1.0f, 1.0f, 0.5f);
  S(P_ARC, CL_SEC, 0, 1.0f, 0, 0.32f, 0.36f, 0.24f);
  S(P_BOX, CL_SEC, 0, 0.8f, 0.26f, 0.9f, 0.08f, 0.02f);
}
static void drink() {
  S(P_CYL, CL_GLASS, 0, 0.45f, 0, 0.6f, 0.9f, 0.6f, 0, 0, 0, R_ALPHA);
  S(P_CYL, CL_MAIN, 0, 0.32f, 0, 0.52f, 0.6f, 0.52f);
  S(P_CYL, CL_LITE, 0, 0.63f, 0, 0.53f, 0.04f, 0.53f);
  S(P_CYL, CL_WHITE, 0.12f, 0.75f, 0, 0.06f, 0.9f, 0.06f, 0, 0, -15);
}
// food
static void cake() {
  S(P_CYL, CL_MAIN, 0, 0.3f, 0, 1.6f, 0.6f, 1.6f);
  S(P_CYL, CL_SEC, 0, 0.64f, 0, 1.64f, 0.1f, 1.64f);
  S(P_CYL, CL_MAIN, 0, 0.9f, 0, 1.1f, 0.45f, 1.1f);
  S(P_CYL, CL_SEC, 0, 1.14f, 0, 1.14f, 0.06f, 1.14f);
  for (int i = 0; i < 4; i++) {
    float a = i * 1.5708f + 0.4f;
    S(P_CYL, i & 1 ? CL_BLUE : CL_PINK, sinf(a) * 0.3f, 1.35f, cosf(a) * 0.3f, 0.06f, 0.36f, 0.06f);
    J(A_FLAME, P_CONE, CL_FIRE, sinf(a) * 0.3f, 1.55f, cosf(a) * 0.3f, 0, 0.06f, 0, 0.09f, 0.16f, 0.09f, 0, 0, 0, i * 1.1f);
  }
}
static void pizza() {
  // tilted 40 degrees toward the viewer; the toppings sit on the tilted top
  const float T = 40, c = cosf(T * D), sn = sinf(T * D);
  S(P_DISC, CL_MAIN, 0, 0.1f, 0, 2.0f, 0.1f, 2.0f, T);
  S(P_TORUS, CL_WOOD, 0, 0.1f, 0, 1.9f, 0.5f, 1.9f, T);
  auto on = [&](float x, float z, float& wy, float& wz) { wy = 0.1f + 0.07f * c - sn * z; wz = 0.07f * sn + c * z; };
  for (int i = 0; i < 6; i++) {
    float a = i * 1.047f + 0.3f, r = i & 1 ? 0.55f : 0.3f, x = sinf(a) * r, z = cosf(a) * r, wy, wz;
    on(x, z, wy, wz); S(P_DISC, CL_RED, x, wy, wz, 0.32f, 0.04f, 0.32f, T);
  }
  float wy, wz; on(0.1f, 0.1f, wy, wz); S(P_BOX, CL_GREEN, 0.1f, wy, wz, 0.2f, 0.03f, 0.12f, T, 30);
}
static void apple() {
  S(P_BALL, CL_MAIN, 0, 0.45f, 0, 0.9f, 0.82f, 0.9f);
  S(P_CYL, CL_WOOD, 0, 0.92f, 0, 0.05f, 0.24f, 0.05f, 0, 0, 10);
  S(P_BALL, CL_GREEN, 0.12f, 0.95f, 0, 0.24f, 0.04f, 0.12f, 0, 0, -20);
}
static void burger() {
  S(P_CYL, CL_GOLD, 0, 0.1f, 0, 1.3f, 0.2f, 1.3f);
  S(P_CYL, CL_BROWN, 0, 0.3f, 0, 1.35f, 0.2f, 1.35f);
  S(P_BOX, CL_YELLOW, 0, 0.42f, 0, 1.3f, 0.04f, 1.3f, 0, 45);
  S(P_DISC, CL_GREEN, 0, 0.47f, 0, 1.4f, 0.04f, 1.4f);
  S(P_DISC, CL_RED, 0, 0.52f, 0, 1.2f, 0.05f, 1.2f);
  S(P_DOME, CL_GOLD, 0, 0.82f, 0, 1.32f, 0.6f, 1.32f);
  for (int i = 0; i < 5; i++) S(P_BALL, CL_WHITE, sinf(i * 1.3f) * 0.35f, 1.06f, cosf(i * 1.3f) * 0.3f, 0.06f, 0.03f, 0.1f);
}
static void icecream() {
  S(P_CONE, CL_GOLD, 0, 0.5f, 0, 0.6f, 1.0f, 0.6f, 180);
  S(P_BALL, CL_MAIN, 0, 1.08f, 0, 0.7f, 0.6f, 0.7f);
  S(P_BALL, CL_SEC, 0, 1.48f, 0, 0.58f, 0.5f, 0.58f);
  S(P_BALL, CL_RED, 0, 1.78f, 0, 0.18f, 0.18f, 0.18f);
}
static void bread() {
  S(P_BALL, CL_MAIN, 0, 0.32f, 0, 1.0f, 0.64f, 1.7f);
  for (int i = 0; i < 3; i++) S(P_BOX, CL_DEEP, 0, 0.62f, -0.4f + i * 0.4f, 0.5f, 0.04f, 0.06f, 0, 20);
}
static void banana() { S(P_ARC, CL_MAIN, 0, 0.6f, 0, 0.7f, 0.5f, 0.6f, 0, 0, 180); S(P_BOX, CL_BROWN, -0.66f, 0.66f, 0, 0.08f, 0.14f, 0.08f); }
static void carrot() {
  S(P_CONE, CL_MAIN, 0, 0.6f, 0, 0.36f, 1.2f, 0.36f, 180);
  for (int i = 0; i < 3; i++) S(P_CONE, CL_GREEN, (i - 1) * 0.08f, 1.38f, 0, 0.08f, 0.4f, 0.08f, 0, 0, (i - 1) * 25.0f);
}
static void egg() { S(P_BALL, CL_MAIN, 0, 0.42f, 0, 0.6f, 0.84f, 0.6f); }
// symbols
static void question() {
  S(P_ARC, CL_GLOWM, 0, 1.6f, 0, 0.7f, 0.6f, 0.6f, 0, 0, -30);
  S(P_BOX, CL_GLOWM, 0.08f, 1.08f, 0, 0.2f, 0.5f, 0.2f, 0, 0, -10);
  S(P_BALL, CL_GLOWM, 0.05f, 0.55f, 0, 0.25f, 0.25f, 0.25f);
}
static void exclaim() {
  S(P_BOX, CL_GLOWM, 0, 1.3f, 0, 0.28f, 1.1f, 0.28f);
  S(P_BALL, CL_GLOWM, 0, 0.45f, 0, 0.3f, 0.3f, 0.3f);
}
static void note() {
  S(P_BALL, CL_MAIN, 0, 0.3f, 0, 0.55f, 0.4f, 0.3f, 0, 0, 20);
  S(P_BOX, CL_MAIN, 0.24f, 0.95f, 0, 0.08f, 1.3f, 0.08f);
  J(A_FLAG, P_BOX, CL_MAIN, 0.26f, 1.5f, 0, 0.18f, 0, 0, 0.36f, 0.12f, 0.06f, 0, 0, -30);
}
static void bolt() {
  S(P_BOX, CL_GLOWM, 0.15f, 1.5f, 0, 0.3f, 1.0f, 0.15f, 0, 0, -20);
  S(P_BOX, CL_GLOWM, 0, 1.0f, 0, 0.6f, 0.18f, 0.15f, 0, 0, 25);
  S(P_BOX, CL_GLOWM, -0.15f, 0.5f, 0, 0.3f, 1.0f, 0.15f, 0, 0, -20);
}
static void check() {
  S(P_BOX, CL_GLOWM, -0.25f, 0.6f, 0, 0.25f, 0.6f, 0.25f, 0, 0, 40);
  S(P_BOX, CL_GLOWM, 0.25f, 0.9f, 0, 0.25f, 1.2f, 0.25f, 0, 0, -30);
}
static void cross() {
  S(P_BOX, CL_GLOWM, 0, 0.8f, 0, 0.26f, 1.4f, 0.26f, 0, 0, 45);
  S(P_BOX, CL_GLOWM, 0, 0.8f, 0, 0.26f, 1.4f, 0.26f, 0, 0, -45);
}
static void face(bool sad) {
  S(P_BALL, CL_MAIN, 0, 0, 0, 1.6f, 1.6f, 1.6f);
  for (int k = -1; k <= 1; k += 2) S(P_BALL, CL_DARK, k * 0.26f, 0.22f, 0.7f, 0.16f, 0.26f, 0.1f);
  S(P_ARC, CL_DARK, 0, sad ? -0.45f : -0.08f, 0.72f, 0.42f, 0.32f, 0.1f, 0, 0, sad ? 0 : 180);
  if (sad) S(P_BALL, CL_BLUE, 0.3f, -0.05f, 0.72f, 0.1f, 0.16f, 0.06f, 0, 0, 0, R_EMIT);
}
static void bubble() {
  S(P_BALL, CL_MAIN, 0, 0, 0, 2.0f, 1.3f, 0.5f);
  S(P_PYR, CL_MAIN, -0.5f, -0.75f, 0, 0.5f, 0.6f, 0.3f, 0, 0, 160);
  for (int i = -1; i <= 1; i++) J(A_BLINK, P_BALL, CL_DARK, 0, 0, 0, i * 0.4f, 0, 0.24f, 0.2f, 0.2f, 0.06f, 0, 0, 0, i * 1.2f);
}
static void arrow(bool up) {
  S(P_BOX, CL_GLOWM, 0, 0, 0, 0.4f, 1.4f, 0.4f, 0, 0, up ? 0 : 180);
  S(P_CONE, CL_GLOWM, 0, up ? 1.0f : -1.0f, 0, 1.0f, 0.7f, 1.0f, 0, 0, up ? 0 : 180);
}
static void globe() {
  J(A_SPINY, P_BALL, CL_MAIN, 0, 0, 0, 0, 0, 0, 2, 2, 2);
  float L[5][3] = {{0.4f, 0.4f, 0.75f}, {-0.6f, 0.1f, 0.7f}, {0.2f, -0.5f, -0.8f}, {-0.3f, 0.6f, -0.7f}, {0.85f, -0.2f, -0.3f}};
  for (int i = 0; i < 5; i++) J(A_SPINY, P_ROCK, CL_SEC, 0, 0, 0, L[i][0] * 0.9f, L[i][1] * 0.9f, L[i][2] * 0.9f, 0.7f, 0.5f, 0.7f, i * 40.0f, i * 70.0f);
}
static void lock() {
  S(P_BOX, CL_GOLD, 0, 0.5f, 0, 1.0f, 0.8f, 0.4f);
  S(P_ARC, CL_SILVER, 0, 0.9f, 0, 0.32f, 0.45f, 0.4f);
  S(P_BOX, CL_DARK, 0, 0.5f, 0.21f, 0.1f, 0.25f, 0.02f);
}
static void drop() {
  S(P_BALL, CL_MAIN, 0, 0.5f, 0, 1.0f, 1.0f, 1.0f, 0, 0, 0, R_ALPHA);
  S(P_CONE, CL_MAIN, 0, 1.25f, 0, 0.72f, 0.7f, 0.72f, 0, 0, 0, R_ALPHA);
}
static void balloon() {
  J(A_SWAY, P_BALL, CL_MAIN, 0, 0, 0, 0, 2.2f, 0, 0.9f, 1.1f, 0.9f);
  J(A_SWAY, P_CONE, CL_MAIN, 0, 0, 0, 0, 1.6f, 0, 0.16f, 0.14f, 0.16f);
  J(A_SWAY, P_CYL, CL_WHITE, 0, 0, 0, 0, 0.8f, 0, 0.015f, 1.6f, 0.015f);
}
static void prim(int p, float w, float h, float d, float y) { S(p, CL_MAIN, 0, y, 0, w, h, d); }

// ------------------------------------------------------------------ the list (names as error.blue's scenes use them)
// kinds: K_GROUND stands, K_AIR hovers, K_SKY high up, K_FAR big and at the back, K_WATER floats
void modelsInit() {
  if (nm) return;
  if (!pool) pool = (Part*)malloc(sizeof(Part) * MAXP);
  if (!pool) return;
  M("person", 0x3F7FBF, 0x2B2B3B, 0.4f, 1.8f, K_GROUND); person(false);
  M("child", 0xE85D5D, 0x2F4F8F, 0.3f, 1.2f, K_GROUND); person(true);
  M("robot", 0x9AA4B0, 0x5A6470, 0.5f, 2.0f, K_GROUND); robot();
  M("ghost", 0xF2F2FF, 0xFFFFFF, 0.6f, 1.9f, K_GROUND, B_FLOAT); ghost();
  M("alien", 0x6BD66B, 0x3A8A3A, 0.4f, 1.9f, K_GROUND); alien();
  M("monster", 0x8E44AD, 0x5B2C6F, 0.8f, 2.3f, K_GROUND); monster();
  M("dog", 0xB07840, 0x6B4423, 0.6f, 0.9f, K_GROUND); dog(false);
  M("cat", 0xE08A3C, 0xFFFFFF, 0.45f, 0.75f, K_GROUND); dog(true);
  M("bird", 0x3A7BD5, 0xF5C542, 0.4f, 0.6f, K_AIR, B_FLY); bird();
  M("fish", 0xFF8C1A, 0xFFFFFF, 0.5f, 0.9f, K_AIR, B_SWIM); fish(false);
  M("whale", 0x3E6E9E, 0xDDE6EE, 2.0f, 3.5f, K_WATER, B_SWIM); fish(true);
  M("horse", 0x8B5A2B, 0x2B1B0E, 0.9f, 2.2f, K_GROUND); horse(false);
  M("cow", 0xF5F5F5, 0x222222, 1.0f, 2.0f, K_GROUND); horse(true);
  M("pig", 0xF4A6B4, 0xD9788A, 0.6f, 1.0f, K_GROUND); pig();
  M("sheep", 0xF2F2F2, 0x2A2A2A, 0.6f, 1.2f, K_GROUND); sheep();
  M("rabbit", 0xD8D0C8, 0xFFFFFF, 0.35f, 1.0f, K_GROUND, B_JUMP); rabbit();
  M("bear", 0x7A4E2D, 0x7A4E2D, 0.7f, 1.7f, K_GROUND); bear(false);
  M("panda", 0xF5F5F5, 0x222222, 0.7f, 1.7f, K_GROUND); bear(true);
  M("elephant", 0x8D9399, 0x8D9399, 1.3f, 2.8f, K_GROUND); elephant();
  M("snake", 0x5DAA3C, 0xE0C33C, 0.6f, 0.4f, K_GROUND, B_WALK); snake();
  M("butterfly", 0xFF5FA2, 0xFFD23F, 0.35f, 0.5f, K_AIR, B_FLY); butterfly();
  M("spider", 0x2A2A2A, 0x2A2A2A, 0.6f, 0.7f, K_GROUND); spider();
  M("dinosaur", 0x4CAF50, 0x2E7D32, 1.3f, 3.0f, K_GROUND); dino(false);
  M("dragon", 0xC62828, 0xFFB300, 1.4f, 3.0f, K_GROUND); dino(true);
  M("snowman", 0xD32F2F, 0xFFFFFF, 0.6f, 2.4f, K_GROUND); snowman();
  M("tree", 0x4E9A3A, 0x4E9A3A, 1.0f, 3.2f, K_GROUND); tree();
  M("pine", 0x2E6B3A, 0x2E6B3A, 1.0f, 3.3f, K_GROUND); pine();
  M("palm", 0x3E9B3E, 0x3E9B3E, 1.2f, 3.4f, K_GROUND); palm();
  M("flower", 0xFF4F7A, 0xFFD23F, 0.25f, 0.8f, K_GROUND); flower();
  M("bush", 0x3E8E3E, 0x3E8E3E, 0.7f, 0.9f, K_GROUND); bush();
  M("rock", 0x8A8A8A, 0x8A8A8A, 0.8f, 1.0f, K_GROUND); rock();
  M("grass", 0x5CAF3C, 0x5CAF3C, 0.4f, 0.6f, K_GROUND); grass();
  M("mountain", 0x7D7468, 0x7D7468, 4.5f, 6.0f, K_FAR); mountain();
  M("cloud", 0xF4F6FA, 0xF4F6FA, 1.5f, 1.2f, K_SKY, B_FLOAT); cloud();
  M("sun", 0xFFD54F, 0xE08A00, 1.5f, 2.0f, K_SKY, B_SPIN); sun();
  M("moon", 0xE8E6D8, 0xE8E6D8, 1.0f, 2.0f, K_SKY, B_FLOAT); moon();
  M("planet", 0xE0884A, 0xE8D7A8, 2.2f, 2.0f, K_SKY, B_SPIN); planet();
  M("star", 0xFFD54F, 0xFFD54F, 0.6f, 1.2f, K_AIR, B_SPIN); star();
  M("fire", 0xFF7A1A, 0xFF7A1A, 0.6f, 1.0f, K_GROUND); fire();
  M("volcano", 0x5A4636, 0x5A4636, 3.8f, 4.5f, K_FAR); volcano();
  M("rainbow", 0xFF0000, 0xFF0000, 5.0f, 5.0f, K_FAR, B_IDLE, 1); rainbow();
  M("mushroom", 0xD9342B, 0xFFFFFF, 0.55f, 1.2f, K_GROUND); mushroom();
  M("cactus", 0x4C9A4C, 0x4C9A4C, 0.6f, 2.3f, K_GROUND); cactus();
  M("house", 0xEADBC0, 0xB5412F, 1.7f, 3.3f, K_GROUND); house();
  M("tower", 0x6F8197, 0x6F8197, 1.2f, 9.5f, K_GROUND); tower();
  M("castle", 0xB8B0A0, 0x8E2F2F, 2.4f, 6.5f, K_GROUND); castle();
  M("tent", 0xE8892A, 0xE8892A, 1.3f, 1.6f, K_GROUND); tent();
  M("lighthouse", 0xD32F2F, 0xFFFFFF, 1.0f, 6.3f, K_GROUND); lighthouse();
  M("windmill", 0xC9B38A, 0x8A5A3A, 1.1f, 4.6f, K_GROUND); windmill();
  M("factory", 0x9A8F87, 0x6E6560, 2.2f, 4.2f, K_GROUND); factory();
  M("bridge", 0x8A7A6A, 0xB0413E, 1.2f, 2.8f, K_GROUND); bridge();
  M("car", 0xD32F2F, 0xD32F2F, 1.3f, 1.4f, K_GROUND); car();
  M("bus", 0xF2B705, 0xF2B705, 1.8f, 2.2f, K_GROUND); bus();
  M("truck", 0x2F6FB0, 0xEEEEEE, 1.9f, 2.5f, K_GROUND); truck();
  M("bike", 0x1E88E5, 0x1E88E5, 0.6f, 1.0f, K_GROUND); bike();
  M("train", 0x2E7D32, 0xB71C1C, 2.6f, 3.0f, K_GROUND); train();
  M("plane", 0xF0F0F0, 0x1E5AA8, 2.8f, 1.2f, K_SKY, B_FLY); plane();
  M("boat", 0xB5412F, 0xF5F5F5, 1.0f, 3.2f, K_WATER); boat(false);
  M("ship", 0x283593, 0xD32F2F, 2.2f, 4.2f, K_WATER); boat(true);
  M("rocket", 0xEEEEEE, 0xD32F2F, 0.8f, 4.8f, K_GROUND); rocket();
  M("helicopter", 0xD32F2F, 0xD32F2F, 1.4f, 2.0f, K_AIR, B_FLY); helicopter();
  M("ufo", 0xB0BEC5, 0x7CFC00, 1.6f, 1.2f, K_AIR, B_FLOAT); ufo();
  M("ball", 0xE53935, 0xFFFFFF, 0.5f, 1.0f, K_GROUND, B_JUMP); ball();
  M("book", 0x8E2F2F, 0x8E2F2F, 1.0f, 0.3f, K_GROUND); book();
  M("phone", 0x222222, 0x222222, 0.4f, 1.6f, K_GROUND); phone();
  M("computer", 0x333840, 0x333840, 0.9f, 1.2f, K_GROUND); computer();
  M("tv", 0x222222, 0x222222, 1.3f, 2.1f, K_GROUND); tv();
  M("chair", 0xA0703C, 0x6B4A2A, 0.5f, 1.5f, K_GROUND); chair();
  M("table", 0xA0703C, 0x6B4A2A, 1.2f, 0.85f, K_GROUND); tableM();
  M("bed", 0x3F6FB5, 0x3F6FB5, 1.3f, 1.2f, K_GROUND); bed();
  M("cup", 0xF5F5F5, 0xF5F5F5, 0.45f, 0.6f, K_GROUND); cup();
  M("bottle", 0x2E7D32, 0xD4AF37, 0.25f, 1.5f, K_GROUND); bottle();
  M("gift", 0xD32F2F, 0xFFD54F, 0.5f, 1.2f, K_GROUND); gift();
  M("clock", 0x8E2F2F, 0x6B4A2A, 0.8f, 2.0f, K_GROUND); clock();
  M("bulb", 0xFFF59D, 0xFFF59D, 0.5f, 1.4f, K_AIR, B_FLOAT); bulb();
  M("guitar", 0xC0702A, 0xDDDDDD, 0.5f, 2.6f, K_GROUND); guitar();
  M("drum", 0xD32F2F, 0xD32F2F, 0.6f, 1.4f, K_GROUND); drum();
  M("piano", 0x222222, 0x222222, 1.3f, 1.8f, K_GROUND); piano();
  M("umbrella", 0x1E88E5, 0xFFFFFF, 1.1f, 2.3f, K_GROUND); umbrella();
  M("key", 0xD4AF37, 0xD4AF37, 0.6f, 1.0f, K_AIR, B_SPIN); key();
  M("money", 0xD4AF37, 0xD4AF37, 0.8f, 1.3f, K_GROUND); coins();
  M("crown", 0xD4AF37, 0xD4AF37, 0.5f, 1.0f, K_AIR, B_SPIN); crown();
  M("heart", 0xE53950, 0xE53950, 0.6f, 1.2f, K_AIR, B_FLOAT); heart();
  M("letter", 0xF5F1E6, 0x8A7A6A, 0.8f, 1.1f, K_AIR, B_FLOAT); letter();
  M("pencil", 0xF2C12E, 0xF2C12E, 0.2f, 2.0f, K_AIR, B_SPIN); pencil();
  M("camera", 0x333333, 0x333333, 0.6f, 1.0f, K_GROUND); camera();
  M("hat", 0x222222, 0xB71C1C, 0.65f, 0.9f, K_GROUND); hat();
  M("trophy", 0xD4AF37, 0xFFFFFF, 0.5f, 1.6f, K_GROUND); trophy();
  M("flag", 0xD32F2F, 0xFFFFFF, 0.6f, 2.6f, K_GROUND); flag();
  M("bag", 0x8E5A2F, 0x5A3A1F, 0.5f, 1.2f, K_GROUND); bag();
  M("drink", 0xC8952A, 0xFFFFFF, 0.3f, 1.0f, K_GROUND); drink();
  M("cake", 0xF4A6B4, 0xFFFFFF, 0.8f, 1.6f, K_GROUND); cake();
  M("pizza", 0xF2C14E, 0xF2C14E, 1.0f, 0.6f, K_AIR, B_FLOAT); pizza();
  M("apple", 0xD32F2F, 0xD32F2F, 0.45f, 1.0f, K_GROUND); apple();
  M("burger", 0x8A5A2B, 0x8A5A2B, 0.7f, 1.1f, K_GROUND); burger();
  M("icecream", 0xF48FB1, 0xFFF8E1, 0.35f, 1.9f, K_GROUND); icecream();
  M("bread", 0xC8873A, 0xC8873A, 0.8f, 0.65f, K_GROUND); bread();
  M("banana", 0xF5D33A, 0xF5D33A, 0.6f, 1.0f, K_GROUND); banana();
  M("carrot", 0xEF7C1A, 0xEF7C1A, 0.2f, 1.6f, K_GROUND); carrot();
  M("egg", 0xF3E5C8, 0xF3E5C8, 0.3f, 0.85f, K_GROUND); egg();
  M("question", 0xFFD54F, 0xFFD54F, 0.5f, 2.0f, K_AIR, B_SPIN); question();
  M("exclaim", 0xFF5252, 0xFF5252, 0.3f, 2.0f, K_AIR, B_FLOAT); exclaim();
  M("note", 0x222222, 0x222222, 0.4f, 1.6f, K_AIR, B_FLOAT); note();
  M("bolt", 0xFFEB3B, 0xFFEB3B, 0.5f, 2.0f, K_AIR, B_FLOAT); bolt();
  M("check", 0x4CAF50, 0x4CAF50, 0.5f, 1.5f, K_AIR, B_FLOAT); check();
  M("cross", 0xF44336, 0xF44336, 0.5f, 1.6f, K_AIR, B_FLOAT); cross();
  M("smiley", 0xFFD54F, 0xFFD54F, 0.8f, 1.6f, K_AIR, B_FLOAT); face(false);
  M("sadface", 0x90A4AE, 0x90A4AE, 0.8f, 1.6f, K_AIR, B_FLOAT); face(true);
  M("speech", 0xFFFFFF, 0xFFFFFF, 1.0f, 1.4f, K_AIR, B_FLOAT); bubble();
  M("up", 0x4CAF50, 0x4CAF50, 0.5f, 2.1f, K_AIR, B_FLOAT); arrow(true);
  M("down", 0xF44336, 0xF44336, 0.5f, 2.1f, K_AIR, B_FLOAT); arrow(false);
  M("globe", 0x1E6FD9, 0x4CAF50, 1.0f, 2.0f, K_AIR, B_FLOAT); globe();
  M("lock", 0xD4AF37, 0xB0BEC5, 0.5f, 1.3f, K_AIR, B_FLOAT); lock();
  M("balloon", 0xE53935, 0xFFFFFF, 0.5f, 2.7f, K_GROUND); balloon();
  M("drop", 0x4FC3F7, 0x4FC3F7, 0.5f, 1.6f, K_AIR, B_FLOAT); drop();
  M("cube", 0x4FC3F7, 0x4FC3F7, 0.6f, 1.2f, K_GROUND); prim(P_BOX, 1.2f, 1.2f, 1.2f, 0.6f);
  M("sphere", 0x4FC3F7, 0x4FC3F7, 0.6f, 1.2f, K_GROUND); prim(P_BALL, 1.2f, 1.2f, 1.2f, 0.6f);
  M("pyramid", 0xD9B66A, 0xD9B66A, 2.0f, 3.0f, K_GROUND); prim(P_PYR, 4.0f, 3.0f, 4.0f, 1.5f);
  M("cylinder", 0x4FC3F7, 0x4FC3F7, 0.5f, 1.4f, K_GROUND); prim(P_CYL, 1.0f, 1.4f, 1.0f, 0.7f);
  M("ring", 0xD4AF37, 0xD4AF37, 0.6f, 1.2f, K_AIR, B_SPIN); S(P_TORUS, CL_MAIN, 0, 0, 0, 1.2f, 1.2f, 1.2f, 90);
  N_MODELS = nm;
}
int modelFind(const char* name) {
  if (!name) return -1;
  for (int i = 0; i < nm; i++) if (!strcasecmp(table[i].name, name)) return i;
  return -1;
}
const Part* modelParts(int m) { return m >= 0 && m < nm ? pool + table[m].p0 : nullptr; }
int modelSpecial(int m) { return m >= 0 && m < nm ? special[m] : 0; }
