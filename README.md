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

