// musicdet.cpp: see musicdet.h. A 512-point FFT every 20 ms (Hann window), then:
//   bands     16, about 30 Hz .. 7 kHz on a log scale; each one against its own loudness of the last seconds
//   beats     spectral flux (how much louder each frequency got), the bass counted double; a beat when it jumps
//             over its recent average (at most ~210 a minute); the tempo from the flux's autocorrelation (4 s)
//   music?    over the last 4 s (tried on speech and music recordings, clean and through a room):
//             rhythm    the flux repeats at a beat's distance (0.3 .. 1.2 s); talk hardly does (< 0.28)
//             centre    the spectrum's centre stays put; talk jumps between vowels and s / f / t
//             highs     the share of high frequencies changes little from frame to frame (talk: a lot)
//             presence  there has to be sound most of the time
//   -> a score, smoothed; music on after a few seconds above, off after a few seconds below (or quiet).
#include "musicdet.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
static void* mdAlloc(size_t n) { void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM); return p ? p : malloc(n); }
#else
static void* mdAlloc(size_t n) { return malloc(n); }
#endif

#define HALF (MD_N / 2)
#define HIST 200                 // 4 s of frames (50 a second)
static float *win, *re, *im, *cosT, *sinT, *lmag, *lprev;
static uint16_t* rev;
static float *fluxH, *cenH, *hfH, *presH;   // the last 4 s
static int hN = 0, hPos = 0;
static int bandLo[MD_BANDS + 1];
static float bHi[MD_BANDS], bLo[MD_BANDS], bVis[MD_BANDS];
static float lvHi = -60, lvLo = -70, lvVis = 0, nfloor = -60;
static MusicState st;
static float fluxAvg = 0, lowAvg = 0, oAvg = 0.01f;
static int sinceBeat = 100;
static float hint = 0;
static int force = 0, frameN = 0;
static volatile bool ready = false;
static float onFor = 0, offFor = 0;

bool mdInit() {
  if (ready) return true;
  win = (float*)mdAlloc(MD_N * 4); re = (float*)mdAlloc(MD_N * 4); im = (float*)mdAlloc(MD_N * 4);
  cosT = (float*)mdAlloc(HALF * 4); sinT = (float*)mdAlloc(HALF * 4);
  lmag = (float*)mdAlloc((HALF + 1) * 4); lprev = (float*)mdAlloc((HALF + 1) * 4);
  rev = (uint16_t*)mdAlloc(MD_N * 2);
  fluxH = (float*)mdAlloc(HIST * 4); cenH = (float*)mdAlloc(HIST * 4); hfH = (float*)mdAlloc(HIST * 4); presH = (float*)mdAlloc(HIST * 4);
  if (!win || !re || !im || !cosT || !sinT || !lmag || !lprev || !rev || !fluxH || !cenH || !hfH || !presH) return false;
  for (int i = 0; i < MD_N; i++) win[i] = 0.5f - 0.5f * cosf(2 * (float)M_PI * i / MD_N);
  for (int i = 0; i < HALF; i++) { cosT[i] = cosf(2 * (float)M_PI * i / MD_N); sinT[i] = -sinf(2 * (float)M_PI * i / MD_N); }
  int bits = 0; while ((1 << bits) < MD_N) bits++;
  for (int i = 0; i < MD_N; i++) { int r = 0; for (int b = 0; b < bits; b++) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[i] = r; }
  // the bands: 40 Hz .. 7 kHz, log; at least one bin each
  for (int k = 0; k <= MD_BANDS; k++) {
    float f = 40.0f * powf(7000.0f / 40.0f, k / (float)MD_BANDS);
    int b = (int)(f / (16000.0f / MD_N) + 0.5f);
    if (b < 1) b = 1;
    if (k && b <= bandLo[k - 1]) b = bandLo[k - 1] + 1;
    bandLo[k] = b;
  }
  for (int k = 0; k < MD_BANDS; k++) { bHi[k] = -50; bLo[k] = -80; bVis[k] = 0; }
  memset(lprev, 0, (HALF + 1) * 4);
  memset(&st, 0, sizeof(st));
  ready = true;
  return true;
}
const MusicState& mdState() { return st; }
void mdHint(float k) { if (k > hint) hint = k; }
void mdForce(int m) { force = m; }

