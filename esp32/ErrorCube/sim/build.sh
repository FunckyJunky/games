#!/bin/sh
# Builds the PC simulator (sim/cubesim) from the same source files as the cube.
cd "$(dirname "$0")/.."
g++ -O2 -std=c++17 -Wno-unused-result -o sim/cubesim sim/sim.cpp sim/simsys.cpp \
  src/core/gfx.cpp src/core/g3d.cpp src/core/fx.cpp src/core/holo.cpp src/core/img.cpp src/core/media.cpp src/ui/ui.cpp src/ui/buttons.cpp src/apps/*.cpp src/js/jsapp.cpp -x c src/js/duktape.c src/lib/cubeqr.c -x none -lm -lz
