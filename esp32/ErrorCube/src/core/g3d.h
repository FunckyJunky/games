// g3d.h: small 3D for the cube's interface: a camera, an object transform, anti-aliased glowing lines,
// filled polygons (flat shaded), and some shapes. Draws into fb (gfx.h). Plain C++ (also in the simulator).
#pragma once
#include <stdint.h>
#include "gfx.h"

struct V3 { float x, y, z; };
static inline V3 v3(float x, float y, float z) { V3 v = {x, y, z}; return v; }

// the camera: at (x, y, z) looking along +z (yaw turns right, pitch tilts down), f = focal length in pixels
void g3dCam(float x, float y, float z, float yaw, float pitch, float roll, float f, float cx, float cy);
// what's drawn next: scaled, turned (yaw around y, then pitch around x, then roll around z), moved to pos
void g3dObj(V3 pos, float yaw, float pitch, float roll, float scale);
V3 g3dWorld(V3 p);                          // object -> world
bool g3dProj(V3 w, float& sx, float& sy, float& z);   // world -> screen; false = behind the camera
void g3dFog(float nearZ, float farZ);       // lines get dimmer between these distances (0, 0 = off)
extern bool g3dGlow;                        // lines get a soft glow

// 2D (screen pixels, floats for sub-pixel smoothness)
void aaLine(float x0, float y0, float x1, float y1, uint16_t c, int a32 = 32);
void glowLine(float x0, float y0, float x1, float y1, uint16_t c, int a32 = 32);
void fillPoly(const float* xy, int n, uint16_t c, int a32 = 32);   // convex or concave, even-odd

// 3D, in object space (g3dObj)
void l3(V3 a, V3 b, uint16_t c, int a32 = 32);
void l3(float ax, float ay, float az, float bx, float by, float bz, uint16_t c, int a32 = 32);
void ring3(V3 c, float r, int n, int axis, uint16_t col, int a32 = 32, float a0 = 0, float a1 = 6.2832f);   // axis 0: in the y-z plane, 1: x-z, 2: x-y
void box3(V3 c, V3 h, uint16_t col, int a32 = 32);            // wireframe box, h = half sizes
void quad3(V3 a, V3 b, V3 c, V3 d, uint16_t col, int a32);     // filled
float g3dDepth(V3 p);                       // camera distance of an object-space point
