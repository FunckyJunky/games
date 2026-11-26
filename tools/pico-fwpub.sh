#!/bin/bash
# pico-fwpub.sh: build the PICO-CUBE firmware (esp32/PicoCube) and put it on the site.
#   tools/pico-fwpub.sh "what's new"          build, then publish (PICO-CUBEs update themselves over WiFi)
#   tools/pico-fwpub.sh --no-build "notes"    publish the last build (/tmp/picobuild)
# Bump CUBE_VERSION in esp32/PicoCube/src/pico/pico_config.h first: cubes install a version newer than their own.
# Writes public/pico8/flash/: pico.bin, bootloader.bin, partitions.bin, boot_app0.bin (USB installer),
# manifest.json (ESP Web Tools) and firmware.json ({ver, path, size, md5, notes, date}: what PICO-CUBEs check,
# and what ERROR-CUBE installs from UPDATE > PICO-CUBE).
set -e
G=$(cd "$(dirname "$0")/.." && pwd); S=$G/esp32/PicoCube; F=$G/public/pico8/flash; B=/tmp/picobuild
HW=${ARDUINO_HW:-$HOME/Arduino/hardware/espressif/esp32}
FQBN="espressif:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,FlashSize=16M,FlashMode=qio"
BUILD=1; if [ "$1" = "--no-build" ]; then BUILD=0; shift; fi
NOTES=${1:-""}
VER=$(grep -o 'CUBE_VERSION *"[^"]*"' $S/src/pico/pico_config.h | grep -o '"[^"]*"' | tr -d '"')
if [ $BUILD = 1 ]; then
  CT=$(ls -d $HOME/.arduino15/packages/builtin/tools/ctags/* 2>/dev/null | head -1)
  arduino-cli compile --fqbn "$FQBN" ${CT:+--build-property "runtime.tools.ctags.path=$CT"} --output-dir $B $S
fi
strings $B/PicoCube.ino.bin | grep -q "^PICO-CUBE/$VER" || { echo "the build isn't PICO-CUBE $VER"; exit 1; }
mkdir -p $F
cp $B/PicoCube.ino.bin $F/pico.bin
cp $B/PicoCube.ino.bootloader.bin $F/bootloader.bin
cp $B/PicoCube.ino.partitions.bin $F/partitions.bin
cp $HW/tools/partitions/boot_app0.bin $F/boot_app0.bin
SIZE=$(stat -c %s $F/pico.bin); MD5=$(md5sum $F/pico.bin | cut -d' ' -f1); DATE=$(date -u +%Y-%m-%dT%H:%MZ)
python3 - "$F" "$VER" "$SIZE" "$MD5" "$NOTES" "$DATE" <<'PY'
import json, sys
f, ver, size, md5, notes, date = sys.argv[1:]
json.dump({"name": "PICO-CUBE", "ver": ver, "path": "/pico8/flash/pico.bin", "size": int(size), "md5": md5, "notes": notes, "date": date},
          open(f + "/firmware.json", "w"), indent=2)
m = {"name": "PICO-CUBE", "version": ver, "new_install_prompt_erase": True, "builds": [{"chipFamily": "ESP32-S3", "parts": [
  {"path": "bootloader.bin", "offset": 0}, {"path": "partitions.bin", "offset": 32768},
  {"path": "boot_app0.bin", "offset": 57344}, {"path": "pico.bin", "offset": 65536}]}]}
json.dump(m, open(f + "/manifest.json", "w"), indent=2)
PY
echo "published PICO-CUBE $VER ($SIZE bytes, md5 $MD5)"
