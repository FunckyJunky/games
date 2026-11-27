// scene.cpp: a scene (world.h) on the screen. Every frame: the sky, the ground (it moves for water), what the place
// puts around by itself, the things with their movements, weather and effects, a slowly turning camera.
// A new scene comes in with a transition: the old things shrink away, the sky and ground blend over, the new things
// pop up one after another, the camera glides to its new place.
#include "world.h"
#include "../core/gfx.h"
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ArduinoJson.h>

const char* const BEHAVIORS[B_N] = {"idle", "walk", "run", "jump", "fly", "swim", "spin", "dance", "wave", "talk", "sleep", "shake", "float", "drive", "grow", "fall"};
const char* const PLACES[PL_N] = {"grass", "forest", "jungle", "desert", "beach", "sea", "snow", "mountains", "city", "room", "space", "moon", "underwater", "clouds", "lava", "stage", "void"};
const char* const TIMES[TM_N] = {"day", "sunset", "night", "dawn"};
const char* const WEATHERS[WE_N] = {"clear", "cloudy", "rain", "snow", "storm", "fog", "wind"};
const char* const MOODS[MO_N] = {"neutral", "happy", "calm", "sad", "scary", "exciting", "love", "mystery", "angry", "funny"};
const char* const FXS[FX_N] = {"none", "hearts", "sparkles", "fire", "bubbles", "music", "confetti", "leaves", "zzz", "questions", "money", "stars", "smoke"};
const char* const CAMS[CAM_N] = {"orbit", "close", "wide", "top", "low", "fly"};
const char* const ATS[AT_N] = {"center", "left", "right", "back", "front", "around", "sky", "far"};

