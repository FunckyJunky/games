// musicdet.h: what the microphones hear, as music. Every 20 ms the last 512 samples (16 kHz) ->
//   16 bands (each against its own recent loudness), bass / mid / high, beats (and the tempo),
//   and "is this music?" (steady sound + rhythm + few short gaps, unlike talking).
// Plain C++: runs in mic.cpp on the cube and on the PC (simulator, tests with real recordings).
#pragma once
#include <stdint.h>

#define MD_N 512                 // samples looked at (32 ms)
#define MD_BANDS 16
struct MusicState {
  float band[MD_BANDS];          // 0..1 (bass first)
  float bass, mid, high, level;  // 0..1
  float beat;                    // 1 at a beat, then down to 0 (about 0.3 s)
  uint32_t beats;                // beats so far
  float bpm;                     // tempo (0 = none found)
  float score;                   // 0..1: how much it sounds like music (smoothed)
  bool music;                    // music playing (score with hysteresis, a few seconds to change)
  // why (tests, the page)
  float fPresence, fGaps, fRhythm, fSteady;
};
bool mdInit();                                  // false = no memory
void mdFrame(const int16_t* x, float gateDb);  // x: the last MD_N samples; gateDb: the mic's speech gate now
const MusicState& mdState();
void mdHint(float k);                           // speech to text heard music (♪, [Music]): k 0..1
void mdForce(int mode);                         // 0 normal, 1 never music, 2 always music
