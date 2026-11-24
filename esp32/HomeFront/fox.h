// FOX: a small 3D fox living in the screen, DOS green on black. The same as public/thuisfront/dev/fox.js:
// foxStep() lets it feel the board move, scrFox() draws it, foxJson() is its pose for the phones watching.
#pragma once
#include <stdint.h>

struct FoxPose {                  // the order of FOX_KEYS in fox.js
  float px, pz, hop, hd, rl, pt, tb, lf, hy, hp, el, er, ta, tu, wp, wa;
  int ey, md, ov;
};

struct FoxSim {
  float px = -0.2f, pz = -0.1f, vx = 0, vz = 0, hop = 0, vy = 0, hd = -0.65f, rl = 0, pt = 0, tb = 0, tbv = 0, lf = 0;
  float hy = 0, hp = 0, el = 0, er = 0, ta = 0, tu = 0.15f, wp = 0, wa = 0;
  int ey = 0, md = 12, ov = 0;
  float t = 0, gl[3] = { 0, 1, 0 }, still = 0, startle = 0, dizzy = 0, wake = 0, hello = 2.5f, blink = 2, look = 3, lookY = 0, lookP = 0;
  float calm = 0, wander = 5, tx = 0, tz = 0, twitch = 4, bump = 0;
  int twitchE = 0;
  bool walking = false, falling = false, sensor = false;
};

// f: what the accelerometer feels, in g, in screen directions (x right, y up, z out of the screen), 1 g up when still;
// w: turning in rad/s (same directions); nullptr = no sensor. dt: seconds.
void foxStep(FoxSim& S, const float* f, const float* w, float dt);
void foxPoseOf(const FoxSim& S, FoxPose& p);
void scrFox(const FoxPose& p, float t, int pal);   // the whole screen
int foxJson(const FoxPose& p, char* out, int n);  // "[px,pz,...]"
