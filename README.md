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
plays the tracks in `public/games/leviathan-voyage/SoundRooms/` on its radios:

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

## Access code (Leviathan)

`games.json` marks a program with `"lock": true`. ERROR-DOS then asks for a
5-digit code on its own number pad before it runs it. The very first time, the
pad asks to choose the code (type it twice). The server keeps only a salted
hash of it in D1 (table `settings`), never in this repository. A right code
sets a cookie for 30 days; without it the game's files are closed (pages send
you back to the menu, other files get 403). 8 wrong tries per 10 minutes, then
the pad waits. To change the code: tap [C] CHANGE CODE on the pad (or open
error.blue/?code=leviathan-voyage, which also works on a phone that is already
let in), type the old code, then the new one twice; old cookies stop working.
If the old code is lost: delete the row `lock:leviathan-voyage` in the D1 table
`settings` (Cloudflare dashboard) and choose a new one, or set a Worker secret
`LEVIATHAN_CODE`, which always wins.

## App mode

Every page behaves like an installed app: no double-tap or pinch zoom
(`/lib/nozoom.js`, `touch-action: manipulation`); game pages get this added by
the Worker as they are served.
