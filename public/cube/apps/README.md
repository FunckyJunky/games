# ERROR-CUBE web apps

A web app is one JavaScript file in this folder plus a line in `apps.json`. The cube lists it in its **WEB**
folder, downloads it when you open it (or when the control page at error.blue/cube says OPEN ON CUBE), keeps a
copy on its own drive and runs it itself (Duktape, ES5). Try it in a browser first: `run.html?app=ID`.

```json
{ "id": "hello", "name": "HELLO", "file": "hello.js", "v": "1", "color": "#39ff7a", "info": "start here" }
```

`id` 1-23 letters/digits, `name` up to 10 characters (the menu), `info` up to 15. Change `v` when you change the
file: cubes with an older copy download it again.

## The functions the cube calls

| function | when |
|---|---|
| `init()` | once, at the start |
| `update(dt)` | about 30 times a second (`dt` = ms since the last time; `fps(n)` changes it) |
| `draw()` | after `update`: draw the whole screen, 480 x 480 |
| `touch(type, x, y)` | `'down'`, `'move'`, `'up'`, `'tap'` |
| `swipe(dir)` | `'left'`, `'right'`, `'up'`, `'down'`. A swipe to the right from the left edge always goes back |
| `key(k)` | the buttons: `'prev'` (-), `'next'` (+), `'ok'` (hold +), `'back'` (hold -). Return `true` if you used it; otherwise `'back'` leaves the app |
| `onmsg(data)` | the control page (APPS tab) sent `data` to the app |

## What an app can use

Colours are numbers `0xRRGGBB`. `FG`, `DIM`, `BG` follow the cube's colour scheme; also `RED`, `WARN`, `WHITE`, `BLACK`.

| drawing | |
|---|---|
| `cls(c)` | fill the screen (default `BG`) |
| `rect(x, y, w, h, c)` / `frame(x, y, w, h, c)` | filled / outline |
| `line(x0, y0, x1, y1, c)` / `thick(x0, y0, x1, y1, t, c)` | thin / round brush `t` px |
| `circ(x, y, r, c)` / `ring(x, y, r, c, t)` | filled / anti-aliased ring |
| `pset(x, y, c)`, `fade(x, y, w, h, c, a)` | one pixel; colour over an area with alpha `a` (0..1) |
| `text(s, x, y, c, size, align, glow)` | ERROR-DOS letters (VT323). size 1 = 15x32 cells, 2 = 20x42, 3 = 60x96 (digits `0-9 : - . A P M %` only). align 0 left, 1 centre, 2 right. `glow` false = no glow. Returns the width |
| `textw(s, size)` | width in pixels (characters x cell width) |
| `box(x, y, w, h, c)` | the ERROR-DOS double box |
| `button(label, x, y, w, h, on)` | a DOS button (draw only: test taps in `touch`) |
| `bar(x, y, w, h, pct)` | a progress bar 0..100 |
| `cam3(x, y, z, yaw, pitch, roll, f, cx, cy)`, `xf3(x, y, z, angle, scale)`, `line3(ax, ay, az, bx, by, bz, c)`, `proj3(x, y, z)` | 3D lines, as in pocket.DOS |

| the rest | |
|---|---|
| `tilt()` | gravity in g: `[x right, y down, z into the screen]`. Standing upright: about `[0, 1, 0]` |
| `gyro()` | turning speed, degrees/s |
| `tone(hz, ms, vol)`, `beep(name)` | `beep`: `'key' 'move' 'beep' 'thud' 'crackle' 'disk' 'ok' 'err' 'msg' 'alarm'` |
| `ms()`, `clock()` | milliseconds; `{h, m, s, day, mon, year, wday}` (null if the clock isn't set) |
| `battery()` | percent, -1 = no battery |
| `save(key, value)`, `load(key, default)` | kept on the cube (up to 8 KB per app, JSON) |
| `get(path, function (status, text) {})` | GET something from error.blue, e.g. `get('/api/online', ...)` |
| `send(obj)` | to the control page (shows in its APPS log) |
| `fps(n)`, `exit()`, `print(s)` | frames per second (1-60); back to the menu; a line on the USB serial port |
| `APP`, `VERSION`, `W`, `H` | this app's id, the firmware version, 480, 480 |

Errors stop the app with the message on screen (R = try again). Keep `draw()` light: Duktape is a small
interpreter, a few hundred drawing calls a frame is fine.
