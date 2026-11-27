#!/bin/bash
# dream-fwpub.sh: build the DREAM-CUBE firmware (esp32/DreamCube) and put it on the site.
#   tools/dream-fwpub.sh "what's new"          build, then publish (DREAM-CUBEs update themselves over WiFi)
#   tools/dream-fwpub.sh --no-build "notes"    publish the last build (/tmp/dreambuild)
# Bump CUBE_VERSION in esp32/DreamCube/src/dream/dream_config.h first: cubes install a version newer than their own.
# Writes public/dream/flash/: dream.bin, bootloader.bin, partitions.bin, boot_app0.bin (USB installer),
# DREAM-CUBE-flash.zip (flash.bat + the files), manifest.json (ESP Web Tools) and firmware.json ({ver, path, size, md5, notes, date}: what DREAM-CUBEs check,
# and what ERROR-CUBE installs from UPDATE > FW Other > DREAM-CUBE).
set -e
G=$(cd "$(dirname "$0")/.." && pwd); S=$G/esp32/DreamCube; F=$G/public/dream/flash; B=/tmp/dreambuild
HW=${ARDUINO_HW:-$HOME/Arduino/hardware/espressif/esp32}
FQBN="espressif:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,FlashSize=16M,FlashMode=qio"
BUILD=1; if [ "$1" = "--no-build" ]; then BUILD=0; shift; fi
NOTES=${1:-""}
VER=$(grep -o 'CUBE_VERSION *"[^"]*"' $S/src/dream/dream_config.h | grep -o '"[^"]*"' | tr -d '"')
if [ $BUILD = 1 ]; then
  CT=$(ls -d $HOME/.arduino15/packages/builtin/tools/ctags/* 2>/dev/null | head -1)
  arduino-cli compile --fqbn "$FQBN" ${CT:+--build-property "runtime.tools.ctags.path=$CT"} --output-dir $B $S
fi
strings $B/DreamCube.ino.bin | grep -q "^DREAM-CUBE/$VER" || { echo "the build isn't DREAM-CUBE $VER"; exit 1; }
mkdir -p $F
cp $B/DreamCube.ino.bin $F/dream.bin
cp $B/DreamCube.ino.bootloader.bin $F/bootloader.bin
cp $B/DreamCube.ino.partitions.bin $F/partitions.bin
cp $HW/tools/partitions/boot_app0.bin $F/boot_app0.bin
SIZE=$(stat -c %s $F/dream.bin); MD5=$(md5sum $F/dream.bin | cut -d' ' -f1); DATE=$(date -u +%Y-%m-%dT%H:%MZ)
python3 - "$F" "$VER" "$SIZE" "$MD5" "$NOTES" "$DATE" <<'PY'
import json, sys
f, ver, size, md5, notes, date = sys.argv[1:]
json.dump({"name": "DREAM-CUBE", "ver": ver, "path": "/dream/flash/dream.bin", "size": int(size), "md5": md5, "notes": notes, "date": date},
          open(f + "/firmware.json", "w"), indent=2)
m = {"name": "DREAM-CUBE", "version": ver, "new_install_prompt_erase": True, "builds": [{"chipFamily": "ESP32-S3", "parts": [
  {"path": "bootloader.bin", "offset": 0}, {"path": "partitions.bin", "offset": 32768},
  {"path": "boot_app0.bin", "offset": 57344}, {"path": "dream.bin", "offset": 65536}]}]}
json.dump(m, open(f + "/manifest.json", "w"), indent=2)
PY
# flash.bat + the four files in one zip, for USB without a browser (esp32/DreamCube/flash)
python3 - "$F" "$S/flash" "$VER" <<'PY'
import os, sys, zipfile
f, src, ver = sys.argv[1:]
d = "DREAM-CUBE-" + ver + "/"
with zipfile.ZipFile(f + "/DREAM-CUBE-flash.zip", "w", zipfile.ZIP_DEFLATED) as z:
    for n in ("flash.bat", "README.txt"): z.write(os.path.join(src, n), d + n)
    for n in ("bootloader.bin", "partitions.bin", "boot_app0.bin", "dream.bin"): z.write(os.path.join(f, n), d + n)
PY
echo "published DREAM-CUBE $VER ($SIZE bytes, md5 $MD5)"
