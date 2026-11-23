// The 1.47B board's QMI8658 motion sensor on I2C (SDA 48 / SCL 47). Only the accelerometer is used: the movement is
// how far the acceleration is from its slow average (gravity), so tilting, picking up, shaking and tapping all count.
#include <Arduino.h>
#include <Wire.h>
#include "lcd.h"
#include "motion.h"

static uint8_t addr = 0;           // 0 = no sensor
static float gx, gy, gz, last = 0;
static bool haveAvg = false;
static uint32_t lastRead = 0;

static bool wr(uint8_t r, uint8_t v) { Wire.beginTransmission(addr); Wire.write(r); Wire.write(v); return Wire.endTransmission() == 0; }
static int rd(uint8_t r, uint8_t* b, int n) {
  Wire.beginTransmission(addr); Wire.write(r);
  if (Wire.endTransmission(false) != 0) return 0;
  int got = Wire.requestFrom((int)addr, n);
  for (int i = 0; i < got; i++) b[i] = Wire.read();
  return got;
}

bool motionBegin() {
  if (lcdBacklightPin() != 46) return false;     // the plain 1.47: no sensor, and GPIO 48 is its backlight
  Wire.begin(48, 47, 400000);
  for (uint8_t a : { 0x6B, 0x6A }) {
    addr = a;
    uint8_t who = 0;
    if (rd(0x00, &who, 1) == 1 && who == 0x05) break;   // WHO_AM_I
    addr = 0;
  }
  if (!addr) { Serial.println("MOTION: no QMI8658"); return false; }
  wr(0x60, 0xB0); delay(30);                     // reset
  wr(0x02, 0x40);                                // CTRL1: address auto-increment, little-endian data
  wr(0x03, 0x17);                                // CTRL2: accelerometer +-4 g, 62.5 per second
  wr(0x08, 0x01);                                // CTRL7: accelerometer on (gyro off)
  delay(30);
  Serial.printf("MOTION: QMI8658 at 0x%02X\n", addr);
  return true;
}

float motionPoll() {
  if (!addr) return -1;
  uint32_t now = millis();
  if (now - lastRead < 25) return last;
  lastRead = now;
  uint8_t b[6];
  if (rd(0x35, b, 6) != 6) return last;          // AX_L .. AZ_H
  float x = (int16_t)(b[0] | b[1] << 8) / 8192.0f, y = (int16_t)(b[2] | b[3] << 8) / 8192.0f, z = (int16_t)(b[4] | b[5] << 8) / 8192.0f;
  if (!haveAvg) { gx = x; gy = y; gz = z; haveAvg = true; }
  float dx = x - gx, dy = y - gy, dz = z - gz;
  last = sqrtf(dx * dx + dy * dy + dz * dz);
  gx += (x - gx) * 0.05f; gy += (y - gy) * 0.05f; gz += (z - gz) * 0.05f;   // gravity follows in about a second
  return last;
}

float motionThreshold(int sens) { return 0.5f * powf(0.06f, (constrain(sens, 1, 10) - 1) / 9.0f); }   // 0.5 g .. 0.03 g