static void fft() {
  for (int i = 0; i < MD_N; i++) { int j = rev[i]; if (j > i) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; } }
  for (int len = 2; len <= MD_N; len <<= 1) {
    int h = len >> 1, step = MD_N / len;
    for (int i = 0; i < MD_N; i += len)
      for (int j = 0; j < h; j++) {
        float wr = cosT[j * step], wi = sinT[j * step];
        int a = i + j, b = a + h;
        float xr = re[b] * wr - im[b] * wi, xi = re[b] * wi + im[b] * wr;
        re[b] = re[a] - xr; im[b] = im[a] - xi; re[a] += xr; im[a] += xi;
      }
  }
}
static inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static inline float hAt(const float* h, int back) { return h[(hPos - 1 - back + HIST * 2) % HIST]; }

// the music features over the last 4 s
static void features() {
  int n = hN < HIST ? hN : HIST;
  if (n < 100) { st.fPresence = st.fGaps = st.fRhythm = st.fSteady = 0; return; }
  float pr = 0; for (int i = 0; i < n; i++) pr += hAt(presH, i);
  st.fPresence = pr / n;
  // rhythm: the flux's autocorrelation at 0.3 .. 1.2 s, with its double (a beat repeats)
  float mean = 0; for (int i = 0; i < n; i++) mean += hAt(fluxH, i);
  mean /= n;
  static float x[HIST];
  for (int i = 0; i < n; i++) x[i] = hAt(fluxH, n - 1 - i) - mean;
  float r0 = 0; for (int i = 0; i < n; i++) r0 += x[i] * x[i];
  float best = 0; int bestL = 0;
  if (r0 > 1e-9f) {
    for (int l = 15; l <= 60; l++) {
      float r = 0; for (int i = l; i < n; i++) r += x[i] * x[i - l];
      r /= r0 * (n - l) / n;
      float r2 = 0;
      if (2 * l < n - 20) { for (int i = 2 * l; i < n; i++) r2 += x[i] * x[i - 2 * l]; r2 /= r0 * (n - 2 * l) / n; }
      float v = r * 0.65f + (r2 > 0 ? r2 : 0) * 0.35f;
      if (v > best) { best = v; bestL = l; }
    }
  }
  st.fRhythm = best;
  if (best > 0.2f && bestL) {
    float bpm = 3000.0f / bestL;
    while (bpm < 75) bpm *= 2;
    while (bpm > 185) bpm /= 2;
    st.bpm = st.bpm ? st.bpm * 0.8f + bpm * 0.2f : bpm;
  }
  // centre: how far the spectrum's centre moves (octaves, spread) where there's sound
  float cs = 0, cq = 0; int cn = 0;
  for (int i = 0; i < n; i++) if (hAt(presH, i) > 0) { float c = hAt(cenH, i); cs += c; cq += c * c; cn++; }
  st.fGaps = cn > 20 ? sqrtf(fmaxf(0, cq / cn - (cs / cn) * (cs / cn))) : 9;
  // highs: how much the share of high frequencies jumps (dB, frame to frame)
  float d = 0; int dn = 0;
  for (int i = 1; i < n; i++) if (hAt(presH, i) > 0 && hAt(presH, i - 1) > 0) { d += fabsf(hAt(hfH, i) - hAt(hfH, i - 1)); dn++; }
  st.fSteady = dn > 20 ? d / dn : 9;
}

