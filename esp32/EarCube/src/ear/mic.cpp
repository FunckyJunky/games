// mic.cpp: the cube's two microphones (ES7210, MIC1 = left, MIC2 = right) -> 16 kHz mono speech pieces.
//  - the I2S bus runs at 44.1 kHz stereo for the speaker (board/sound.cpp); the ES7210 sends on its own data line
//  - left, right or both (the average) -> a polyphase low-pass resampler 44.1 -> 16 kHz -> a 40 s ring in PSRAM
//  - every 20 ms: how loud it is against the background noise it learnt -> speech or not
//  - speech from its start (+ 0.3 s before) to a pause (+ 0.25 s after) = a piece; a long one is cut at its
//    quietest moment (LIVE after 7 s, RECORD after 25 s). Pieces with under 0.3 s of speech are dropped.
// Runs in its own task (core 1, above the interface and the speaker mixer, below WiFi).
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>
#include "../core/sys.h"
#include "../board/sound.h"
#include "ear.h"
#include "es7210.h"

#define RING (EAR_RATE * 40)             // 40 s (1.28 MB): pieces are copied out long before they're overwritten
#define FRAME 320                        // 20 ms at 16 kHz
#define KEEP 2048                        // frame levels kept (41 s): where to cut a long piece
#define PRE 4800                         // 0.3 s before the speech started
#define POST 4000                        // 0.25 s after it stopped

static int16_t* ring = nullptr;
static volatile uint32_t wpos = 0;
static float* fdb = nullptr;
static bool found = false;
static volatile int runMode = 0, newMode = -1;
static volatile bool flushReq = false;
static QueueHandle_t pieces = nullptr;
static volatile float peak[3] = {-90, -90, -90};
static volatile float nowDb = -90, floorDb = -60, gateDb = -50;
static volatile bool speaking = false;
static volatile int gainWant = -1;

// ------------------------------------------------------------------ 44.1 kHz -> 16 kHz (160 / 441)
// Output k sits at input k * 441 / 160. A 64-tap windowed sinc (cut at 7 kHz, Blackman), one set of taps for each
// of the 160 places between two input samples.
#define TAPS 64
#define PH 160
#define HIST 128
static float* coef = nullptr;            // PH x TAPS
static float hist[HIST];
static uint32_t inN = 0;                 // input samples so far
static uint64_t acc = 0;                 // the next output's place, in 1/160 input samples
static void resamplerInit() {
  coef = (float*)heap_caps_malloc(PH * TAPS * sizeof(float), MALLOC_CAP_SPIRAM);
  if (!coef) return;
  const float fc = 7000.0f / 44100.0f, half = TAPS / 2;
  for (int p = 0; p < PH; p++) {
    float sum = 0, *h = coef + p * TAPS;
    for (int j = 0; j < TAPS; j++) {
      float d = (j - (half - 1)) - p / (float)PH;              // distance from the output's place
      float x = 2 * fc * d;
      float s = fabsf(x) < 1e-6f ? 1.0f : sinf((float)M_PI * x) / ((float)M_PI * x);
      float w = fabsf(d) >= half ? 0 : 0.42f + 0.5f * cosf((float)M_PI * d / half) + 0.08f * cosf(2 * (float)M_PI * d / half);
      h[j] = 2 * fc * s * w; sum += h[j];
    }
    for (int j = 0; j < TAPS; j++) h[j] /= sum;               // no change in loudness
  }
}

// ------------------------------------------------------------------ speech or not, and the pieces
static bool inSeg = false;
static uint32_t segFrom = 0, lastSpeechEnd = 0, segSpeechMs = 0;
static int silFrames = 0;
static uint8_t recent = 0;                // the last 5 frames: speech bits
static double sumSq = 0;
static float hp = 0, hpPrev = 0;          // a 70 Hz high-pass (handling noise, rumble)
static int16_t framePeak = 0;

