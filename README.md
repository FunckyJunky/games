# Game Lab

A test site for HTML games at **https://error.blue**, hosted on Cloudflare.
Every push to `main` is deployed automatically.

## How it's organised

```
public/                 the website
  index.html            game menu (ERROR-DOS): the DOS list, or the 3D view (V, or VIEW in setup):
                        a large panel with every program's 3D logo as a card, scrolling sideways in 3D
  games.json            list of games shown on the menu
  lib/gamelab.js        helper for scores, saves, multiplayer and music
  games/
    pixel-hop/          one folder per game (index.html + music, images...)
    pixel-party/
  apps/
    error-chat/         ERROR-CHAT: text chat by channel code; with 2+ people in a channel: CAM / MIC (/cam /mic /hangup)
  tusa/                 Туса У Берліні
  music3d/              STUDIO 3D (the FunKey game, as WebAssembly), at error.blue/music3d
  vgadai/               Вгадай що?! (quiz show)
  games/mistwalkers/    Mistwalkers (3D walk for three, built from the World Tester project)
  games/tussen-de-linies/  Tussen de Linies (LINIES.HTM): you're the interpreter on a training field, between Dutch
                        instructors and Ukrainian trainees. Listen inside a speaker's circle, keep it (3 slots),
                        bring it to the arrow and pick the right translation (NL <-> UA quiz). Solo = 5 days with
                        scores; SAMEN = 2-4 players in room tussen-de-linies/<CODE> (?room=CODE): the oldest player
                        hosts the game and sends the whole state 8x a second, the next one takes over if they leave.
                        The sentences are in PHR near the top of the file; DAYS sets how busy each day is
  glasses/              Home screen for Meta Ray-Ban Display glasses: 3D app slider (swipes or head motion), big clock
                        with compass above (DOS look; colour, sounds and head motion set in glasses/settings/)
  glasses/mini/         Mini Hub: Clock, Counter, Dice, Snake (open one directly with ?app=clock|counter|dice|snake)
  glasses/hud/          Target HUD: dot-matrix horizon/tilt display with simulated targets (head-motion sensors)
  glasses/flight/       Dot Flight: wireframe flight sim steered by where you look
  glasses/synth/        SYNTH.EXE: synth sequencer + drum machine played with Neural Band hand motion (3D DOS style)
  glasses/crumbs/       CRUMBS.EXE: breadcrumb trail recorder with turn-by-turn navigation back to the start
  glasses/geolock/      GEOLOCK.EXE: pinch to lock a spot, then an arrow (3D or 2D), compass line, radar or 360 view
                        leads you back, with optional voice directions (or voice only, screen blank).
                        Spots + settings at /api/hud/<CODE>/geo; arrows.js is shared with /hud
  glasses/spectrum/     SPECTRUM.EXE: spectrum analyzer (bars, 3D, waterfall) with LP/HP/BP/notch/peak filters
  glasses/drunk/        DRUNK.EXE (Drunk-o-meter): walking stability game - WALK THE LINE (head sway/tilt, straight line,
                        Neural Band hand steadiness) and EGG & SPOON (keep a virtual egg on a spoon). Settings + results:
                        /api/hud/<CODE>/app/drunk, set on error.blue/hud (DRUNK screen). Just a game, not a sobriety test
  glasses/devil/        DEVIL.EXE: run from the devil. He rises 200 m away (real GPS spot) and walks at you at 4 km/h;
                        world-locked 3D view from the head sensors, steps from the accelerometer
  glasses/settings/     SETUP.EXE: compass under clock, head motion (speed, invert), HUD colour, sounds, full screen HUD, apps on home,
                        and the 6-letter CODE that links the glasses to the control panel
  glasses/presence.js   tells the control panel the glasses are on and which app is open (every glasses app loads it)
  glasses/yaro/         Yaro-Vission: the wearer gets orders (move, take, jump, hug, dance) from the CON page
  glasses/con/          Yaro-Vission CON: live first-person view from the wearer + game controls (error.blue/glasses/con)
  glasses/coms/         COMS.EXE: see the phone's camera, hear its mic and get its text read out loud (link.js = the link)
  coms/                 COMS phone page (error.blue/coms): send camera (front/back), mic and text to COMS.EXE
  radar/                Friend Radar (error.blue/radar): show a QR code, a friend scans it and sees you live on a radar;
                        accept them to see them back. Room friend-radar/<CODE>, positions end-to-end encrypted with the key
                        in the link's #part
  rifle/                SPACE RIFLE (error.blue/rifle): GPS + compass laser tag for phones. Join by QR (room space-rifle/<CODE>,
                        encrypted like Friend Radar). The phone's top edge is the barrel; a red-dot scope shows a fisheye 3D
                        neon grid world (own canvas renderer, drawn at half resolution) with wireframe troopers where the
                        players really are. Hit zone widens with GPS error. 3 hull, 5 cells, shield, respawn, practice drone
                        SPOT: point at a player you can see and the compass fixes their direction (mutual spots averaged,
                        fades after 3 min). Steps + compass move you between GPS fixes (dead reckoning)
  lib/qrcode.js         QR code generator used by radar/ and rifle/ (MIT, Kazuhiko Arase)
  glasses/mykolai/      Святий Миколайчик: find the gifts left for Yaro (3D radar, close-up finder, pick up at 7 m)
  glasses/room404/      ROOM404.EXE (Room 404): a small 3D room you look round with head motion, like VR. Pinch = walk/stop,
                        hold pinch = walk, pinch twice = take or put down (table, chairs, shelf, sill, floor, on the dice).
                        Neural Band wrist twist spins what you hold. Lamp switches, the door doesn't open. Own z-buffered
                        renderer; the room's state is kept in localStorage "room404.state"
  glasses/SM/           Святий Миколайчик gift page for phones: leave a gift on the spot where you stand (error.blue/glasses/SM)
  hud/                  Glasses HUD control panel for phones (error.blue/hud), ERROR-DOS look, installable web app:
                        connected or not, app in use, live preview, show/hide apps, HUD settings, GeoLock settings and
                        saved spots, Святий Миколайчик and Yaro-Vission pages. Uses /api/hud/<CODE> and the live link.
                        Edit public/hud/ directly (index.html, sw.js, manifest.webmanifest, icons)
  pocket/               pocket.DOS (error.blue/pocket): the menu.js and apps the ESP32-S3 1.47" LCD board downloads and runs
                        (Duktape), played the same way in the browser; listed in apps.json (bump v to make boards fetch again).
                        apps/walk.js = DOT WALK: a 3D night walk through a little town drawn as a dot matrix (53 x 28 dots:
                        draw at 3 px per dot, then black lines over the gaps). Raycaster with walls of different heights,
                        tilt to walk/turn (calibration like NEON FLIGHT), TAP = map, find 8 places (saved on the board).
                        Lowers its own detail when the board can't keep up
  thuisfront/           HOME FRONT (error.blue/thuisfront): screens at home, controlled from a phone (see "Home Front" below).
                        index.html = the phone side (3D carousel of screens, then the COMM web app), text.js = text to
                        picture, dev/ = the screen itself (dev/index.html runs in any browser, dev/render.js draws for
                        both), flash/ = put the ESP32 firmware on a board from Chrome (ESP Web Tools)
lib/online.js         "I'm in this game" beacon for the who's-online list (GET /api/online); game pages under
                        /games/, /voyage and /music3d get it from the server, other pages load it themselves
esp32/                  ERROR-OLED: ESP32-C3 + OLED box that shows who's online in which game and does ERROR-CHAT
                        (see esp32/README.md)
esp32/HomeFront/        HOME FRONT firmware for the ESP32-S3 1.47" LCD board (see esp32/HomeFront/README.md)
src/index.js            server: scores, saves and multiplayer rooms (/api/*)
tools/soundrooms.mjs    makes the radio track lists (runs before every deploy)
wrangler.jsonc          Cloudflare settings (database, rooms)
```

