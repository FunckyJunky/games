# ERROR-CUBE

ERROR-DOS on the **Waveshare ESP32-S3-Touch-AMOLED-2.16** (480x480 AMOLED with touch, AXP2101 power chip with
battery, PCF85063 clock, QMI8658 motion sensor, ES8311 speaker, TF card, buttons + / PWR / -), linked to error.blue.

- **Phone control:** error.blue/cube. Add cubes by their link code (ONLINE > LINK on the cube, or scan its QR code).
  Per cube: STATUS (live screen, battery, WiFi...), SETTINGS (every setting, built from what the cube sends),
  SEND (a message, live camera video, sound from the microphone, a photo), APPS (open a web app on the cube),
  SYSTEM (updates, restart).
- **Web apps:** JavaScript files in public/cube/apps (API: public/cube/apps/README.md). New apps need no new firmware.
- **Updates over WiFi:** the cube checks public/cube/flash/firmware.json (at start, then every 6 h by default,
  or CHECK on the phone / UPDATE.EXE). AUTO UPDATE installs a newer version when the screen is asleep;
  INSTALL does it right away. A new firmware has to run for 30 s, or the cube goes back to the old one (rollback).
  Also: a .bin on the TF card (FILES > INSTALL FIRMWARE) or the cube's own web page (http://IP/ > FIRMWARE).
- **First install with USB:** error.blue/cube/flash (ESP Web Tools). It also has Waveshare's factory firmware.

## Programs

