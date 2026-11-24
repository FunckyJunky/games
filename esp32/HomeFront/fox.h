// FOX: a small 3D fox living in the screen, DOS green on black. The same as public/thuisfront/dev/fox.js:
// foxStep() lets it feel the board move and live its own life, scrFox() draws it, foxJson() is its pose for the phones.
#pragma once
#include <stdint.h>

struct FoxPose {                  // the order of FOX_KEYS in fox.js
  float px, pz, hop, hd, rl, pt, tb, lf, hy, hp, el, er, ta, tu, wp, wa;
  int ey, md, ov;
  float cr, gx, gz;
  int gm;
  float sc, fp, bw, dd;
};

struct FoxSim {                   // as foxNew() in fox.js
  float px = -0.2f, pz = -0.1f, vx = 0, vz = 0, hop = 0, vy = 0, hd = -0.65f, rl = 0, pt = 0, tb = 0, tbv = 0, lf = 0;
  float hy = 0, hp = 0, el = 0, er = 0, ta = 0, tav = 0, tu = 0.15f, tuv = 0, wp = 0, wa = 0;
  int ey = 0, md = 12, ov = 0;
  float t = 0, gl[3] = { 0, 1, 0 }, still = 0, startle = 0, dizzy = 0, wake = 0, hello = 2.5f, blink = 2, look = 3, lookY = 0, lookP = 0;
  float pause = 1.5f, sit = 0, sniff = 0, tx = 0, tz = 0;
  bool walking = false, slide = false;
  int on = 1;
  float twitch = 4;
  int twitchE = 0;
  float bump = 0, nap = 0, yawn = 0, hdPrev = -0.65f, hdv = 0;
  bool sensor = false;
  // going to a tapped spot
  float cr = 0, gx = 0, gz = 0;
  int gm = 0, gs = 0, gph = 0;
  float gt = 0, gdur = 0, leap = 0;
  bool jumping = false;
  float found = 0;
  int rollDir = 1;
  float pounce = 0;
  // idle acts (tricks)
  float sc = 0, fp = 0, bw = 0, dd = 0;
  int act = 0, lastAct = 0;
  float at = 0;
  int aph = 0, adir = 1;
  float a0 = 0, acx = 0, acz = 0, ar = 0, an = 0, rest = 0, tada = 0, missed = 0;
};

// f: what the accelerometer feels, in g, in screen directions (x right, y up, z out of the screen), 1 g up when still;
// w: turning in rad/s (same directions); f nullptr = no sensor, w nullptr = no gyroscope. dt: seconds.
void foxStep(FoxSim& S, const float* f, const float* w, float dt);
// TAP: go to that spot on the floor (style 1-4, 0 = pick one); IDLE: do a trick now (1-9, 0 = pick one)
void foxGo(FoxSim& S, float x, float z, int style = 0);
void foxAct(FoxSim& S, int force = 0);
extern float (*foxRnd)();         // tests: a random 0..1 of their own (nullptr: the hardware's)
// CENTRE (as foxZero in fox.js): turn the "up" felt then to straight up, and take the resting gyro off
struct FoxZero { bool on = false; float R[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, b[3] = { 0, 0, 0 }; };
void foxZeroOf(const float* zero6, FoxZero& Z);
void foxZeroApply(const FoxZero& Z, float* f, float* w);   // in place
void foxPoseOf(const FoxSim& S, FoxPose& p);
void scrFox(const FoxPose& p, float t, int pal);   // the whole screen
int foxJson(const FoxPose& p, char* out, int n);  // "[px,pz,...]"