## Adding a game

1. Put the game in its own folder: `public/games/<game-name>/index.html`
   (music, images and other files go in the same folder).
2. Add it to `public/games.json`:

```json
{
  "title": "My Game",
  "description": "One line about the game.",
  "path": "games/my-game/",
  "color": "#4dd4ff",
  "tag": "new"
}
```

   `folder` is `games`, `online` or `apps` (the three tabs in the ERROR-DOS menu).
   `logo` picks the 3D logo: `hop`, `party`, `wall`, `ship`, `chat`, `tusa`, `vgadai`,
   `studio`, `mist`, `tolk`, or leave it out for the name over a synthwave sun.

3. Push to `main`. The site updates within about a minute.

## Radio tracks (SoundRooms)

A game can have a `SoundRooms` folder, just like the one on the FunKey's SD card. Leviathan Voyage
plays the tracks in `public/voyage/SoundRooms/` on its radios:

- `radio1.mp3`, `radio2.mp3`, ... are the channels, in that order.
- `manual.mp3` is the MANUAL channel. It also plays as narration while the manual is open.
- Any other `.mp3` or `.m4a` becomes a channel too, after those, in alphabetical order.

To add one, put the file in that folder and push. On GitHub: open the folder, then
**Add file → Upload files**. Each file can be up to **25 MB**. A bigger file is left off the site,
with a warning in the build log. Mono at 22 kHz and 64 kbps sounds the same as on the FunKey and fits
about 50 minutes in 25 MB.

Before each deploy, `tools/soundrooms.mjs` writes the folder's `tracks.json`, which lists the files and
how long each one is. You don't need to edit it.

## Using scores, saves, multiplayer and music in a game

Add `<script src="/lib/gamelab.js"></script>` to the game. The top of
`public/lib/gamelab.js` lists everything it can do, for example:

```js
await GameLab.submitScore('my-game', 'Alex', 1234);
const top = await GameLab.topScores('my-game', 10);

await GameLab.save('my-game', { level: 3 });
const progress = await GameLab.load('my-game');

const room = GameLab.joinRoom('my-game', 'ABCD', { onMessage: (data, from) => {} });
room.send({ x: 10, y: 20 });
const who = await GameLab.whoIsIn('my-game', 'ABCD');   // { players: [{ name }], max: 16 }, without joining

const music = GameLab.music('music.mp3');
music.play();
```

Music files: mp3 works in every browser. Each file can be up to 25 MB.

## Running it on a computer

```
npm install
npm run dev      # http://localhost:8787 with a local test database
```

## Glasses HUD control panel (error.blue/hud)

A phone page for the glasses home screen at error.blue/glasses.

- Linking: SETUP.EXE on the glasses shows a 6-letter CODE (top right). Type it on error.blue/hud.
  The code is made on the glasses the first time (localStorage `glasses.code`).
- Settings and hidden apps are saved on the server under that code: `GET/PUT /api/hud/<CODE>`,
  D1 table `hud_profiles`. The newest change wins (glasses or panel). The glasses check on opening
  the home screen and every 30 s.
- Live link (changes show up on the glasses at once): the glasses and the panel both join the
  WebSocket room `/api/room/hud/<CODE>` (the glasses as "glasses", the panel as "panel").
  `presence.js` does this on the glasses side for every app:
  - panel → glasses: `{t:"settings", settings, updated}` is stored in `glasses.settings` and handed to
    the open app (`GlassesPresence.onSettings(fn)`). The home screen and SETUP.EXE apply it at once;
    CRUMBS and SPECTRUM restart for a new colour. Other messages go to `GlassesPresence.on(type, fn)`,
    e.g. `{t:"geo"}` makes GEOLOCK.EXE fetch its spots and settings now.
  - glasses → panel: `{t:"hello"|"state", app, focus, state}` (app in use, card in view) and
    `{t:"settings"}` when SETUP.EXE changes something. The glasses answer each change with `{t:"ack"}`,
    so the panel can say "applied on the glasses".
- Any glasses app can keep its own settings and a results log there: `GET/PUT /api/hud/<CODE>/app/<app>`
  (flat values only: true/false, numbers, short text; newest wins) and `POST .../app/<app>/log {entry}`
  (the newest 30 are kept). Live: the panel sends `{t:"<app>", settings, updated}` and the app listens with
  `GlassesPresence.on("<app>", fn)`; the app sends results with `GlassesPresence.send("<app>-result", {entry})`.
  DRUNK.EXE is the first to use it.
- The bottom of the panel is a rail of 3D keycaps that scrolls sideways (cover-flow, like the glasses home).
- Web app: `public/hud/manifest.webmanifest`, icons and `sw.js` (keeps the page for a bad connection;
  never caches /api/). The page's INSTALL.EXE box shows how to add it to an iPhone or Android home screen.
- Connected / app in use: every glasses app loads `/glasses/presence.js`, which posts
  `/api/hud/<CODE>/ping` every 15 s (and "hidden" when the display goes off). The panel counts the
  glasses as connected if a ping came in the last 40 s; on the home screen it also shows the card
  you're looking at. Inside the panel's live preview the script stays silent.
- The app list is read from `ALL_ROWS` in `public/glasses/index.html`, so new apps added there show
  up in the panel by themselves. A new glasses app should load
  `<script src="/glasses/presence.js" data-app="<id>"></script>` with the same id as in `ALL_ROWS`.

