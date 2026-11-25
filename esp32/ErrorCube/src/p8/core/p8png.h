// p8png.h (ERROR-CUBE): the PNG decoder for .p8.png carts, instead of lodepng.
// 8-bit RGB / RGBA, not interlaced (what PICO-8 writes). Inflate: the ESP32-S3's ROM (miniz), zlib in the simulator.
#pragma once
#include <vector>
#include <stddef.h>
// 0 = ok; rgba: 4 bytes a pixel
unsigned p8png_decode(std::vector<unsigned char>& rgba, unsigned& w, unsigned& h, const unsigned char* data, size_t size);
const char* p8png_error_text(unsigned e);
