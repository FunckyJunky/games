#!/bin/bash
# ear-fwpub.sh: build the EAR-CUBE firmware (esp32/EarCube) and put it on the site.
#   tools/ear-fwpub.sh "what's new"          build, then publish (EAR-CUBEs update themselves over WiFi)
#   tools/ear-fwpub.sh --no-build "notes"    publish the last build (/tmp/earbuild)
# Bump CUBE_VERSION in esp32/EarCube/src/ear/ear_config.h first: cubes install a version newer than their own.
# Writes public/ear/flash/: ear.bin, bootloader.bin, partitions.bin, boot_app0.bin (USB installer),
# EAR-CUBE-flash.zip (flash.bat + the files), manifest.json (ESP Web Tools) and firmware.json ({ver, path, size, md5, notes, date}: what EAR-CUBEs check,
# and what ERROR-CUBE installs from UPDATE > FW > EAR-CUBE).
set -e
G=$(cd "$(dirname "$0")/.." && pwd); S=$G/esp32/EarCube; F=$G/public/ear/flash; B=/tmp/earbuild
HW=${ARDUINO_HW:-$HOME/Arduino/hardware/espressif/esp32}
FQBN="espressif:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,FlashSize=16M,FlashMode=qio"
BUILD=1; if [ "$1" = "--no-build" ]; then BUILD=0; shift; fi
NOTES=${1:-""}
VER=$(grep -o 'CUBE_VERSION *"[^"]*"' $S/src/ear/ear_config.h | grep -o '"[^"]*"' | tr -d '"')
if [ $BUILD = 1 ]; then
  CT=$(ls -d $HOME/.arduino15/packages/builtin/tools/ctags/* 2>/dev/null | head -1)
  arduino-cli compile --fqbn "$FQBN" ${CT:+--build-property "runtime.tools.ctags.path=$CT"} --output-dir $B $S
fi
strings $B/EarCube.ino.bin | grep -q "^EAR-CUBE/$VER" || { echo "the build isn't EAR-CUBE $VER"; exit 1; }
mkdir -p $F
cp $B/EarCube.ino.bin $F/ear.bin
cp $B/EarCube.ino.bootloader.bin $F/bootloader.bin
cp $B/EarCube.ino.partitions.bin $F/partitions.bin
cp $HW/tools/partitions/boot_app0.bin $F/boot_app0.bin
SIZE=$(stat -c %s $F/ear.bin); MD5=$(md5sum $F/ear.bin | cut -d' ' -f1); DATE=$(date -u +%Y-%m-%dT%H:%MZ)
python3 - "$F" "$VER" "$SIZE" "$MD5" "$NOTES" "$DATE" <<'PY'
import json, sys
f, ver, size, md5, notes, date = sys.argv[1:]
json.dump({"name": "EAR-CUBE", "ver": ver, "path": "/ear/flash/ear.bin", "size": int(size), "md5": md5, "notes": notes, "date": date},
          open(f + "/firmware.json", "w"), indent=2)
m = {"name": "EAR-CUBE", "version": ver, "new_install_prompt_erase": True, "builds": [{"chipFamily": "ESP32-S3", "parts": [
  {"path": "bootloader.bin", "offset": 0}, {"path": "partitions.bin", "offset": 32768},
  {"path": "boot_app0.bin", "offset": 57344}, {"path": "ear.bin", "offset": 65536}]}]}
json.dump(m, open(f + "/manifest.json", "w"), indent=2)
PY
# flash.bat + the four files in one zip, for USB without a browser (esp32/EarCube/flash)
python3 - "$F" "$S/flash" "$VER" <<'PY'
import os, sys, zipfile
f, src, ver = sys.argv[1:]
d = "EAR-CUBE-" + ver + "/"
with zipfile.ZipFile(f + "/EAR-CUBE-flash.zip", "w", zipfile.ZIP_DEFLATED) as z:
    for n in ("flash.bat", "README.txt"): z.write(os.path.join(src, n), d + n)
    for n in ("bootloader.bin", "partitions.bin", "boot_app0.bin", "ear.bin"): z.write(os.path.join(f, n), d + n)
PY
echo "published EAR-CUBE $VER ($SIZE bytes, md5 $MD5)"