| folder | programs |
|---|---|
| APPS | CLOCK (time, alarm, timer, stopwatch), MUSIC, VIDEO, CALC, PAINT (saves BMP), LEVEL, SNAKE, SOUND, FILES (A: SD card, C: flash) |
| WEB | the web apps from error.blue/cube/apps |
| ONLINE | WHO (who's online on error.blue), CHAT (ERROR-CHAT, keyboard EN / UA), LINK (code + QR for the phone) |
| SYSTEM | SETUP, WIFI, UPDATE, INFO (system, power, sensors, network), COMMAND (C:\> prompt) |

**Look (SETUP > LOOK):** MENU 3D (the programs as glass cards on a turning carousel over a synthwave floor, each
with its own turning wireframe icon; the camera leans with how you hold the cube) or DOS (the list).
GLASS: CURVED (a CRT tube: the edges bend, the middle stays sharp), FISHEYE or FLAT, with COLOUR FRINGE (red and
blue drift apart toward the edges). ANIMATION 3D: the cube turns between screens (up = open, down = back, the
3D menu's folders turn up/down); SIMPLE: a glitch instead. Glitches: tears, roll, jitter, blocks, static, inverse
bands, garbled letters, colour split, VHS tracking band, interlace, datamosh, smear, signal loss. Also: the list's
selection bar slides, rows type themselves in, dialogs open like DOS windows, QUICK slides down, a ring where you
tap, and the 3D INTRO at start (starfield warp, ERROR in solid block letters, lens flare). BATTERY SAVER turns the
effects off.

Swipe down from the top (or tap the status bar): QUICK (light, volume, WiFi, sound, offline, sleep, setup, power).
Swipe right: back. Buttons (default, SETUP > BUTTONS to change): - / + move, hold + = OK, hold - = BACK,
- and + together = QUICK, PWR = screen off/on, hold PWR = power menu (6 s = hard off). LIVE opens by itself when the phone sends video or sound.

## New in 1.4.1

- **PICO-8 steadier:** z8lua uses setjmp/longjmp (LUA_USE_LONGJMP) instead of C++ exceptions, which it threw on
  every coroutine yield (every frame); the cart task runs at priority 0 (a long cart start can't starve core 0's
  idle task: the task watchdog panics after 5 s), a 32 KB stack (it checks there's room for it first), and C++
  errors inside FAKE-08 end the cart instead of the cube. The pause menu shows FPS and the stack left.
- **Crash report:** after a crash or watchdog restart the cube says so once, with the task, PC, cause and backtrace
  from the core dump (INFO > SYSTEM and the phone's STATUS show it too). Decode with
  `xtensa-esp32s3-elf-addr2line -pfiaC -e ErrorCube.ino.elf <addresses>` against that version's ELF.
- **Settings kept:** also in C:\cube.cfg and A:\ERRORCUBE\cube.cfg (settings + saved WiFi, CRC checked), restored
  when NVS has none (a reinstall that erased the flash). Settings from a newer firmware load too (after a rollback).
  Saved 0.6 s after a change (was 1.5 s), and right before a PICO-8 cart starts. FACTORY RESET removes the files.
- **LOW GRAPHICS** (SETUP, LOOK, phone): the plainest interface: DOS menu, no HOLO 3D, glass, glow, scanlines,
  glitches, animations, wallpaper, CRT or intro (lowFx(): what BATTERY SAVER turns off, without slowing the CPU).
- **Alarm:** plays a song (CLOCK > ALARM > SOUND > SONG, or hold a song in MUSIC / FILES: ALARM SONG), at FULL
  VOLUME (sndForceVolume) whatever VOLUME is; moving or picking up the cube snoozes it (motion from the IMU after
  2 s), SNOOZE FOR 1..30 min. A song alarm stops after 10 min, beeps after 3.
- The glass table isn't worked out again when coming back from a game or LOW GRAPHICS.

## New in 1.4: PICO-8

- **PICO8.EXE:** plays PICO-8 carts (.p8 and .p8.png) from A:\PICO8 (SD card) and C:\pico8. Comes with TOLK and
  TILT TEST (carts/*.p8 -> src/p8/carts.h with tools/cube-carts.py). Tap a cart in FILES and it plays too.
- **Controls:** tilt the cube = the arrows (degrees from how you hold it: CALIBRATE, or how it's held when a cart
  starts), - = O, + = X (SWAP), PWR = the cube's menu (CONTINUE, RESTART, CALIBRATE, QUIT), long touch too.
  SETUP > PICO-8: tilt before it moves, invert left/right and up/down, swap, TEST IT. Buttons reach the game raw
  (btnHook), a press waits until the cart reads it (30 fps carts read every other step).
- **The emulator:** FAKE-08 (MIT, github.com/jtothebell/fake-08) on z8lua (MIT) in src/p8, trimmed for the ESP32:
  no iostreams / regex / codecvt / map (cart.cpp, vm.cpp, filehelpers.cpp, emojiconversion.cpp rewritten in those
  parts), lodepng -> p8png.cpp (the ROM's inflate), no zip / ini / eris save states, its audio in float (the S3 has
  no double FPU). Lua's memory is in PSRAM (lauxlib l_alloc). The cart runs in its own task on core 0 (28 KB stack,
  60 steps a second), sound into the media stream at 22.05 kHz, the picture (128x128) scaled to 480x480 with no
  glass or scanlines (Screen::plain). ~300 KB of flash.
- **From the phone:** error.blue/cube > PICO-8: the carts on the cube (PLAY, DEL), ADD CARTS (.p8 / .p8.png) and
  NEW CART: paste Lua (or a whole .p8) and SAVE / SAVE + PLAY. Into A:\PICO8, or C:\pico8 without a card.

## New in 1.3.3

- **MP4 (MOV, WebM...) on the cube:** the chip can't decode phone video (H.264 High profile, HD), so the phone converts
  it once into the cube's MJPEG AVI. Tap an MP4 on the cube: if its AVI twin (same name, .avi) is there it plays;
  else a QR code opens the cube's own page http://IP/cv -> /files?conv=PATH, which loads the MP4, converts it in the
  phone's browser (error.blue/cube/conv.js), sends the AVI next to it, says DONE (POST /api/prog) and plays it
  (POST /api/open). The cube shows the phone's progress. VIDEO lists MP4s (hidden once converted).
- **Uploads convert videos:** error.blue/cube > FILES and the cube's /files page convert MP4s on the way (checkbox).
  error.blue/cube uploads now keep 4 pieces on the way (faster); a lost piece is sent again.
- **conv.js** (public/cube/conv.js): plays the video muted at 2x and copies frames with requestVideoFrameCallback
  (slows down when frames get skipped), sound decoded once at 22.05 kHz mono; seek per frame on old browsers.
- **Volume slider** in MUSIC's NOW PLAYING (tap VOL to mute) and in VIDEO's overlay (on the right).

## New in 1.3.2

- **Phone link fixed ("SSL - Memory allocation failed"):** mbedTLS keeps 16 KB in + 16 KB out buffers per connection
  in internal RAM, and 1.3's media task (40 KB stack) left too little for two connections. Now the media task starts
  the first time music or video plays, with a 12 KB stack (minimp3's 16 KB scratch is in PSRAM), and the player state,
  DATAMOSH and PAINT buffers moved to PSRAM (globals 95.7 KB -> 76 KB). The requests and the live link take turns
  for handshakes (tlsMx); the requests' kept-alive connection closes when RAM is short; an update or another request
  that matters makes the link step aside for a moment (linkYield), background ones wait.
- **Why a link fails:** LINK.EXE shows the last error, INFO.EXE > NET shows FAST RAM, PHONE LINK and the last error.
- **error.blue/cube:** when the cube doesn't answer, the page checks the room: no cube on that code (it gets a new
  code after a reinstall) -> says so, with THE CUBE SHOWS ANOTHER CODE.

## New in 1.3.1

- **Tap a file, it opens in its program:** MP3 / WAV in MUSIC (the folder becomes the playlist), AVI in VIDEO,
  pictures in SHOW (swipe through the folder), text in the reader, a .bin asks to install. MP4 says where to convert
  it. Hold a file (or O) for its options: SET AS WALLPAPER, COPY, RENAME, DELETE. Holding a picture in SHOW sets it as
  the wallpaper. Long presses work everywhere now (EV_LONG, and a button's long action).
- **Open on the cube from the phone:** a play button next to music, videos, pictures and text in error.blue/cube >
  FILES and on the cube's own page http://IP/files (message "fopen" / POST /api/open).

## New in 1.3

- **MUSIC.EXE:** MP3 (minimp3, public domain) and WAV from the SD card (A:\MUSIC first) or the flash. Library by
  folder, NOW PLAYING with title / artist from ID3 tags, a picture of the sound (bars that move with HOLO, a wave),
  a seek bar (drag), previous / play-pause / next, repeat (off / all / one), shuffle. It keeps playing when you
  leave (a note in the status bar); buttons can do PLAY / PAUSE, NEXT SONG, PREVIOUS SONG, MUSIC (SETUP > BUTTONS).
- **VIDEO.EXE:** MJPEG AVI with PCM sound, in sync with the sound (frames are skipped when it's behind), full
  screen; tap for the bar, tap again to pause, tap the bar to jump, - / + = 10 s back / on.
- **error.blue/cube/video.html:** converts any video the browser can open (MP4, MOV, WebM) into that AVI, in the
  browser: size, shape (whole / fill the square), frames a second, quality, sound, from / length. SAVE it, or SEND
  it to the cube's A:\VIDEO through error.blue.
- Sound: the mixer runs at 44.1 kHz stereo now (music, video, the phone's stream and the beeps together).
  Code: src/core/media.cpp (decoding, its own task on core 0), src/board/sound.cpp, src/apps/apps_music.cpp.

## New in 1.2

- **HOLO 3D** (SETUP > HOLO 3D): tilt the cube and the interface looks deep, like a hologram. Every part sits on a
  depth layer (wallpaper far behind, box frames behind the text, the selection bar and buttons raised, the bars in
  front, dialogs floating) and moves by its own amount; raised parts cast shadows and show their sides; the curved
  glass, its colour fringe, a light reflection and the perspective move the other way. Motion: gyro + gravity fused
  (sees every turn) or gravity only; the "straight" position follows you slowly (AUTO CENTRE) or press CENTRE NOW.
  Presets SUBTLE / NORMAL / DEEP / EXTREME and ~20 settings (depth, which layer stays still, pop-out, shadows, sides,
  wallpaper, glass shift, perspective, reflection, rainbow, full tilt angle, dead zone, smoothing, invert). TEST
  shows a tunnel of frames and the tilt. Code: src/core/holo.cpp (+ the draw offset in gfx.cpp, the lens table in fx.cpp).
- **SETUP in groups with icons:** SCREEN (screen, look, HOLO 3D), CONTROLS (sound, buttons), CONNECT (WiFi,
  OFFLINE, error.blue), DEVICE (time, power, updates, system).
- **SCREEN page:** light, WALLPAPER (glass, dots, grid, stars, room, or your own picture: FILES > SET AS WALLPAPER),
  and "when you stop using it" spelled out: IDLE (dim, off after...), THEN (black or the LOW POWER CLOCK: big,
  small, analog or DOS; its light, date, battery, messages; WiFi as normal / saving power / off while it shows).
  The CPU slows down 3 s after the screen goes off.
- **BUTTONS:** each of -, +, PWR: press, double press and hold, each set to any action (prev, next, OK, back, home,
  quick panel, screen, power menu, volume, light, HOLO on/off, HOLO centre, offline, screenshot, torch, glitch,
  clock, files, setup); together: - and +, hold - and +, hold - then PWR, hold + then PWR; timing (hold after,
  double press gap, repeat while held, PWR hold time, PWR force-off time), swap - and +, which buttons wake. TEST
  shows what was detected. Code: src/ui/buttons.cpp.
- **OFFLINE** (airplane mode): quick panel, SETUP or a button. WiFi off, a plane in the status bar, programs that
  need the internet ask first.
- **SD card in FILES:** put a card in any time (it's mounted when you open A:), EJECT, new folder, copy between
  the card and the flash, rename, delete (empty folders too), pictures (JPEG / BMP, swipe through a folder),
  text files, firmware. Screenshots (a button action) go to A:\SHOTS.
- **Files from the phone:** error.blue/cube > FILES (from anywhere, through the link, about 50 KB/s), or the cube's
  own page http://IP/files on the same WiFi (fast, drag and drop, download).
- **Updates:** a message when a new firmware is ready (UPDATE NOW / LATER), or INSTALL BY ITSELF while the screen is
  off (SETUP > UPDATES).

## How it's built

```
ErrorCube.ino     start-up and the main loop (interface on core 1, network task on core 0)
src/core/         config.h (version, pins, address), sys.h (settings + what the board reports), gfx (drawing),
                  g3d (3D: camera, glowing anti-aliased lines, filled polygons), fx (the lens, glitches, the cube turn),
                  fonts.h (made by tools/mkfont.py: VT323 + GNU Unifont for Cyrillic, with glow)
src/ui/           screens, touch gestures, buttons, lists, settings rows, keyboard, dialogs, quick panel,
                  screen off / dim / clock when off, glitches, scanlines
src/apps/         apps_core (DOS menu, ERROR-BIOS, program list), menu3d (the 3D menu), icons3d (a wireframe icon
                  per program), intro (the 3D start), apps_tools, apps_online, apps_setup, apps_media
src/js/           jsapp.cpp: the web app API on Duktape 2.7 (duktape.c, MIT)
src/board/        board.cpp (CO5300 screen via Arduino_GFX, CST9220 touch, AXP2101, PCF85063, QMI8658, TF card,
                  flash drive, buttons), sound.cpp (ES8311 + a small synthesizer, streamed sound)
src/net/          net.cpp (WiFi, phone setup, the cube's web page, internet time), cloud.cpp (error.blue)
src/lib/          cubeqr (QR codes, Nayuki, MIT)
sim/              the interface on a PC: sh sim/build.sh, then commands on stdin (see sim.cpp) -> screenshots,
                  `rec MS` records frames (25 a second) for a video
tools/mkfont.py   the letters
```

A new program: a `Screen` (see ui.h) in src/apps, and a line in `appsRegister()` (apps_core.cpp).

## Building

Arduino-ESP32 **3.3.11**; libraries as in Waveshare's examples: GFX Library for Arduino 1.6.4, SensorLib 0.3.3,
XPowersLib 0.2.6, plus ArduinoJson 7. Board settings: ESP32S3 Dev Module, USB CDC On Boot Enabled, Flash 16MB QIO,
Partition Scheme **16M Flash (3MB APP/9.9MB FATFS)** (`app3M_fat9M_16MB`), PSRAM **OPI PSRAM**.

New version: bump `CUBE_VERSION` in src/core/config.h, then `tools/cube-fwpub.sh "what changed"` and push.

## The link (room error-cube/CODE)

The cube joins `wss://error.blue/api/room/error-cube/<CODE>?name=CUBE-XXXX`, phones as `PANEL`. Messages (the room
wraps them as `{type:"msg", from, data}`):

- cube -> phone: `{t:"schema", dev, v, caps:[...], sets:[{g, k, n, t:"toggle"|"choice"|"slider"|"text", o, lo, hi}]}`
  (when a phone says hello), `{t:"state", dev, id, name, v, prog, awake, bat, chg, usb, mv, ssid, ip, rssi, up, heap,
  psram, temp, sd, upd:{st, ver, pct, msg, notes}, cfg:{k: v}}` (on changes, every 5 s),
  `{t:"shot", w:96, h:96, d}` (the screen, RGB332, base64, while watched), `{t:"app", id, data}` (from a web app).
- phone -> cube: `{t:"hello"}`, `{t:"set", k, v}`, `{t:"cmd", c:"wake"|"sleep"|"home"|"beep"|"check"|"update"|"restart"}`,
  `{t:"say", text}`, `{t:"app", id}`, `{t:"appmsg", data}`, `{t:"watch", on}`,
  `{t:"vid", n, i, of, d}` (a JPEG, base64, in parts under 12000 characters), `{t:"aud", d}` (16 kHz mu-law, base64).

Settings (cfg in src/core/sys.h) are saved as one block: add new ones only at the end of the struct, then a newer
firmware keeps everything an older one saved.

Another kind of device can use the same page: add it to `DRIVERS` in public/cube/index.html and send the same
schema/state messages.
