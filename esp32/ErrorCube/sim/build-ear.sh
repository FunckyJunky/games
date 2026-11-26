#!/bin/sh
# Builds the PC simulator for EAR-CUBE (sim/earsim): ERROR-CUBE's board fakes and interface, EAR-CUBE's screens
# (../EarCube/src/ear) with its microphones, scribe and error.blue faked (simear.cpp).
#   sh sim/build-ear.sh && printf 'wait 2600\nshot live\n' | ./sim/earsim --out /tmp/eshots
cd "$(dirname "$0")/.."
g++ -O2 -std=c++17 -Wno-unused-result -DEAR_SIM -include ../EarCube/src/ear/ear_config.h -o sim/earsim sim/sim.cpp sim/simsys.cpp sim/simear.cpp \
  src/core/gfx.cpp src/core/g3d.cpp src/core/fx.cpp src/core/holo.cpp src/core/img.cpp src/core/media.cpp src/ui/ui.cpp src/ui/buttons.cpp \
  ../EarCube/src/ear/screens.cpp ../EarCube/src/ear/common.cpp -x c src/lib/cubeqr.c -x none -lm -lz
