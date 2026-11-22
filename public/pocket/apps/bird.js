// NEON BIRD for pocket.DOS - tap to flap through the gates.
var S = 0, T = 0, y = 40, vy = 0, pipes = [], score = 0, best = 0, scroll = 0, flash = 0;
var city = [];
function ct(s, y, c, k) { k = k || 1; text(s, (W - textw(s, k)) >> 1, y, c, k); }
function wrap(v, n) { v = v % n; return v < 0 ? v + n : v; }
function light(c, n) { led(c); flash = n; }

function init() {
  best = load('best', 0);
  for (var i = 0; i < 24; i++) city.push(6 + irnd(28));
}
function reset() {
  y = 38; vy = -2.2; score = 0; pipes = [];
  for (var i = 0; i < 3; i++) pipes.push({ x: 170 + i * 64, g: 22 + irnd(40), p: false });
}
function die() {
  S = 2; T = 0; vy = -1.5; light(8, 14);
  if (score > best) { best = score; save('best', best); }
}

function update() {
  T++;
  if (flash > 0 && --flash == 0) led(0);
  if (S != 2) scroll += 0.6;
  if (S == 0) {
    y = 40 + Math.sin(T / 7) * 4;
    if (btnp()) { reset(); S = 1; T = 0; }
    return;
  }
  if (S == 2) {
    if (y < 77) { vy += 0.3; y += vy; if (y > 77) y = 77; }
    if (T > 25 && btnp()) { S = 0; T = 0; pipes = []; }
    return;
  }
  if (btnp()) vy = -2.4;
  vy += 0.2; if (vy > 3) vy = 3;
  y += vy;
  if (y < 3) { y = 3; vy = 0; }
  var sp = Math.min(1.1 + score * 0.03, 2);
  for (var i = 0; i < pipes.length; i++) {
    var p = pipes[i];
    p.x -= sp;
    if (p.x < -14) { p.x += 192; p.g = 20 + irnd(44); p.p = false; }
    if (!p.p && p.x + 12 < 37) { p.p = true; score++; light(11, 4); }
    if (p.x < 43 && p.x + 12 > 37 && (y - 3 < p.g - 14 || y + 3 > p.g + 14)) { die(); return; }
  }
  if (y > 77) die();
}

function draw() {
  cls(1);
  var i, x;
  for (i = 0; i < 26; i++) pset(wrap(i * 37 - scroll * 0.15, 160) | 0, (i * 23) % 60, i % 3 ? 5 : 6);
  for (i = 0; i < 24; i++) {
    x = (wrap(i * 8 - scroll * 0.5, 192) | 0) - 16;
    rect(x, 80 - city[i], 7, city[i], 2);
    if (i % 3 == 0) pset(x + 2, 82 - city[i] + 4, 0x7a3aa0);
  }
  for (i = 0; i < pipes.length; i++) {
    var p = pipes[i], px = p.x | 0;
    rect(px, 0, 12, p.g - 14, 0x062a36); box(px, -1, 12, p.g - 13, 12); rect(px - 1, p.g - 17, 14, 3, 12);
    rect(px, p.g + 14, 12, 80 - p.g - 14, 0x062a36); box(px, p.g + 14, 12, 67 - p.g, 12); rect(px - 1, p.g + 14, 14, 3, 12);
  }
  rect(0, 80, 160, 6, 0x1a0a3a); line(0, 80, 159, 80, 14);
  for (i = 0; i < 18; i++) { x = (wrap(i * 10 - scroll * 2, 180) | 0) - 10; line(x, 82, x - 3, 85, 0x5a1a7a); }
  var by = y | 0;
  circ(40, by, 3, S == 2 ? 9 : 10); pset(41, by - 1, 0); pset(44, by, 9); pset(44, by + 1, 9);
  line(37, by, 35, by + (vy < 0 ? -2 : 1), S == 2 ? 8 : 9);

  if (S == 1) ct('' + score, 4, 7, 2);
  if (S == 0) {
    ct('NEON BIRD', 14, 12, 2);
    ct('TAP TO FLAP', 34, 6);
    if ((T >> 4) & 1) ct('TAP TO START', 52, 7);
    ct('BEST ' + best, 66, 10);
  }
  if (S == 2) {
    rect(20, 16, 120, 54, 0); box(20, 16, 120, 54, 8);
    ct('GAME OVER', 21, 8, 2);
    ct('SCORE ' + score, 41, 7);
    ct('BEST ' + best, 51, 10);
    if (T > 25 && (T >> 3) & 1) ct('TAP TO RETRY', 61, 6);
  }
}
