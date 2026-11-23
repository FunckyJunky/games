// The 320x172 screen: an index buffer (one byte per pixel) turned into colours while it is sent.
#pragma once
#include <stdint.h>

#define LCD_W 320
#define LCD_H 172

void lcdBegin(bool flip);
void lcdFlip(bool flip);                     // turn the picture round (USB plug left or right)
// lutDim != null: scanlines. plane != null: pixels with colour number 255 take their colour from it (photo / colour video)
void lcdPush(const uint8_t* fb, const uint16_t* lut, const uint16_t* lutDim, const uint16_t* plane);
void lcdBacklight(int percent);              // 0 = off
int lcdBacklightPin();
uint16_t lcdColor(int r, int g, int b);      // RGB565 (the panel is set to take it low byte first, as it sits in memory)