static const float KPI = 3.14159265f;
static inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static inline float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static inline uint32_t hsh(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static inline float h01(uint32_t x) { return (hsh(x) & 0xFFFFFF) / 16777216.0f; }
static Col desat(Col c, float k) { float g = c.r * 0.3f + c.g * 0.59f + c.b * 0.11f; return clerp(c, col(g, g, g), k); }
static Col tint(Col c, Col t, float k) { return clerp(c, col(c.r * t.r * 1.6f, c.g * t.g * 1.6f, c.b * t.b * 1.6f), k); }
static Col c565toCol(uint16_t c) { return col((c >> 11) / 31.0f, ((c >> 5) & 63) / 63.0f, (c & 31) / 31.0f); }

// ------------------------------------------------------------------ the place, sky and light
enum { G_NONE, G_GRASS, G_SAND, G_SNOW, G_WATER, G_CITY, G_FLOOR, G_ROCKY, G_LAVA, G_CLOUD, G_SEAFLOOR, G_STAGE, G_VOID, G_BEACH, G_MOON };
struct Env {
  Col top, hor, below, fog; float fogN, fogF;
  Vec sun; Col sunC, ambS, ambG;
  float sunDisc, moonDisc, stars, rain, snow, storm, wind, night;
  int ground; Col g1, g2;
  float animK;               // how lively things move
};
static Env envA, envB, env;  // from, to, now
static const uint32_t SKY[TM_N][3] = {   // top, horizon, sun colour
  {0x3B7DD8, 0xA9D4F5, 0xFFF4DC}, {0x2B2D6E, 0xFF8A50, 0xFFB070}, {0x03060F, 0x17213D, 0x8FA6D6}, {0x4A6FB0, 0xF7B7A3, 0xFFD2B8}};
static Env makeEnv(const SceneDef& d) {
  Env e;
  int tm = d.time % TM_N;
  e.top = hexCol(SKY[tm][0]); e.hor = hexCol(SKY[tm][1]); e.sunC = hexCol(SKY[tm][2]);
  e.night = tm == TM_NIGHT ? 1 : 0;
  e.sunDisc = tm == TM_NIGHT ? 0 : 1; e.moonDisc = tm == TM_NIGHT ? 1 : 0;
  e.stars = tm == TM_NIGHT ? 1 : tm == TM_DAWN ? 0.15f : 0;
  float sunH = tm == TM_DAY ? 0.85f : tm == TM_NIGHT ? 0.6f : 0.16f;
  e.sun = vnorm(vec(tm == TM_DAY ? 0.45f : -0.55f, sunH, tm == TM_DAY ? 0.55f : -0.75f));
  float li = tm == TM_NIGHT ? 0.55f : tm == TM_DAY ? 1.0f : 0.8f;
  e.sunC = cmul(e.sunC, li);
  e.ambS = tm == TM_NIGHT ? col(0.26f, 0.3f, 0.42f) : cmul(clerp(e.top, col(1, 1, 1), 0.55f), 0.62f);
  e.ambG = cmul(e.ambS, 0.55f);
  e.fogN = 16; e.fogF = 40;
  e.rain = e.snow = e.storm = e.wind = 0;
  e.animK = 1;
  // the place
  int g = G_GRASS; uint32_t g1 = 0x5DA544, g2 = 0x4A8F3A;
  switch (d.place) {
    case PL_FOREST: g1 = 0x4F8F3A; g2 = 0x3D7330; e.fogN = 12; e.fogF = 32; break;
    case PL_JUNGLE: g1 = 0x3E8A2E; g2 = 0x2F6E25; e.fogN = 8; e.fogF = 28; e.hor = clerp(e.hor, hexCol(0x9CCB8A), 0.4f); break;
    case PL_DESERT: g = G_SAND; g1 = 0xE3C27A; g2 = 0xD4AE62; e.hor = clerp(e.hor, hexCol(0xF2D6A0), 0.35f); break;
    case PL_BEACH: g = G_BEACH; g1 = 0xEBD49A; g2 = 0x2F8FC9; break;
    case PL_SEA: g = G_WATER; g1 = 0x2A7FBA; g2 = 0x1E6699; break;
    case PL_SNOW: g = G_SNOW; g1 = 0xEEF3F8; g2 = 0xD8E2EC; e.hor = clerp(e.hor, hexCol(0xE6EEF5), 0.4f); break;
    case PL_MOUNTAINS: g = G_ROCKY; g1 = 0x6E9A4E; g2 = 0x8C8576; e.fogN = 20; e.fogF = 50; break;
    case PL_CITY: g = G_CITY; g1 = 0x55575C; g2 = 0x8E9096; break;
    case PL_ROOM: g = G_FLOOR; g1 = 0xB8834F; g2 = 0x9C6C3E; e.ambS = cmul(col(1, 0.92f, 0.8f), 0.6f); e.ambG = cmul(e.ambS, 0.7f);
      e.sun = vnorm(vec(0.5f, 0.8f, 0.6f)); e.sunC = col(0.75f, 0.68f, 0.55f); e.fogN = 30; e.fogF = 60; e.sunDisc = e.moonDisc = 0; break;
    case PL_SPACE: g = G_NONE; e.top = hexCol(0x000004); e.hor = hexCol(0x060818); e.stars = 1; e.sunDisc = 1.2f; e.moonDisc = 0;
      e.sun = vnorm(vec(-0.7f, 0.35f, -0.6f)); e.sunC = col(1, 0.97f, 0.9f); e.ambS = col(0.12f, 0.13f, 0.2f); e.ambG = col(0.06f, 0.06f, 0.1f);
      e.fogN = 60; e.fogF = 120; e.night = 1; break;
    case PL_MOON: g = G_MOON; g1 = 0x9A9A96; g2 = 0x7E7E7A; e.top = hexCol(0x000004); e.hor = hexCol(0x0A0A14); e.stars = 1; e.sunDisc = 1;
      e.moonDisc = 0; e.sun = vnorm(vec(-0.6f, 0.5f, -0.6f)); e.sunC = col(1, 1, 0.95f); e.ambS = col(0.14f, 0.14f, 0.18f); e.ambG = col(0.08f, 0.08f, 0.1f);
      e.fogN = 50; e.fogF = 100; e.night = 1; break;
    case PL_UNDERWATER: g = G_SEAFLOOR; g1 = 0xC9B98A; g2 = 0xA89A70; e.top = hexCol(0x0B4F7A); e.hor = hexCol(0x0E6E96);
      e.sunDisc = e.moonDisc = 0; e.stars = 0; e.sun = vnorm(vec(0.1f, 1, 0.2f)); e.sunC = col(0.55f, 0.8f, 0.85f);
      e.ambS = col(0.3f, 0.5f, 0.6f); e.ambG = col(0.15f, 0.25f, 0.3f); e.fogN = 3; e.fogF = 24; break;
    case PL_CLOUDS: g = G_CLOUD; g1 = 0xF6F7FA; g2 = 0xE3E8F0; break;
    case PL_LAVA: g = G_LAVA; g1 = 0x2B2220; g2 = 0x3A2C26; e.hor = clerp(e.hor, hexCol(0xC2502A), 0.5f); e.top = clerp(e.top, hexCol(0x3A1410), 0.5f);
      e.fogN = 12; e.fogF = 36; break;
    case PL_STAGE: g = G_STAGE; g1 = 0x1E1A2E; g2 = 0x2A2440; e.top = hexCol(0x05040A); e.hor = hexCol(0x1A1030); e.sunDisc = e.moonDisc = 0;
      e.stars = 0.3f; e.sunC = col(0.6f, 0.55f, 0.7f); e.ambS = col(0.3f, 0.25f, 0.4f); e.ambG = col(0.15f, 0.1f, 0.2f); e.night = 1; break;
    case PL_VOID: g = G_VOID; g1 = 0x050608; g2 = 0x08090C; e.top = hexCol(0x000000); e.hor = clerp(hexCol(0x000000), c565toCol(C_FG), 0.22f);
      e.sunDisc = e.moonDisc = 0; e.stars = 0.6f; e.sunC = col(0.7f, 0.75f, 0.8f); e.ambS = col(0.3f, 0.32f, 0.38f); e.ambG = col(0.15f, 0.15f, 0.2f);
      e.fogN = 14; e.fogF = 34; e.night = 1; break;
  }
  e.ground = g; e.g1 = hexCol(g1); e.g2 = hexCol(g2);
  if (e.night && d.place != PL_SPACE && d.place != PL_MOON && d.place != PL_ROOM) { e.g1 = cmul(e.g1, 0.8f); e.g2 = cmul(e.g2, 0.8f); }
  // weather
  bool outside = d.place != PL_ROOM && d.place != PL_SPACE && d.place != PL_MOON && d.place != PL_UNDERWATER;
  if (outside) switch (d.weather) {
    case WE_CLOUDY: e.top = desat(e.top, 0.55f); e.hor = desat(e.hor, 0.4f); e.sunDisc *= 0.3f; e.sunC = cmul(e.sunC, 0.7f); break;
    case WE_RAIN: e.top = cmul(desat(e.top, 0.8f), 0.6f); e.hor = cmul(desat(e.hor, 0.7f), 0.75f); e.sunDisc = 0; e.moonDisc *= 0.3f; e.stars = 0;
      e.sunC = cmul(e.sunC, 0.45f); e.rain = 1; e.fogN = 8; e.fogF = 30; break;
    case WE_STORM: e.top = cmul(desat(e.top, 0.9f), 0.3f); e.hor = cmul(desat(e.hor, 0.8f), 0.45f); e.sunDisc = 0; e.moonDisc = 0; e.stars = 0;
      e.sunC = cmul(e.sunC, 0.3f); e.rain = 1.6f; e.storm = 1; e.wind = 1; e.fogN = 6; e.fogF = 26; break;
    case WE_SNOW: e.top = clerp(desat(e.top, 0.6f), col(0.8f, 0.82f, 0.86f), 0.3f); e.hor = clerp(e.hor, col(0.88f, 0.9f, 0.93f), 0.5f);
      e.sunDisc *= 0.3f; e.snow = 1; e.fogN = 8; e.fogF = 30; break;
    case WE_FOG: e.fogN = 2; e.fogF = 15; e.sunDisc *= 0.4f; e.stars *= 0.2f; e.hor = desat(e.hor, 0.5f); break;
    case WE_WIND: e.wind = 1; break;
  }
  if (d.place == PL_UNDERWATER) e.animK = 0.7f;
  // mood: a tint over everything
  switch (d.mood) {
    case MO_HAPPY: e.top = desat(e.top, -0.2f); e.sunC = cmul(e.sunC, 1.08f); e.animK = 1.25f; break;
    case MO_FUNNY: e.top = desat(e.top, -0.25f); e.animK = 1.4f; break;
    case MO_EXCITING: e.animK = 1.5f; break;
    case MO_CALM: e.animK = 0.65f; break;
    case MO_SAD: e.top = desat(e.top, 0.5f); e.hor = tint(desat(e.hor, 0.4f), col(0.45f, 0.55f, 0.75f), 0.4f); e.sunC = cmul(e.sunC, 0.8f); e.animK = 0.6f; break;
    case MO_SCARY: e.top = cmul(tint(e.top, col(0.4f, 0.6f, 0.4f), 0.5f), 0.5f); e.hor = cmul(tint(e.hor, col(0.6f, 0.3f, 0.3f), 0.5f), 0.6f);
      e.sunC = cmul(e.sunC, 0.6f); e.fogN *= 0.6f; e.fogF *= 0.7f; break;
    case MO_LOVE: e.hor = tint(e.hor, col(0.9f, 0.5f, 0.65f), 0.55f); e.top = tint(e.top, col(0.7f, 0.45f, 0.75f), 0.3f); break;
    case MO_MYSTERY: e.hor = tint(e.hor, col(0.5f, 0.4f, 0.8f), 0.5f); e.top = tint(e.top, col(0.35f, 0.3f, 0.6f), 0.5f); e.fogN *= 0.6f; e.fogF *= 0.75f; break;
    case MO_ANGRY: e.hor = tint(e.hor, col(0.9f, 0.3f, 0.2f), 0.55f); e.top = tint(e.top, col(0.5f, 0.15f, 0.1f), 0.4f); e.animK = 1.3f; break;
  }
  e.fog = e.ground == G_VOID ? hexCol(0x020203) : e.hor;
  e.below = e.ground == G_NONE ? clerp(e.hor, e.top, 0.6f) : e.fog;
  return e;
}
static Env mixEnv(const Env& a, const Env& b, float t) {
  Env e = t < 0.5f ? a : b;
  e.top = clerp(a.top, b.top, t); e.hor = clerp(a.hor, b.hor, t); e.below = clerp(a.below, b.below, t); e.fog = clerp(a.fog, b.fog, t);
  e.fogN = a.fogN + (b.fogN - a.fogN) * t; e.fogF = a.fogF + (b.fogF - a.fogF) * t;
  e.sun = vnorm(vlerp(a.sun, b.sun, t)); e.sunC = clerp(a.sunC, b.sunC, t); e.ambS = clerp(a.ambS, b.ambS, t); e.ambG = clerp(a.ambG, b.ambG, t);
  e.sunDisc = a.sunDisc + (b.sunDisc - a.sunDisc) * t; e.moonDisc = a.moonDisc + (b.moonDisc - a.moonDisc) * t;
  e.stars = a.stars + (b.stars - a.stars) * t; e.rain = a.rain + (b.rain - a.rain) * t; e.snow = a.snow + (b.snow - a.snow) * t;
  e.storm = a.storm + (b.storm - a.storm) * t; e.wind = a.wind + (b.wind - a.wind) * t; e.night = a.night + (b.night - a.night) * t;
  e.animK = a.animK + (b.animK - a.animK) * t;
  e.g1 = clerp(a.g1, b.g1, t); e.g2 = clerp(a.g2, b.g2, t);
  return e;
}

// ------------------------------------------------------------------ things on screen
enum { X_CUSTOM = -1, X_ROOM = -2, X_STAGE = -3, X_ISLAND = -4, X_RAYS = -5, X_CRATERS = -6 };
struct Inst {
  int16_t model; uint8_t beh, decor, cp0, ncp, sleep;
  float x, y, z, yaw, s, rad, h;
  float cx, cz, orb, phase;
  Col c1, c2, skin, hair;
  float born;
};
#define MAXI 96
static Inst* cur = nullptr; static Inst* old = nullptr; static int nCur = 0, nOld = 0;   // in PSRAM on the cube
static CPart curParts[MAX_CPARTS];
static CPart oldParts[MAX_CPARTS];
static SceneDef nowDef;
static double clock0 = 0;      // seconds, since the start
static float tShow = -100;     // when the current scene came
static float tOldGone = -100;
static bool softUp = true;
static float camSpeedK = 1;
static bool decorOn = true, fxOn = true;
void sceneDecor(bool on) { decorOn = on; }
void sceneFx(bool on) { fxOn = on; }
static float userYaw = 0, userPitch = 0;
static float bounds = 4, heightMax = 2, ctrX = 0, ctrY = 1, ctrZ = 0;
struct Cam { float R, H, ty, speed, arc, tx, tz; };
static Cam camA, camB;
static int lastTris = 0;
const SceneDef& sceneNow() { return nowDef; }
void sceneSoft(bool on) { softUp = on; }
void sceneSpeed(float s) { camSpeedK = s; }
void sceneDrag(float dx, float dy) { userYaw -= dx * 0.009f; userPitch = clampf(userPitch + dy * 0.004f, -0.35f, 0.6f); }
int sceneTris() { return lastTris; }
static float now() { return (float)fmod(clock0, 7200.0); }
float sceneTransition() { return clampf((now() - tShow) / 2.2f, 0, 1); }

static const uint32_t SKINS[5] = {0xF1C27D, 0xE0AC69, 0xC68642, 0x8D5524, 0xFFDBAC};
static const uint32_t HAIRS[5] = {0x2B1B0E, 0x6B4423, 0xD9B44A, 0x1A1A1A, 0xA0522D};
static const uint32_t SHIRTS[8] = {0x3F7FBF, 0xD9534F, 0x5CB85C, 0xF0AD4E, 0x8E44AD, 0x16A085, 0xE67E22, 0x34495E};

static Col partCol(const Inst& in, int c, int& fl) {
  switch (c) {
    case CL_MAIN: return in.c1;
    case CL_SEC: return in.c2;
    case CL_SKIN: return in.skin;
    case CL_HAIR: return in.hair;
    case CL_WHITE: return col(0.93f, 0.93f, 0.92f);
    case CL_BLACK: return col(0.06f, 0.06f, 0.07f);
    case CL_DARK: return col(0.2f, 0.2f, 0.23f);
    case CL_GRAY: return col(0.55f, 0.56f, 0.58f);
    case CL_RED: return hexCol(0xD32F2F);
    case CL_YELLOW: return hexCol(0xF6CF3A);
    case CL_GREEN: return hexCol(0x4CAF50);
    case CL_BROWN: return hexCol(0x8D5A2B);
    case CL_BLUE: return hexCol(0x1E6FD9);
    case CL_ORANGE: return hexCol(0xF08A24);
    case CL_PINK: return hexCol(0xF48FB1);
    case CL_GLASS: fl |= R_ALPHA; return col(0.65f, 0.85f, 0.95f);
    case CL_LIGHT: fl |= R_EMIT; return col(1, 0.95f, 0.72f);
    case CL_LITE: return clerp(in.c1, col(1, 1, 1), 0.4f);
    case CL_DEEP: return cmul(in.c1, 0.65f);
    case CL_GOLD: return hexCol(0xE0B33C);
    case CL_LEAF: return hexCol(0x2E7D32);
    case CL_WOOD: return hexCol(0x6B4A2A);
    case CL_FIRE: fl |= R_EMIT; return col(1, 0.5f, 0.1f);
    case CL_EYE: fl |= R_EMIT; return col(0.6f, 1, 1);
    case CL_SCREEN: fl |= R_EMIT; return col(0.35f, 0.75f, 1);
    case CL_WIN: if (env.night > 0.5f) { fl |= R_EMIT; return col(1, 0.82f, 0.45f); } return col(0.55f, 0.72f, 0.88f);
    case CL_PURPLE: return hexCol(0x8E44AD);
    case CL_SILVER: return hexCol(0xC0C8D0);
    case CL_GLOWM: fl |= R_EMIT; return in.c1;
  }
  return in.c1;
}
static bool living(int m) { return m >= 0 && m <= modelFind("snowman") && m != modelFind("snowman"); }
static bool vehicle(const char* n) {
  static const char* V[] = {"car", "bus", "truck", "bike", "train", "boat", "ship"};
  for (auto v : V) if (!strcmp(n, v)) return true;
  return false;
}

// a part's turn for the thing's behaviour (radians), its scale for flames and beats
static void partMove(const Part& p, const Inst& in, float t, int beh, bool moving, float& ax, float& ay, float& az, float& sc, float& tx) {
  ax = ay = az = 0; sc = 1; tx = 0;
  float k = env.animK;
  float w = t * k + in.phase;
  switch (p.anim) {
    case A_LEGL: case A_LEGR: {
      float s = p.anim == A_LEGL ? 1 : -1;
      if (beh == B_WALK) ax = s * 0.55f * sinf(w * 6.5f);
      else if (beh == B_RUN) ax = s * 0.95f * sinf(w * 12);
      else if (beh == B_DANCE) ax = s * 0.35f * sinf(w * 7);
      else if (beh == B_SWIM) ax = s * 0.4f * sinf(w * 5);
      else if (beh == B_JUMP) ax = -0.35f * fabsf(sinf(w * KPI * 1.5f));
      break;
    }
    case A_ARML: case A_ARMR: {
      float s = p.anim == A_ARML ? -1 : 1;
      if (beh == B_WALK) ax = s * 0.5f * sinf(w * 6.5f);
      else if (beh == B_RUN) ax = s * 0.9f * sinf(w * 12);
      else if (beh == B_WAVE && s > 0) az = 2.5f + 0.35f * sinf(w * 9);
      else if (beh == B_DANCE) az = s * (1.3f + 0.8f * sinf(w * 7 + (s > 0 ? KPI : 0)));
      else if (beh == B_TALK) ax = -0.5f + 0.25f * sinf(w * 5 + s);
      else if (beh == B_SHAKE) az = s * 0.3f + 0.15f * sinf(w * 40);
      else if (beh == B_JUMP || beh == B_FLY) az = s * 0.6f;
      else az = s * 0.05f * sinf(w * 1.3f);
      break;
    }
    case A_WINGL: case A_WINGR: {
      float s = p.anim == A_WINGL ? -1 : 1, f = in.rad < 0.5f ? 15 : in.rad > 1.2f ? 5 : 10;
      float amp = beh == B_FLY || beh == B_FALL ? 0.8f : beh == B_IDLE || beh == B_SLEEP ? 0.12f : 0.4f;
      az = s * amp * sinf(w * f);
      break;
    }
    case A_TAIL: ay = 0.5f * sinf(w * (beh == B_RUN ? 14 : 7)); break;
    case A_HEAD:
      if (beh == B_TALK) ax = 0.14f * sinf(w * 10);
      else if (beh == B_SLEEP) ax = 0.35f;
      else if (beh == B_DANCE) az = 0.18f * sinf(w * 7);
      else if (beh == B_WALK || beh == B_RUN) ax = 0.05f * sinf(w * 13);
      else ay = 0.4f * sinf(w * 0.6f + in.phase);
      break;
    case A_SPINY: ay = t * (p.ph ? p.ph : 1.0f); break;
    case A_SPINZ: az = t * (p.ph ? p.ph : 1.0f); break;
    case A_SPINX: ax = p.ph ? t * p.ph : (moving ? t * 6 * k : 0); break;
    case A_FLAME: sc = 1 + 0.22f * sinf(t * 13 + p.ph) + 0.1f * sinf(t * 31 + p.ph * 2); break;
    case A_FINS: ay = 0.6f * sinf(w * 9); break;
    case A_TRUNK: ax = 0.25f * sinf(w * 1.5f + p.ph); break;
    case A_HANDH: az = -t * 0.1f; break;
    case A_HANDM: az = -t * 1.2f; break;
    case A_BEAT: { float b = sinf(t * 6); b = b > 0 ? b * b * b * b : 0; sc = 1 + 0.16f * b; break; }
    case A_SWAY: az = (0.03f + env.wind * 0.1f) * sinf(t * (1.1f + env.wind) + in.phase + p.ph); ax = 0.02f * sinf(t * 0.9f + in.phase); break;
    case A_SNAKE: tx = 0.22f * sinf(w * 4 - p.ph); break;
    case A_PAGE: az = KPI * (0.5f - 0.5f * cosf(t * 1.1f)); break;
    case A_FLAG: ay = 0.35f * sinf(t * 5 - p.ph); break;
    case A_BLINK: sc = sinf(t * 4 + p.ph) > 0 ? 1 : 0.6f; break;
    case A_BOB: tx = 0; break;
  }
}
static void drawCustom(const Inst& in, const Mat& base, const CPart* cps) {
  for (int i = 0; i < in.ncp; i++) {
    const CPart& c = cps[in.cp0 + i];
    if (in.cp0 + i >= MAX_CPARTS) break;
    Mat m = matMul(base, matMul(matT(c.x, c.y, c.z), matMul(matRy(c.ry * 0.0174533f), matS(c.w * 0.5f, c.h * 0.5f, c.d * 0.5f))));
    r3dMesh(c.prim, m, hexCol(c.c), c.prim == P_RING || c.prim == P_TUBE ? R_NOCULL : 0);
  }
}
static void drawRainbow(const Mat& base) {
  static const uint32_t RB[7] = {0xFF3B30, 0xFF9500, 0xFFE620, 0x4CD964, 0x34AADC, 0x5856D6, 0xAF52DE};
  for (int b = 0; b < 7; b++) {
    float r0 = 5.0f - b * 0.42f, r1 = r0 + 0.42f;
    Col c = hexCol(RB[b]);
    for (int i = 0; i < 14; i++) {
      float a0 = KPI * i / 14, a1 = KPI * (i + 1) / 14;
      Vec p0 = matApply(base, vec(r0 * cosf(a0), r0 * sinf(a0), 0)), p1 = matApply(base, vec(r1 * cosf(a0), r1 * sinf(a0), 0));
      Vec p2 = matApply(base, vec(r1 * cosf(a1), r1 * sinf(a1), 0)), p3 = matApply(base, vec(r0 * cosf(a1), r0 * sinf(a1), 0));
      r3dTri(p0, p1, p2, c, R_EMIT | R_ALPHA | R_NOCULL); r3dTri(p0, p2, p3, c, R_EMIT | R_ALPHA | R_NOCULL);
    }
  }
}
// what a place brings by itself, in code
static void drawDecor(const Inst& in, const Mat& base, float t) {
  switch (in.model) {
    case X_ROOM: {
      Col wall = hexCol(0xE8DCC8), wall2 = hexCol(0xD8C8B0);
      r3dMesh(P_BOX, matMul(base, matMul(matT(0, 4, -8), matS(10, 4, 0.2f))), wall);
      r3dMesh(P_BOX, matMul(base, matMul(matT(-10, 4, 0), matS(0.2f, 4, 8))), wall2);
      r3dMesh(P_BOX, matMul(base, matMul(matT(3, 4.5f, -7.75f), matS(1.8f, 1.4f, 0.05f))), env.night > 0.5f ? col(0.08f, 0.1f, 0.25f) : col(0.6f, 0.82f, 1), R_EMIT);
      r3dMesh(P_BOX, matMul(base, matMul(matT(3, 4.5f, -7.7f), matS(0.06f, 1.4f, 0.06f))), col(1, 1, 1));
      r3dMesh(P_BOX, matMul(base, matMul(matT(3, 4.5f, -7.7f), matS(1.8f, 0.06f, 0.06f))), col(1, 1, 1));
      r3dMesh(P_DISC, matMul(base, matMul(matT(0, 0.02f, 0), matS(4.5f, 0.02f, 3.2f))), hexCol(0x8E3B46));
      r3dMesh(P_BOX, matMul(base, matMul(matT(0, 0.15f, -7.75f), matS(10, 0.15f, 0.1f))), hexCol(0x8A6A4A));
      break;
    }
    case X_STAGE: {
      r3dMesh(P_DISC, matMul(base, matMul(matT(0, 0.1f, 0), matS(6, 0.1f, 6))), hexCol(0x2A2238));
      static const uint32_t L[4] = {0xFF3B8B, 0x3BD1FF, 0xFFE03B, 0x8B3BFF};
      for (int i = 0; i < 4; i++) {
        float a = t * 0.7f + i * KPI / 2, lx = sinf(a) * 3.5f, lz = cosf(a) * 3.5f;
        Vec top = vec(lx * 0.4f, 9, lz * 0.4f - 2), bot = vec(lx, 0.2f, lz);
        Vec d = vsub(top, bot); float len = sqrtf(vdot(d, d));
        Mat m = matMul(base, matMul(matT((top.x + bot.x) / 2, (top.y + bot.y) / 2, (top.z + bot.z) / 2),
                matMul(matRy(atan2f(d.x, d.z)), matMul(matRx(acosf(d.y / len) * 1.0f), matS(1.3f, len / 2, 1.3f)))));
        r3dMesh(P_CONE, m, hexCol(L[i]), R_EMIT | R_ALPHA | R_NOCULL);
      }
      for (int k = -1; k <= 1; k += 2) r3dMesh(P_BOX, matMul(base, matMul(matT(k * 7.0f, 1.2f, -3), matS(0.9f, 1.2f, 0.8f))), hexCol(0x1A1A1E));
      break;
    }
    case X_ISLAND: r3dMesh(P_DOME, matMul(base, matMul(matT(0, -0.55f, 0), matS(in.s, 0.6f, in.s))), hexCol(0xE8D29A)); break;
    case X_RAYS:
      for (int i = 0; i < 4; i++) {
        float x = (h01(i * 7 + 1) - 0.5f) * 16, z = (h01(i * 7 + 2) - 0.5f) * 12 - 2, a = 0.15f * sinf(t * 0.4f + i);
        r3dMesh(P_BOX, matMul(base, matMul(matT(x, 7, z), matMul(matRz(0.25f + a), matS(0.3f + h01(i) * 0.4f, 9, 0.05f)))), col(0.7f, 0.95f, 1), R_EMIT | R_ALPHA | R_NOCULL);
      }
      break;
    case X_CRATERS:
      for (int i = 0; i < 9; i++) {
        float a = i * 2.39f, r = 3 + i * 1.6f;
        r3dMesh(P_TORUS, matMul(base, matMul(matT(cosf(a) * r, 0.0f, sinf(a) * r), matS(0.8f + h01(i) * 1.2f, 0.25f, 0.8f + h01(i) * 1.2f))), hexCol(0x6E6E6A));
      }
      break;
  }
}
static float popScale(float u) {   // 0..1 -> overshoots a little, settles at 1
  if (u <= 0) return 0;
  if (u >= 1) return 1;
  float c1 = 1.9f, c3 = c1 + 1, v = u - 1;
  return 1 + c3 * v * v * v + c1 * v * v;
}
static void drawInst(const Inst& in, float t, float life, const CPart* cps) {
  if (life <= 0.01f) return;
  float x = in.x, y = in.y, z = in.z, yaw = in.yaw, roll = 0, pitch = 0, sk = 1;
  int beh = in.beh;
  bool moving = beh == B_WALK || beh == B_RUN || beh == B_DRIVE || beh == B_SWIM || beh == B_FLY;
  float k = env.animK, w = t * k + in.phase;
  if (moving) {
    float v = beh == B_WALK ? 0.9f : beh == B_RUN ? 2.4f : beh == B_DRIVE ? 2.6f : beh == B_SWIM ? 0.7f : 1.6f;
    float a = in.phase + w * v / in.orb;
    x = in.cx + cosf(a) * in.orb; z = in.cz + sinf(a) * in.orb;
    yaw = atan2f(-sinf(a), cosf(a));   // along the circle
    if (beh == B_FLY) { y = in.y + 0.35f * sinf(w * 1.7f); roll = -0.25f; }
    if (beh == B_FLY && in.model >= 0 && !strcmp(MODELS[in.model].name, "rocket")) {
      float f = fmodf(w * 0.12f + in.phase, 1.0f); x = in.cx; z = in.cz; y = in.y + f * f * 16; yaw = w * 0.5f; roll = 0;
    }
    if (beh == B_SWIM) y = in.y + 0.15f * sinf(w * 2.2f);
  }
  switch (beh) {
    case B_JUMP: y += fabsf(sinf(w * KPI * 1.4f)) * (0.35f + in.h * 0.3f); break;
    case B_SPIN: yaw += w * 1.4f; break;
    case B_DANCE: yaw += sinf(w * 2.2f) * 0.9f; y += fabsf(sinf(w * 6)) * 0.1f * in.h; break;
    case B_SHAKE: x += sinf(w * 47) * 0.05f; z += sinf(w * 53) * 0.04f; yaw += sinf(w * 31) * 0.06f; break;
    case B_FLOAT: y += 0.25f * sinf(w * 1.3f) * (in.h > 1 ? 1 : in.h + 0.3f); yaw += sinf(w * 0.3f) * 0.6f; break;
    case B_GROW: sk = 1 + 0.15f * sinf(w * 2.6f); break;
    case B_FALL: { float f = fmodf(w * 1.2f + in.phase, 1.0f); y = in.y + 9 - f * 9.5f; roll = w * 2; pitch = w * 1.3f; break; }
    case B_IDLE: sk = 1 + 0.012f * sinf(w * 2.1f); break;
  }
  if (in.sleep) { roll = KPI / 2 * 0.97f; y += in.rad * 0.55f; }
  // pop in, shrink away
  float s = in.s * sk * (life < 1 ? (in.decor ? smooth(life) : popScale(life)) : 1);
  if (s <= 0.001f) return;
  Mat base = matMul(matT(x, y, z), matMul(matRy(yaw), matMul(matRx(pitch), matMul(matRz(roll), matS(s, s, s)))));
  if (in.model < 0) { if (in.model == X_CUSTOM) drawCustom(in, base, cps); else drawDecor(in, base, t); return; }
  if (modelSpecial(in.model) == 1) { drawRainbow(base); return; }
  const Model& m = MODELS[in.model];
  const Part* ps = modelParts(in.model);
  for (int i = 0; i < m.np; i++) {
    const Part& p = ps[i];
    float ax, ay, az, sc, tx;
    partMove(p, in, t, beh, moving, ax, ay, az, sc, tx);
    if (p.anim == A_BOB) tx = 0;
    int fl = p.fl;
    Col c = partCol(in, p.col, fl);
    if (p.anim == A_BLINK && sc < 1) { c = cmul(c, 0.35f); sc = 1; }
    Mat mp = matT(p.px + tx, p.py + (p.anim == A_BOB ? 0.12f * sinf(t * 2 + in.phase) : 0), p.pz);
    if (ax || ay || az) mp = matMul(mp, matMul(matRy(ay), matMul(matRx(ax), matRz(az))));
    if (sc != 1) mp = matMul(mp, matS(sc, sc, sc));
    if (p.rx || p.ry || p.rz) mp = matMul(mp, matMul(matRy(p.ry), matMul(matRx(p.rx), matRz(p.rz))));
    mp = matMul(mp, matMul(matT(p.ox, p.oy, p.oz), matS(p.sx, p.sy, p.sz)));
    r3dMesh(p.prim, matMul(base, mp), c, fl);
  }
}

// ------------------------------------------------------------------ the ground
#define GN 18
#define GC 3.0f
static float groundH(int g, float x, float z, float t) {
  float r = sqrtf(x * x + z * z), far = smooth((r - 6) / 8);
  float n = sinf(x * 0.31f + 1.3f) * cosf(z * 0.27f - 0.6f) + 0.5f * sinf(x * 0.63f - z * 0.51f + 2.1f);
  switch (g) {
    case G_NONE: return -40;
    case G_GRASS: return n * 0.7f * far;
    case G_SAND: return (sinf(x * 0.22f + z * 0.12f) * 1.1f + n * 0.4f) * far;
    case G_SNOW: return n * 0.8f * far;
    case G_WATER: return -0.15f + 0.16f * sinf(x * 0.6f + t * 1.4f) + 0.11f * sinf(z * 0.75f + t * 1.1f + x * 0.3f);
    case G_BEACH: return z > 4.5f ? -0.3f + 0.12f * sinf(x * 0.6f + t * 1.4f) + 0.08f * sinf(z * 0.8f + t * 1.2f) : n * 0.2f * far - (z > 2 ? (z - 2) * 0.1f : 0);
    case G_ROCKY: return (n * 1.2f + sinf(x * 1.1f) * 0.3f) * far;
    case G_MOON: return (n * 0.6f + 0.3f * sinf(x * 1.3f + z)) * far;
    case G_LAVA: return n * 0.5f * far;
    case G_CLOUD: return -0.2f + 0.35f * (sinf(x * 0.9f + t * 0.2f) * cosf(z * 0.8f) + 0.6f * sinf(x * 1.7f + z * 1.3f));
    case G_SEAFLOOR: return n * 0.6f * far + 0.15f * sinf(x * 1.4f) * cosf(z * 1.1f);
    case G_VOID: return -0.02f;
  }
  return 0;
}
static Col groundCol(const Env& e, int g, int i, int j, float x, float z, int& fl) {
  float v = h01(i * 131 + j * 7919);
  Col c = clerp(e.g1, e.g2, v);
  switch (g) {
    case G_CITY: {
      bool road = abs(i - GN / 2) == 0 || abs(j - GN / 2) == 0 || abs(i - GN / 2) == 1 || abs(j - GN / 2) == 1;
      bool road1 = (abs(i - GN / 2) <= 1 && abs(j - GN / 2) > 1) || (abs(j - GN / 2) <= 1 && abs(i - GN / 2) > 1);
      c = road ? cmul(e.g1, 0.9f + v * 0.15f) : clerp(e.g2, e.g1, v * 0.3f);
      if (road1 && ((i + j) & 1) && (abs(i - GN / 2) == 0 || abs(j - GN / 2) == 0)) c = clerp(c, col(0.9f, 0.9f, 0.85f), 0.25f);
      break;
    }
    case G_FLOOR: c = clerp(e.g1, e.g2, (i & 1) ? 0.8f : 0.1f + v * 0.2f); break;
    case G_LAVA: if (v > 0.9f) { fl |= R_EMIT; c = clerp(col(0.85f, 0.25f, 0.04f), col(1, 0.55f, 0.1f), (v - 0.9f) * 10); } break;
    case G_BEACH: if (z > 4.5f) c = clerp(hexCol(0x2F8FC9), hexCol(0x2577AA), v); else c = clerp(e.g1, hexCol(0xDCC080), v * 0.6f); break;
    case G_CLOUD: fl |= R_SOFT; break;
    case G_STAGE: c = clerp(e.g1, e.g2, ((i + j) & 1) ? 1.0f : 0.0f); break;
    case G_WATER: c = clerp(e.g1, e.g2, v * 0.5f); break;
  }
  return c;
}
static void drawGround(float t, float tr) {
  int ga = envA.ground, gb = envB.ground;
  if (ga == G_NONE && gb == G_NONE) return;
  float e = smooth(tr);
  static Vec P[GN + 1][GN + 1];
  float o = -GN * GC / 2;
  for (int j = 0; j <= GN; j++) for (int i = 0; i <= GN; i++) {
    float x = o + i * GC, z = o + j * GC;
    float h = ga == gb ? groundH(gb, x, z, t) : groundH(ga, x, z, t) * (1 - e) + groundH(gb, x, z, t) * e;
    P[j][i] = vec(x, h, z);
  }
  for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) {
    float cx = o + (i + 0.5f) * GC, cz = o + (j + 0.5f) * GC;
    int fa = 0, fb = 0;
    Col c = ga == gb || e >= 1 ? groundCol(env, gb, i, j, cx, cz, fb) : clerp(groundCol(env, ga, i, j, cx, cz, fa), groundCol(env, gb, i, j, cx, cz, fb), e);
    int fl = e < 0.5f ? fa : fb;
    if (gb == G_NONE && e > 0.5f) fl = fa;
    // the two triangles, the diagonal alternating (looks less like a grid)
    if ((i + j) & 1) { r3dTri(P[j][i], P[j + 1][i], P[j + 1][i + 1], c, fl); r3dTri(P[j][i], P[j + 1][i + 1], P[j][i + 1], cmul(c, 0.97f), fl); }
    else { r3dTri(P[j][i], P[j + 1][i], P[j][i + 1], c, fl); r3dTri(P[j + 1][i], P[j + 1][i + 1], P[j][i + 1], cmul(c, 0.97f), fl); }
  }
  // the neon grid of the void
  if (gb == G_VOID && e > 0.3f) {
    Col g = c565toCol(C_FG);
    int a = (int)(14 * smooth((e - 0.3f) / 0.7f));
    for (int k = -9; k <= 9; k++) {
      r3dStreak(vec(k * 3.0f, 0, -27), vec(k * 3.0f, 0, 27), g, a);
      r3dStreak(vec(-27, 0, k * 3.0f), vec(27, 0, k * 3.0f), g, a);
    }
  }
}

