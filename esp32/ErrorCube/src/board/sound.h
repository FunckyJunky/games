// sound.h: the speaker (ES8311 codec + amplifier). Every sound is made up on the spot, like on error.blue,
// plus the sound that the control page streams to the cube.
#pragma once
#include <stdint.h>

bool sndBegin();                              // false = no codec found
void sndFeed(const int16_t* s, int n);        // 16 kHz mono from the phone
