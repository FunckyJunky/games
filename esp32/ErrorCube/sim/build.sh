#!/bin/sh
# Builds the PC simulator (sim/cubesim) from the same source files as the cube.
# PICO-8 (src/p8: FAKE-08 + z8lua) is big: its objects are kept in sim/obj and only rebuilt when changed.
cd "$(dirname "$0")/.."
mkdir -p sim/obj
P8OBJ=""
for f in src/p8/*.cpp src/p8/core/*.cpp src/p8/lua/*.cpp; do
  o=sim/obj/$(echo "$f" | tr / _).o
  NEW=0; [ ! -f "$o" ] && NEW=1
  for h in src/core/sys.h src/p8/*.h src/p8/core/*.h src/p8/lua/*.h; do [ -f "$o" ] && [ "$h" -nt "$o" ] && NEW=1; done
  if [ $NEW = 1 ] || [ "$f" -nt "$o" ]; then g++ -O2 -std=c++17 -w -c "$f" -o "$o" || exit 1; fi
  P8OBJ="$P8OBJ $o"
done
g++ -O2 -std=c++17 -Wno-unused-result -o sim/cubesim sim/sim.cpp sim/simsys.cpp \
  src/core/gfx.cpp src/core/g3d.cpp src/core/fx.cpp src/core/holo.cpp src/core/img.cpp src/core/media.cpp src/ui/ui.cpp src/ui/buttons.cpp src/apps/*.cpp src/js/jsapp.cpp -x c src/js/duktape.c src/lib/cubeqr.c -x none $P8OBJ -lm -lz
