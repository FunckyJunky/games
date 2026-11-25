// holo.h: HOLO 3D. The cube's tilt (motion sensor) makes the picture look deep, like a hologram:
// every part of the interface sits on a depth layer and moves by its own amount when you tilt the cube,
// raised parts cast shadows and show their sides, a backdrop sits far behind, and the glass in front
// (the curved lens, its colour fringe, a light reflection, the perspective) moves the other way.
// Plain C++ (also in the simulator). Settings: cfg.holo* (SETUP > HOLO 3D).
#pragma once
#include <stdint.h>

// depth layers (0 = the text plane; + toward you, - behind)
#define H_BACK   -3.0f   // the backdrop (dots, grid, stars, room)
#define H_PANEL  -1.0f   // box frames behind the text
#define H_TEXT    0.0f   // lists, text, most of a program
#define H_RAISE   0.45f  // the selection bar, buttons
#define H_CHROME  1.0f   // the status bar and the F-key bar
float holoOverH();       // dialogs, the keyboard's keys, the quick panel, messages: in front of everything

void holoInit();
bool holoTick();                 // every loop: read the motion sensor; true = draw again (the picture moved)
bool holoActive();               // HOLO is on now (setting on, not saver, screen awake)
bool holoLayersOn();             // depth layers are on (not for full-screen programs like PAINT)
float holoTX();                  // the tilt now, -1..1 (after range, dead zone, invert, smoothing)
float holoTY();
void holoRecenter();             // the way you hold it now is "straight"
void holoFrameStart(bool flat);  // uiFrame: a new picture (flat = no layers: full-screen programs)

float holoSet(float h);          // what's drawn next sits on depth h; returns the depth before
float holoAt();                  // the depth now
void holoShift(float h, int& dx, int& dy);   // how far depth h moves on the screen now
struct HoloZ { float p; explicit HoloZ(float h) : p(holoSet(h)) {} ~HoloZ() { holoSet(p); } };

void holoShadow(int x, int y, int w, int h, float above);                 // a box raised 'above' layers casts a shadow
void holoWalls(int x, int y, int w, int h, uint16_t c, float depth, bool filled);   // the sides of a box, 'depth' layers deep
void holoBackdrop();             // the wallpaper, far behind (cfg.holoBack: glass, dots, grid, stars, room, picture)
bool holoWallOk();               // the wallpaper picture could be shown
void holoGlass();                // hand the glass effects (lens shift, perspective, glare, rainbow) to fxPost
void holoUnmap(int& x, int& y);  // a touch on the screen -> where it is on the text plane (layers only)
int holoFps();                   // frames drawn per second (HOLO TEST)
