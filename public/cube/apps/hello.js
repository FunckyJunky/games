// HELLO: the smallest ERROR-CUBE web app, and a tour of the API.
// An app is a few functions; the cube calls them:
//   init()            once, at the start
//   update(dt)        about 30 times a second (dt = ms since last time)
//   draw()            after update: draw the whole screen (480 x 480)
//   touch(type, x, y) type = 'down' | 'move' | 'up' | 'tap'
//   swipe(dir)        'left' | 'right' | 'up' | 'down' (a swipe from the left edge always goes back)
//   key(k)            the buttons: 'prev' (-), 'next' (+), 'ok' (hold +), 'back' (hold -). Return true if you used it.
//   onmsg(data)       something the control page (error.blue/cube) sent to this app
var dots = [], taps = 0, t = 0;

function init() {
  taps = load('taps', 0);           // saved on the cube between runs
}

function update(dt) {
  t += dt;
  for (var i = dots.length - 1; i >= 0; i--) { dots[i].r += dt * 0.25; if (dots[i].r > 200) dots.splice(i, 1); }
}

function draw() {
  cls();                                            // the colour scheme's background
  box(20, 20, 440, 440, FG);                        // the ERROR-DOS double box
  text('HELLO, CUBE', 240, 50, FG, 2, 1);           // text(s, x, y, colour, size 1|2|3, align 0 left|1 centre|2 right)
  var c = clock();
  if (c) { text(pad(c.h) + ':' + pad(c.m), 240, 120, FG, 3, 1); text(':' + pad(c.s), 240, 226, DIM, 2, 1); }
  for (var i = 0; i < dots.length; i++) ring(dots[i].x, dots[i].y, dots[i].r, DIM, 3);
  text('TAPS: ' + taps, 240, 330, DIM, 1, 1);
  var g = tilt();                                   // gravity in g: [x right, y down, z out of the screen]
  bar(90, 380, 300, 30, Math.round(50 + g[0] * 50)); // a DOS bar, 0..100
  text('TILT', 240, 412, DIM, 1, 1, false);
}

function touch(type, x, y) {
  if (type == 'tap') { dots.push({ x: x, y: y, r: 4 }); taps++; save('taps', taps); tone(880 + (taps % 8) * 110, 60); }
}

function key(k) {
  if (k == 'next') { taps = 0; save('taps', 0); beep('ok'); return true; }
  return false;                                     // 'back' still leaves the app
}

function pad(n) { return (n < 10 ? '0' : '') + n; }
