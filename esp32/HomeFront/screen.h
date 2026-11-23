// What HOME FRONT draws. The same drawing as public/thuisfront/dev/render.js on error.blue,
// so the phone's preview and the browser screen look like the board.
#pragma once
#include <Arduino.h>
#include "lcd.h"

enum { IX_T = 0, IX_V = 16, IX_U = 32, IX_BLACK = 48, IX_RED = 49, IX_WHITE = 50 };
enum Anim : uint8_t { A_STATIC, A_SCROLL, A_RISE, A_TYPE, A_BLINK, A_WAVE, A_GLITCH, A_PULSE };
enum Mode : uint8_t { M_TEXT, M_VIDEO, M_BOTH };

struct TextPic {                 // HFT1, unpacked: one byte per pixel (level 0-15), in PSRAM
  uint16_t w = 0, h = 0;
  int16_t x = 0, y = 0;
  uint16_t n = 0;
  uint16_t* boxes = nullptr;     // n x (x, y, w, h)
  uint8_t* px = nullptr;
};
TextPic* textDecode(const uint8_t* d, size_t len);   // nullptr if broken
void textFree(TextPic* t);

struct CommSet { uint8_t mode = M_TEXT, anim = A_STATIC, speed = 5, tpal = 0, vpal = 0, bg = 0; };

struct ScreenInfo {
  char id[16] = "", name[24] = "", wifi[34] = "", ip[20] = "", ap[24] = "";
  bool online = false;
};

extern uint8_t* FB;              // 320 x 172 colour numbers

void scrBegin();
void scrFlush(bool scanlines);
// the parts
void scrFill(int x, int y, int w, int h, uint8_t c);
void scrText(const char* s, int x, int y, int k, uint8_t c);
void scrCenter(const char* s, int y, int k, uint8_t c);
int scrTextW(const char* s, int k);
void scrLine(int x0, int y0, int x1, int y1, uint8_t c);
// whole screens
void scrComm(const CommSet& c, const TextPic* t, const uint8_t* frame, int fw, int fh, bool fresh, float tA, uint32_t nowMs);
void scrIdle(float t, const ScreenInfo& in);
void scrConnected(float t, const ScreenInfo& in);
void scrConnecting(float t, const ScreenInfo& in);
void scrBoot(float t, const char* const* lines, int n);
void scrSetup(float t, const ScreenInfo& in, const char* status);
void scrJoining(float t, const ScreenInfo& in, const char* ssid, bool lowPower);
void scrInfoBox(const ScreenInfo& in, const char* extra);
void scrBanner(const char* s);
void scrOfflineDot(uint32_t nowMs);
