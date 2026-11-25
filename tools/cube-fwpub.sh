#!/bin/bash
# cube-fwpub.sh: build the ERROR-CUBE firmware and put it on the site.
#   tools/cube-fwpub.sh "what's new"          build, then publish (cubes update themselves over WiFi)
#   tools/cube-fwpub.sh --no-build "notes"    publish the last build (/tmp/cubebuild)
# Bump CUBE_VERSION in esp32/ErrorCube/src/core/config.h first: cubes install a version newer than their own.
# Writes public/cube/flash/: cube.bin, bootloader.bin, partitions.bin, boot_app0.bin (USB installer),
# manifest.json (ESP Web Tools) and firmware.json (what the cubes check: {ver, path, size, md5, notes, date}).
set -e
G=$(cd "$(dirname "$0")/.." && pwd); S=$G/esp32/ErrorCube; F=$G/public/cube/flash; B=/tmp/cubebuild
HW=${ARDUINO_HW:-$HOME/Arduino/hardware/espressif/esp32}
FQBN="espressif:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,FlashSize=16M,FlashMode=qio"
BUILD=1; if [ "$1" = "--no-build" ]; then BUILD=0; shift; fi
NOTES=${1:-""}
VER=$(grep -o 'CUBE_VERSION *"[^"]*"' $S/src/core/config.h | grep -o '"[^"]*"' | tr -d '"')
if [ $BUILD = 1 ]; then
  CT=$(ls -d $HOME/.arduino15/packages/builtin/tools/ctags/* 2>/dev/null | head -1)
  arduino-cli compile --fqbn "$FQBN" ${CT:+--build-property "runtime.tools.ctags.path=$CT"} --output-dir $B $S
fi
mkdir -p $F
cp $B/ErrorCube.ino.bin $F/cube.bin
cp $B/ErrorCube.ino.bootloader.bin $F/bootloader.bin
cp $B/ErrorCube.ino.partitions.bin $F/partitions.bin
cp $HW/tools/partitions/boot_app0.bin $F/boot_app0.bin
SIZE=$(stat -c %s $F/cube.bin); MD5=$(md5sum $F/cube.bin | cut -d' ' -f1); DATE=$(date -u +%Y-%m-%dT%H:%MZ)
python3 - "$F" "$VER" "$SIZE" "$MD5" "$NOTES" "$DATE" <<'PY'
import json, sys
f, ver, size, md5, notes, date = sys.argv[1:]
json.dump({"ver": ver, "path": "/cube/flash/cube.bin", "size": int(size), "md5": md5, "notes": notes, "date": date},
          open(f + "/firmware.json", "w"), indent=2)
m = {"name": "ERROR-CUBE", "version": ver, "new_install_prompt_erase": True, "builds": [{"chipFamily": "ESP32-S3", "parts": [
  {"path": "bootloader.bin", "offset": 0}, {"path": "partitions.bin", "offset": 32768},
  {"path": "boot_app0.bin", "offset": 57344}, {"path": "cube.bin", "offset": 65536}]}]}
json.dump(m, open(f + "/manifest.json", "w"), indent=2)
PY
echo "published ERROR-CUBE $VER ($SIZE bytes, md5 $MD5)"
