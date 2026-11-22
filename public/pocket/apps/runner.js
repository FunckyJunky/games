// NEON RUN for pocket.DOS - tap to jump, hold for a higher jump. Don't jump into the drones.
var S = 0, T = 0, py = 70, vy = 0, ground = true, obs = [], parts = [];
var dist = 0, score = 0, best = 0, sp = 2, next = 60, flash = 0, GY = 70;
function ct(s, y, c, k) { k = k || 1; text(s, (W - textw(s, k)) >> 1, y, c, k); }
function wrap(v, n) { v = v % n; return v < 0 ? v + n : v; }

function init() { best = load('best', 0); }
function reset() { py = GY; vy = 0; ground = true; obs = []; parts = []; dist = 0; score = 0; sp = 2; next = 60; }
function boom(x, y, c, n) {
  for (var i = 0; i < n; i++) parts.push({ x: x, y: y, vx: rnd(4) - 2, vy: -rnd(3), c: c, l: 20 + irnd(20) });
}
function die() {
  S = 2; T = 0; led(8); flash = 15; boom(29, py - 5, 12, 24);
  if (score > best) { best = score; save('best', best); }
}

function update() {
  T++;
  if (flash > 0 && --flash == 0) led(0);
  for (var i = parts.length - 1; i >= 0; i--) {
    var q = parts[i]; q.x += q.vx; q.y += q.vy; q.vy += 0.15; if (--q.l <= 0 || q.y > 86) parts.splice(i, 1);
  }
  if (S == 0) { dist += 1; if (btnp()) { reset(); S = 1; T = 0; } return; }
  if (S == 2) { if (T > 25 && btnp()) { S = 0; T = 0; obs = []; } return; }

  sp = Math.min(2 + dist / 3000, 4);
  dist += sp; score = (dist / 10) | 0;
  if (btnp() && ground) { vy = -3.4; ground = false; led(13); flash = 3; }
  vy += (btn() && vy < 0) ? 0.17 : 0.38;
  py += vy;
  if (py >= GY) { if (!ground) boom(29, GY, 14, 4); py = GY; vy = 0; ground = true; }
  else if (T % 2 == 0 && ground) parts.push({ x: 25, y: GY - 1, vx: -1, vy: 0, c: 14, l: 6 });

  next -= sp;
  if (next <= 0) {
    if (score > 30 && rnd() < 0.3) obs.push({ x: 165, y: GY - 22, w: 10, h: 5, d: 1 });
    else { var h = 6 + irnd(10); obs.push({ x: 165, y: GY - h, w: 6 + irnd(10), h: h, d: 0 }); }
    next = 55 + irnd(70) + sp * 8;
  }
  for (i = obs.length - 1; i >= 0; i--) {
    var o = obs[i]; o.x -= sp;
    if (o.x + o.w < 0) { obs.splice(i, 1); continue; }
    if (o.x < 32 && o.x + o.w > 26 && o.y < py && o.y + o.h > py - 10) { die(); return; }
  }
}

function draw() {
  cls(0x07031a);
  var i, x;
  // sun with stripes, city, perspective floor
  circ(126, 40, 14, 0xff3a8a); circ(126, 40, 11, 9); circ(126, 38, 7, 10);
  rect(110, 42, 33, 1, 0x07031a); rect(110, 45, 33, 2, 0x07031a); rect(110, 49, 33, 2, 0x07031a);
  for (i = 0; i < 20; i++) {
    x = (wrap(i * 11 - dist * 0.2, 220) | 0) - 20;
    var h = 4 + ((i * 7) % 9);
    rect(x, 50 - h, 9, h, 0x1a0a3a);
  }
  rect(0, 50, 160, 36, 0x12042a);
  line(0, 50, 159, 50, 14);
  for (i = 0; i < 6; i++) { var z = wrap(i / 6 + dist / 300, 1); var yy = 50 + (36 * z * z) | 0; line(0, yy, 159, yy, 0x4a0a6a); }
  for (i = 0; i < 16; i++) {
    var b = wrap(i * 24 - dist, 384) - 112;
    line(80 + ((b - 80) / 5 | 0), 50, b | 0, 85, 0x4a0a6a);
  }
  line(0, GY, 159, GY, 12);
  for (i = 0; i < obs.length; i++) {
    var o = obs[i], ox = o.x | 0;
    if (o.d) {
      rect(ox, o.y, o.w, o.h, 14); rect(ox + 2, o.y + 1, o.w - 4, 3, 2);
      line(ox - 2, o.y - 1 - (T & 1), ox + 3, o.y - 1, 7); line(ox + o.w - 3, o.y - 1, ox + o.w + 2, o.y - 1 - (T & 1), 7);
      if ((T >> 2) & 1) pset(ox + (o.w >> 1), o.y + o.h, 8);
    } else { rect(ox, o.y, o.w, o.h, 0x3a0a1a); box(ox, o.y, o.w, o.h, 8); }
  }
  for (i = 0; i < parts.length; i++) pset(parts[i].x | 0, parts[i].y | 0, parts[i].c);
  if (S != 2) {
    var p = py | 0, k = (T >> 2) & 1;
    rect(26, p - 10, 6, 6, 12); pset(30, p - 8, 0);
    rect(27, p - 4, 4, 2, 13);
    if (!ground) { line(27, p - 2, 26, p, 12); line(30, p - 2, 32, p - 1, 12); }
    else if (k) { line(27, p - 2, 26, p, 12); line(30, p - 2, 31, p, 12); }
    else { line(28, p - 2, 28, p, 12); line(29, p - 2, 30, p, 12); }
  }
  if (S == 1) { text('' + score, 4, 4, 7, 2); }
  if (S == 0) {
    rect(4, 4, 152, 40, 0); box(4, 4, 152, 40, 14);
    ct('NEON RUN', 9, 14, 2);
    ct('TAP = JUMP  HOLD = HIGHER', 25, 6);
    ct('BEST ' + best, 34, 10);
    if ((T >> 4) & 1) ct('TAP TO START', 76, 7);
  }
  if (S == 2) {
    rect(20, 14, 120, 54, 0); box(20, 14, 120, 54, 8);
    ct('GAME OVER', 19, 8, 2);
    ct('DISTANCE ' + score, 39, 7);
    ct('BEST ' + best, 49, 10);
    if (T > 25 && (T >> 3) & 1) ct('TAP TO RETRY', 59, 6);
  }
}
