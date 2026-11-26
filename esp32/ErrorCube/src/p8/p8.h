// p8.h: the cube's PICO-8 player. The emulator is FAKE-08 (MIT, github.com/jtothebell/fake-08) on z8lua (MIT),
// in src/p8/core and src/p8/lua, trimmed for the ESP32 (no iostreams, regex, lodepng, zip, ini, save states).
// On the board the cart runs in its own task on core 0 (60 steps a second, sound into the media stream at
// 22.05 kHz); the screen (apps_p8.cpp) shows the latest frame and passes the buttons on. In the simulator the
// screen's tick drives it (p8Pump).
#pragma once
#include <stdint.h>

// buttons, PICO-8's numbering
enum { P8_LEFT = 1, P8_RIGHT = 2, P8_UP = 4, P8_DOWN = 8, P8_O = 16, P8_X = 32, P8_PAUSE = 64 };

bool p8Start(const char* path);          // loads the cart and starts it (false: p8Error says why)
void p8Stop();                           // ends it (waits for the task)
bool p8Running();                        // false once it ended by itself (an error, or the cart's menu: exit)
const char* p8Error();                   // why it couldn't start / why it stopped ("" = it was asked to)
void p8Buttons(uint8_t held);            // what's held now (P8_*)
void p8Pause(bool on);                   // stops the cart (and its sound) while the cube shows its own menu
void p8Pump();                           // the simulator: one step (the board has its own task)
// the latest picture: 128x128 RGB565, and a number that changes with every new one
const uint16_t* p8Frame(uint32_t* seq);
int p8Fps();                             // steps a second it actually manages
int p8StackLeft();                       // bytes of the cart task's stack never used (-1: not known)