void mdFrame(const int16_t* x, float gateDb) {
  if (!ready) return;
  frameN++;
  // loudness, the spectrum
  double ss = 0;
  for (int i = 0; i < MD_N; i++) { float v = x[i]; ss += v * v; re[i] = v * win[i] * (1.0f / 32768); im[i] = 0; }
  float ms = (float)(ss / MD_N);
  float db = ms > 0 ? 10 * log10f(ms / (32768.0f * 32768.0f)) : -90;
  bool present = db > gateDb;
  // its own background (comes down at once, goes up 0.1 dB a second): "sound" = 8 dB over it
  if (db < nfloor) nfloor += (db - nfloor) * 0.1f; else nfloor += 0.002f;
  bool sound = db > nfloor + 8 && db > -62;
  fft();
  float flux = 0, fluxLow = 0, eAll = 1e-12f, eHigh = 0, eC = 0;
  for (int b = 1; b <= HALF; b++) {
    float p = re[b] * re[b] + im[b] * im[b];
    float l = logf(1 + 400 * sqrtf(p));
    float d = l - lprev[b];
    if (d > 0 && b <= 200) { flux += d; if (b <= 8) fluxLow += d; }
    lprev[b] = l;
    lmag[b] = p;
    eAll += p; eC += p * b;
    if (b >= 80) eHigh += p;                  // 2.5 kHz and up
  }
  // the bands
  for (int k = 0; k < MD_BANDS; k++) {
    float p = 0; for (int b = bandLo[k]; b < bandLo[k + 1]; b++) p += lmag[b];
    float bdb = 10 * log10f(p + 1e-12f);
    if (bdb > bHi[k]) bHi[k] = bdb; else bHi[k] -= 0.03f;          // its loud: comes at once, goes 1.5 dB a second
    if (bdb < bLo[k]) bLo[k] += (bdb - bLo[k]) * 0.2f; else bLo[k] += 0.01f;
    if (bLo[k] > bHi[k] - 6) bLo[k] = bHi[k] - 6;
    float range = bHi[k] - bLo[k]; if (range < 20) range = 20; if (range > 40) range = 40;
    float v = clamp01((bdb - (bHi[k] - range)) / range);
    v = v * v * (1.4f - 0.4f * v);                                // quiet parts lower, loud ones stand out
    if (!present && db < gateDb - 8) v = 0;
    bVis[k] += (v - bVis[k]) * (v > bVis[k] ? 0.75f : 0.22f);
    st.band[k] = bVis[k];
  }
  float b3 = 0, m3 = 0, h3 = 0;
  for (int k = 0; k < 4; k++) b3 += st.band[k];
  for (int k = 4; k < 11; k++) m3 += st.band[k];
  for (int k = 11; k < 16; k++) h3 += st.band[k];
  st.bass = b3 / 4; st.mid = m3 / 7; st.high = h3 / 5;
  if (db > lvHi) lvHi = db; else lvHi -= 0.02f;
  if (db < lvLo) lvLo += (db - lvLo) * 0.2f; else lvLo += 0.01f;
  { float range = lvHi - lvLo; if (range < 20) range = 20; float v = clamp01((db - (lvHi - range)) / range); lvVis += (v - lvVis) * (v > lvVis ? 0.6f : 0.15f); st.level = lvVis; }
  // beats: the flux (bass counted double) jumping over its average
  float o = flux + fluxLow * 2;
  if (!present) o *= 0.3f;
  sinceBeat++;
  bool beat = o > oAvg * 1.55f && o > 2.0f && sinceBeat >= 14;
  // with a tempo: a beat that's due counts with less
  if (!beat && st.bpm > 0 && present) {
    int per = (int)(3000 / st.bpm + 0.5f);
    if (sinceBeat >= per - 1 && sinceBeat <= per + 1 && o > oAvg * 1.2f && o > 1.5f) beat = true;
  }
  oAvg += (o - oAvg) * 0.06f;
  if (beat) { sinceBeat = 0; st.beats++; st.beat = 1; } else st.beat *= 0.86f;
  fluxAvg += (flux - fluxAvg) * 0.05f; lowAvg += (fluxLow - lowAvg) * 0.05f;
  // the history
  fluxH[hPos] = sound ? flux : 0; cenH[hPos] = log2f(eC / eAll * (16000.0f / MD_N) + 1);
  hfH[hPos] = 10 * log10f(eHigh / eAll + 1e-9f); presH[hPos] = sound ? 1 : 0;
  hPos = (hPos + 1) % HIST; if (hN < HIST) hN++;
  if (frameN % 10) return;
  // 5 times a second: is it music?
  features();
  float r = clamp01((st.fRhythm - 0.2f) / 0.22f);                  // rhythm 0.2 .. 0.42 (talk: under 0.28)
  float c = clamp01((0.85f - st.fGaps) / 0.35f);                    // centre spread 0.85 .. 0.5 octaves
  float h = clamp01((2.9f - st.fSteady) / 0.9f);                    // highs jump 2.9 .. 2.0 dB
  float sc = 0.5f * r + 0.25f * c + 0.25f * h;
  float strong = 0.9f * clamp01((st.fRhythm - 0.32f) / 0.3f);      // a clear beat alone is enough (rap)
  float tonal = 0.85f * c * h;                                      // both very steady: music without drums
  if (strong > sc) sc = strong;
  if (tonal > sc) sc = tonal;
  sc *= clamp01((st.fPresence - 0.25f) / 0.25f);                    // sound 25% .. 50% of the time
  sc += hint * 0.5f;
  if (sc > 1) sc = 1;
  hint *= 0.97f;
  st.score += (sc - st.score) * 0.25f;
  if (force == 1) { st.music = false; return; }
  if (force == 2) { st.music = true; return; }
  // on after ~3 s above 0.6, off after ~5 s below 0.35 (or 3 s of quiet)
  if (st.score > 0.6f) { onFor += 0.2f; offFor = 0; } else if (st.score < 0.35f || st.fPresence < 0.3f) { offFor += st.fPresence < 0.3f ? 0.35f : 0.2f; onFor = 0; }
  else { onFor *= 0.9f; offFor *= 0.9f; }
  if (!st.music && onFor >= 3) st.music = true;
  if (st.music && offFor >= 5) st.music = false;
}
