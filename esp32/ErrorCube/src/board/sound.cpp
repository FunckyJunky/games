// sound.cpp: see sound.h. 44.1 kHz stereo, 16 bit, a small mixer task feeding I2S: the made-up sounds,
// the phone's stream (16 kHz, upsampled) and media (music, video: any rate, resampled).
#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include <math.h>
#include "../core/sys.h"
#include "sound.h"
extern "C" {
#include "es8311.h"
}

#define RATE 44100
#define PHONE_RATE 16000
static I2SClass i2s;
static es8311_handle_t codec = nullptr;
static bool ok = false;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// voices: square, sine, noise, saw; frequency slides from f0 to f1
enum { W_SQ, W_SIN, W_NOISE, W_SAW };
struct Voice { uint8_t w; float f0, f1, vol; uint32_t len, pos, delay; float ph; float lp; bool on; };
static Voice vs[16];
static volatile bool stopAll = false;
static int16_t* ring = nullptr;                 // streamed sound
static const int RING = PHONE_RATE * 2;
static uint32_t phPhase = 0;                    // 16.16: where we are between two phone samples
// media: stereo ring at 44.1 kHz (about 0.75 s)
static const int MRING = 32768;
static int16_t* mring = nullptr;                // L, R pairs
static volatile uint32_t mw = 0, mr = 0, mplayed = 0;
static volatile bool mpause = false;
static uint32_t mPhase = 0; static int16_t mLastL = 0, mLastR = 0;   // the resampler
static int16_t wave[512]; static volatile uint32_t wpos = 0;
static volatile uint32_t rw = 0, rr = 0;
static uint32_t lastSound = 0;
static int codecVol = -1;
static uint32_t noiseS = 12345;

static void add(uint8_t w, float f0, float f1, int ms, float vol, int delayMs = 0) {
  if (!ok) return;
  portENTER_CRITICAL(&mux);
  for (Voice& v : vs) if (!v.on) {
    v.w = w; v.f0 = f0; v.f1 = f1; v.vol = vol; v.len = ms * RATE / 1000; v.pos = 0; v.delay = delayMs * RATE / 1000; v.ph = 0; v.lp = 0; v.on = true;
    break;
  }
  portEXIT_CRITICAL(&mux);
}
void tone(int hz, int ms, int vol) { if (cfg.vol) add(W_SQ, hz, hz, ms, 0.18f * vol / 100); }
void sndStop() { stopAll = true; }

void snd(int kind) {
  if (!ok || !cfg.vol) return;
  bool fx = cfg.sounds;
  switch (kind) {
    case SND_KEY: if (cfg.clicks) { add(W_NOISE, 3500, 3500, 12, 0.10f); add(W_SQ, 1800, 1800, 8, 0.03f); } break;
    case SND_MOVE: if (fx) add(W_SQ, 1200, 1200, 18, 0.06f); break;
    case SND_BEEP: add(W_SQ, 988, 988, 160, 0.12f); break;
    case SND_THUD: if (fx) add(W_SIN, 70, 60, 120, 0.4f); break;
    case SND_CRACKLE: if (fx) for (int i = 0; i < 4; i++) add(W_NOISE, 900 + esp_random() % 3000, 900, 30 + esp_random() % 50, 0.08f + (esp_random() % 80) / 1000.0f, esp_random() % 120); break;
    case SND_FLOPPY:
      if (!fx) break;
      add(W_SAW, 58, 58, 500, 0.05f);
      for (int t = 0; t < 500; t += 70 + esp_random() % 90) { add(W_SQ, 95 + esp_random() % 30, 95, 30, 0.08f, t); add(W_NOISE, 1800, 1800, 20, 0.08f, t); }
      break;
    case SND_OK: add(W_SQ, 880, 880, 70, 0.1f); add(W_SQ, 1320, 1320, 90, 0.1f, 75); break;
    case SND_ERR: add(W_SQ, 220, 180, 260, 0.14f); break;
    case SND_MSG: add(W_SIN, 1320, 1320, 90, 0.35f); add(W_SIN, 1760, 1760, 140, 0.35f, 100); break;
    case SND_ALARM: for (int i = 0; i < 4; i++) add(W_SQ, 1000, 1000, 90, 0.16f, i * 150); break;
    case SND_ON: if (fx) add(W_SIN, 300, 1400, 160, 0.12f); break;
    case SND_OFF: if (fx) add(W_SIN, 1400, 200, 220, 0.12f); break;
  }
}

