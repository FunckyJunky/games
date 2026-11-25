// font_types.h: what fonts.h is made of (see tools/mkfont.py)
#pragma once
#include <stdint.h>
struct CubeGlyph { uint16_t cp; int8_t x, y; uint8_t w, h; uint32_t off; };
struct CubeFont { uint8_t cw, ch, base; uint16_t n; const CubeGlyph* g; const uint8_t* data; };
