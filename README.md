# Game Lab

A test site for HTML games at **https://error.blue**, hosted on Cloudflare.
Every push to `main` is deployed automatically.

## How it's organised

```
public/                 the website
  index.html            game menu
  games.json            list of games shown on the menu
  lib/gamelab.js        helper for scores, saves, multiplayer and music
  games/
    pixel-hop/          one folder per game (index.html + music, images...)
    pixel-party/
  apps/
    error-chat/         ERROR-CHAT: text chat by channel code
  tusa/                 Туса У Берліні
  music3d/              STUDIO 3D (the FunKey game, as WebAssembly), at error.blue/music3d
  vgadai/               Вгадай що?! (quiz show)
  games/mistwalkers/    Mistwalkers (3D walk for three, built from the World Tester project)
  glasses/              Mini Hub for Meta Ray-Ban Display glasses (600x600, D-pad keys), at error.blue/glasses
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
   `studio`, `mist`, or leave it out for the name over a synthwave sun.

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