// ------------------------------------------------------------------ sky, weather, effects
static void drawSky(float t) {
  r3dSkyRows(env.top, env.hor, env.below);
  if (env.sunDisc > 0.05f) r3dSkyDisc(env.sun, 9, clerp(env.hor, col(1, 0.97f, 0.85f), clampf(env.sunDisc, 0, 1)), clampf(env.sunDisc, 0, 1.2f));
  if (env.moonDisc > 0.05f) r3dSkyDisc(vnorm(vec(0.5f, 0.55f, -0.7f)), 7, clerp(env.hor, col(0.92f, 0.92f, 0.85f), env.moonDisc), 0.5f * env.moonDisc);
  if (env.stars > 0.02f) {
    int n = (int)(170 * env.stars);
    for (int i = 0; i < n; i++) {
      float a = h01(i * 3 + 1) * 2 * KPI, el = asinf(0.04f + 0.96f * h01(i * 3 + 2));
      Vec d = vec(cosf(el) * cosf(a), sinf(el), cosf(el) * sinf(a));
      float tw = 0.6f + 0.4f * sinf(t * (1.5f + h01(i) * 3) + i);
      int al = (int)(32 * env.stars * tw * (0.4f + 0.6f * h01(i * 5)));
      r3dSkyDot(d, i % 7 == 0 ? col(0.8f, 0.85f, 1) : col(1, 1, 0.92f), al > 32 ? 32 : al);
    }
  }
}
static float flashAt = -100, flashNext = 3;
static void drawWeather(float t, Vec ctr) {
  if (env.rain > 0.05f) {
    int n = (int)(110 * clampf(env.rain, 0, 1.6f));
    for (int i = 0; i < n; i++) {
      float x = ctr.x + (h01(i * 5 + 1) - 0.5f) * 22, z = ctr.z + (h01(i * 5 + 2) - 0.5f) * 22;
      float y = 11 - fmodf(t * 11 + h01(i * 5 + 3) * 11, 11);
      r3dStreak(vec(x, y, z), vec(x - env.wind * 0.25f, y + 0.7f, z), col(0.75f, 0.8f, 0.9f), 13);
    }
  }
  if (env.snow > 0.05f) {
    int n = (int)(140 * env.snow);
    for (int i = 0; i < n; i++) {
      float x = ctr.x + (h01(i * 5 + 1) - 0.5f) * 22 + sinf(t * 0.7f + i) * 0.6f, z = ctr.z + (h01(i * 5 + 2) - 0.5f) * 22;
      float y = 10 - fmodf(t * 1.1f + h01(i * 5 + 3) * 10, 10);
      r3dSprite(vec(x, y, z), 0.05f, col(1, 1, 1), SP_DISC, 28);
    }
  }
  if (env.storm > 0.3f) {
    if (t > flashNext) { flashAt = t; flashNext = t + 3 + h01((uint32_t)(t * 10)) * 6; }
    float f = t - flashAt;
    r3dFlash = f < 0.5f ? (f < 0.08f || (f > 0.16f && f < 0.24f) ? 0.9f : 0.25f) * (1 - f * 1.6f) : 0;
    if (f < 0.25f) {
      Vec p = vec(ctr.x + (h01((uint32_t)flashAt * 7) - 0.5f) * 16, 12, ctr.z - 6 - h01((uint32_t)flashAt * 3) * 6);
      for (int i = 0; i < 6; i++) {
        Vec q = vec(p.x + (h01((uint32_t)flashAt + i * 11) - 0.5f) * 2.5f, p.y - 2.1f, p.z);
        r3dStreak(p, q, col(1, 1, 1), 32);
        p = q;
      }
    }
  } else r3dFlash = 0;
  if (env.wind > 0.3f) {
    for (int i = 0; i < 24; i++) {
      float u = fmodf(t * 0.25f + h01(i * 9), 1.0f);
      float x = ctr.x - 12 + u * 24, z = ctr.z + (h01(i * 9 + 1) - 0.5f) * 16, y = 0.5f + h01(i * 9 + 2) * 4 + sinf(t * 3 + i) * 0.4f;
      r3dSprite(vec(x, y, z), 0.09f, i & 1 ? hexCol(0x8BAA3C) : hexCol(0xC88A2E), SP_SQUARE, 30);
    }
  }
}
// effects: around the middle (the scene's fx), or from a thing (smoke, embers, zzz)
static void fxAt(int fx, float t, Vec c, float spread, int n, float seed) {
  for (int i = 0; i < n; i++) {
    uint32_t s = (uint32_t)(i * 13 + seed * 1000);
    float hx = h01(s + 1) - 0.5f, hz = h01(s + 2) - 0.5f, hy = h01(s + 3);
    switch (fx) {
      case FX_HEARTS: { float u = fmodf(t * 0.22f + hy, 1.0f); Vec p = vec(c.x + hx * spread, c.y + u * 4.5f, c.z + hz * spread);
        r3dSprite(p, 0.22f + 0.1f * sinf(t * 3 + i), i % 3 ? col(1, 0.3f, 0.45f) : col(1, 0.6f, 0.75f), SP_HEART, (int)(32 * (1 - u * u))); break; }
      case FX_SPARKLES: { float tw = sinf(t * 4.5f + i * 2.1f); if (tw < 0) break; Vec p = vec(c.x + hx * spread * 1.4f, c.y + hy * 4, c.z + hz * spread * 1.4f);
        r3dSprite(p, 0.16f + 0.12f * tw, i & 1 ? col(1, 0.92f, 0.5f) : col(1, 1, 1), SP_SPARK, (int)(32 * tw)); break; }
      case FX_FIRE: { float u = fmodf(t * 0.7f + hy, 1.0f); Vec p = vec(c.x + hx * spread * (1 - u * 0.5f) + sinf(t * 3 + i) * 0.1f, c.y + u * 2.8f, c.z + hz * spread * (1 - u * 0.5f));
        r3dSprite(p, 0.05f + 0.04f * (1 - u), clerp(col(1, 0.85f, 0.3f), col(1, 0.3f, 0.05f), u), SP_SQUARE, (int)(32 * (1 - u))); break; }
      case FX_BUBBLES: { float u = fmodf(t * 0.18f + hy, 1.0f); Vec p = vec(c.x + hx * spread + sinf(t * 2 + i) * 0.15f, c.y + u * 6, c.z + hz * spread);
        r3dSprite(p, 0.07f + 0.08f * h01(s + 4), col(0.85f, 0.95f, 1), SP_RING, 26); break; }
      case FX_MUSIC: { float u = fmodf(t * 0.2f + hy, 1.0f); Vec p = vec(c.x + hx * spread + sinf(t * 1.5f + i) * 0.5f, c.y + 0.5f + u * 4, c.z + hz * spread);
        Col nc = i % 3 == 0 ? c565toCol(C_FG) : i % 3 == 1 ? col(1, 1, 1) : col(1, 0.85f, 0.3f);
        r3dSprite(p, 0.24f, nc, SP_NOTE, (int)(32 * sinf(u * KPI))); break; }
      case FX_CONFETTI: { float u = fmodf(t * 0.16f + hy, 1.0f); Vec p = vec(c.x + hx * spread * 1.5f + sinf(t * 2 + i) * 0.3f, c.y + 7 - u * 7.5f, c.z + hz * spread * 1.5f);
        static const uint32_t CF[5] = {0xFF3B8B, 0x3BD1FF, 0xFFE03B, 0x6BFF3B, 0xFF8A3B};
        if (sinf(t * 8 + i) > -0.6f) r3dSprite(p, 0.07f, hexCol(CF[i % 5]), SP_SQUARE, 32); break; }
      case FX_LEAVES: { float u = fmodf(t * 0.1f + hy, 1.0f); Vec p = vec(c.x + hx * spread * 1.5f + sinf(t * 1.3f + i) * 0.8f, c.y + 6 - u * 6.3f, c.z + hz * spread * 1.5f);
        r3dSprite(p, 0.1f, i & 1 ? hexCol(0xD9822B) : hexCol(0xB5442B), SP_SQUARE, 30); break; }
      case FX_ZZZ: { float u = fmodf(t * 0.3f + i / (float)n, 1.0f); Vec p = vec(c.x + u * 0.8f + sinf(t * 2 + i) * 0.15f, c.y + u * 2.2f, c.z);
        r3dSprite(p, 0.12f + u * 0.25f, col(1, 1, 1), SP_Z, (int)(32 * sinf(u * KPI))); break; }
      case FX_QUESTIONS: { float u = fmodf(t * 0.15f + hy, 1.0f); Vec p = vec(c.x + hx * spread, c.y + 1 + u * 3.5f, c.z + hz * spread);
        r3dSprite(p, 0.3f, col(1, 0.85f, 0.3f), SP_QUEST, (int)(32 * sinf(u * KPI))); break; }
      case FX_MONEY: { float u = fmodf(t * 0.25f + hy, 1.0f); Vec p = vec(c.x + hx * spread * 1.4f, c.y + 7 - u * 7.3f, c.z + hz * spread * 1.4f);
        r3dSprite(p, 0.12f, sinf(t * 6 + i) > 0 ? col(1, 0.85f, 0.25f) : col(0.85f, 0.65f, 0.15f), SP_DISC, 32); break; }
      case FX_STARS: { float tw = 0.5f + 0.5f * sinf(t * 3 + i * 1.7f); Vec p = vec(c.x + hx * spread * 2, c.y + 2 + hy * 6, c.z + hz * spread * 2);
        r3dSprite(p, 0.15f + 0.1f * tw, col(1, 0.95f, 0.6f), SP_STAR, (int)(32 * tw));
        if (i == 0) { float u = fmodf(t * 0.35f, 1.0f); if (u < 0.3f) { Vec a = vec(c.x - 10 + u * 50, c.y + 10 - u * 12, c.z - 12); r3dStreak(a, vec(a.x - 1.8f, a.y + 0.45f, a.z), col(1, 1, 0.9f), 30); } }
        break; }
      case FX_SMOKE: { float u = fmodf(t * 0.18f + hy, 1.0f); Vec p = vec(c.x + hx * 0.4f + u * 1.2f * (0.4f + env.wind), c.y + u * 4.5f, c.z + hz * 0.4f);
        r3dSprite(p, 0.12f + u * 0.5f, env.night > 0.5f ? col(0.35f, 0.35f, 0.4f) : col(0.75f, 0.75f, 0.75f), SP_DISC, (int)(20 * (1 - u))); break; }
    }
  }
}
struct Emit { int fx; Vec p; float spread; int n; };
static Emit emits[16]; static int nEmit = 0;