int sndMediaRoom() { return mring ? MRING - 1 - (int)(mw - mr) : 0; }
int sndMediaQueued() { return (int)(mw - mr); }
uint32_t sndMediaPlayed() { return mplayed; }
void sndMediaPause(bool p) { mpause = p; }
void sndMediaClear() { mr = mw; mplayed = 0; mPhase = 0; mpause = false; }
void sndMediaPut(const int16_t* lr, int frames, int rate) {
  if (!mring || rate <= 0) return;
  if (rate == RATE) {
    for (int i = 0; i < frames && (int)(mw - mr) < MRING - 1; i++) { uint32_t k = (mw % MRING) * 2; mring[k] = lr[i * 2]; mring[k + 1] = lr[i * 2 + 1]; mw++; }
    return;
  }
  // linear resampling: step through the input in 16.16 steps of rate / 44100
  uint32_t step = (uint32_t)(((uint64_t)rate << 16) / RATE);
  int i = 0;
  while ((int)(mw - mr) < MRING - 1) {
    int idx = mPhase >> 16;
    if (idx >= frames) break;
    int fr = mPhase & 0xFFFF;
    int16_t l0 = idx ? lr[(idx - 1) * 2] : mLastL, r0 = idx ? lr[(idx - 1) * 2 + 1] : mLastR;
    int16_t l1 = lr[idx * 2], r1 = lr[idx * 2 + 1];
    uint32_t k = (mw % MRING) * 2;
    mring[k] = l0 + (((l1 - l0) * fr) >> 16); mring[k + 1] = r0 + (((r1 - r0) * fr) >> 16);
    mw++;
    mPhase += step;
    i = idx;
  }
  (void)i;
  mLastL = lr[(frames - 1) * 2]; mLastR = lr[(frames - 1) * 2 + 1];
  mPhase -= (uint32_t)frames << 16;
}
void sndMediaWave(int16_t* out, int n) { uint32_t p = wpos; for (int i = 0; i < n; i++) out[i] = wave[(p - n + i) & 511]; }

void sndFeed(const int16_t* s, int n) {
  if (!ring) return;
  int peak = 0;
  for (int i = 0; i < n; i++) {
    if ((uint32_t)(rw - rr) >= RING - 1) rr = rw - RING / 2;   // too far behind: drop the old part
    ring[rw % RING] = s[i]; rw++;
    int a = abs(s[i]); if (a > peak) peak = a;
  }
  S.audLevel = peak * 100 / 20000 > 100 ? 100 : peak * 100 / 20000;
  S.audAt = millis();
}