## Yaro-Vission (error.blue/glasses/yaro + error.blue/glasses/con)

A game where one person wears the Meta Ray-Ban Display glasses and another steers them like a game
character, watching a live first-person view.

- `public/glasses/yaro/index.html`: the glasses app (YARO card on the glasses home screen, and the Mini Hub tile "Yaro"). It shows a link code; pinch
  starts it (sound, voice, head sensors and the camera need that gesture). Orders appear as dot-matrix
  arrows and stick-figure animations and are spoken (swipe up: voice on/off). Pinch = DONE, which tells
  CON. Swipe left/right: colour. Swipe down twice: new code. A move that CON stops refreshing ends by
  itself after 2.5 s
- `public/glasses/con/index.html`: CON. Type the code, then CONTROL YARO: full-screen live view with an
  FPV HUD (compass and tilt from the glasses' head sensors, battery, ping, fps), a D-pad held to move
  (8 ways; the red middle is STOP), ACTION with take, jump, hug and dance, XP for every action Yaro
  confirms. Keys W A S D / arrows, Space, 1-4, E, F; gamepads too. Settings: video Auto / Direct /
  Low-res, which camera, picture fit, sound
- BE THE CAMERA (on the CON page): Meta's web apps don't list camera access, so the glasses app only
  tries `getUserMedia` and goes on without it. The wearer's phone can be the camera instead: same code,
  BE THE CAMERA, phone in a shirt pocket. It also shows the orders, buzzes, has a DONE button and sends
  its compass heading
- `public/glasses/yaro/link.js`: the shared link. Everyone is in the GameLab room
  `yaro-vission/<CODE>` (names GLASSES, CON, CAM). Messages: `hi` (role, cam), `mv` {f, r} (resent
  every second while held), `act` {a, id}, `stop`, `got`, `done`, `tel` {h, p, bat}, `ping`/`pong`.
  Video is WebRTC (STUN only, like TUSA), set up through the room; when it can't connect within 12 s,
  CON asks for `jpg`: about 5 small JPEG frames a second through the room (each under 16 KB)

## COMS (error.blue/coms + error.blue/glasses/coms)

A phone sends its camera, microphone and text to the glasses. One way: phone to glasses.

- Linking: the room is the glasses' own 6-letter CODE (the one SETUP.EXE and error.blue/hud use).
  `public/coms/index.html` takes `?code=`, then the code it used last (`coms.code`), then the one linked on
  error.blue/hud (`hud.panel.code`), so a phone already linked on the panel connects by itself. Otherwise
  type the code that COMS.EXE shows.
- `public/coms/index.html` (phone): GO LIVE (camera + mic with one permission question), CAMERA BACK / FRONT /
  OFF, MIC ON / PUSH (hold to talk) / OFF, text with quick phrases, BUZZ, the read-out voice (AUTO: Cyrillic is
  read in Ukrainian, otherwise the phone's language; or EN, UK, NL, DE), FROM name. Each message shows
  "on the glasses" and "read out loud"; a message sent while the glasses are away goes out when they connect.
  SETUP: PICTURE LOW / MID / HIGH (640x360, 1280x720, 1920x1080 and the bitrate), SOUND VOICE (echo and noise
  cleanup) or ROOM / MUSIC (raw), LINK AUTO / DIRECT / RELAY. Settings in `coms.cfg`. Keeps the screen on while live.
- `public/glasses/coms/index.html` (COMS.EXE on the home screen, Coms in Mini Hub): pinch to start (sound and
  voice need it); a message that came in before is read then. Picture FIT / FILL / SMALL / OFF (swipe left/right,
  `coms.view`), swipe up = phone sound on/off, swipe down = read-out voice on/off, pinch = read the last message
  again (again within 6 s = the one before). With the camera off it shows a live sound wave. The phone's sound
  is turned down while a message is read. Colours follow the HUD colour. Inside the panel's live preview it
  doesn't connect.
- `public/glasses/coms/link.js`: GameLab room `coms/<CODE>` (names `glasses`, `phone`). Messages: `hi`
  (phone: name, v, a, cam, mic, talk, link, liveAt), `want` {mode rtc|relay|off}, `rtc`, `say` {id, s, lang},
  `got`/`said` {id}, `ring`, `gst` (glasses state: started, sound, voice, view, bat, src, mode, w, h, fps).
  DIRECT: WebRTC (STUN only), one connection with a video and an audio slot, so switching camera or turning
  camera/mic off and on needs no new connection. When it can't connect within 10 s (AUTO), or with RELAY:
  JPEG frames (~6 a second, cut into parts under 16 KB) and the sound as 16 kHz mu-law in 100 ms slices,
  both through the room.

## Святий Миколайчик (error.blue/glasses/SM + error.blue/glasses/mykolai)

Anyone can play Saint Nicholas and leave a little gift for Yaro on a GPS spot; Yaro finds it with the glasses.

- `public/glasses/SM/index.html`: the gift page for phones (night sky, a 3D statue on an embroidered pillow,
  a gift tag form). Find me (phone location, needs ±30 m or better), pick one of six statue colours, write
  From and a note (140 characters), Leave the gift here. The page remembers the gifts this phone left
  (`sm.mine` in localStorage, with each gift's secret key) and shows them with "Waiting for Yaro" or
  "Yaro found it"; it checks every 15 s and throws confetti when one gets found. Take it back works until
  Yaro finds it. The map uses OpenStreetMap tiles and shows only your own gifts.
- `public/glasses/SM/statue.js`: the statue (low-poly St Nicholas with mitre, crozier, stole and a present),
  drawn on a 2D canvas; both pages use it. `SMStatue.LOOKS` are the six colours.
- `public/glasses/SM/fonts/`: Rubik and Rubik Bubbles (Latin + Cyrillic), served from the site.
- `public/glasses/mykolai/index.html`: the glasses app (MYKOLAI.EXE on the home screen, Mykolai in Mini Hub).
  LIST: waiting gifts, nearest first (pinch = go get it, swipe right = menu with DEMO GIFT, FOUND GIFTS,
  sound and head sensor). HUNT: a 3D radar with sweep, distance, direction and walking time; within 30 m
  the precise finder (3D view with the statue standing on its spot, a 7 m ring and a WARM/HOT bar;
  swipe up/down switches to a map), Geiger ticks getting faster. Within 7 m (and GPS ±30 m or better)
  the gift is received: confetti, the note, the Щедрик bells, and the server is told. Heading comes from
  the compass when there is one, otherwise from walking plus head turns. The font has Cyrillic capitals,
  so names like МАМА show.
- server: `/api/sm/gifts` in `src/index.js`, D1 table `sm_gifts`. Picking up checks that the glasses are
  within 20 m of the gift. 12 gifts per hour per network, 200 waiting at most; found gifts are listed for
  30 days.

## DEVIL.EXE (error.blue/glasses/devil)

A GPS chase game for the glasses: DEVIL.EXE on the home screen (APPS row) and Devil in Mini Hub.

- Pinch to start. Look straight ahead for the 3-2-1 (that sets "level"), then he rises out of the ground
  200 m away in a random direction and walks straight at you at 4 km/h, without stopping. Within 2 m he
  takes you: he lunges, screams, and the run ends with the time you survived (best time is kept).
  Walking faster than 4 km/h keeps you alive. Get 400 m (twice the start distance) away and he loses your
  trail; 12 s later he rises again, 200 m away, somewhere else. Speed (3-6 km/h) and start distance
  (100-500 m) are in the menu.
- The view is world-locked, like VR: black is see-through on the display, so all you see is him, standing
  where he is. REAL SIZE matches the lens (about 14 degrees across, 1:1 with the world: a 2 m devil at 200 m
  is a few pixels, at 5 m he fills the view). VR WIDE squeezes 40 degrees in. Swipe up/down switches.
  Head turn, nod and tilt (roll) come from `deviceorientation` (the glasses' gyroscope + accelerometer).
  When he's out of view, red chevrons show which way to turn ("BEHIND YOU").
- Which way is north: the compass when the device gives one; otherwise learned while you walk (GPS course
  vs. head turn, only while the accelerometer counts steps and the gyroscope says you're not turning your
  head). Until then he's placed relative to where you looked at the start. HUD shows COMPASS / WALK-N / NORTH ?.
- Location: the phone's GPS, smoothed with a small Kalman filter plus the phone's speed.
- Sound: heartbeat getting faster, a low drone that opens up, his hoof steps panned left/right (muffled
  behind you), a deep devil voice (speech synthesis, menu: DEVIL VOICE), a sting when you spot him.
  Pinch during a run says how far and which way he is.
- A run survives the display going off or a reload (localStorage `devil.run`, kept 30 min): he keeps
  walking meanwhile, and on wake he has walked those minutes toward you.
- TEST RUN (menu) works without GPS: he rises 30 m away and swipe up/down walks you 4 m.
- Settings: localStorage `devil.cfg`; best time `devil.best`. `window.__devil` is a debug handle.

## Web app

error.blue can be installed as an app (Install app in the browser menu, or
Share > Add to Home Screen on an iPhone; ERROR-DOS setup has INSTALL APP).

- `public/manifest.webmanifest` and `public/icons/`: name, icons, full screen
- `public/sw.js`: keeps the menu and played games working offline; scores,
  chat and multiplayer always go to the server
- `public/lib/appback.js`: added to every game page by the server; in the
  installed app it shows a small "< ERROR-DOS" button on pages without a back link
- `public/lib/fullscreen.js`: FULL SCREEN in ERROR-DOS setup (saved as `full` in
  `errordos-settings`). The menu loads it, the server adds it to every game page, and
  TUSA, Вгадай що?! and ERROR-CHAT load it themselves. Browsers only allow full screen
  after a tap or a key and leave it when a new page opens, so with FULL SCREEN on each
  page goes full screen at its first tap or key. Leaving full screen yourself (Esc, the
  back gesture) turns the setting off. Where full screen isn't possible (iPhone), setup
  shows N/A and points to INSTALL APP.

## Туса У Берліні (error.blue/tusa)

`public/tusa/`: Vika, Vito and Oleksiy meet at Hoongy in Berlin.

- log in as VIKA (also VICA, VICTORIA), VITO or OLEKSIY
- `art.js` draws everything; `tusa.js` is the game; `radio/` holds the stations
- the shared restaurant is the `TusaRoom` Durable Object at `/api/tusa`
- radio: put an mp3 in `public/tusa/radio/` and add it to `radio/stations.json`.
  The radio stands on a cabinet inside Hoongy (left wall): walk to it and press A to
  go off → 88.4 → 93.7 → off. It is only heard inside, louder close by. Every station
  starts at 20% of its song when the evening begins and keeps playing (and looping)
  in the background, the same for everyone.
- `voice/hoongy-commentary.mp3` (your recording from 03:49) fades in on the street while you wait alone; it fades out and stops
  for good when the first friend arrives
- 3 jokes within 25 seconds make the other friends roll on the floor laughing;
  walking cancels the laughing. Nobody talks in words: no speech bubbles
- Hoongy (and its kitchen) only opens once a friend has arrived (the 18:00 clock is running)
- Oleksiy rides his step around the street until he parks it (A); A next to it to ride again
- installable on its own: `tusa/manifest.webmanifest` (start /tusa/, full screen, no zoom)
- look: hand-drawn Japanese cartoon style with ink outlines, watercolour and
  ink-wash backgrounds and a washi paper texture, all drawn in code in `art.js`
  (big still parts are painted once into cached layers)
- every text has one fixed language, Ukrainian or Dutch; everyone sees the same
  (laughing, facing, Oleksiy's step, doors, and street life from the shared clock)
- going home from Berlin and walking in through your own door ends the game
- the radio has no button on screen: only the cabinet inside Hoongy controls it
- late twilight on the street: a blue dusk wash, glowing windows, neon, lamps and
  headlights; through Hoongy's window you see the room in 3D (like the photos)
- all three at one table: the camera moves in; friends enjoy every bite (sparkles)
- camps have a fence, a guard and an exit gate: through the gate you leave for
  Berlin, and come home
- the friends are drawn after a real photo of Vito, Vika and Oleksiy; coming home
  shows that photo as a pixel-art polaroid (`snap/`): the picture stands still and
  the three of them breathe, sway and blink; it is also on the end card
- talking: the 🎙 PRATEN button (or M) turns the microphone on; everyone with the
  microphone on is in one call (WebRTC, set up through /api/tusa), and the
  speaker's mouth moves
  (your own radio goes quiet while your mic is on)
- the top bar is a dot-matrix DOS screen (place, time, and a ticker with friends,
  radio, order, mic); the DOS button is a 3D ERROR-DOS logo that glitches, now and
  then into ERROR.BLUE
- Vito and Oleksiy, standing close, get a 👃 NEUSJE button (or N): a nose kiss
- notifications: 🔔 MELDINGEN on the login screen. Typing your name sends
  "… is net in de game gestapt" to every other phone that turned them on (once per
  10 minutes per friend). Web Push from the Worker (`/api/push`,
  `/api/tusa/hello`); the VAPID keys are made on first use and kept in D1
  (`settings` row `vapid`), phones in table `push_subs`. On an iPhone it works
  only when TUSA is on the home screen (iOS 16.4+). For local tests put
  `PUSH_LOCAL_TEST="1"` in `.dev.vars`

## Mistwalkers (error.blue/games/mistwalkers/)

`public/games/mistwalkers/index.html`: a slow morning walk for up to three explorers through misty
pine woods, in 3D. Listed in ERROR-DOS under ONLINE (MISTWALK), with the `mist` logo.

- the page is built from the World Tester project (one generated file, three.js inside); don't edit it
  by hand, rebuild it and copy `build/game-errorblue.html` over it
- pick a name, Female or Male, colours and a camp name. Everyone with the same camp name walks
  together, three per camp. The camp name becomes the room code (letters and digits, at most 8)
- rooms: the normal GameLab rooms (`/api/room/mistwalkers/<CODE>`); each explorer sends its whole
  state about 8 times a second (`{ t: 'st', s: {...} }`). The one who has been there longest keeps
  the clock, so everyone has the same time and weather
- Talk: turns your intercom on and off. Everyone with the intercom on is in one call (WebRTC, set up
  through the same room, like TUSA); voices get quieter with distance and are silent beyond 160 m
- camp: campfire, tent, Christmas lights, sit, sausages, coffee, sleep; moves: dance, hug, wave,
  cheer, sit, lie down; chat with emoji
- time: real time by default, in the average of everyone's time zones (a shared switch under
  Advanced turns it off; then a time-speed slider and the first explorer's clock take over)
- weather: mist and rain, and alone (or without music) it slowly turns into a storm. When two
  explorers stand within 15 m of each other, both playing, and one of them on the drum machine,
  the fog and rain lift within 300 m of them
- light: tree lights (wind them round the nearest tree, up to eight; brighter at night) and an oil
  lantern at the hip with a shimmering flame. Three real lights go to the lantern, the nearest fires
  and lit trees
- wildlife: after dark an owl sometimes perches on a tree and hoots; at dawn and dusk a curious fox
  sometimes comes to sit and watch (only when it's quiet, never near a fire). On a dark night with
  no music and no fire near, a witch stands 200 m away: she creeps closer while you don't look, stops
  at 25 m, and draws back faster than you can walk. The mist is thinner around her; her eyes are
  dark green
- music (lo-fi, everyone in sync): drum machine, note synth and e-piano (set down on the ground and
  sit at it, or play it from your backpack), guitar, flute and saxophone (played in your hands).
  Each has styles, eight steps (two beats each, four bars) and a 🎲 Surprise me button. Tempo, key,
  scale and chords are shared: anyone can change them (newest change wins). Instruments get quieter
  with distance (Advanced: hearing distance). Characters groove to what they hear
- drum machine: its own punch bus (compressor and drive), a deep 808 bass on the chord's root that
  follows the kick, and eleven styles (boom bap, lazy, breaks, jazz hop, trap, house, one drop,
  shuffle, half time, bossa, dusty) with the odd fill. No record crackle or hiss on the music
- while your intercom is on, the game's own sounds (music, rain, wind) go quiet on your phone, so
  nothing echoes back into the call
- starting setup (the owner's own settings, `GD` in the World Tester's main.js): quality Low, Cinematic
  lens, film look, GI, ray tracing, map optimization and the phone fix on, light strength 185%,
  distance 255%, glow 55%, brightness +4, contrast +33, gamma 1.51. A player's own choices are kept;
  lens and quality saved automatically under the old defaults gave way once (`wt.cfgv` = 2).
  Advanced settings ends with "Copy my settings" (one line of text with everything set right now)
- XREAL glasses (desktop only, Advanced settings): VR mode for the XREAL One / One Pro. Head tracking
  comes from the local XREAL bridge (Documents\xreal-vr\Start XREAL VR.bat, ws://localhost:8765/ws,
  28-byte samples: dt, gyro xyz rad/s, accel xyz m/s^2), fused in the page (gyro + gravity tilt
  correction, drift tracking; from the XREAL VR page). Your head looks around, the mouse turns your body,
  W walks where you look; view "Through your eyes" (own explorer hidden) or "Behind you". The camera
  uses the glasses' field of view (57 deg diagonal by default) and no depth-of-field blur. Stereo 3D:
  side-by-side, one picture per eye (full SBS on a 32:9 screen, half SBS on 16:9), the interface hidden,
  messages shown in both eyes. Calibration: 5 steps (still, left, right, up, down) learn the sensor
  axes; recenter (R), flip up/down, field of view, eye distance, 3D depth, prediction. Keys: R recenter,
  F fullscreen on the glasses' screen, 3 stereo on/off. Stored in wt.xr and wt.xrmap.
  Fullscreen switch (on the glasses or on this screen); Center view button; "Head only" (the glasses turn
  just your explorer's head, the camera stays behind you; the head pose goes out in presence as `hd`
  so others see it). VR comfort: immersive view (interface hidden; Tab turns it on and off, H shows the interface for a moment), comfort vignette while
  walking or turning (strength slider), walk where you look, head tilt on/off (level horizon), eye
  height, steadiness (smoothing), turn speed, interface size
- Advanced settings, Lighting & map: global illumination (screen-space bounce light and ambient
  occlusion), ray tracing (screen-space reflections on wet ground and water) and map optimization
  (the world is only built as far as the fog lets you see, and far terrain is much simpler; the
  picture stays the same, memory use drops a lot). GI and ray tracing are heavy on phones.
  Three light sliders: strength (how much fires, lanterns and lamps light up around them), distance
  (how far their light reaches) and glow (how strongly the flames, bulbs and lamps themselves shine,
  with a bloom halo). Picture: brightness, contrast and gamma, with a Reset button
- Advanced settings, Quality: "Phone graphics fix" switch, for the black-and-white blotchy picture
  some Android phones show. It renders without multisampling, keeps NaN/infinite values off the
  screen (scene, sky, clouds and every picture pass) and drops the film look's automatic soft focus.
  On by default in Mistwalkers (in the World Tester: on for phones and tablets that aren't Apple). Shaders also no longer call
  smoothstep with reversed edges (undefined in GLSL; the rewrite looks the same where it worked)

## Вгадай що?! (error.blue/vgadai)

`public/vgadai/`: a TV quiz show. Listed in ERROR-DOS under ONLINE (VGADAI).

- log in on the DOS screen like in TUSA. VIKA, VITO and OLEKSIY (same names and
  spellings as TUSA) look like the TUSA friends; any other name (2-10 letters or digits)
  gets a random look. That look's seed is kept in D1 (`vgadai_players`), so it is the
  same on every phone; 🎲 in the lobby rolls a new one
- every game is an episode: pick a season (01-99) and an episode (01-99) in the lobby.
  Together they are the 4-digit room code (S03 E12 = `0312`). 🎬 NEW EPISODE only works
  for a code nobody used yet; 📺 JOIN goes into an existing one. The lobby shows the
  episode's leaderboard before you join, and the episodes on air right now
- one player types a question; the others type one word or one number and race to press
  the green button. The first press is heard; the one who asked presses ТАК or НІ.
  НІ: the game goes on (that player waits 3 seconds). ТАК: +1, everyone sees the answer,
  and 7 seconds later the next player asks. The one asking can give a 💡 hint, or end the
  round with 🏳 (nobody knew) and show the answer
- each episode keeps its own leaderboard (wins, questions asked) and the history of every
  question and guess. There is no global leaderboard. ⚙ → 🔄 resets the episode
- on-screen keyboard: the ERROR keyboard with a Ukrainian layout (ЙЦУКЕН, і ї є ґ ') and
  УКР / EN; a computer keyboard works too
- the show host reads questions written in Cyrillic out loud in Ukrainian (the phone's
  own speech voice; ⚙ → 🗣 turns it off). If the device has no Ukrainian voice, it says so once
- 🔔 notifications (⚙): a new question and a new answer in your episode. Web Push like TUSA,
  but the subscriptions live in the episode, not in `push_subs`. Not shown while the game
  is open on screen. Tapping one opens `/vgadai/?ep=0312`
- the studio is drawn in 3D in `vgadai.js` (perspective floor of LED tiles, podiums,
  spotlights, curtains, a sign with bulbs) with the TUSA characters from `/tusa/art.js`
  standing in it as paper cut-outs, and a handheld camera that sways and follows the action
- server: the `VgadaiRoom` Durable Object, one per episode (`/api/vgadai/<SSEE>`);
  `/api/vgadai/<SSEE>/info`, `/api/vgadai/live`, `/api/vgadai/look`
- ⚙ → 🛠 Розширені erases: one episode or a whole season (history and leaderboard; the code
  is free again), only the players (looks and every leaderboard; the questions stay), or
  everything. Each needs a 1.6 s press and hold. Anyone can do it: there is no password.
  `/api/vgadai/erase` asks each episode's Durable Object to erase itself (`/<SSEE>/erase`);
  the list of episodes is the D1 table `vgadai_rooms`
- installable on its own: `vgadai/manifest.webmanifest` (full screen, no zoom)
- on a computer F switches full screen (when you aren't typing a name, question or answer;
  then F is a letter), and so does the small ⛶ F button (bottom left of the studio and the
  lobby, top right of the login). Esc opens the menu (continue, full screen on/off, settings,
  leave the episode, ERROR-DOS); in Chrome and Edge the game keeps Esc in full screen
  (Keyboard Lock), holding Esc still leaves. Phones and tablets don't get the switch

## Studio 3D (error.blue/music3d)

`public/music3d/`: the FunKey S game STUDIO 3D, a music studio to walk around in and
play (drum machine, two synths with their sequencers, a master panel, all cabled up). The C game
runs unchanged as `studio3d.wasm`; `index.html` draws its 240 x 240 picture crisp and as big as it
fits (whole multiples of 240 when there is room), plays its 22 kHz sound, and keeps the song,
the room's lights, the settings and where you stand in the browser (localStorage `studio3d-save`).

- computer: click the picture and the mouse looks around (pointer lock). W/S walk, A/D step
  sideways, arrows or Q/E turn, left click = A, right click = Y (hold on knobs and faders), the
  wheel turns knobs, faders and note sliders a step at a time, Space play/stop, hold Tab for the
  map (M keeps it on), Esc opens the menu and gives the mouse back, F full screen. R/V look up and
  down without a mouse. In the menu: arrows or W/S, Enter, Esc, the wheel, or click a row.
- phone or tablet: the stick (left) walks and steps sideways, dragging the picture looks around,
  tapping a control turns the view onto it (too far: "COME CLOSER"), then A or Y (held on knobs).
  ▶, MAP and MENU sit in the top bar; in the menu, tap a row to pick it and again to do it.
  Upright: picture on top, controls under it; on its side: stick left, picture, A and Y right.
- gamepad: left stick walks, right stick looks, A (bottom) and X (left) are the game's A and Y,
  B backs out of menus, LB/RB step sideways, Start plays, Back/Select opens the menu, LT shows the map.
- the game's hints follow what you use: mouse buttons on a computer, A / Y on touch and gamepads.
- its menu's last line, SAVE, BACK TO ERROR-DOS, saves and returns to the menu here.
- JAM ONLINE (on the start screen): one studio online for everyone, up to 16 people, no room
  codes. Type a name, GO IN. It is the GameLab room `studio3d` / `STUDIO` (no server state);
  the start screen and the jam panel say who is in now (`GET /api/room/studio3d/STUDIO/who`,
  asked every 6 s). INVITE shares or copies `error.blue/music3d/?jam`, which opens the jam panel.
  When the studio is full the panel says so, and a player who still gets turned away plays alone.
  - shared: the whole song (every knob, pattern, step and note, mute/solo, the drum kit), PLAY
    and STOP, and the room's LEDs and neon. Each change goes out as short `key=value` lines
    (`web_net_diff`), the others apply them (`web_net_apply`).
  - the player who has been in the studio longest holds the song (♫ in the list): a newcomer
    gets it from them (`hello` -> `state`), they send the clock every second and the whole song
    every 4 s, so everyone ends up the same even if two people turned one knob at once (what you
    changed in the last 1.5 s is not overwritten). When they leave, the next one holds it.
  - the clock: every player renders the song themselves, in step with the holder's clock. Each
    one measures the way to the holder (half the quicker round trips) and adds that to the
    clock they receive; far off, they jump at once; a little off (over 20 ms), only after three
    readings in a row agree. Someone else's PLAY starts that far into the first step.
  - notes and drum hits played by hand are sent as they happen (`ev`); each player's spot, view,
    and the control under their dot go out 10 times a second (`pose`) and the others see a robot
    in that player's colour with the name over its head (A-Z, Cyrillic spelled out), sitting
    when they sit, with a beam to what they point at; the map shows them as coloured dots.
  - your own song is not touched: online, the studio's song is kept as `studio3d-jam`, and
    LEAVE (✕) goes back to your own. Whoever opens an empty studio brings the studio's song as
    they last had it.
  - tap ONLINE in the top bar for who is in, INVITE and LEAVE. On a phone it gets a row of its
    own under the top bar.
  - test: `npx wrangler dev`, then `python3 tools/jamtest.py http://127.0.0.1:8787 out` in the
    game's source (two players, one on a computer and one on a phone, and a full studio).
- ERROR-DOS shows it as STUDIO3D.HTM in GAMES, with its own wireframe logo (`studio`). The old
  address /games/studio3d/ redirects to /music3d/ (`SHORT_PATHS` in `src/index.js`).
- the source is the game's own (`studio3d-source.tar.gz` in the Funkey S project). After a change:
  `make web` there (needs the WASI sysroot, see its README), copy `web/studio3d.wasm` here and
  raise `?v=` where `index.html` loads it: the service worker keeps `.wasm` files, so a new
  address is what makes phones fetch the new one.

## Access code (Leviathan)

Leviathan Voyage lives at error.blue/voyage (files in `public/voyage/`); the old
address /games/leviathan-voyage/ redirects there (`SHORT_PATHS` in `src/index.js`).

`games.json` marks a program with `"lock": true`. ERROR-DOS then asks for a
5-digit code on its own number pad before it runs it. Leviathan has a fixed
code (the owner knows it); `src/index.js` holds only a salted hash of it in
`LOCK_FIXED`. It's a fun lock, not strong security. A right code sets a cookie
for 30 days; without it the game's files are closed (pages send you back to the
menu, other files get 403). 8 wrong tries per 10 minutes, then the pad waits.
To change the code: put a new salt and sha256(salt + ":" + code) in
`LOCK_FIXED`, or set a Worker secret `LEVIATHAN_CODE`, which always wins.
(A program with `"lock": true` but no fixed code asks the first visitor to
choose one, and offers [C] CHANGE CODE.)

## App mode

Every page behaves like an installed app: no double-tap or pinch zoom
(`/lib/nozoom.js`, `touch-action: manipulation`); game pages get this added by
the Worker as they are served.

## Who's online (error.blue/api/online, for ERROR-OLED)

- Every game/app page loads `/lib/online.js`. It posts `{sid, path, name, detail}` to `/api/online` every 25 s,
  and `{sid, bye}` when the tab closes or is hidden. Someone counts as online for 60 s after their last beat.
- The name is `window.errorOnline.name` if the page set it. Otherwise it's the ERROR-CHAT/GameLab name
  (`gamelab-player-name`), then the TUSA/Вгадай що name (`tusa-name`), then "guest".
  TUSA sends the character, Вгадай що?! the player plus `S01E04`, and the glasses their app (e.g. `FLIGHT.EXE`).
  A game can add its own: `window.errorOnline = { name, detail }; ErrorOnline.update();`
- `GET /api/online` returns `{now, total, games: [{id, title, dos, folder, n, players: [{name, detail, secs}]}]}`.
  Pages are matched to games through games.json (plus `/rifle`, `/radar`, `/glasses`, `/oled` and the menu at `/`).
  Room and channel codes are never sent.
- The list lives in memory in one Durable Object (`OnlineBoard`). After a deploy it fills up again within 25 s.

## ERROR-CHAT without a WebSocket

`GET /api/chat/<CODE>/log?after=ID&name=X&limit=20` returns the messages after ID and who is in the channel.
`POST /api/chat/<CODE>/say {name, text}` sends a message. Whoever reads with a name shows up in the channel's who-list
(web users see "X joined") and leaves 45 s after their last read. `&bg=1` reads without joining.
The log also lists `cams: [{id, name}]`, who shares a camera right now.

## ERROR-CHAT camera and microphone

With two or more people in a channel, CAM and MIC appear in the top bar (and on the on-screen keyboard).
Browsers send camera and microphone straight to each other with WebRTC (STUN only, no TURN): the ChatRoom
Durable Object only passes the handshake along. WebSocket messages (client -> room):
- `{t:'media', cam, mic}` - what I share now; everyone gets `{type:'media', id, name, cam, mic}`.
  Every socket has an `id`: in `hello` (`id`, `peers: [{id, name, cam, mic, rtc}]`), `join` (`peer`), `leave` and `nick`.
  Pages that can do WebRTC connect with `&rtc=1`.
- `{t:'rtc', to: ID, data}` - offer/answer/ICE for one person; they get `{type:'rtc', from, name, data}`.
- `{t:'frame', w, h, data}` - a tiny picture of my camera: 4-bit grey, 2 pixels a byte (high half first), base64.
  The page sends 160 x 86 about 5 a second, only for 6 s after the room said `{type:'watch'}`.
- `{t:'getframe', id}` - a browser that can't get a direct line watches those pictures instead (`{type:'frame', ...}`).

`GET /api/chat/<CODE>/frame?id=ID&name=X&s=2` gives the newest picture of that camera (or the first one):
`{id, name, w, h, seq, at, data, cams}`, `{wait:true}` while none came yet, `{off:true}` when nobody shares.
`s=2` = half size (80 x 43, a quarter of the bytes). Asking tells the sharer someone watches. pocket.DOS
ERROR-CHAT shows `[WATCH NAME]` in its TAP list and draws the pictures with `img4()` in its screen colour.

## Home Front (error.blue/thuisfront)

Screens at home that show one web app, controlled from a phone. In ERROR-DOS under APPS.
- **A screen** is an ESP32-S3 1.47" board with the HomeFront firmware (esp32/HomeFront), or any browser at
  error.blue/thuisfront/dev (tap it for name and home code). Its id is `HF-XXXXXX` (board) or `WEB-XXXXXX` (browser).
- **error.blue/thuisfront**: the screens in a 3D carousel (swipe sideways), pick one, send it a web app.
  An optional HOME CODE (set on the screen) hides screens from phones without the same code.
- **Password per screen**: a new screen asks "SET PASSWORD / NO PASSWORD" in its panel (later: PASSWORD: ON/OFF).
  With a password, another phone or browser sees the screen as LOCKED and gets nothing (no state, no text, no photo)
  until it types the password; then it gets a key (localStorage `hf.keys`) so it is asked only once. Changing the
  password needs the old one and locks every other browser out again. PBKDF2 on the server; 5 wrong tries = wait
  30 s, doubling (max 15 min). FORGET needs the key too.
- **COMM**, the first web app: TEXT (14 fonts, size or FIT, still / scroll / rise / type / blink / wave / glitch / pulse,
  speed, colour, background, align, style), PHOTO with the text on top (take or pick one; fill the screen or the
  whole photo, brightness, contrast, filter), VIDEO from the phone's camera (NONE = real colours, or DOS GREEN /
  YELLOW / BLUE / GRAY; sharp 320x172 or fast 160x86, smooth or dithered, brightness, contrast, frames per second)
  or VIDEO+TEXT, and SCREEN ON/OFF, EFFECTS ON/OFF (also in the screen's panel: off = still text, no scanlines,
  no glitches, no blinking, no moving noise; state `fx`), light, scanlines, CLOSE WEB APP. The preview on the phone is drawn with the
  screen's own code.
- **CLOCK**, the second web app: the time in big dot-matrix digits (5x7, unlit dots dimly visible), DOS colours,
  black or dark background, 24 h or 12 h (AM/PM), seconds as a row of 60 dots / big HH:MM:SS / off, date on/off,
  time zone from the phone (IANA name for browser screens; turned into a POSIX TZ string for the board, which keeps
  the time itself with NTP). Drawn by drawClock() in dev/render.js and scrClock() in screen.cpp (pixel-identical).
- CLOCK **SCREEN ON: WHEN MOVED** (firmware 1.9): clock.motion 1 = the screen is dark until the device moves (or
  the button is pressed, or the phone changes something), then on for clock.mtime seconds (5 s - 30 min);
  clock.msens 1 (a shake) - 10 (a touch) -> motionThreshold() 0.5 g .. 0.03 g from the slow average (gravity).
  The board reads the 1.47B's QMI8658 accelerometer (motion.cpp; I2C 48/47, only when the backlight is on 46, because
  on the plain 1.47 pin 48 is the backlight) and says imu:1 in its hello; a browser screen uses devicemotion (phones,
  tablets; iOS asks on the first tap) and stays on when it never feels anything. Without a sensor: always on.
  COMM has the same, with its own comm.motion / msens / mtime (firmware 1.10; SCREEN ON block under the COMM tabs);
  there new text, a new photo or incoming video also keep it awake.
- Time zone fix (1.9): configTime() for the internet time sets TZ to UTC; applyTz() now compares with getenv("TZ") and
  runs again right after configTime(), so the clock no longer shows UTC (an hour early in Amsterdam) after a restart.
  Ping reports lt (the board's HH:MM) and tz (its TZ), shown in /api/hf/list as lt / tzNow.
- **ETA**, the third web app: "when I'll be there". Pick IN 5 MIN ... 2 H or AT a time, a description (8x8 DOS
  letters, wrapped over 1-3 lines), +5 / -5 MIN, I'M HERE, CLEAR. The screen shows the description, a big
  dot-matrix countdown (H:MM:SS / MM:SS, then ARRIVING NOW / +MM:SS LATE), a 50-block progress bar and the times.
  State `eta {from, to (unix seconds), text, pal, done, lang}`; the screen counts by itself. drawEta() / scrEta() match.
  From 1.6: Cyrillic letters (А-Я plus Ukrainian Є І Ї Ґ and Ё; small letters show as capitals) in an 8x8 table
  next to the DOS font (CYR in render.js = CYR8 in screen.cpp, made by tools/hf-cyr.py); text goes as UTF-8.
  `lang` 0 = English, 1 = Ukrainian words on the screen (ETA_TXT) and on the phone's ETA panel.
  A transport word in the description turns the bar into a little scene (sceneOf + SCENE_WORDS; whole words or word
  starts, the first one wins): a train (TRAIN, TREIN, SPRINTER, INTERCITY, ПОТЯГ, ПОТЯЗІ, ПОЇЗД, ЕЛЕКТРИЧКА) rides under
  the overhead wire to a station with a canopy, platform clock, clock tower and buffer stop (drawTrainScene / trainScene);
  a car (CAR, AUTO, АВТО, АВТІВКА, АВТІВКОЮ, АВТОМОБІЛЬ, МАШИНА) or a taxi (TAXI, ТАКСІ, ТАКСИ) drives along a road to a
  house (drawCarScene / carScene, from 1.7). Scene colours 51-63 (SCENE).
- The phone draws the letters (any web font) into a picture (HFT1: 16 grey levels, run-length coded, plus letter
  boxes for TYPE); the screen only moves it around, so it keeps going without the phone, and after a restart.
- Server: Durable Object `HomeFront` (src/homefront.js, one instance "main"). WebSockets:
  `/thuisfront/dev/ws?id=` for screens, `/api/hf/ws?id=&home=` for phones; `GET /api/hf/list?home=`, `POST /api/hf/forget`.
  The protocol is at the top of src/homefront.js. Video frames (HFV1 4-bit grey, or HFV2 a colour JPEG for NONE)
  go to a screen only after it said it showed the last one, so a slow screen gets fewer frames instead of a delay.
  A photo (HFP1) is a 320x172 JPEG made on the phone; the screen puts the filter on itself and keeps it in flash.
  Position: in PHOTO mode with a picked photo / GIF, drag the phone's preview to move it, pinch (or ZOOM slider,
  mouse wheel) to zoom 1-5x, CENTRE to reset. comm.ppx / ppy (where its middle is, from the screen's middle, in
  screen pixels) and pzm (zoom x100); baked into the JPEG(s) by photoPixels(), so screens and firmware need nothing.
  An animated GIF (from 1.8): the phone decodes it (public/thuisfront/gif.js: LZW, transparency, disposal, interlace),
  keeps at most 48 frames (longer GIFs: every n-th frame, shown as long), makes each one 320x172 like a photo and sends
  HFA1: "HFA1" u16 n, u16 0, n x (u16 delay ms, u32 bytes), then the n JPEGs (at most 1 MB; lower JPEG quality / fewer
  frames until it fits). It takes the photo's place (comm.pb). The board decodes every frame once into its own 320x172
  picture in PSRAM (as many as fit with 700 KB to spare, else every n-th) and plays them by time (also with effects off);
  flash (128 KB) keeps the whole HFA1 only when it fits, else its first frame ("pb*" in photo.pb), and then error.blue
  sends it again after a restart. Ping reports psram (KB free) and anim (frames playing).
- The board's screen chip takes its colours little-endian (RAMCTRL B0h = 00 E8), so lcdColor() does not swap bytes.
- **Updates over WiFi (OTA)**, from firmware 1.4: the board has two app slots (Arduino IDE: Tools > Partition Scheme >
  Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS); arduino-cli: PartitionScheme=min_spiffs) and says `ota:1` in its hello. When src/hf_firmware.js names a
  newer version, the server sends `{t:"ota", ver, url, size, md5}`; the board downloads
  /thuisfront/flash/homefront.bin into the other slot (progress on its screen), checks the MD5 and restarts.
  Also an UPDATE FIRMWARE button in the screen's panel. A failed version is not tried again for an hour.
  New firmware: build, then tools: fwpub.sh copies the bins to public/thuisfront/flash and writes src/hf_firmware.js.
- Changing the drawing: change dev/render.js AND esp32/HomeFront/screen.cpp the same way.