// ------------------------------------------------------------------ building a scene's things
static uint32_t pickCol(uint32_t want, uint32_t own) { return want == NOCOL ? own : want; }
static float unitS = 3.8f;
static void slotPos(int at, int k, int total, float& x, float& z) {
  float u = unitS;
  switch (at) {
    case AT_LEFT: x = -u; z = -0.15f * u; break;
    case AT_RIGHT: x = u; z = -0.15f * u; break;
    case AT_BACK: x = 0; z = -1.25f * u; break;
    case AT_FRONT: x = 0.1f * u; z = 0.75f * u; break;
    case AT_AROUND: { float a = 2 * KPI * k / (total > 0 ? total : 1) + 0.3f, r = u * 1.15f + (total > 4 ? (total - 4) * 0.3f * u / 3 : 0); x = cosf(a) * r; z = sinf(a) * r; break; }
    case AT_SKY: x = (k % 3 - 1) * 5.5f; z = -5 - (k / 3) * 3; break;
    case AT_FAR: { float a = -KPI / 2 + (k % 2 ? 1 : -1) * (0.35f + (k / 2) * 0.5f) * (k ? 1 : 0); x = cosf(a) * 17; z = sinf(a) * 17; break; }
    default: x = 0; z = 0;
  }
}
static int addInst(Inst* list, int& n, int model, float x, float y, float z, float s, uint32_t c1, uint32_t c2, int beh, float born, int decor, uint32_t seed) {
  if (n >= MAXI) return -1;
  Inst& in = list[n++];
  memset(&in, 0, sizeof(in));
  in.model = model; in.x = in.cx = x; in.y = y; in.z = in.cz = z; in.s = s; in.beh = beh; in.born = born; in.decor = decor;
  in.phase = h01(seed) * 6.28f;
  in.yaw = (h01(seed + 7) - 0.5f) * 1.2f;
  in.c1 = hexCol(c1); in.c2 = hexCol(c2);
  in.skin = hexCol(SKINS[hsh(seed + 3) % 5]); in.hair = hexCol(HAIRS[hsh(seed + 5) % 5]);
  if (model >= 0) { in.rad = MODELS[model].rad * s; in.h = MODELS[model].h * s; } else { in.rad = 2; in.h = 1; }
  in.orb = 1.4f + in.rad;
  return n - 1;
}
static void decorFor(const SceneDef& d, Inst* list, int& n, float born, float camR, bool low) {
  struct Dec { const char* m; int count; float r0, r1, s0, s1; uint32_t c; };
  Dec D[6]; int nd = 0;
  auto dec = [&](const char* m, int c, float r0, float r1, float s0, float s1, uint32_t col = NOCOL) { if (nd < 6) D[nd++] = {m, c, r0, r1, s0, s1, col}; };
  switch (d.place) {
    case PL_GRASS: dec("tree", 4, 11, 17, 0.8f, 1.3f); dec("flower", 7, 4, 9, 0.9f, 1.4f); dec("bush", 3, 8, 13, 0.8f, 1.2f); dec("grass", 6, 3, 10, 1, 1.5f); break;
    case PL_FOREST: dec("tree", 8, 8, 16, 0.9f, 1.5f); dec("pine", 6, 9, 17, 1.0f, 1.6f); dec("mushroom", 3, 4, 8, 0.6f, 1.0f); dec("bush", 3, 6, 10, 0.8f, 1.2f); break;
    case PL_JUNGLE: dec("palm", 6, 7, 15, 1.0f, 1.5f); dec("tree", 6, 8, 16, 1.1f, 1.6f, 0x2F7D2A); dec("bush", 5, 5, 11, 1.0f, 1.6f, 0x2F8E3A); dec("flower", 4, 4, 8, 1.2f, 1.8f, 0xFF4F1A); break;
    case PL_DESERT: dec("cactus", 4, 6, 14, 0.8f, 1.3f); dec("rock", 3, 6, 13, 0.7f, 1.4f, 0xB08A5A); dec("pyramid", 1, 17, 20, 2.5f, 3.0f); break;
    case PL_BEACH: dec("palm", 3, 6, 10, 1.0f, 1.3f); dec("rock", 2, 7, 11, 0.5f, 0.9f); break;
    case PL_SNOW: dec("pine", 9, 8, 17, 0.9f, 1.5f, 0x2E5B4A); dec("rock", 2, 7, 12, 0.6f, 1.0f, 0xC8D2DC); break;
    case PL_MOUNTAINS: dec("mountain", 4, 19, 23, 1.0f, 1.6f); dec("pine", 6, 8, 15, 0.9f, 1.4f); dec("rock", 3, 6, 11, 0.6f, 1.2f); break;
    case PL_CITY: dec("tower", 10, 11, 17, 0.6f, 1.25f); dec("house", 2, 9, 12, 0.9f, 1.0f, 0xC9C3B5); break;
    case PL_SPACE: dec("planet", 2, 20, 24, 1.8f, 3.0f, 0x7A5AD9); dec("star", 3, 9, 14, 0.5f, 0.8f); break;
    case PL_MOON: dec("rock", 4, 6, 14, 0.6f, 1.3f, 0x8A8A86); dec("globe", 1, 22, 22, 3.0f, 3.0f); break;
    case PL_UNDERWATER: dec("rock", 4, 6, 13, 0.7f, 1.4f, 0x6E7E86); dec("grass", 8, 4, 12, 2.0f, 3.2f, 0x2E9A5A); break;
    case PL_CLOUDS: dec("cloud", 6, 9, 15, 1.2f, 2.0f); break;
    case PL_LAVA: dec("volcano", 1, 18, 18, 1.4f, 1.4f); dec("rock", 5, 6, 13, 0.8f, 1.5f, 0x2A2220); break;
    default: break;
  }
  uint32_t seed = (uint32_t)(d.place * 977 + d.time * 31 + d.weather * 7);
  for (int i = 0; i < nd; i++) {
    int m = modelFind(D[i].m);
    if (m < 0) continue;
    for (int k = 0; k < D[i].count; k++) {
      uint32_t s = seed + i * 101 + k * 17;
      float r0 = D[i].r0, r1 = D[i].r1;
      bool tall = MODELS[m].h * D[i].s1 > 1.2f || low;
      bool y0sky = !strcmp(D[i].m, "planet") || !strcmp(D[i].m, "globe");
      float need = MODELS[m].kind == K_SKY || y0sky ? camR * 1.7f : camR * 1.45f + 1;
      if ((tall || y0sky) && r0 < need) { float k2 = need - r0; r0 += k2; r1 += k2; }
      float a = (k + h01(s) * 0.7f) / D[i].count * 2 * KPI + i, r = r0 + (r1 - r0) * h01(s + 1);
      float sc = D[i].s0 + (D[i].s1 - D[i].s0) * h01(s + 2);
      if (!strcmp(D[i].m, "globe")) { a = -2.2f; }
      float y = MODELS[m].kind == K_SKY || !strcmp(D[i].m, "planet") || !strcmp(D[i].m, "globe") ? 7 + h01(s + 3) * 4 : 0;
      if (!strcmp(D[i].m, "cloud")) y = -0.6f;
      if (!strcmp(D[i].m, "star")) y = 4 + h01(s + 3) * 4;
      if (d.place == PL_BEACH && !strcmp(D[i].m, "palm") && sinf(a) * r > 3) a += KPI;
      int j = addInst(list, n, m, cosf(a) * r, y, sinf(a) * r, sc, pickCol(D[i].c, MODELS[m].c1), MODELS[m].c2, MODELS[m].anim == B_FLOAT || MODELS[m].anim == B_SPIN ? MODELS[m].anim : B_IDLE,
                      born + 0.15f + h01(s + 4) * 0.8f, 1, s);
      if (j >= 0) list[j].yaw = h01(s + 5) * 6.28f;
    }
  }
  float b = born + 0.1f;
  if (d.place == PL_ROOM) addInst(list, n, X_ROOM, 0, 0, 0, 1, 0, 0, B_IDLE, b, 1, 1);
  if (d.place == PL_STAGE) addInst(list, n, X_STAGE, 0, 0, 0, 1, 0, 0, B_IDLE, b, 1, 2);
  if (d.place == PL_UNDERWATER) addInst(list, n, X_RAYS, 0, 0, 0, 1, 0, 0, B_IDLE, b, 1, 3);
  if (d.place == PL_MOON) addInst(list, n, X_CRATERS, 0, 0, 0, 1, 0, 0, B_IDLE, b, 1, 4);
}
static Cam camFor(const SceneDef& d);
static void build(const SceneDef& d, float born) {
  nCur = 0; nEmit = 0;
  memcpy(curParts, d.cp, sizeof(curParts));
  bool space = d.place == PL_SPACE, sea = d.place == PL_SEA, water = sea || d.place == PL_UNDERWATER;
  int groundN = 0, slotUsed[AT_N] = {0};
  static const int AUTO[] = {AT_CENTER, AT_RIGHT, AT_LEFT, AT_BACK, AT_FRONT, AT_AROUND, AT_AROUND, AT_AROUND};
  int autoK = 0, idx = 0;
  // the space between slots: from the biggest thing that stands
  float big = 0.4f;
  for (int t = 0; t < d.nth; t++) {
    int m = d.th[t].model;
    if (m >= 0 && (MODELS[m].kind == K_FAR || MODELS[m].kind == K_SKY)) continue;
    float r = (m >= 0 ? MODELS[m].rad : 1.2f) * (d.th[t].s > 0 ? clampf(d.th[t].s, 0.2f, 4) : 1);
    if (m >= 0 && MODELS[m].h < 0.95f) r *= fminf(2.0f, 0.95f / MODELS[m].h);
    if (r > big) big = r;
  }
  unitS = clampf(big * 2.2f + 1.3f, 2.0f, 6.0f);
  for (int t = 0; t < d.nth; t++) {
    const Thing& th = d.th[t];
    int m = th.model;
    const Model* md = m >= 0 ? &MODELS[m] : nullptr;
    int kind = md ? md->kind : K_GROUND;
    int n = th.n < 1 ? 1 : th.n > 8 ? 8 : th.n;
    if (kind == K_FAR || kind == K_SKY) n = n > 3 ? 3 : n;
    float s = th.s > 0 ? clampf(th.s, 0.2f, 4) : 1;
    float h = md ? md->h : th.custH > 0 ? th.custH : 1;
    if (md && h < 0.95f && kind != K_FAR) s *= fminf(2.0f, 0.95f / h);          // small things a little bigger: it's a dream
    int beh = th.anim;
    if (md && beh == B_IDLE && md->anim != B_IDLE) beh = md->anim;
    if (md && vehicle(md->name) && (beh == B_WALK || beh == B_RUN)) beh = B_DRIVE;
    if (md && kind == K_WATER && !water && beh == B_SWIM) beh = B_FLOAT;
    int at = th.at;
    if (kind == K_FAR && at != AT_AROUND) at = AT_FAR;
    if (kind == K_SKY && at != AT_AROUND && at != AT_FAR) at = AT_SKY;
    if (at >= AT_N) {
      at = kind == K_SKY ? AT_SKY : kind == K_FAR ? AT_FAR : AUTO[autoK < 8 ? autoK++ : 7];
    }
    for (int k = 0; k < n; k++) {
      float x, z, y = 0;
      int sk = at == AT_AROUND ? k : slotUsed[at];
      slotPos(at, sk, at == AT_AROUND ? n : 1, x, z);
      if (at != AT_AROUND && k > 0) {   // copies around their slot
        float sp = (md ? md->rad : 0.8f) * s * 2.3f + 0.4f, a = k * 2.4f, r = sp * sqrtf((float)k);
        x += cosf(a) * r; z += sinf(a) * r * 0.8f;
      }
      if (kind == K_AIR || (space && kind != K_FAR)) y = 1.3f + (md ? md->h : 1) * s * 0.3f + (k & 1) * 0.7f;
      if (kind == K_SKY || at == AT_SKY) { y = 6.5f + (k % 3) * 1.2f; if (at != AT_SKY) z -= 4; }
      if (md && !strcmp(md->name, "plane")) y = 5.5f + k;
      if (beh == B_FLY && y < 2) y = 2.2f + (md ? md->h * s * 0.3f : 0);
      if (water && kind != K_SKY && kind != K_AIR && d.place == PL_UNDERWATER) y = 0.6f + k * 0.2f;
      uint32_t seed = (uint32_t)(t * 7919 + k * 131 + d.title[0] * 31 + d.title[1]);
      uint32_t c1 = md ? pickCol(th.c, md->c1) : 0xFFFFFF, c2 = md ? pickCol(th.c2, md->c2) : 0xFFFFFF;
      if (md && n > 1 && th.c == NOCOL && (!strcmp(md->name, "person") || !strcmp(md->name, "child"))) c1 = SHIRTS[(t * 3 + k) % 8];
      int j = addInst(cur, nCur, m >= 0 ? m : X_CUSTOM, x, y, z, s, c1, c2, beh == B_SLEEP ? B_IDLE : beh, born + 0.45f + idx * 0.13f, 0, seed);
      idx++;
      if (j < 0) continue;
      Inst& in = cur[j];
      if (m < 0) { in.cp0 = th.cp0; in.ncp = th.ncp; in.rad = 1.2f * s; in.h = h * s; }
      in.sleep = beh == B_SLEEP && (m < 0 || (MODELS[m].kind == K_GROUND && living(m)));
      // facing the camera (+z) more or less
      if (at == AT_AROUND) in.yaw = atan2f(-x, -z) + KPI;
      else in.yaw = (h01(seed + 9) - 0.5f) * 1.4f;
      if (kind == K_GROUND || kind == K_WATER) groundN++;
      if (beh == B_SLEEP && nEmit < 16) emits[nEmit++] = {FX_ZZZ, vec(x, y + in.h * 0.6f + 0.4f, z), 0, 3};
      // things that smoke or burn
      if (md && nEmit < 16) {
        const char* nm = md->name;
        if (!strcmp(nm, "fire")) { emits[nEmit++] = {FX_FIRE, vec(x, 0.4f * s, z), 0.6f * s, 16}; }
        else if (!strcmp(nm, "volcano")) { emits[nEmit++] = {FX_SMOKE, vec(x, 4.6f * s, z), 1, 12}; }
        else if (!strcmp(nm, "factory")) { emits[nEmit++] = {FX_SMOKE, vec(x + 1.4f * s, 4.3f * s, z - 0.8f * s), 1, 10}; }
        else if (!strcmp(nm, "cup") || !strcmp(nm, "drink")) { emits[nEmit++] = {FX_SMOKE, vec(x, 0.7f * s, z), 0.3f, 5}; }
        else if (!strcmp(nm, "train") && beh == B_IDLE) { emits[nEmit++] = {FX_SMOKE, vec(x, 3.0f * s, z + 1.5f * s), 1, 8}; }
        else if (!strcmp(nm, "dragon")) { emits[nEmit++] = {FX_FIRE, vec(x, 2.4f * s, z + 1.6f * s), 0.4f, 10}; }
      }
    }
    if (at < AT_N) slotUsed[at]++;
  }
  // ground things don't stand inside each other
  for (int it = 0; it < 10; it++)
    for (int i = 0; i < nCur; i++) for (int j = i + 1; j < nCur; j++) {
      Inst& a = cur[i]; Inst& b = cur[j];
      if (a.decor || b.decor || a.model < -1 || b.model < -1 || fabsf(a.y - b.y) > 1.5f) continue;
      float dx = b.x - a.x, dz = b.z - a.z, dd = sqrtf(dx * dx + dz * dz), need = (a.rad + b.rad) * 0.95f;
      if (dd < need) {
        if (dd < 0.01f) { dx = 1; dz = 0.3f; dd = 1.04f; }
        float push = (need - dd) * 0.5f / dd;
        a.x -= dx * push; a.z -= dz * push; b.x += dx * push; b.z += dz * push;
      }
    }
  for (int i = 0; i < nCur; i++) { cur[i].cx = cur[i].x; cur[i].cz = cur[i].z; }
  // in the sea, what stands gets a little island of its own
  if (sea) for (int i = 0, n0 = nCur; i < n0; i++) {
    const Inst& in = cur[i];
    if (in.decor || in.y > 1 || (in.model >= 0 && MODELS[in.model].kind != K_GROUND) || in.model < -1) continue;
    bool mv = in.beh == B_WALK || in.beh == B_RUN || in.beh == B_DRIVE;
    addInst(cur, nCur, X_ISLAND, in.x, 0, in.z, in.rad * 1.2f + 1.0f + (mv ? in.orb : 0), 0, 0, B_IDLE, born + 0.25f, 1, 9 + i);
  }
  // how big it all is, and where its middle is (for the camera)
  float mnx = 1e9, mxx = -1e9, mnz = 1e9, mxz = -1e9, mny = 1e9, mxy = -1e9;
  for (int pass = 0; pass < 2 && mnx > mxx; pass++)
    for (int i = 0; i < nCur; i++) {
      const Inst& in = cur[i];
      if (in.decor || in.model < -1) continue;
      int kd = in.model >= 0 ? MODELS[in.model].kind : K_GROUND;
      if (kd == K_FAR || (kd == K_SKY && !pass)) continue;
      bool mv = in.beh == B_WALK || in.beh == B_RUN || in.beh == B_DRIVE || in.beh == B_FLY || in.beh == B_SWIM;
      float r = in.rad + (mv ? in.orb : 0);
      mnx = fminf(mnx, in.x - r); mxx = fmaxf(mxx, in.x + r); mnz = fminf(mnz, in.z - r); mxz = fmaxf(mxz, in.z + r);
      mny = fminf(mny, in.y); mxy = fmaxf(mxy, in.y + in.h);
    }
  if (mnx > mxx) { mnx = mnz = -2; mxx = mxz = 2; mny = 0; mxy = 1.5f; }
  ctrX = (mnx + mxx) / 2; ctrZ = (mnz + mxz) / 2; ctrY = (mny + mxy) / 2;
  bounds = fmaxf(mxx - mnx, mxz - mnz) / 2; heightMax = mxy - mny;
  if (d.fx != FX_NONE && nEmit < 16) emits[nEmit++] = {d.fx, vec(0, 0.2f, 0), bounds * 1.2f + 2, d.fx == FX_CONFETTI ? 50 : d.fx == FX_SPARKLES ? 30 : 16};
  if (d.place == PL_UNDERWATER && nEmit < 16) emits[nEmit++] = {FX_BUBBLES, vec(0, 0, 0), 12, 24};
  Cam c = camFor(d);
  decorFor(d, cur, nCur, born, c.R + sqrtf(c.tx * c.tx + c.tz * c.tz), d.cam == CAM_LOW);
}
static Cam camFor(const SceneDef& d) {
  Cam c;
  float R = clampf(2.4f + bounds * 1.75f + heightMax * 1.15f, 3.8f, 22);
  c.R = R; c.ty = clampf(ctrY, 0.5f, 12); c.H = c.ty + R * 0.28f;
  c.speed = 0.11f; c.arc = 0; c.tx = ctrX * 0.8f; c.tz = ctrZ * 0.8f;
  switch (d.cam) {
    case CAM_CLOSE: c.R = clampf(R * 0.72f, 3.2f, 12); c.H = c.ty + c.R * 0.18f; break;
    case CAM_WIDE: c.R = R * 1.45f; c.H = c.ty + c.R * 0.42f; break;
    case CAM_TOP: c.R = R * 0.45f; c.H = c.ty + R * 1.15f; break;
    case CAM_LOW: c.R = R * 0.85f; c.H = 0.45f; c.ty += 0.9f; break;
    case CAM_FLY: c.R = R * 1.15f; c.H = c.ty + 1.2f; c.speed = 0.3f; break;
  }
  if (d.place == PL_ROOM) { c.arc = 0.75f; c.R = clampf(c.R, 6, 11); c.H = clampf(c.H, 2, 6); }
  if (d.mood == MO_CALM || d.mood == MO_SAD) c.speed *= 0.6f;
  if (d.mood == MO_EXCITING || d.mood == MO_FUNNY) c.speed *= 1.6f;
  return c;
}

