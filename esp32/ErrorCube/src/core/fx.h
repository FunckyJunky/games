// fx.h: picture effects for the finished frame: scanlines and glitches, all on fb itself (no extra memory).
// Plain C++ (also in the simulator).
#pragma once
#include <stdint.h>

void fxPost(bool fringe, bool scan);       // the last pass: scanlines on top (fringe: gone in 1.5)

// glitches (on fb)
void fxRgbSplit(int dx);
void fxVhs(uint32_t seed, float phase);
void fxInterlace(int dx);
void fxStretch(uint32_t seed);
void fxSignalLoss(uint32_t seed);