static volatile int forceVol = -1;
void sndForceVolume(int v) { forceVol = v; }
static void task(void*) {
  static int16_t buf[256 * 2];
  bool pa = false;
  for (;;) {
    if (stopAll) { portENTER_CRITICAL(&mux); for (Voice& v : vs) v.on = false; portEXIT_CRITICAL(&mux); stopAll = false; }
    int vol = forceVol >= 0 ? forceVol : cfg.vol;
    int want = vol ? 45 + vol * 55 / 100 : 0;
    if (want != codecVol && codec) { es8311_voice_volume_set(codec, want, nullptr); codecVol = want; }
    bool any = false;
    // a stream from the phone: wait until 120 ms are in, so it doesn't stutter
    bool stream = (uint32_t)(rw - rr) > PHONE_RATE * 12 / 100 || ((uint32_t)(rw - rr) > 0 && millis() - S.audAt > 300);
    bool media = mring && !mpause && mr != mw;
    for (int i = 0; i < 256; i++) {
      float m = 0;
      portENTER_CRITICAL(&mux);
      for (Voice& v : vs) {
        if (!v.on) continue;
        if (v.delay) { v.delay--; continue; }
        float t = (float)v.pos / v.len, f = v.f0 + (v.f1 - v.f0) * t;
        v.ph += f / RATE; if (v.ph >= 1) v.ph -= 1;
        float env = 1.0f - t; env *= env;
        float smp;
        switch (v.w) {
          case W_SQ: smp = v.ph < 0.5f ? 1 : -1; break;
          case W_SIN: smp = sinf(v.ph * 6.2832f); break;
          case W_SAW: smp = v.ph * 2 - 1; break;
          default: noiseS ^= noiseS << 13; noiseS ^= noiseS >> 17; noiseS ^= noiseS << 5;
            smp = ((int32_t)noiseS) / 2147483648.0f;
            v.lp += (smp - v.lp) * (v.f0 / 8000.0f); smp = smp - v.lp;   // a rough high-pass: a click, not a hiss
        }
        m += smp * v.vol * env;
        if (++v.pos >= v.len) v.on = false;
        any = true;
      }
      portEXIT_CRITICAL(&mux);
      if (stream && rr != rw) {                 // 16 kHz -> 44.1 kHz: between two phone samples
        int16_t a = ring[rr % RING], b = (uint32_t)(rr + 1 - rw) ? ring[(rr + 1) % RING] : a;
        m += (a + (b - a) * (int32_t)(phPhase & 0xFFFF) / 65536) / 32768.0f * 0.9f;
        phPhase += (PHONE_RATE << 16) / RATE;
        while (phPhase >= 65536 && rr != rw) { phPhase -= 65536; rr++; }
        any = true;
      }
      float ml = m, mrr = m;
      if (media && mr != mw) {
        uint32_t k = (mr % MRING) * 2;
        ml += mring[k] / 32768.0f; mrr += mring[k + 1] / 32768.0f;
        wave[wpos & 511] = (mring[k] + mring[k + 1]) / 2; wpos++;
        mr++; mplayed++; any = true;
      }
      if (ml > 1) ml = 1; if (ml < -1) ml = -1; if (mrr > 1) mrr = 1; if (mrr < -1) mrr = -1;
      buf[i * 2] = (int16_t)(ml * 30000); buf[i * 2 + 1] = (int16_t)(mrr * 30000);
    }
    if (any) {
      lastSound = millis();
      if (!pa) { digitalWrite(PIN_PA, HIGH); pa = true; }
    } else if (pa && millis() - lastSound > 600) { digitalWrite(PIN_PA, LOW); pa = false; }
    i2s.write((uint8_t*)buf, sizeof(buf));
  }
}

bool sndBegin() {
  pinMode(PIN_PA, OUTPUT);
  digitalWrite(PIN_PA, LOW);
  i2s.setPins(PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT, PIN_I2S_DIN, PIN_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) return false;
  codec = es8311_create(0, ES8311_ADDRRES_0);
  if (!codec) return false;
  const es8311_clock_config_t clk = {false, false, true, RATE * 256, RATE};
  if (es8311_init(codec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
  es8311_sample_frequency_config(codec, clk.mclk_frequency, clk.sample_frequency);
  es8311_microphone_config(codec, false);
  ring = (int16_t*)heap_caps_malloc(RING * 2, MALLOC_CAP_SPIRAM);
  mring = (int16_t*)heap_caps_malloc(MRING * 4, MALLOC_CAP_SPIRAM);
  ok = true;
  xTaskCreatePinnedToCore(task, "snd", 4096, nullptr, 3, nullptr, 1);
  return true;
}
