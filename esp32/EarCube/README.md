# EAR-CUBE

The third firmware for the ERROR-CUBE (Waveshare ESP32-S3-Touch-AMOLED-2.16): speech to text. The cube's two
microphones (ES7210) listen to the talk around it; error.blue writes it down (Whisper large v3 turbo on Cloudflare
Workers AI, `src/stt.js`). Page: [error.blue/ear](https://error.blue/ear/) (USB installer).

## On the cube

- **LIVE**: tap the microphone; the words appear a few seconds after they're said, with the time. SAVE (a file),
  CLEAR. Needs the internet (no internet: it offers RECORD). SETUP > LIVE: SAVE THE TEXT also writes LIVE-date.txt.
- **REC**: one long session into `A:\EAR\<date>_<time>.txt` (TF card; `C:\ear` without one), `[22:55:07] text` per
  line. Every piece is written to the card first (`A:\EAR\WAIT\n.wav + n.inf`) and sent in order, so it works without
  internet and survives a restart: the text catches up when the internet is back. KEEP THE SOUND: the whole session as
  a WAV next to the text (TF card).
- **TEXTS**: newest first, tap = read (the one being recorded grows), hold = delete.
- **SETUP**: language (AUTO, NL, EN, DE, FR, UK, RU, ES, IT, PL, PT, TR), write it in English (Whisper translates),
  sensitivity, mic gain (ES7210 0..37.5 dB), both / left / right, pause length, MIC TEST (levels, background noise,
  the speech line); times in the text, keep the sound, save live text; the hotspot (on, name, password, QR codes);
  WiFi; volume, light, rotation, screen off, colours; update; BACK TO ERROR-CUBE; memory, battery, TF card.
- Buttons: - / + move (scroll the text on LIVE), hold + = OK (start / stop on LIVE and REC), hold - = next tab,
  PWR = screen. The screen can go off while it listens; the dim clock shows REC and the time.

## Its own WiFi and the page

The hotspot **EAR-CUBE-XXXX** is on all the time (with the home WiFi at the same time). It gets an 8-digit password at
the first start (SETUP > HOTSPOT shows it; PHONE: QR CODES has a QR code to join and one to open the page). A phone on it
opens **http://192.168.4.1** (it opens by itself on most phones); on the home WiFi: `http://<IP>/`.
The page (`src/ear/web_page.h`, one file, nothing from the internet): LIVE (start / stop, the text as it comes, COPY ALL,
SAVE AS A FILE, big letters), TEXTS (read, .txt / .wav download, delete), SETTINGS, WIFI (join the home WiFi, install a .bin).

API (`src/ear/web.cpp`): `GET /api/ear?since=N` (status, levels, lines after N), `POST /api/ear/go m=live|rec|stop`,
`POST /api/ear/save`, `POST /api/ear/clear`, `GET /api/ear/files`, `GET /api/ear/text?p=`, `GET|POST /api/ear/cfg`;
from net.cpp: `/dl?p=`, `/api/rm`, `/info.json`, `/scan`, `/save` (WiFi), `/update` (firmware).

## How it works

```
mic.cpp     ES7210 (es7210.cpp, after Espressif's driver, Apache-2.0) on the speaker's I2S bus: 44.1 kHz stereo
            -> both / left / right -> 64-tap polyphase low-pass resampler (7 kHz) -> 16 kHz, 70 Hz high-pass
            -> a 40 s ring in PSRAM. Every 20 ms: level vs the background noise it learns (falls fast, rises slowly)
            -> speech or not (SENSITIVITY: margin over the noise and a floor). Speech + 0.3 s before .. pause + 0.25 s
            = a piece (LIVE: shorter pauses, max 7 s; RECORD: max 25 s, cut at the quietest 20 ms of the last 1.5 s).
            Pieces with < 0.3 s of speech are dropped. Task on core 1, priority 5.
scribe.cpp  sessions; a piece -> WAV (quiet speech made louder, up to x8) -> the queue; LIVE pieces in memory (dropped
            without internet, at most 3 waiting); RECORD pieces on the card first. The text that comes back -> the
            transcript + the line list (64 lines) for the screen and the page.
enet.cpp    the HTTPS worker: POST /api/stt?task=&lang=&id= with the WAV over one kept-open TLS connection; updates
            (/ear/flash/firmware.json); the switch to ERROR-CUBE. Retries: 2, 5, 15, 30, 60 s; 429 (the day's limit):
            10 min; a 4xx: that piece is skipped.
screens.cpp LIVE / REC / TEXTS / SETUP, READ, MIC TEST, PHONE (QR), WIFI, FIRMWARE, the boot and the dim clock.
web.cpp     the page's API; netWebExtra() and netHotspot() are the two hooks in the shared net.cpp (weak there).
```

Settings are EAR-CUBE's own (`EarCfg`, NVS "ear"); ERROR-CUBE's `Cfg` (WiFi, volume, light...) is shared as it is.

Built from ERROR-CUBE's parts (symlinks, like PICO-CUBE): `src/board`, `src/core`, `src/ui`, `src/lib`,
`src/net/net.cpp, net.h, files_page.h, certs.h`. `core/config.h` includes `src/ear/ear_config.h` when it's there.
Shared changes for EAR-CUBE (no effect on the others): `sndMicRead()` in sound.cpp, the two weak hooks in net.cpp.

## error.blue: /api/stt (src/stt.js)

POST a WAV (16 kHz mono 16 bit from the cube; 0.2 .. 60 s, up to 3 MB) -> `{ok, text, lang, secs}`. Needs the Workers AI
binding (`"ai": {"binding": "AI"}` in wrangler.jsonc). Minutes per day are counted in D1 (`stt_use`, all and per hashed
address); over `STT_DAY_MINUTES` (default 200, the free allowance is about 214 min) -> 429. Whisper's invented subtitle
credits ("Amara.org", "Ondertiteld door...") are dropped. The audio isn't stored.

## Building

    tools/ear-fwpub.sh "what's new"      # bump CUBE_VERSION in src/ear/ear_config.h first
    git add public/ear && git commit && git push   # Cloudflare deploys; EAR-CUBEs see the update

Same board settings as ERROR-CUBE (app3M_fat9M_16MB, OPI PSRAM). Simulator (screens on a PC):
`cd esp32/ErrorCube && sh sim/build-ear.sh`, then e.g. `printf 'wait 2600\ntap 240 250\nwait 8000\nshot live\n' | ./sim/earsim --out /tmp/eshots`
(`open MICTEST`, `open PHONE`, `open WIFI`; SIM_NOMIC=1, SIM_OFFLINE=1, SIM_WAIT=n, SIM_LONG=1).
