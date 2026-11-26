// es7210.h: the ES7210 (4-channel microphone ADC) on the cube's I2C bus at 0x40.
// Based on Espressif's es7210 driver (esp-bsp/components/es7210, Apache-2.0, SPDX: 2022 Espressif Systems):
// the same registers and clock table, talking through Arduino's I2C (Wire's bus) like board/es8311.c does.
#pragma once
#include <stdint.h>

#define ES7210_ADDR 0x40

bool es7210Begin(uint32_t rate, uint32_t mclk, int gainStep);   // I2S slave, standard I2S, 16 bit: MIC1 left, MIC2 right
bool es7210Gain(int gainStep);                                    // 0..14: 0, 3, 6 ... 33, 34.5, 36, 37.5 dB