// ------------------------------------------------------------------ showing one
static bool inited = false;
static void initOnce() {
  if (inited) return;
  inited = true;
  r3dInit(); modelsInit();
  cur = (Inst*)calloc(MAXI, sizeof(Inst)); old = (Inst*)calloc(MAXI, sizeof(Inst));
}
void sceneShow(const SceneDef& d, bool instant) {
  initOnce();
  float t = now();
  float tr = sceneTransition();
  // the old things go (as they are now)
  Env from = mixEnv(envA, envB, smooth(tr));
  Cam ca = camA, cb = camB; float ce = smooth((t - tShow) / 2.6f);
  Cam cnow = {ca.R + (cb.R - ca.R) * ce, ca.H + (cb.H - ca.H) * ce, ca.ty + (cb.ty - ca.ty) * ce, ca.speed + (cb.speed - ca.speed) * ce, ca.arc + (cb.arc - ca.arc) * ce,
              ca.tx + (cb.tx - ca.tx) * ce, ca.tz + (cb.tz - ca.tz) * ce};
  memcpy(old, cur, sizeof(Inst) * nCur); nOld = nCur; memcpy(oldParts, curParts, sizeof(oldParts));
  tOldGone = t;
  nowDef = d;
  build(d, instant ? t - 10 : t);
  envA = instant ? makeEnv(d) : from; envB = makeEnv(d);
  camB = camFor(d); camA = instant ? camB : cnow;
  if (instant) { nOld = 0; tShow = t - 10; } else tShow = t;
}
void sceneDraw(uint16_t* out, float dt) {
  initOnce();
  clock0 += dt > 0.25f ? 0.25f : dt;
  float t = now();
  float tr = sceneTransition(), e = smooth(tr);
  env = mixEnv(envA, envB, e);
  // the camera
  float ce = smooth((t - tShow) / 2.6f);
  float R = camA.R + (camB.R - camA.R) * ce, H = camA.H + (camB.H - camA.H) * ce, ty = camA.ty + (camB.ty - camA.ty) * ce;
  float sp = (camA.speed + (camB.speed - camA.speed) * ce) * camSpeedK, arc = camA.arc + (camB.arc - camA.arc) * ce;
  static float ang = 0.4f; static float lastT = 0;
  float step = t - lastT; if (step < 0 || step > 0.3f) step = 0.03f; lastT = t;
  ang += step * sp;
  float a = arc > 0 ? sinf(ang * 1.4f) * arc : ang;
  a += userYaw;
  userPitch *= 0.995f;
  float Hh = H + userPitch * R;
  float tx = camA.tx + (camB.tx - camA.tx) * ce, tz = camA.tz + (camB.tz - camA.tz) * ce;
  Vec eye = vec(tx + sinf(a) * R, Hh + 0.2f * sinf(t * 0.37f), tz + cosf(a) * R);
  if (env.ground != G_NONE && eye.y < 0.35f) eye.y = 0.35f;
  Vec target = vec(tx, ty, tz);
  r3dBegin(eye, target, 58);
  r3dLight(env.sun, env.sunC, env.ambS, env.ambG);
  { float need = R + bounds * 0.6f + 9, k = env.fogF < need ? need / env.fogF : 1; r3dFog(env.fogN * k, env.fogF * k, env.fog); }
  { float k = 0.12f + 0.3f * env.night + 0.15f * clampf(env.rain, 0, 1); r3dFill(col(k * 0.95f, k, k * 1.1f)); }
  drawSky(t);
  drawGround(t, tr);
  // the old things shrink away (0.5 s), the new ones pop in
  float go = (t - tOldGone) / 0.55f;
  if (go < 1) for (int i = 0; i < nOld; i++) drawInst(old[i], t, 1 - go, oldParts);
  else nOld = 0;
  for (int i = 0; i < nCur; i++) {
    if (!decorOn && cur[i].decor && cur[i].model >= 0) continue;
    float life = (t - cur[i].born) / (cur[i].decor ? 0.9f : 0.6f);
    if (life <= 0) continue;
    drawInst(cur[i], t, life > 1 ? 1 : life, curParts);
  }
  drawWeather(t, target);
  if (fxOn) for (int i = 0; i < nEmit; i++) if (t - tShow > 1.2f) fxAt(emits[i].fx, t, emits[i].p, emits[i].spread, emits[i].n, i * 0.37f);
  r3dRender(out, softUp);
  lastTris = r3dTris();
}

