// world.h: what DREAM-CUBE can show. models.cpp: ~90 low-poly models built from the unit shapes (r3d.h), each
// part with its own little movement (legs, wings, wheels...). scene.cpp: a scene from error.blue (JSON) -> the
// place, sky, weather, things, their movements, the camera; drawn every frame. Plain C++ (sim + cube).
#pragma once
#include "r3d.h"
#include <stdint.h>
#include <stddef.h>

// ------------------------------------------------------------------ models
// colours a part can have: 0 / 1 = the thing's own two (from the scene, else the model's), the rest fixed
enum {
  CL_MAIN, CL_SEC, CL_SKIN, CL_WHITE, CL_BLACK, CL_DARK, CL_GRAY, CL_RED, CL_YELLOW, CL_GREEN, CL_BROWN, CL_BLUE, CL_ORANGE, CL_PINK,
  CL_GLASS,   // light blue, see-through
  CL_LIGHT,   // glows: warm white
  CL_LITE,    // the main colour, lighter
  CL_DEEP,    // the main colour, darker
  CL_GOLD, CL_LEAF, CL_WOOD,
  CL_FIRE,    // glows: orange
  CL_EYE,     // glows: white (robot eyes, stars)
  CL_SCREEN,  // glows: cyan blue
  CL_WIN,     // windows: light blue by day, glowing yellow at night
  CL_HAIR, CL_PURPLE, CL_SILVER, CL_GLOWM,   // CL_GLOWM: the main colour, glowing
  CL_N
};
// movements of a part (around its pivot)
enum {
  A_NONE, A_LEGL, A_LEGR, A_ARML, A_ARMR, A_WINGL, A_WINGR, A_TAIL, A_HEAD, A_SPINY, A_SPINZ, A_SPINX, A_FLAME,
  A_FINS, A_TRUNK, A_HANDH, A_HANDM, A_BEAT, A_SWAY, A_SNAKE, A_PAGE, A_FLAG, A_BLINK, A_EARS, A_BOB
};
struct Part {
  uint8_t prim, col, anim, fl;
  float px, py, pz;          // pivot (model space, y = 0 is the ground)
  float ox, oy, oz;          // the shape's centre, from the pivot
  float sx, sy, sz;          // half sizes
  float rx, ry, rz;          // fixed turn (radians): y, then x, then z
  float ph;                  // the movement's phase (snakes, flags)
};
// where a thing goes by itself
enum { K_GROUND, K_AIR, K_SKY, K_FAR, K_WATER };
struct Model {
  const char* name;
  uint32_t c1, c2;           // its own colours
  float rad, h;              // footprint radius, height (model units: about metres)
  uint8_t kind, anim;        // where it goes, how it moves by itself (B_* below)
  uint16_t p0, np;           // its parts
};
extern const Model* MODELS;
extern int N_MODELS;
void modelsInit();
int modelFind(const char* name);           // -1 = none
const Part* modelParts(int m);
int modelSpecial(int m);                   // drawn in code (rainbow...): 1.., 0 = parts only

// what a thing does (a scene's "a")
enum {
  B_IDLE, B_WALK, B_RUN, B_JUMP, B_FLY, B_SWIM, B_SPIN, B_DANCE, B_WAVE, B_TALK, B_SLEEP, B_SHAKE, B_FLOAT,
  B_DRIVE, B_GROW, B_FALL, B_N
};
extern const char* const BEHAVIORS[B_N];

// ------------------------------------------------------------------ scenes
enum {
  PL_GRASS, PL_FOREST, PL_JUNGLE, PL_DESERT, PL_BEACH, PL_SEA, PL_SNOW, PL_MOUNTAINS, PL_CITY, PL_ROOM, PL_SPACE,
  PL_MOON, PL_UNDERWATER, PL_CLOUDS, PL_LAVA, PL_STAGE, PL_VOID, PL_N
};
enum { TM_DAY, TM_SUNSET, TM_NIGHT, TM_DAWN, TM_N };
enum { WE_CLEAR, WE_CLOUDY, WE_RAIN, WE_SNOW, WE_STORM, WE_FOG, WE_WIND, WE_N };
enum { MO_NEUTRAL, MO_HAPPY, MO_CALM, MO_SAD, MO_SCARY, MO_EXCITING, MO_LOVE, MO_MYSTERY, MO_ANGRY, MO_FUNNY, MO_N };
enum { FX_NONE, FX_HEARTS, FX_SPARKLES, FX_FIRE, FX_BUBBLES, FX_MUSIC, FX_CONFETTI, FX_LEAVES, FX_ZZZ, FX_QUESTIONS,
       FX_MONEY, FX_STARS, FX_SMOKE, FX_N };
enum { CAM_ORBIT, CAM_CLOSE, CAM_WIDE, CAM_TOP, CAM_LOW, CAM_FLY, CAM_N };
enum { AT_CENTER, AT_LEFT, AT_RIGHT, AT_BACK, AT_FRONT, AT_AROUND, AT_SKY, AT_FAR, AT_N };
extern const char* const PLACES[PL_N];
extern const char* const TIMES[TM_N];
extern const char* const WEATHERS[WE_N];
extern const char* const MOODS[MO_N];
extern const char* const FXS[FX_N];
extern const char* const CAMS[CAM_N];
extern const char* const ATS[AT_N];

#define MAX_THINGS 8
#define MAX_CPARTS 24
struct CPart { uint8_t prim; uint32_t c; float x, y, z, w, h, d, ry; };   // "custom": built by error.blue
struct Thing {
  int16_t model;             // -1 = custom (parts cp0 .. cp0 + ncp)
  uint8_t n, anim, at;
  float s;
  uint32_t c, c2;            // 0xRRGGBB; 0x1000000 = the model's own
  uint8_t cp0, ncp;
  float custH;
};
struct SceneDef {
  char title[48];
  char said[160];            // what it was made from (shown small)
  uint8_t place, time, weather, mood, fx, cam;
  Thing th[MAX_THINGS]; int nth;
  CPart cp[MAX_CPARTS]; int ncp;
};
#define NOCOL 0x1000000u
void sceneDefault(SceneDef& d);            // the start: a glowing cube over a calm night sea
bool sceneParse(const char* json, SceneDef& d);   // error.blue's answer -> d (false = not a scene)

// the one on screen
void sceneShow(const SceneDef& d, bool instant = false);   // with a transition
const SceneDef& sceneNow();
void sceneDraw(uint16_t* out, float dt);   // a frame into the 480 x 480 screen
void sceneDrag(float dx, float dy);        // a finger turns the camera (pixels)
void sceneSoft(bool on);                   // smooth 2x up (else sharp pixels)
void sceneSpeed(float s);                  // camera speed (0 = still)
void sceneDecor(bool on);                  // the place's own trees, houses... (off = faster)
void sceneFx(bool on);                     // hearts, sparkles, notes, smoke... (rain and snow stay)
float sceneTransition();                   // 0..1 while a new scene comes in, 1 = done
int sceneTris();
