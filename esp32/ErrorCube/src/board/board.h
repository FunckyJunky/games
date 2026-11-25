// board.h: the Waveshare ESP32-S3-Touch-AMOLED-2.16 itself: screen, touch, buttons, power chip, clock chip,
// motion sensor, TF card and the cube's own flash drive. Sound is in sound.h.
#pragma once
#include <stdint.h>

void boardBegin();             // power, screen, touch, motion, clock, buttons; fills S.touch, S.pmu ...
void boardDrives();            // drive C: (formatted the first time), the TF card, the speaker
void boardPoll();              // touch, buttons, power key, battery, motion: feeds uiTouch / uiKey
void boardShow();              // put fb on the panel (only the rows that changed), turned by cfg.rot
void boardCrt(int dir);        // the CRT power-on (1) / power-off (-1) animation, from fb
void boardPanel(bool on);
void boardSetRtcFromSystem();  // after the internet time came in
void boardSlow(bool slow);     // battery saver / screen off: slower CPU
