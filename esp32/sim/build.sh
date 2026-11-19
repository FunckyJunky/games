#!/bin/sh
# builds the PC simulator: sh build.sh U8G2_SRC ARDUINOJSON_SRC
# (U8G2_SRC = the U8g2 library's src folder, ARDUINOJSON_SRC = ArduinoJson's src folder)
set -e
U8=${1:?U8g2 src folder}; AJ=${2:?ArduinoJson src folder}
cd "$(dirname "$0")"
mkdir -p obj
[ -f obj/libu8g2.a ] || { for f in "$U8"/clib/*.c; do gcc -O2 -c -I"$U8" "$f" -o obj/$(basename "$f" .c).o; done; ar rcs obj/libu8g2.a obj/*.o; }
g++ -O1 -g -std=c++17 -Wall -Wno-unused-function -I"$U8" -I"$AJ" -o oledsim sim.cpp ../ErrorOled/ui.cpp ../ErrorOled/model.cpp ../ErrorOled/hidparse.cpp obj/libu8g2.a
echo built
