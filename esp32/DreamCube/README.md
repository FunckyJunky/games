# DREAM-CUBE

## New in 1.3

- **The live transcript streams:** every line the cube hears goes to error.blue/dream with its next link (at once).
  The lines are put together more safely on the cube, and when error.blue keeps saying no to a line it's dropped
  instead of blocking the ones after it. error.blue also takes what the cube says it just heard when no line came
  with it, so cubes on 1.1 / 1.2 stream too. The page says when the cube is old, paused, or still has lines waiting.

## New in 1.2

- **Linking by itself:** open error.blue/dream on a phone or computer on the same network; the cube asks on its
  screen **LINK A PHONE?** (with a number the page shows too): YES, and the page remembers the cube. (The page finds
  cubes seen from its address, or from its IPv4 via api.ipify.org: phones often come over IPv6, the cube over IPv4.
  Always with the YES on the cube: shared addresses (CGNAT) can't link to someone else's cube.) The code still works.
- **Live transcript** on the page (LIVE tab): every line the cube hears, as it comes (the cube sends a new line at
  once with its link). **SAVE** keeps it as a transcript (and starts a new one), **CLEAR** starts again without
  saving. **Saved transcripts:** READ, DOWNLOAD (.txt), RENAME, DELETE.
- **3D tab:** how speech becomes 3D (kept on error.blue for this cube): STYLE (playful, literal, dreamy, symbolic),
  HOW MANY THINGS, PLACE / TIME (also the real clock) / WEATHER / MOOD / EFFECTS / CAMERA (from the words or fixed),
  TITLE LANGUAGE, STORY (go on from the last scene), and a free WISH for the AI. And how it looks on the cube:
  COLOURS (natural, vivid, ERROR-DOS = one colour, the cube's scheme), EFFECTS on / off, camera, pixels, detail...
- **CUBE tab:** all its settings: listening (new scene, thinking, language, sensitivity, gain, microphones, pause),
  screen + sound (brightness, volume, screen off after, wake on a new scene, colour scheme, auto update).

## New in 1.1

- **WiFi made simple:** no hotspot and no phone page any more. The cube scans, you pick a network and type the
  password on the cube; DREAM-CUBE starts only once it's connected. 1.0's always-on hotspot made the WiFi hop channels
  while joining (and took memory the join needs): joining could get stuck.
- The 3D's fast buffers are smaller (16-row bands) and only taken when the picture starts (after WiFi), in internal
  RAM only when 80 KB stays free for WiFi + TLS.
- Shared net.cpp: `netHold` (no tries by themselves while a network is being picked), `netStopTrying()`,
  `netReason()` (why the last join failed, from the WiFi driver's disconnect reason).

The fourth firmware for the ERROR-CUBE (Waveshare ESP32-S3-Touch-AMOLED-2.16): the cube listens, and turns what's said
around it into a little animated 3D world on its screen. The two microphones (ES7210, EAR-CUBE's `mic.cpp`) cut speech
into pieces; error.blue writes them down (`/api/stt`, Whisper) and, when enough words have come, makes a scene of them
(`/api/dream`, an AI on Cloudflare Workers AI, `src/dream.js`). The cube draws the scene itself, in 3D, animated.

Pages: [error.blue/dream](https://error.blue/dream/) (the control page) and [error.blue/dream/flash](https://error.blue/dream/flash/)
(USB installer + `DREAM-CUBE-flash.zip` with **flash.bat**).

## On the cube

- **The DREAM view** (it starts there): the scene, full screen. Its title at the top (types itself in), the words it
  heard at the bottom, a dot top left: green listening, white hearing, orange sending / dreaming, red a problem, `II`
  paused. **Drag** turns the camera. **Tap**: the menu (**II** pause / **▶** listen, **T** type words to dream, **L** the
  scenes, **S** setup). Buttons: **-** an older scene, **+** a newer one (hold **+** = menu).
- A new scene comes with a short glitch: the old things shrink away, the sky and the ground blend over, the new things
  pop up one after another and the camera glides to its new place. A new scene also wakes the screen (SETUP).
- **SCENES**: the last 40 (tap one to show it again). Every scene is also kept on the TF card: `A:\DREAM\<date>.txt`
  (time, title, what was said, the scene's JSON); the newest is shown again after a restart (`C:\dream\last.json`).
- **SETUP**: NEW SCENE (OFTEN / NORMAL / CALM: how many words and seconds between scenes), THINKING (QUICK = the small
  AI, about 1 s; SMART = the big one, 2-3 s), words and title on screen, CAMERA (still / slow / normal / fast), PIXELS
  (smooth / sharp), DETAIL (the place's own trees and houses; drops by itself when frames get slow), wake on a new
  scene, frame rate; LISTENING (language, sensitivity, gain, microphones, pause length, MIC TEST); CONTROL PAGE (the QR
  code + code, link on / off, a new code); THE WIFI; sound + screen; UPDATE / AUTO UPDATE; BACK TO ERROR-CUBE.

## WiFi

No WiFi yet (or the saved ones don't work): the cube shows the networks it finds (saved ones first, then the
strongest). Tap one, type its password on the cube's keyboard, and it connects. Only then does DREAM-CUBE start.
Saved networks are tried by themselves at the start ("CONNECTING TO ..."; ESC = pick another). A failed try says why
(WRONG PASSWORD?, NOT FOUND (2.4 GHZ ONLY)...). WiFi gone for a minute: back to the WiFi screen. SETUP > WIFI picks
another network. (1.0 had an always-on hotspot with a phone page: gone in 1.1, it made joining get stuck.)

## The control page: error.blue/dream

Opened on the same network as the cube, it finds it: the cube asks LINK A PHONE? (YES). Or the cube's 8-letter code
(SETUP > CONTROL PAGE: scan the QR code, or type the code). Tabs: LIVE (the cube now + its buttons, the live
transcript, saved transcripts), SCENES (DREAM OF..., the scenes), 3D (interpretation + look), CUBE (settings). There: online or not,
listening / paused, battery, WiFi; the scene now and what was heard; **PAUSE / LISTEN, SCREEN ON / OFF, OLDER / NEWER**;
**DREAM OF...** (type anything, the cube shows it); the **scenes** it made (what was said, the place, the things as
icons; **SHOW ON THE CUBE**, **DELETE**); all the **settings**. The cube asks error.blue every 4 s (12 s with the screen
off): `POST /api/dream/cube {code, st, last}` -> the page's commands. Anyone with the code can use the page: a new code
on the cube makes the old link stop working.

## The 3D (src/dream: r3d, models, scene)

- **r3d.cpp**: flat-shaded triangles with a 1/z depth buffer at 240 x 240, drawn in bands of 40 rows in internal RAM,
  then 2x up into the 480 x 480 screen (smooth or sharp). Sun + sky + a soft light from the camera, fog, see-through
  triangles, sprites (hearts, notes, snow, sparks...), streaks (rain, lightning, the neon grid), sky discs and stars.
- **models.cpp**: ~120 low-poly models built from 15 unit shapes (box, ball, cylinder, cone, pyramid, wedge, torus,
  disc, dome, star, heart, rock, arc, ring, tube): people, animals, plants, buildings, vehicles, things, food and
  symbols for ideas (heart, money, question, bulb, clock, up / down, speech, globe...). Their parts move: legs, arms,
  wings, tails, heads, wheels, rotors, flames, clock hands, pages, flags.
- **scene.cpp**: a scene from error.blue -> the place (17: grass, forest, jungle, desert, beach, sea, snow, mountains,
  city, room, space, moon, underwater, clouds, lava, stage, void), its sky for the time of day, weather (rain, snow,
  storm with lightning, fog, wind), mood (a tint, how lively things move), effects, what the place brings by itself
  (trees, buildings, planets...), the things placed in slots (center, left, right, back, front, around, sky, far) and
  doing what they do (walk, run, jump, fly, swim, spin, dance, wave, talk, sleep, shake, float, drive, grow, fall), and a
  camera that frames them and circles slowly. Unknown things can come as parts ("custom").
- The scene format (`src/dream.js` checks every answer against it):
  `{title, said, place, time, weather, mood, fx, cam, things: [{m, n, a, at, c, c2, s} | {m: "custom", parts: [...]}]}`

## error.blue (src/dream.js)

- `POST /api/dream {text, lang, id, code, mind, prev}` -> `{ok, scene, n, via}`: Workers AI
  (`@cf/meta/llama-3.1-8b-instruct-fp8-fast` QUICK, `@cf/meta/llama-3.3-70b-instruct-fp8-fast` SMART; env
  `DREAM_QUICK` / `DREAM_SMART` to change), with a word list (English, Dutch, German) when the AI fails or today's limit
  (`DREAM_DAY_SCENES`, default 1500) is reached. A scene costs roughly 15 (QUICK) to 90 (SMART) neurons; Workers AI's
  free allowance is 10,000 a day, shared with EAR-CUBE's speech to text.
- `POST /api/dream/cube` (state, live lines, pairing answers -> commands), `GET /api/dream/near` + `/api/dream/pair`
  (linking by itself), `GET|POST /api/dream/app` (the page). D1: `dream_cube` (+ ipk, interp, linen, mark),
  `dream_cmd`, `dream_scene` (300 per cube), `dream_line` (3000 per cube), `dream_tx`, `dream_pair`, `dream_use`.

## Build, test, publish

- PC simulator: `sh esp32/ErrorCube/sim/build-dream.sh && printf 'wait 8000\nshot dream\n' | ./esp32/ErrorCube/sim/dreamsim --out /tmp/d`
  (`SIM_SCENE=k`, `SIM_NONET=1`, `SIM_PAUSE=1`, `SIM_FPS=1`, `SIM_EVERY=ms`).
- Build + publish: bump `CUBE_VERSION` in `src/dream/dream_config.h`, then `tools/dream-fwpub.sh "what's new"` and push.
  It writes `public/dream/flash/` (dream.bin, bootloader, partitions, boot_app0, manifest.json, firmware.json and
  `DREAM-CUBE-flash.zip` with `flash/flash.bat` + `flash/README.txt`).
- FQBN: `espressif:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,FlashSize=16M,FlashMode=qio`.
