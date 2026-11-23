// The 1.47B board's QMI8658 motion sensor: how much the board is being moved (CLOCK: screen on when moved).
#pragma once
bool motionBegin();                // true = a sensor answered (only on the 1.47B: on the 1.47 pin 48 is the backlight)
float motionPoll();                // call often: the movement now in g (0 = still), -1 = no sensor
float motionThreshold(int sens);   // sensitivity 1 (a shake) .. 10 (a touch) -> g (as motionThreshold in render.js)
