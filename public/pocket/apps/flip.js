// NEON FLIP for pocket.DOS - tap to flip gravity: run on the floor or on the ceiling, miss the spikes.
var S = 0, T = 0, py = 0, vy = 0, g = 1, onS = true, spk = [], trail = [], parts = [];
var dist = 0, score = 0, best = 0, next = 80, sp = 1.8, flash = 0, lastTop = false;
var TOP = 12, BOT = 74, PX = 36, PS = 6;
function ct(s, y, c, k) { k = k || 1; text(s, (W - textw(s, k)) >> 1, y, c, k); }
function wrap(v, n) { v = v % n; return v < 0 ? v + n : v; }

function init() { best = load('best', 0); reset(); }
function reset() { py = BOT - PS; vy = 0; g = 1; onS = true; spk = []; trail = []; parts = []; dist = 0; score = 0; next = 90; sp = 1.8; lastTop = true; }
function die() {
  S = 2; T = 0; led(8); flash = 15;
  for (var i = 0; i < 26; i++) parts.push({ x: PX + 3, y: py + 3, vx: rnd(4) - 2, vy: rnd(4) - 2, l: 20 + irnd(25) });
  if (score > best) { best = score; save('best', best); }
}

function update() {
  T++;
  if (flash > 0 && --flash == 0) led(0);
  var i;
  for (i = parts.length - 1; i >= 0; i--) { var q = parts[i]; q.x += q.vx; q.y += q.vy; q.vx *= 0.95; q.vy *= 0.95; if (--q.l <= 0) parts.splice(i, 1); }
  if (S == 0) { dist += 1; py = BOT - PS - Math.abs(Math.sin(T / 9)) * 4; if (btnp()) { reset(); S = 1; T = 0; } return; }
  if (S == 2) { if (T > 25 && btnp()) { S = 0; T = 0; reset(); } return; }

  sp = Math.min(1.8 + dist / 2500, 3.6);
  dist += sp; score = (dist / 10) | 0;
  if (btnp() && onS) { g = -g; onS = false; led(g > 0 ? 14 : 12); flash = 3; }
  vy += 0.5 * g; if (vy > 5) vy = 5; if (vy < -5) vy = -5;
  py += vy;
  onS = false;
  if (py + PS >= BOT) { py = BOT - PS; vy = 0; onS = g > 0; }
  if (py <= TOP) { py = TOP; vy = 0; onS = g < 0; }
  trail.push(py); if (trail.length > 8) trail.shift();

  next -= sp;
  if (next <= 0) {
    var top = rnd() < 0.5;
    var w = 6 * (1 + irnd(3));
    spk.push({ x: 164, w: w, top: top });
    next = w + (top == lastTop ? 14 + irnd(40) : 30 + sp * 16 + irnd(40));
    lastTop = top;
  }
  for (i = spk.length - 1; i >= 0; i--) {
    var s = spk[i]; s.x -= sp;
    if (s.x + s.w < 0) { spk.splice(i, 1); continue; }
    if (s.x + 1 < PX + PS && s.x + s.w - 1 > PX) {
      if (s.top ? py < TOP + 5 : py + PS > BOT - 5) { die(); return; }
    }
  }
}

function spikes(x, w, base, d) {
  for (var k = 0; k < w; k += 6) { line(x + k, base, x + k + 3, base + d * 6, 8); line(x + k + 3, base + d * 6, x + k + 6, base, 8); line(x + k + 3, base, x + k + 3, base + d * 3, 0x801030); }
}
function draw() {
  cls(0x060214);
  var i, x;
  for (i = 0; i < 12; i++) { x = (wrap(i * 16 - dist * 0.4, 192) | 0) - 16; rect(x, TOP + 6, 2, BOT - TOP - 12, 0x0c0628); }
  rect(0, 0, 160, TOP, 0x0a0a30); line(0, TOP, 159, TOP, 12);
  rect(0, BOT, 160, 86 - BOT, 0x200a30); line(0, BOT, 159, BOT, 14);
  for (i = 0; i < 18; i++) {
    x = (wrap(i * 10 - dist, 180) | 0) - 10;
    line(x, TOP - 2, x + 4, TOP - 6, 0x16306a); line(x, BOT + 2, x + 4, BOT + 6, 0x5a1a6a);
  }
  for (i = 0; i < spk.length; i++) { var s = spk[i]; if (s.top) spikes(s.x | 0, s.w, TOP + 1, 1); else spikes(s.x | 0, s.w, BOT - 1, -1); }
  if (S != 2) {
    for (i = 0; i < trail.length; i++) pset(PX - 2 - (trail.length - i) * 2, (trail[i] | 0) + 3, i > 4 ? 10 : 9);
    var p = py | 0;
    rect(PX, p, PS, PS, 10); box(PX, p, PS, PS, 7);
    var ey = g > 0 ? p + 2 : p + 3;
    pset(PX + 3, ey, 0); pset(PX + 5, ey, 0);
  }
  for (i = 0; i < parts.length; i++) pset(parts[i].x | 0, parts[i].y | 0, i & 1 ? 10 : 9);
  if (S == 1) text('' + score, 4, 2, 7);
  if (S == 0) {
    ct('NEON FLIP', 22, 10, 2);
    ct('TAP = FLIP GRAVITY', 40, 6);
    ct('BEST ' + best, 50, 12);
    if ((T >> 4) & 1) ct('TAP TO START', 2, 7);
  }
  if (S == 2) {
    rect(20, 18, 120, 50, 0); box(20, 18, 120, 50, 8);
    ct('GAME OVER', 23, 8, 2);
    ct('DISTANCE ' + score, 42, 7);
    ct('BEST ' + best, 52, 10);
    if (T > 25 && (T >> 3) & 1) ct('TAP', 60, 6);
  }
}
