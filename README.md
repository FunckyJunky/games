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
  vgadai/               Вгадай що?! (quiz show)
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
   `logo` picks the 3D logo: `hop`, `party`, `wall`, `ship`, `chat`, or leave it
   out for the name over a synthwave sun.

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
