#!/bin/sh
# Builds the PC simulator for PICO-CUBE (sim/picosim): ERROR-CUBE's board fakes and interface, PICO-CUBE's screens
# (../PicoCube/src/pico) and the PICO-8 emulator (attic/p8 = PicoCube/src/p8).
#   sh sim/build-pico.sh && printf 'wait 3000\nshot home\n' | ./sim/picosim --out /tmp/pshots
cd "$(dirname "$0")/.."
mkdir -p sim/obj
P8OBJ=""
for f in attic/p8/*.cpp attic/p8/core/*.cpp attic/p8/lua/*.cpp; do
  o=sim/obj/$(echo "$f" | tr / _).o
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then g++ -O2 -std=c++17 -w -iquote src/ui -DPICO_SIM -include ../PicoCube/src/pico/pico_config.h -c "$f" -o "$o" || exit 1; fi
  P8OBJ="$P8OBJ $o"
done
g++ -O2 -std=c++17 -Wno-unused-result -DPICO_SIM -include ../PicoCube/src/pico/pico_config.h -o sim/picosim sim/sim.cpp sim/simsys.cpp sim/simpico.cpp \
  src/core/gfx.cpp src/core/g3d.cpp src/core/fx.cpp src/core/holo.cpp src/core/img.cpp src/core/media.cpp src/ui/ui.cpp src/ui/buttons.cpp \
  ../PicoCube/src/pico/pico.cpp $P8OBJ -lm -lz
