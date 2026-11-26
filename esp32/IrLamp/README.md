# IR-LAMP

An ESP32 with an IR receiver and an IR LED. It learns any remote, sends the signals again from its own
web page, and keeps a live link to error.blue so **error.blue/lamp** (LAMP.EXE) and
**error.blue/lamp/game** (Lamplighter) can work the RGB lamp from anywhere.

## Wiring (classic ESP32 DevKit)

| part | pin |
|---|---|
| IR receiver (VS1838B / KY-022) OUT | GPIO 13 (VCC 3V3, GND) |
| IR LED | GPIO 12 → 100 Ω → LED + ; LED − → GND (or through an NPN transistor for 5-10 m) |

Don't swap the two: GPIO 12 has to be low when the ESP32 starts.

## Putting it on the board

Arduino IDE with "esp32 by Espressif" (2.x or 3.x), board **ESP32 Dev Module**, no extra libraries.
Open `IrLamp.ino`, pick the USB port (not a Bluetooth COM port), Upload.
Uploading again keeps the saved buttons, the Wi-Fi and the link code (LittleFS + Preferences).

## Using it

- The board always makes its own Wi-Fi **IR-Remote** (password `12345678`): http://192.168.4.1.
  On the home Wi-Fi it is also http://irremote.local (or its IP).
- **Learn**: press a remote button at the receiver, name it, Save. Tap a saved button to send it.
- **Download .txt** = every name, protocol, code and the raw timings. **Restore .txt** puts them back.
- **Online · error.blue** card: Wi-Fi settings (scan, password, Connect, Forget), the 6-letter **LINK CODE**,
  New code, Link on/off, and a button that opens error.blue/lamp with the code filled in.
- Serial Monitor (115200, Newline): `HELP`, `STATUS`, `CODE`, `NEWCODE`, `LINK ON|OFF`,
  `WIFI "network name" password`, `WIFI OFF`.

## How it talks to error.blue

A WebSocket to `wss://error.blue/api/room/ir-lamp/<LINK CODE>?name=IR-LAMP` (the normal GameRoom; no server
code of its own). It checks error.blue's certificate with the roots in the sketch (same as HomeFront) and goes
on without the check if that ever fails ("TLS: skip" on the page). `{"type":"ping"}` every 25 s.

Pages send to the board (`{to: <board id>, data}`):

| data | does |
|---|---|
| `{t:"ir", c:"00F720DF", r:1}` | send this NEC code (r = how many times, 1-10) |
| `{t:"btn", i:4}` | send saved button #4 (the learned signal, any protocol) |
| `{t:"clr"}` | drop what still waits in the queue |
| `{t:"hi"}` | the board answers with `dev` |

The board sends to everyone in the room:

| data | when |
|---|---|
| `{t:"dev", n, v, ip, rssi, q, gap, b:[[name, protocol, code]], h:[...]}` | on joining, when a page joins, on `hi`. `h` = the last 24 NEC codes sent or received, oldest first: pages rebuild the lamp's state from it |
| `{t:"tx", c, f, q}` | a code went out (`f` = the room id of the page that asked, or `local` for the board's own page) |
| `{t:"rx", c}` | the real remote was pressed (NEC); other protocols: `{t:"rx", p, c}` |
| `{t:"full"}` | the queue (32) is full |

Commands from error.blue go out one at a time, `IR_QUEUE_GAP_MS` (220 ms) apart, so the lamp catches every press.

## The lamp (public/lamp/lamp.js)

`LAMP_KEYS` is the 24-key RGB remote, row by row: ☀+ ☀− OFF ON / R G B W / then the three colour columns
(red → orange → yellow, green → cyan → sky, blue → purple → pink) with FLASH, STROBE, FADE, SMOOTH on the right.
20 codes are learned from the real remote (ir_signals.txt); the 4 effect keys are the standard codes of that remote.
`rgb` is what the lamp looks like, used for the drawings and for picking the nearest colour.
Brightness is counted in 8 steps; Lamplighter turns the lamp all the way down once when it starts, so it knows where it is.

## Without the board

error.blue/lamp has **Try the remote without one** (a demo that only draws the lamp), and Lamplighter plays fine
without a lamp linked.
