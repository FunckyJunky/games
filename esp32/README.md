# ERROR-OLED

A tiny ERROR-DOS computer for error.blue, on an ESP32-C3 board with a built-in OLED.

- **WHO.EXE**: who is online on error.blue right now, and in which game. This is the screen you see after start-up.
  It pages through everyone by itself. When someone comes or goes, a line pops up at the bottom and the LED blinks.
- **CHAT.EXE**: read and write ERROR-CHAT. It uses the same channels as error.blue/apps/error-chat.
- **C:\\>**: a DOS prompt. It has `HELP`, `DIR`, `WHO`, `CHAT`, `SAY`, `NAME`, `CODE`, `WIFI`, `BT`, `SETUP`, `LOG`, `MEM`,
  `TIME`, `VER`, `CLS` and `REBOOT`. Typing a game's DOS name (e.g. `TUSA`) shows who's in it.
- **BT.EXE**: connects a Bluetooth keyboard and mouse.
- **WIFI.EXE**: picks a WiFi network, or uses PHONE SETUP (a setup page on your phone).
- **Setup web page**: once the box is on your WiFi, open `http://<its IP>/` or `http://error-oled.local/`.
  Type `WEB` on the box to see the address. It's also on 192.168.4.1 during PHONE SETUP. The page sets WiFi, name and chat
  channel. It also has the Bluetooth part: scan settings (scan time, active/passive, show keyboards only, named devices or
  everything, minimum signal, automatic reconnect), the list of everything the box hears with a PAIR button per device,
  pairing by typed address, the pairing code, and FORGET ALL.
- **SETUP.EXE**: your name, chat channel, font size, keyboard layout and more.
- **LOG.TXT**: who came and went.

```
ErrorOled/      the Arduino sketch (open ErrorOled.ino)
sim/            the same screens on a PC, for screenshots (build.sh, sim.cpp)
tools/          gen_keymaps.py makes ErrorOled/keymaps.h (US / DE / FR / Ukrainian keyboards)
```

## Putting it on the board

1. **Arduino IDE.** Install the board package "esp32 by Espressif" 3.x. Then install these libraries from the Library Manager:
   **U8g2** (olikraus), **NimBLE-Arduino** 2.x (h2zero) and **ArduinoJson** 7 (bblanchon).
2. Open `ErrorOled/ErrorOled.ino`.
3. Check `config.h`. The default is the board with the built-in **0.42" OLED (72x40)** on SDA 5 / SCL 6.
   For a 0.96" (128x64) screen, switch the `SCREEN_...` line.
4. Set these under **Tools**:
   - Board: **ESP32C3 Dev Module**
   - USB CDC On Boot: **Enabled**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**. WiFi, Bluetooth and HTTPS together need about 1.5 MB.
5. Upload.

## Start-up

The box starts with a 3D welcome animation:
1. The screen switches on like an old TV.
2. Stars rush past.
3. "ERROR" flies in, spinning, in solid 3D block letters.
4. `error.blue` types itself underneath.
5. A LOADING ERROR-DOS bar fills up.

Then the ERROR-BIOS text boot starts. Any key, click or button press skips the animation.
`INTRO` plays it again, and SETUP.EXE → INTRO switches it off.

## The first start

The box starts with ERROR-BIOS, then types `C:\>WHO`. If it doesn't know a WiFi network yet, it opens WIFI.EXE.
There are three ways to set it up:

- **From your phone.** Pick **PHONE SETUP**, join the WiFi **ERROR-OLED** on your phone and open **192.168.4.1**.
  There you set the WiFi, your name and the chat channel.
- **With a keyboard.** Open BT.EXE, then WIFI.EXE.
- **Over USB.** Use the Serial Monitor at 115200 baud with "Newline" on. Every line is a DOS command:
  `WIFI "My Network" mypassword`, `NAME Alex`, `CODE HOONGY`, `SAY hi!`

Each box names itself `OLED-xx` (from its chip), so two boxes don't look like one. You can change the name with `NAME`.

