// What HOME FRONT draws. The same drawing as public/thuisfront/dev/render.js on error.blue,
// so the phone's preview and the browser screen look like the board.
#pragma once
#include <Arduino.h>
#include "lcd.h"
#include <time.h>

enum { IX_T = 0, IX_V = 16, IX_U = 32, IX_BLACK = 48, IX_RED = 49, IX_WHITE = 50, IX_PLANE = 255 };   // 255 = colour picture
// the ETA train scene (real colours): yellow and blue train, glass, rails, dark grey, brick, lamps, platform
enum { IX_NSY = 51, IX_NSB, IX_GLASS, IX_RAIL, IX_DARK, IX_BRICK, IX_LAMP, IX_PLAT, IX_ROOF, IX_LEAF, IX_LEAFD, IX_TRUNK, IX_CREAM };   // 51-63
enum Anim : uint8_t { A_STATIC, A_SCROLL, A_RISE, A_TYPE, A_BLINK, A_WAVE, A_GLITCH, A_PULSE };
enum Mode : uint8_t { M_TEXT, M_VIDEO, M_BOTH, M_PHOTO };
#define NOFILTER 4                // vpal / ppal 4 = no filter, real colours

struct TextPic {                 // HFT1, unpacked: one byte per pixel (level 0-15), in PSRAM
  uint16_t w = 0, h = 0;
  int16_t x = 0, y = 0;
  uint16_t n = 0;
  uint16_t* boxes = nullptr;     // n x (x, y, w, h)
  uint8_t* px = nullptr;
};
TextPic* textDecode(const uint8_t* d, size_t len);   // nullptr if broken
void textFree(TextPic* t);

enum App : uint8_t { APP_NONE, APP_COMM, APP_CLOCK, APP_ETA, APP_FOX };
// FOX: colour; ax/sg = which sensor axis (0 x, 1 y, 2 z) and sign gives the screen's right, up and out (SET UP on the phone)
struct FoxSet { uint8_t pal = 0; int8_t ax[3] = { 0, 1, 2 }, sg[3] = { 1, 1, 1 }; };
struct ClockSet { uint8_t pal = 0, bg = 0, h24 = 1, sec = 1, date = 1, motion = 0, msens = 5; uint16_t mtime = 60; };   // motion 1 = dark until moved, then on mtime s   // sec 0 = none, 1 = row of 60 dots, 2 = HH:MM:SS
struct EtaSet { uint32_t from = 0, to = 0; uint8_t pal = 0, done = 0, lang = 0; char text[128] = ""; };   // lang 0 = English, 1 = Ukrainian   // text: UTF-8, up to 60 letters   // ETA: unix seconds, description
struct CommSet { uint8_t mode = M_TEXT, anim = A_STATIC, speed = 5, tpal = 0, vpal = 0, bg = 0, ppal = NOFILTER, motion = 0, msens = 5; uint16_t mtime = 60; };   // motion: as ClockSet

struct Media {                   // what there is to show besides the text
  const uint8_t* gray = nullptr; int fw = 0, fh = 0; uint32_t grayAt = 0;   // grey video frame (4-bit), for the DOS filters
  const uint16_t* color = nullptr; uint32_t colorAt = 0;                    // colour video frame, 320x172 RGB565
  const uint16_t* photo = nullptr;                                          // the photo, 320x172 RGB565 (filter already in)
};

struct ScreenInfo {
  char id[16] = "", name[24] = "", wifi[34] = "", ip[20] = "", ap[24] = "";
  bool online = false;
};

extern uint8_t* FB;              // 320 x 172 colour numbers
extern bool scrEffects;          // false: all effects off (still text, no glitches, no blinking, no noise)
#define STILL_T 5.3f             // effects off: the idle screen stands still at this moment

void scrBegin();
void scrFlush(bool scanlines, const uint16_t* plane);
void scrPhotoRamp(int pal, uint16_t* out256);       // brightness 0-255 -> a DOS look's colour (for photos)
// the parts
void scrFill(int x, int y, int w, int h, uint8_t c);
void scrTextRamp(int pal);       // text levels 0-15 in a DOS colour on black
void scrText(const char* s, int x, int y, int k, uint8_t c);
void scrCenter(const char* s, int y, int k, uint8_t c);
int scrTextW(const char* s, int k);
void scrLine(int x0, int y0, int x1, int y1, uint8_t c);
// whole screens
// returns the colour picture to show under the text (or nullptr)
const uint16_t* scrComm(const CommSet& c, const TextPic* t, const Media& m, float tA, uint32_t nowMs);
void scrClock(const ClockSet& c, const struct tm* t, int ms);   // t = nullptr: no time yet
void scrEta(const EtaSet& e, int64_t nowMs, bool known, const struct tm* toTm, const struct tm* fromTm);
void scrUpdate(float t, int pct, const char* msg);              // new firmware coming in over WiFi
void scrIdle(float t, const ScreenInfo& in);
void scrConnected(float t, const ScreenInfo& in);
void scrConnecting(float t, const ScreenInfo& in);
void scrBoot(float t, const char* const* lines, int n);
void scrSetup(float t, const ScreenInfo& in, const char* status);
void scrJoining(float t, const ScreenInfo& in, const char* ssid, bool lowPower);
void scrInfoBox(const ScreenInfo& in, const char* extra);
void scrBanner(const char* s);
void scrOfflineDot(uint32_t nowMs);
