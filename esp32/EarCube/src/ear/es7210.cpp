// es7210.cpp: see es7210.h. Register sequence and clock table from Espressif's es7210 driver
// (esp-bsp/components/es7210/es7210.c, Apache-2.0, SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD).
#include <Arduino.h>
#include "esp32-hal-i2c.h"
#include "es7210.h"

// registers
enum {
  R_RESET = 0x00, R_CLOCK_OFF = 0x01, R_MAINCLK = 0x02, R_MASTER_CLK = 0x03, R_LRCK_DIVH = 0x04, R_LRCK_DIVL = 0x05,
  R_POWER_DOWN = 0x06, R_OSR = 0x07, R_MODE = 0x08, R_TIME0 = 0x09, R_TIME1 = 0x0A, R_SDP1 = 0x11, R_SDP2 = 0x12,
  R_ADC34_HPF2 = 0x20, R_ADC34_HPF1 = 0x21, R_ADC12_HPF2 = 0x22, R_ADC12_HPF1 = 0x23, R_ANALOG = 0x40,
  R_MIC12_BIAS = 0x41, R_MIC34_BIAS = 0x42, R_MIC1_GAIN = 0x43, R_MIC2_GAIN = 0x44, R_MIC3_GAIN = 0x45, R_MIC4_GAIN = 0x46,
  R_MIC1_POWER = 0x47, R_MIC2_POWER = 0x48, R_MIC3_POWER = 0x49, R_MIC4_POWER = 0x4A, R_MIC12_POWER = 0x4B, R_MIC34_POWER = 0x4C,
};

// clock coefficients: mclk, lrck, adc_div, dll, doubler, osr, lrck_h, lrck_l
struct Coeff { uint32_t mclk, lrck; uint8_t adcDiv, dll, doubler, osr, lrckH, lrckL; };
static const Coeff COEFF[] = {
  {12288000, 8000, 0x03, 0x01, 0x00, 0x20, 0x06, 0x00}, {16384000, 8000, 0x04, 0x01, 0x00, 0x20, 0x08, 0x00},
  {4096000, 8000, 0x01, 0x01, 0x00, 0x20, 0x02, 0x00},  {11289600, 11025, 0x02, 0x01, 0x00, 0x20, 0x01, 0x00},
  {12288000, 12000, 0x02, 0x01, 0x00, 0x20, 0x04, 0x00}, {4096000, 16000, 0x01, 0x01, 0x01, 0x20, 0x01, 0x00},
  {16384000, 16000, 0x02, 0x01, 0x00, 0x20, 0x04, 0x00}, {12288000, 16000, 0x03, 0x01, 0x01, 0x20, 0x03, 0x00},
  {11289600, 22050, 0x01, 0x01, 0x00, 0x20, 0x02, 0x00}, {12288000, 24000, 0x01, 0x01, 0x00, 0x20, 0x02, 0x00},
  {12288000, 32000, 0x03, 0x00, 0x00, 0x20, 0x01, 0x80}, {16384000, 32000, 0x01, 0x01, 0x00, 0x20, 0x02, 0x00},
  {11289600, 44100, 0x01, 0x01, 0x01, 0x20, 0x01, 0x00}, {12288000, 48000, 0x01, 0x01, 0x01, 0x20, 0x01, 0x00},
};

static bool wr(uint8_t reg, uint8_t v) {
  const uint8_t b[2] = {reg, v};
  return i2cWrite(0, ES7210_ADDR, b, 2, 100) == ESP_OK;
}

bool es7210Gain(int step) {
  if (step < 0) step = 0; if (step > 14) step = 14;
  bool ok = true;
  for (uint8_t r = R_MIC1_GAIN; r <= R_MIC4_GAIN; r++) ok &= wr(r, (uint8_t)step | 0x10);
  return ok;
}

bool es7210Begin(uint32_t rate, uint32_t mclk, int gainStep) {
  const Coeff* c = nullptr;
  for (const Coeff& k : COEFF) if (k.lrck == rate && k.mclk == mclk) c = &k;
  if (!c) return false;
  if (!wr(R_RESET, 0xFF)) return false;          // nobody at 0x40: no microphones
  bool ok = true;
  ok &= wr(R_RESET, 0x32);
  ok &= wr(R_TIME0, 0x30); ok &= wr(R_TIME1, 0x30);              // start-up times
  ok &= wr(R_ADC12_HPF1, 0x2A); ok &= wr(R_ADC12_HPF2, 0x0A);    // high-pass: no DC, no rumble
  ok &= wr(R_ADC34_HPF1, 0x2A); ok &= wr(R_ADC34_HPF2, 0x0A);
  ok &= wr(R_SDP1, 0x60 | 0x00);                                  // 16 bit, standard I2S
  ok &= wr(R_SDP2, 0x00);                                         // no TDM: MIC1 + MIC2 on SDOUT1 (left, right)
  ok &= wr(R_ANALOG, 0xC3);                                       // analog power, VMID
  ok &= wr(R_MIC12_BIAS, 0x70); ok &= wr(R_MIC34_BIAS, 0x70);    // mic bias 2.87 V
  ok &= es7210Gain(gainStep);
  for (uint8_t r = R_MIC1_POWER; r <= R_MIC4_POWER; r++) ok &= wr(r, 0x08);
  ok &= wr(R_OSR, c->osr);
  ok &= wr(R_MAINCLK, c->adcDiv | (c->doubler << 6) | (c->dll << 7));
  ok &= wr(R_LRCK_DIVH, c->lrckH); ok &= wr(R_LRCK_DIVL, c->lrckL);
  ok &= wr(R_POWER_DOWN, 0x04);                                   // DLL off
  ok &= wr(R_MIC12_POWER, 0x0F); ok &= wr(R_MIC34_POWER, 0x0F);  // mic bias, ADC and PGA on
  ok &= wr(R_RESET, 0x71); ok &= wr(R_RESET, 0x41);               // go
  return ok;
}
