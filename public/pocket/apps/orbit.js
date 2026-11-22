// NEON ORBIT for pocket.DOS - you circle the star. Tap to jump between the inner and outer ring.
// Catch the yellow gems, dodge the red mines and the comets.
var S = 0, T = 0, a = 0, lane = 1, rr = 32, items = [], score = 0, best = 0, sp = 0.045, next = 30, flash = 0;
var trail = [], stars = [], parts = [];
var CX = 80, CY = 43, R = [14, 32];
function ct(s, y, c, k) { k = k || 1; text(s, (W - textw(s, k)) >> 1, y, c, k); }

function init() {
  best = load('best', 0);
  for (var i = 0; i < 40; i++) stars.push([irnd(160), irnd(86), irnd(3)]);
}
function reset() { a = -1.57; lane = 1; rr = R[1]; items = []; score = 0; sp = 0.045; next = 30; trail = []; parts = []; }
function burst(x, y, c, n) { for (var i = 0; i < n; i++) parts.push({ x: x, y: y, vx: rnd(3) - 1.5, vy: rnd(3) - 1.5, c: c, l: 12 + irnd(18) }); }
function die(x, y) {
  S = 2; T = 0; led(8); flash = 15; burst(x, y, 12, 26);
  if (score > best) { best = score; save('best', best); }
}
function spawn() {
  var kind = score >= 6 && rnd() < 0.25 ? 2 : (rnd() < 0.5 ? 1 : 0);
  var ang = a + 1.6 + rnd(3.2);
  for (var j = 0; j < items.length; j++) { var d = items[j].a - ang; d = Math.atan2(Math.sin(d), Math.cos(d)); if (Math.abs(d) < 0.5 && items[j].k != 2) return; }
  if (kind == 2) items.push({ k: 2, a: ang, d: 0, l: 999 });
  else items.push({ k: kind, a: ang, r: irnd(2), l: 260 + irnd(120) });
}

function update() {
  T++;
  if (flash > 0 && --flash == 0) led(0);
  var i;
  for (i = parts.length - 1; i >= 0; i--) { var q = parts[i]; q.x += q.vx; q.y += q.vy; if (--q.l <= 0) parts.splice(i, 1); }
  if (S == 0) { a += 0.03; rr = R[1]; if (btnp()) { reset(); S = 1; T = 0; } return; }
  if (S == 2) { if (T > 25 && btnp()) { S = 0; T = 0; items = []; } return; }

  if (btnp()) { lane = 1 - lane; led(lane ? 12 : 15); flash = 3; }
  rr += (R[lane] - rr) * 0.35;
  a += sp;
  var px = CX + Math.cos(a) * rr, py = CY + Math.sin(a) * rr;
  trail.push([px, py]); if (trail.length > 10) trail.shift();
  if (--next <= 0) { spawn(); next = Math.max(18, 50 + irnd(40) - score * 2); }
  for (i = items.length - 1; i >= 0; i--) {
    var it = items[i], ix, iy;
    if (it.k == 2) { it.d += 0.55 + score * 0.01; ix = CX + Math.cos(it.a) * it.d; iy = CY + Math.sin(it.a) * it.d; if (it.d > 100) { items.splice(i, 1); continue; } }
    else { if (--it.l <= 0) { items.splice(i, 1); continue; } ix = CX + Math.cos(it.a) * R[it.r]; iy = CY + Math.sin(it.a) * R[it.r]; }
    var dx = ix - px, dy = iy - py;
    if (dx * dx + dy * dy < 22) {
      if (it.k == 1) { score++; sp = Math.min(sp + 0.0025, 0.09); led(10); flash = 5; burst(ix, iy, 10, 10); items.splice(i, 1); }
      else { die(px, py); return; }
    }
  }
}

function draw() {
  cls(0x02010c);
  var i;
  for (i = 0; i < stars.length; i++) pset(stars[i][0], stars[i][1], stars[i][2] ? 5 : 6);
  ring(CX, CY, R[0], 0x2a1a5a); ring(CX, CY, R[1], 0x2a1a5a);
  var pulse = (Math.sin(T / 6) * 1.5) | 0;
  circ(CX, CY, 7 + pulse, 0x5a1a8a); circ(CX, CY, 5, 14); circ(CX, CY, 3, 0xffaaf0);
  for (i = 0; i < items.length; i++) {
    var it = items[i], ix, iy;
    if (it.k == 2) {
      ix = (CX + Math.cos(it.a) * it.d) | 0; iy = (CY + Math.sin(it.a) * it.d) | 0;
      line(ix, iy, (CX + Math.cos(it.a) * (it.d - 8)) | 0, (CY + Math.sin(it.a) * (it.d - 8)) | 0, 9);
      circ(ix, iy, 2, 10);
      continue;
    }
    ix = (CX + Math.cos(it.a) * R[it.r]) | 0; iy = (CY + Math.sin(it.a) * R[it.r]) | 0;
    if (it.l < 60 && (T >> 2) & 1) continue;
    if (it.k == 1) { line(ix, iy - 3, ix + 3, iy, 10); line(ix + 3, iy, ix, iy + 3, 10); line(ix, iy + 3, ix - 3, iy, 10); line(ix - 3, iy, ix, iy - 3, 10); pset(ix, iy, 7); }
    else { circ(ix, iy, 2, 8); pset(ix, iy - 4, 8); pset(ix, iy + 4, 8); pset(ix - 4, iy, 8); pset(ix + 4, iy, 8); }
  }
  for (i = 0; i < parts.length; i++) pset(parts[i].x | 0, parts[i].y | 0, parts[i].c);
  if (S != 2) {
    for (i = 0; i < trail.length; i++) pset(trail[i][0] | 0, trail[i][1] | 0, i > 6 ? 12 : 13);
    var px = (CX + Math.cos(a) * rr) | 0, py = (CY + Math.sin(a) * rr) | 0;
    circ(px, py, 2, 12); pset(px, py, 7);
  }
  if (S == 1) text('' + score, 4, 4, 10, 2);
  if (S == 0) {
    text('NEON', 4, 6, 15, 2); text('ORBIT', 4, 22, 12, 2);
    text('TAP =', 118, 8, 6); text('SWITCH', 118, 17, 6); text('RING', 118, 26, 6);
    text('BEST ' + best, 4, 76, 10);
    if ((T >> 4) & 1) text('TAP', 136, 76, 7);
  }
  if (S == 2) {
    rect(20, 18, 120, 50, 0); box(20, 18, 120, 50, 8);
    ct('GAME OVER', 23, 8, 2);
    ct('GEMS ' + score, 42, 7);
    ct('BEST ' + best, 52, 10);
    if (T > 25 && (T >> 3) & 1) ct('TAP', 60, 6);
  }
}