// ------------------------------------------------------------------ scenes as JSON
static int findName(const char* s, const char* const* names, int n, int def) {
  if (!s) return def;
  for (int i = 0; i < n; i++) if (!strcasecmp(s, names[i])) return i;
  return def;
}
static uint32_t parseCol(const char* s) {
  if (!s || !*s) return NOCOL;
  if (*s == '#') s++;
  size_t l = strlen(s);
  if (l != 6 && l != 3) return NOCOL;
  for (size_t i = 0; i < l; i++) if (!((s[i] >= '0' && s[i] <= '9') || ((s[i] | 32) >= 'a' && (s[i] | 32) <= 'f'))) return NOCOL;
  uint32_t v = strtoul(s, nullptr, 16);
  if (l == 3) v = ((v & 0xF00) << 12 | (v & 0xF00) << 8) | ((v & 0xF0) << 8 | (v & 0xF0) << 4) | ((v & 0xF) << 4 | (v & 0xF));
  return v;
}
static int primByName(const char* s) {
  static const char* N[] = {"box", "ball", "cyl", "cone", "pyr", "wedge", "torus", "disc", "dome", "star", "heart", "rock", "arc", "ring", "tube"};
  if (!s) return P_BOX;
  for (int i = 0; i < P_N; i++) if (!strcasecmp(s, N[i])) return i;
  if (!strcasecmp(s, "sphere")) return P_BALL;
  if (!strcasecmp(s, "cylinder")) return P_CYL;
  if (!strcasecmp(s, "pyramid")) return P_PYR;
  return P_BOX;
}
void sceneDefault(SceneDef& d) {
  memset(&d, 0, sizeof(d));
  snprintf(d.title, sizeof(d.title), "SAY SOMETHING");
  d.place = PL_VOID; d.time = TM_NIGHT; d.weather = WE_CLEAR; d.mood = MO_MYSTERY; d.fx = FX_SPARKLES; d.cam = CAM_ORBIT;
  modelsInit();
  d.nth = 2;
  d.th[0].model = modelFind("cube"); d.th[0].n = 1; d.th[0].anim = B_FLOAT; d.th[0].at = AT_CENTER; d.th[0].s = 1.4f; d.th[0].c = NOCOL; d.th[0].c2 = NOCOL;
  d.th[1].model = modelFind("speech"); d.th[1].n = 1; d.th[1].anim = B_FLOAT; d.th[1].at = AT_RIGHT; d.th[1].s = 0.8f; d.th[1].c = NOCOL; d.th[1].c2 = NOCOL;
  // the cube glows in the interface's colour
  uint16_t f = C_FG;
  d.th[0].c = ((uint32_t)((f >> 11) * 255 / 31) << 16) | ((uint32_t)(((f >> 5) & 63) * 255 / 63) << 8) | (uint32_t)((f & 31) * 255 / 31);
}
bool sceneParse(const char* json, SceneDef& d) {
  modelsInit();
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonObject o = doc["scene"].is<JsonObject>() ? doc["scene"].as<JsonObject>() : doc.as<JsonObject>();
  if (o.isNull() || !o["things"].is<JsonArray>()) return false;
  memset(&d, 0, sizeof(d));
  snprintf(d.title, sizeof(d.title), "%s", (const char*)(o["title"] | ""));
  snprintf(d.said, sizeof(d.said), "%s", (const char*)(o["said"] | ""));
  d.place = findName(o["place"], PLACES, PL_N, PL_GRASS);
  d.time = findName(o["time"], TIMES, TM_N, TM_DAY);
  d.weather = findName(o["weather"], WEATHERS, WE_N, WE_CLEAR);
  d.mood = findName(o["mood"], MOODS, MO_N, MO_NEUTRAL);
  d.fx = findName(o["fx"], FXS, FX_N, FX_NONE);
  d.cam = findName(o["cam"], CAMS, CAM_N, CAM_ORBIT);
  for (JsonObject t : o["things"].as<JsonArray>()) {
    if (d.nth >= MAX_THINGS) break;
    Thing& th = d.th[d.nth];
    const char* m = t["m"] | "";
    th.model = modelFind(m);
    th.n = t["n"] | 1; th.s = t["s"] | 1.0f;
    th.anim = findName(t["a"], BEHAVIORS, B_N, B_IDLE);
    th.at = findName(t["at"], ATS, AT_N, AT_N);
    th.c = parseCol(t["c"] | ""); th.c2 = parseCol(t["c2"] | "");
    if (th.model < 0) {
      if (!t["parts"].is<JsonArray>()) continue;   // not something it knows
      th.cp0 = d.ncp; th.ncp = 0; th.custH = 0;
      for (JsonObject p : t["parts"].as<JsonArray>()) {
        if (d.ncp >= MAX_CPARTS || th.ncp >= 12) break;
        CPart& c = d.cp[d.ncp++]; th.ncp++;
        c.prim = primByName(p["p"] | "box");
        c.x = clampf(p["x"] | 0.0f, -6, 6); c.y = clampf(p["y"] | 0.0f, -1, 10); c.z = clampf(p["z"] | 0.0f, -6, 6);
        c.w = clampf(p["w"] | 1.0f, 0.02f, 8); c.h = clampf(p["h"] | 1.0f, 0.02f, 8); c.d = clampf(p["d"] | 1.0f, 0.02f, 8);
        c.ry = p["r"] | 0.0f;
        uint32_t cc = parseCol(p["c"] | "");
        c.c = cc == NOCOL ? 0xB0B0B0 : cc;
        if (c.y + c.h / 2 > th.custH) th.custH = c.y + c.h / 2;
      }
      if (!th.ncp) continue;
    }
    d.nth++;
  }
  return d.nth > 0 || d.title[0];
}
