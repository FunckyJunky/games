// img.h: pictures from files (JPEG, 24/32-bit BMP) into RGB565 boxes. Plain C++ (also in the simulator).
#pragma once
#include <stdint.h>
#include <stddef.h>
bool imgIsPicture(const char* name);               // .jpg .jpeg .bmp
// the picture at path into out (ow x oh); mode IMG_STRETCH / IMG_FIT / IMG_COVER (sys.h). err gets a reason
bool imgLoad(const char* path, uint16_t* out, int ow, int oh, int mode, char* err = nullptr, int* iw = nullptr, int* ih = nullptr);
