// The 320x172 screen: an index buffer (one byte per pixel) turned into colours while it is sent.
#pragma once
#include <stdint.h>

#define LCD_W 320
#define LCD_H 172

void lcdBegin(bool flip);
void lcdFlip(bool flip);                     // turn the picture round (USB plug left or right)
void lcdPush(const uint8_t* fb, const uint16_t* lut, const uint16_t* lutDim);   // lutDim != null: scanlines
void lcdBacklight(int percent);              // 0 = off
int lcdBacklightPin();
uint16_t lcdColor(int r, int g, int b);      // RGB565, byte-swapped for the wire