## Using it

**BOOT button**

| press | does |
|---|---|
| click | next / down (in WHO it starts picking people) |
| double-click | back. On WHO it opens the PROGRAMS menu |
| hold (0.6 s) | OK / open. In CHAT it opens the quick messages (HI!, OK, BRB, GG!, ПРИВІТ! ...) |

Without a keyboard, the box shows a letter wheel when you need to type something like a chat code or a name.
Click moves to the next letter and hold picks it.

**Keyboard**

- Arrows, Enter and Esc work everywhere.
- Esc on WHO opens the menu. The Windows/menu key opens it from anywhere.
- F1 shows help. F2 opens chat. F5 refreshes.
- On WHO: `G` groups people by game, `C` opens chat, `D` opens the prompt. You can also just start typing a command.
- In chat: PgUp/PgDn scroll, Tab opens the quick messages. Chat also understands `/code X`, `/nick X`, `/who` and `/me`.
- **Alt+Shift** (or Win+Space) switches between English and Ukrainian letters. The status bar shows **UA** while Ukrainian is on.
  The Latin layout (US / DE / FR) is set in SETUP.EXE.
- Ctrl+Alt+Del restarts the box.

**Mouse**

- The pointer shows up when you move the mouse.
- Click a line to open it. In DIR and HELP, click a program to run it.
- Right-click goes back. The wheel scrolls. Clicking the top bar opens the menu.

**Bluetooth keyboard + mouse**

- In BT.EXE, pick **SCAN**, put the keyboard in pairing mode and pick it.
  Or type `BT PAIR <part of its name>` in the Serial Monitor; `BT LIST` shows what the box can hear.
- If the screen shows a 6-digit code, type it on the keyboard and press Enter.
- After that it reconnects by itself.
- It can hold two devices at a time, e.g. a keyboard and a mouse, or one combo device.
- The ESP32-C3 only has **Bluetooth Low Energy**. Keyboards sold as "Bluetooth 5.x" or "BLE" work.
  Old "Bluetooth 3.0" ones and 2.4 GHz USB-dongle sets don't.

## How it talks to error.blue

- `GET /api/online`: who's online, grouped by game. Pages report themselves with `/lib/online.js`.
  The box asks every 10 s while WHO is on screen, and every 30 s otherwise.
- `GET /api/chat/<CODE>/log?after=ID&name=X`: new chat messages.
  While CHAT is open, the box asks every 3 s and shows up in the channel's who-list. Otherwise it asks every 30 s, quietly.
- `POST /api/chat/<CODE>/say {name, text}`: sends a message.
- `POST /api/online {sid, path: "/oled", name}`: shows the box itself as online, as ERROR-OLED.
  You can switch this off with "SHOW ME" in SETUP.EXE.

The box checks error.blue's HTTPS certificate against the root certificates in `certs.h`.
If that ever fails ("TLS ..." under `NET`), set **TLS → SKIP** in SETUP.EXE.
`MEM` shows the free memory. HTTPS needs about 40 KB of it.
Bluetooth only starts once a keyboard has been paired (or you open BT.EXE), because it takes a big part of that memory.
Bluetooth switches itself off again 45 s after a scan when nothing gets paired, which gives that memory back.
If HTTPS still doesn't fit (with a keyboard connected), the box sends that request over plain HTTP instead.
`NET` then says "VIA HTTP (LOW MEMORY)". **TLS → HTTP** in SETUP.EXE always uses plain HTTP.

## Screenshots without a board

The `sim/` folder runs the real screen code on a PC against `npm run dev`, then turns the screens into PNG files:

```
sh esp32/sim/build.sh <U8g2>/src <ArduinoJson>/src
printf 'wait 4000\nshot who\nkey f2\nwait 800\nshot chat\n' | esp32/sim/oledsim --out /tmp --chan HOONGY
python3 esp32/sim/topng.py /tmp/*.pbm
```
