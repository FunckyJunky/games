# Game Lab

A test site for HTML games, hosted on Cloudflare.

Every push to `main` is deployed automatically by Cloudflare.

## How it's organised

```
public/                 everything in here is the website
  index.html            the game menu
  games.json            list of games shown on the menu
  404.html              "page not found" screen
  games/
    pixel-hop/          one folder per game
      index.html
wrangler.jsonc          Cloudflare settings
```

## Adding a game

1. Put the game in its own folder: `public/games/<game-name>/index.html`
2. Add an entry to `public/games.json`:

```json
{
  "title": "My Game",
  "description": "One line about the game.",
  "path": "games/my-game/",
  "color": "#4dd4ff",
  "tag": "new"
}
```

3. Push to `main`. The site updates within about a minute.
