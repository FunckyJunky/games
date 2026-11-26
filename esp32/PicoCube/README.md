# PICO-CUBE

The second firmware for the ERROR-CUBE (Waveshare ESP32-S3-Touch-AMOLED-2.16): a PICO-8 cart player.
It starts straight into its carts. Page: [error.blue/pico8](https://error.blue/pico8/) (carts, USB installer).

## On the cube

- **CARTS**: C:\pico8 (the cube's flash) and A:\PICO8 (TF card), .p8 and .p8.png. Tap = play, hold = delete. The last
  one played is on top. Two come built in (TOLK, TILT TEST: attic/p8/carts.h).
- **ONLINE**: public/pico8/carts.json, the carts in public/pico8/carts/ (open licenses, each with its source and
  license file). GET / GET + PLAY download to C:\pico8 (plus `<file>.v`, the version: a newer one shows NEW).
- **SETUP**: tilt (degrees for an arrow, left/right and up/down turned round), O/X on - and +, CALIBRATE, TILT TEST;
  volume, brightness, rotation, screen off; WiFi; UPDATE PICO-CUBE; BACK TO ERROR-CUBE; info.
- **Playing**: tilt = arrows, - = O, + = X, PWR (or a long touch) = pause: CONTINUE / RESTART / CALIBRATE / QUIT.

## The two firmwares

Both are built for `app3M_fat9M_16MB` (two 3 MB app slots + the 9 MB FAT drive C:), so each installs the other over
WiFi into the free slot, with rollback (a firmware that doesn't come up for 30 s is replaced by the one before):

- ERROR-CUBE 1.5.3+: SYSTEM > UPDATE > **P8 PICO-8** (1.5.4+: **FW Other > PICO-CUBE**) reads /pico8/flash/firmware.json and installs pico.bin.
- PICO-CUBE: SETUP > **BACK TO ERROR-CUBE** reads /cube/flash/firmware.json and installs cube.bin.

Settings (NVS + C:\cube.cfg), saved WiFi and files are shared: the Cfg struct is the same source.
PICO-CUBE updates itself like ERROR-CUBE does (checks /pico8/flash/firmware.json, asks in SETUP).

## How it's built

PICO-CUBE is ERROR-CUBE's own parts plus a small layer of its own:

    src/board -> ../../ErrorCube/src/board     the hardware
    src/core  -> ../../ErrorCube/src/core      drawing, fonts, settings (config.h includes src/pico/pico_config.h)
    src/ui    -> ../../ErrorCube/src/ui        screens, lists, dialogs, keyboard, sleep
    src/lib   -> ../../ErrorCube/src/lib
    src/p8    -> ../../ErrorCube/attic/p8      FAKE-08 + z8lua (MIT), the emulator
    src/net/net.cpp, net.h, files_page.h, certs.h -> ErrorCube's (WiFi, the cube's web page)
    src/net/pnet.cpp                           error.blue for PICO-CUBE: carts, downloads, updates, the switch
    src/pico/pico_config.h                     FW_NAME, CUBE_VERSION, FW_INFO_PATH, CARTS_PATH, THEME_COLOR
    src/pico/pico.cpp                          the screens (CARTS / ONLINE / SETUP, playing, tilt test, WiFi, firmware)

arduino-cli copies the sketch with the symlinks followed, so `__has_include("../pico/pico_config.h")` in core/config.h
finds PICO-CUBE's file only in this build.

## Building

    tools/pico-fwpub.sh "what's new"      # bump CUBE_VERSION in src/pico/pico_config.h first
    git add public/pico8 && git commit && git push   # Cloudflare deploys; PICO-CUBEs see the update

Simulator (screens + carts on a PC): `cd esp32/ErrorCube && sh sim/build-pico.sh`, then e.g.
`printf 'wait 2600\nfile /c/pico8/tolk.p8\nwait 3000\nshot tolk\n' | ./sim/picosim --out /tmp/pshots`.

## Carts on error.blue

public/pico8/carts.json: `{carts: [{file, name, author, kind (GAME / TOY), license, v, size, about, source, licenseFile}]}`.
Only carts under open licenses, with the license text in carts/licenses/. To add one: copy it to public/pico8/carts/,
add its entry (bump `v` when the file changes), check it in the simulator.

PICO-8 is a fantasy console by Lexaloffle Games. PICO-CUBE is a fan project, not made by or connected to Lexaloffle.
