#!/bin/sh
# Builds the PC simulator for DREAM-CUBE (sim/dreamsim): ERROR-CUBE's board fakes and interface, DREAM-CUBE's screens and
# 3D (../DreamCube/src/dream) with its listening and error.blue faked (simdream.cpp).
#   sh sim/build-dream.sh && printf 'wait 8000\nshot dream\n' | ./sim/dreamsim --out /tmp/dshots
cd "$(dirname "$0")/.."
D=../DreamCube/src/dream
g++ -O2 -std=c++17 -Wno-unused-result -DDREAM_SIM -include $D/dream_config.h -I$HOME/Arduino/libraries/ArduinoJson/src -o sim/dreamsim \
  sim/sim.cpp sim/simsys.cpp sim/simdream.cpp \
  src/core/gfx.cpp src/core/g3d.cpp src/core/fx.cpp src/core/holo.cpp src/core/img.cpp src/core/media.cpp src/ui/ui.cpp src/ui/buttons.cpp \
  $D/screens.cpp $D/r3d.cpp $D/models.cpp $D/scene.cpp -x c src/lib/cubeqr.c -x none -lm -lz
