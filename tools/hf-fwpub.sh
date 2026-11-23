#!/bin/bash
# fwpub.sh: put the last build (/tmp/hfbuild) on the site: web flasher files + src/hf_firmware.js (boards update over WiFi)
set -e
G=$(cd "$(dirname "$0")/.." && pwd); F=$G/public/thuisfront/flash
VER=$(grep -o 'HF_VERSION *"[^"]*"' $G/esp32/HomeFront/config.h | grep -o '"[^"]*"' | tr -d '"')
cp /tmp/hfbuild/HomeFront.ino.bin $F/homefront.bin
cp /tmp/hfbuild/HomeFront.ino.partitions.bin $F/partitions.bin
cp /tmp/hfbuild/HomeFront.ino.bootloader.bin $F/bootloader.bin
SIZE=$(stat -c %s $F/homefront.bin); MD5=$(md5sum $F/homefront.bin | cut -d' ' -f1)
sed -i "s/\"version\": \"[^\"]*\"/\"version\": \"$VER\"/" $F/manifest.json
cat > $G/src/hf_firmware.js <<JS
// The newest HOME FRONT board firmware (written by the build: tools fwpub.sh). Boards that run an older
// version and can update over WiFi get {t:"ota"} with this when they connect.
export const FIRMWARE = { ver: "$VER", path: "/thuisfront/flash/homefront.bin", size: $SIZE, md5: "$MD5" };
JS
echo "published firmware $VER ($SIZE bytes, md5 $MD5)"
