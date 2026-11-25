// sound.cpp: see sound.h. 16 kHz, 16 bit, a small mixer task feeding I2S.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include <math.h>
#include "../core/sys.h"
#include "sound.h"
extern "C" {
#include "es8311.h"
}

#define RATE 16000
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
static const int RING = RATE * 2;
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

static void task(void*) {
  static int16_t buf[256 * 2];
  bool pa = false;
  for (;;) {
    if (stopAll) { portENTER_CRITICAL(&mux); for (Voice& v : vs) v.on = false; portEXIT_CRITICAL(&mux); stopAll = false; }
    int want = cfg.vol ? 45 + cfg.vol * 55 / 100 : 0;
    if (want != codecVol && codec) { es8311_voice_volume_set(codec, want, nullptr); codecVol = want; }
    bool any = false;
    // a stream from the phone: wait until 120 ms are in, so it doesn't stutter
    bool stream = (uint32_t)(rw - rr) > RATE * 12 / 100 || ((uint32_t)(rw - rr) > 0 && millis() - S.audAt > 300);
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
      if (stream && rr != rw) { m += ring[rr % RING] / 32768.0f * 0.9f; rr++; any = true; }
      if (m > 1) m = 1; if (m < -1) m = -1;
      int16_t s = (int16_t)(m * 30000);
      buf[i * 2] = s; buf[i * 2 + 1] = s;
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
  ok = true;
  xTaskCreatePinnedToCore(task, "snd", 4096, nullptr, 3, nullptr, 1);
  return true;
}