static void closePiece(uint32_t from, uint32_t to, uint32_t ms) {
  if (to <= from || ms < 300) return;
  Piece p = {from, to, ms, 0};
  xQueueSend(pieces, &p, 0);
}
static void frameDone() {
  uint32_t f = wpos / FRAME - 1;          // the frame that just filled up
  float ms = (float)(sumSq / FRAME);
  float db = ms > 0 ? 10 * log10f(ms / (32768.0f * 32768.0f)) : -90;
  if (db < -90) db = -90;
  sumSq = 0;
  fdb[f % KEEP] = db;
  nowDb = db;
  // the background: comes down fast, goes up slowly (slower still while someone speaks)
  float fl = floorDb;
  if (db < fl) fl += (db - fl) * 0.10f; else fl += (db - fl) * (inSeg ? 0.0005f : 0.003f);
  if (fl < -90) fl = -90; if (fl > -15) fl = -15;
  floorDb = fl;
  static const float MARGIN[4] = {14, 9, 6, 3}, ABS[4] = {-40, -55, -62, -75};   // dB over the noise; never below
  int s = ecfg.sens & 3;
  float gate = fl + MARGIN[s]; if (gate < ABS[s]) gate = ABS[s];
  gateDb = gate;
  bool sp = db > gate;
  recent = ((recent << 1) | sp) & 31;
  int mode = runMode;
  if (!mode) { inSeg = false; speaking = false; return; }
  static const int END[3] = {20, 35, 60};
  int endF = END[ecfg.pause % 3]; if (mode == 1) { endF -= 10; if (endF < 12) endF = 12; }
  int maxF = mode == 1 ? 350 : 1250;
  uint32_t fEnd = (f + 1) * FRAME;
  if (!inSeg) {
    if (__builtin_popcount(recent) >= 3) {
      inSeg = true;
      uint32_t st = (f >= 4 ? (f - 4) * FRAME : 0);
      segFrom = st > PRE ? st - PRE : 0;
      if (wpos - segFrom > RING - EAR_RATE) segFrom = wpos - (RING - EAR_RATE);
      segSpeechMs = 60; silFrames = 0; lastSpeechEnd = fEnd;
    }
  } else {
    if (sp) { segSpeechMs += 20; silFrames = 0; lastSpeechEnd = fEnd; } else silFrames++;
    if (silFrames >= endF) {
      uint32_t to = lastSpeechEnd + POST; if (to > fEnd) to = fEnd;
      closePiece(segFrom, to, segSpeechMs);
      inSeg = false;
    } else if ((fEnd - segFrom) / FRAME >= (uint32_t)maxF) {
      uint32_t best = f; float bdb = 1e9f;                     // the quietest 20 ms in the last 1.5 s
      for (uint32_t g = f - 75; g <= f; g++) if (fdb[g % KEEP] < bdb) { bdb = fdb[g % KEEP]; best = g; }
      uint32_t cut = best * FRAME + FRAME / 2;
      closePiece(segFrom, cut, segSpeechMs);
      segFrom = cut; segSpeechMs = sp ? 20 : 0;
    }
  }
  speaking = inSeg && silFrames < 8;
}
static void push(float x) {
  // high-pass, then into the ring
  hp = 0.9729f * (hp + x - hpPrev); hpPrev = x;
  float y = hp;
  if (y > 32767) y = 32767; if (y < -32768) y = -32768;
  int16_t v = (int16_t)y;
  ring[wpos % RING] = v;
  sumSq += (double)v * v;
  int a = v < 0 ? -v : v; if (a > framePeak) framePeak = a;
  wpos++;
  if (wpos % FRAME == 0) {
    float pd = framePeak > 0 ? 20 * log10f(framePeak / 32768.0f) : -90;
    float o = peak[0]; peak[0] = pd > o ? pd : o - 1.5f;       // the meter falls 1.5 dB a frame
    framePeak = 0;
    frameDone();
    if (flushReq) {                                            // stop: the piece so far, then the marker
      if (inSeg) { closePiece(segFrom, wpos, segSpeechMs); inSeg = false; }
      Piece m = {wpos, wpos, 0, 1};
      xQueueSend(pieces, &m, pdMS_TO_TICKS(100));
      flushReq = false;
      runMode = 0; speaking = false;
    }
  }
}

