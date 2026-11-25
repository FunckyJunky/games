// fx.h: picture effects for the finished frame: the CRT lens (curved glass + colour fringe), glitches,
// and the cube-turn transition between screens. Plain C++ (also in the simulator).
#pragma once
#include <stdint.h>

// lens: 0 flat, 1 curved (the middle stays 1:1, the edges bend), 2 fisheye (the middle bulges)
void fxLensSetup(int mode);
void fxPost(bool fringe, bool scan);       // the last pass: fb -> fb through the lens (if on), scanlines on top;
                                           // afterwards fxSpare() holds the frame before it (for transitions)
void fxLensTouch(int& x, int& y);          // where a touch on the curved picture really is
int fxLensMode();
// HOLO glass, for the next fxPost: the lens centre moves by (lx, ly) px, the picture leans (ax, ay: perspective,
// about -0.16..0.16), the colour fringe moves by (rbx, rby) px, a soft light reflection at (gx, gy) with strength ga (0..32)
void fxHolo(int lx, int ly, float ax, float ay, int rbx, int rby, int gx, int gy, int ga);

// glitches (on fb)
void fxRgbSplit(int dx);
void fxVhs(uint32_t seed, float phase);
void fxInterlace(int dx);
void fxDatamosh(uint32_t seed);
void fxStretch(uint32_t seed);
void fxSignalLoss(uint32_t seed);

// the cube turns from one screen to the next: dir 0 up, 1 down, 2 left, 3 right; t 0..1
void fxCube(const uint16_t* from, const uint16_t* to, float t, int dir, uint16_t* out);
uint16_t* fxBuf(int i);                    // full-screen work buffers 0..1 (PSRAM)
uint16_t* fxSpare();                       // the lens's other buffer: after fxLens it holds the frame before the lens
