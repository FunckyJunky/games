// r3d.h: DREAM-CUBE's 3D: flat-shaded, z-buffered triangles (low-poly look), a sky, fog, sprites and streaks.
// Renders at RW x RH (240 x 240) in bands (a few rows at a time, in fast internal RAM), then 2x up into fb (480 x 480).
// Plain C++: the same code runs on the cube and in the PC simulator.
#pragma once
#include <stdint.h>

#define RW 240
#define RH 240

struct Vec { float x, y, z; };
static inline Vec vec(float x, float y, float z) { Vec v = {x, y, z}; return v; }
static inline Vec vadd(Vec a, Vec b) { return vec(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline Vec vsub(Vec a, Vec b) { return vec(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline Vec vmul(Vec a, float s) { return vec(a.x * s, a.y * s, a.z * s); }
static inline float vdot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec vcross(Vec a, Vec b) { return vec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
Vec vnorm(Vec a);
Vec vlerp(Vec a, Vec b, float t);

// a colour as floats 0..1 (lighting and fog are worked out like this, then packed to RGB565)
struct Col { float r, g, b; };
static inline Col col(float r, float g, float b) { Col c = {r, g, b}; return c; }
Col hexCol(uint32_t rgb);                 // 0xRRGGBB
Col clerp(Col a, Col b, float t);
Col cmul(Col a, float s);
uint16_t c565(Col c);

// 3x4 matrix (rotation + scale | translation)
struct Mat { float m[12]; };
Mat matId();
Mat matMul(const Mat& a, const Mat& b);   // a * b (b first)
Mat matT(float x, float y, float z);
Mat matS(float x, float y, float z);
Mat matRx(float rad);
Mat matRy(float rad);
Mat matRz(float rad);
Vec matApply(const Mat& m, Vec v);

// unit shapes (made once): all about 2 units across, centred (see r3d.cpp)
enum Prim {
  P_BOX, P_BALL, P_CYL, P_CONE, P_PYR, P_WEDGE, P_TORUS, P_DISC, P_DOME, P_STAR, P_HEART, P_ROCK, P_ARC, P_RING,
  P_TUBE, P_N
};
struct Mesh { const float* v; int nv; const uint16_t* t; int nt; };
const Mesh& primMesh(int p);

// a frame
enum { R_EMIT = 1, R_NOCULL = 2, R_NOFOG = 4, R_ALPHA = 8, R_SOFT = 16 };   // R_ALPHA: 50% see-through
bool r3dInit();                           // band buffers + the triangle list (false = no memory)
void r3dBegin(Vec eye, Vec target, float fovDeg, float roll = 0);
void r3dLight(Vec toSun, Col sun, Col skyAmb, Col groundAmb);
void r3dFog(float nearZ, float farZ, Col c);
void r3dFill(Col c);
void r3dLook(int mode, Col tint);         // colours: 0 as they are, 1 vivid, 2 one colour (tint: ERROR-DOS)                     // a soft light from the camera (so the subject never disappears in the dark)
void r3dSkyRows(Col top, Col horizon, Col below);   // the background, by height above the horizon
void r3dMesh(int prim, const Mat& world, Col c, int flags = 0);
void r3dTri(Vec a, Vec b, Vec c, Col color, int flags = 0);   // world space
enum { SP_DISC, SP_SQUARE, SP_HEART, SP_SPARK, SP_NOTE, SP_Z, SP_QUEST, SP_RING, SP_STAR, SP_DROP };
void r3dSprite(Vec p, float size, Col c, int shape, int alpha32 = 32);   // size in world units
void r3dSkyDisc(Vec dir, float radiusPx, Col c, float glow);   // sun / moon: at infinity, before the world
void r3dSkyDot(Vec dir, Col c, int alpha32);                   // a star
void r3dStreak(Vec a, Vec b, Col c, int alpha32);              // rain, sparks: a line with depth
bool r3dProject(Vec w, float& sx, float& sy, float& depth);    // world -> render pixels (RW x RH)
// draws everything into out (480 x 480): 2x up, soft (smooth) or sharp
void r3dRender(uint16_t* out, bool soft);
int r3dTris();                            // triangles in the last frame (after culling)
extern float r3dFlash;                    // 0..1: everything brighter (lightning)