static void task(void*) {
  static int16_t raw[256 * 2];
  float pkL = 0, pkR = 0; int nPk = 0;
  for (;;) {
    if (gainWant >= 0) { es7210Gain(gainWant); gainWant = -1; }
    if (newMode >= 0) { runMode = newMode; newMode = -1; inSeg = false; recent = 0; }
    size_t n = sndMicRead(raw, sizeof(raw), 50);
    int frames = n / 4;
    if (frames <= 0) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
    int m = ecfg.mics;
    for (int i = 0; i < frames; i++) {
      float l = raw[i * 2], r = raw[i * 2 + 1];
      float al = fabsf(l), ar = fabsf(r); if (al > pkL) pkL = al; if (ar > pkR) pkR = ar;
      float x = m == 1 ? l : m == 2 ? r : (l + r) * 0.5f;
      hist[inN % HIST] = x; inN++;
      // every output whose taps are all in
      for (;;) {
        uint32_t i0 = (uint32_t)(acc / PH), p = (uint32_t)(acc % PH);
        if (i0 + TAPS / 2 >= inN) break;
        const float* h = coef + p * TAPS;
        uint32_t s0 = i0 - (TAPS / 2 - 1);
        float y = 0;
        for (int j = 0; j < TAPS; j++) y += hist[(s0 + j) % HIST] * h[j];
        push(y);
        acc += 441;
      }
    }
    if (++nPk >= 8) {                                          // the left / right meters, about every 46 ms
      float dl = pkL > 0 ? 20 * log10f(pkL / 32768.0f) : -90, dr = pkR > 0 ? 20 * log10f(pkR / 32768.0f) : -90;
      float ol = peak[1], orr = peak[2];
      peak[1] = dl > ol ? dl : ol - 4; peak[2] = dr > orr ? dr : orr - 4;
      pkL = pkR = 0; nPk = 0;
    }
  }
}

// ------------------------------------------------------------------ what the rest asks
bool micBegin() {
  ring = (int16_t*)heap_caps_calloc(RING, sizeof(int16_t), MALLOC_CAP_SPIRAM);
  fdb = (float*)heap_caps_calloc(KEEP, sizeof(float), MALLOC_CAP_SPIRAM);
  pieces = xQueueCreate(24, sizeof(Piece));
  resamplerInit();
  if (!ring || !fdb || !coef || !pieces || !S.codec) return false;
  found = es7210Begin(sndRate(), sndRate() * 256, ecfg.gain);
  if (!found) return false;
  for (float& h : hist) h = 0;
  xTaskCreatePinnedToCore(task, "mic", 4096, nullptr, 5, nullptr, 1);
  return true;
}
bool micOk() { return found; }
void micGain(int step) { gainWant = step; }
void micRun(int mode) { newMode = mode; }
void micFlush() { flushReq = true; }
static int pct(float db) { int p = (int)((db + 66) * 100 / 60); return p < 0 ? 0 : p > 100 ? 100 : p; }
int micLevel(int ch) { return found ? pct(peak[ch < 0 || ch > 2 ? 0 : ch]) : 0; }
float micDb() { return nowDb; }
float micFloorDb() { return floorDb; }
float micGateDb() { return gateDb; }
bool micSpeech() { return speaking; }
uint32_t micPos() { return wpos; }
int micCopy(uint32_t from, int16_t* out, int n) {
  uint32_t w = wpos;
  if (w - from > RING || from + n > w) return -1;
  for (int i = 0; i < n; i++) out[i] = ring[(from + i) % RING];
  if (wpos - from > RING) return -1;                           // overwritten while copying
  return n;
}
bool micPiece(Piece& p, uint32_t waitMs) { return pieces && xQueueReceive(pieces, &p, pdMS_TO_TICKS(waitMs)) == pdTRUE; }
