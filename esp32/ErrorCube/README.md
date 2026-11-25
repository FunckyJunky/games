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
| APPS | CLOCK (time, alarm, timer, stopwatch), CALC, PAINT (saves BMP), LEVEL, SNAKE, SOUND, FILES (A: TF card, C: flash) |
| WEB | the web apps from error.blue/cube/apps |
| ONLINE | WHO (who's online on error.blue), CHAT (ERROR-CHAT, keyboard EN / UA), LINK (code + QR for the phone) |
| SYSTEM | SETUP, WIFI, UPDATE, INFO (system, power, sensors, network), COMMAND (C:\> prompt) |

Swipe down from the top (or tap the status bar): QUICK (light, volume, WiFi, sound, colour, sleep, power).
Swipe right: back. Buttons: - / + move, hold + = OK, hold - = BACK, PWR = screen off/on, hold PWR = power menu
(6 s = hard off). LIVE opens by itself when the phone sends video or sound.

## How it's built

```
ErrorCube.ino     start-up and the main loop (interface on core 1, network task on core 0)
src/core/         config.h (version, pins, address), sys.h (settings + what the board reports), gfx (drawing),
                  fonts.h (made by tools/mkfont.py: VT323 + GNU Unifont for Cyrillic, with glow)
src/ui/           screens, touch gestures, buttons, lists, settings rows, keyboard, dialogs, quick panel,
                  screen off / dim / clock when off, glitches, scanlines
src/apps/         apps_core (menu, ERROR-BIOS, program list), apps_tools, apps_online, apps_setup, apps_media
src/js/           jsapp.cpp: the web app API on Duktape 2.7 (duktape.c, MIT)
src/board/        board.cpp (CO5300 screen via Arduino_GFX, CST9220 touch, AXP2101, PCF85063, QMI8658, TF card,
                  flash drive, buttons), sound.cpp (ES8311 + a small synthesizer, streamed sound)
src/net/          net.cpp (WiFi, phone setup, the cube's web page, internet time), cloud.cpp (error.blue)
src/lib/          cubeqr (QR codes, Nayuki, MIT)
sim/              the interface on a PC: sh sim/build.sh, then commands on stdin (see sim.cpp) -> screenshots
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

Another kind of device can use the same page: add it to `DRIVERS` in public/cube/index.html and send the same
schema/state messages.
