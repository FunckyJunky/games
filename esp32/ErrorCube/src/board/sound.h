// sound.h: the speaker (ES8311 codec + amplifier). Every sound is made up on the spot, like on error.blue,
// plus the sound that the control page streams to the cube.
#pragma once
#include <stdint.h>
#include <stddef.h>

bool sndBegin();                              // false = no codec found
void sndFeed(const int16_t* s, int n);        // 16 kHz mono from the phone
size_t sndMicRead(void* buf, size_t bytes, uint32_t ms);   // the microphones' I2S data (EAR-CUBE): stereo frames at sndRate()
int sndRate();
