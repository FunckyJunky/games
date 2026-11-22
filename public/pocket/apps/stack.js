// NEON STACK for pocket.DOS - tap to drop the sliding block. What hangs over is cut off.
var S = 0, T = 0, rows = [], cur = null, dir = 1, sp = 1, cam = 0, best = 0, score = 0;
var off = 0, deb = [], perfect = 0, msg = '', msgT = 0, flash = 0;
var BH = 6, BASE = 74, stars = [];
function ct(s, y, c, k) { k = k || 1; text(s, (W - textw(s, k)) >> 1, y, c, k); }
function hue(i) {
  var h = (i * 0.07) % 1, f = h * 6, k = f | 0, t = f - k, u = (255 * t) | 0, d = 255 - u;
  var r = [255, d, 40, 40, u, 255][k], g = [u, 255, 255, d, 40, 40][k], b = [40, 40, u, 255, 255, d][k];
  return (r << 16) | (g << 8) | b;
}
function dark(c) { return ((c >> 2) & 0x3f3f3f); }

function init() {
  best = load('best', 0);
  for (var i = 0; i < 30; i++) stars.push([irnd(160), irnd(86), irnd(4)]);
  reset();
}
function reset() { rows = [{ x: 50, w: 60 }]; off = 0; score = 0; sp = 1; cam = 0; deb = []; perfect = 0; msgT = 0; spawn(); }
function spawn() {
  var w = rows[rows.length - 1].w;
  dir = (off + rows.length) % 2 ? 1 : -1;
  cur = { x: dir > 0 ? -w : 160, w: w };
}
function drop() {
  var prev = rows[rows.length - 1];
  var l = Math.max(cur.x, prev.x), r = Math.min(cur.x + cur.w, prev.x + prev.w);
  if (r - l <= 0) {
    deb.push({ x: cur.x, y: BASE - (off + rows.length) * BH, w: cur.w, vy: 0, c: hue(off + rows.length) });
    cur = null; S = 2; T = 0; led(8); flash = 14;
    if (score > best) { best = score; save('best', best); }
    return;
  }
  if (Math.abs(cur.x - prev.x) <= 1.5) {
    cur.x = prev.x; perfect++;
    msg = perfect > 1 ? 'PERFECT x' + perfect : 'PERFECT'; msgT = 30; led(7); flash = 4;
    if (perfect >= 3 && cur.w < 60) { cur.x -= 2; cur.w += 4; msg = 'GROW!'; }
  } else {
    var y = BASE - (off + rows.length) * BH, c = hue(off + rows.length);
    if (cur.x < l) deb.push({ x: cur.x, y: y, w: l - cur.x, vy: 0, c: c });
    if (cur.x + cur.w > r) deb.push({ x: r, y: y, w: cur.x + cur.w - r, vy: 0, c: c });
    cur.x = l; cur.w = r - l; perfect = 0; led(c); flash = 3;
  }
  rows.push({ x: cur.x, w: cur.w });
  if (rows.length > 24) { rows.shift(); off++; }   // only the top of the tower is on screen
  score++;
  sp = Math.min(1 + score * 0.07, 3.2);
  spawn();
}

function update() {
  T++;
  if (flash > 0 && --flash == 0) led(0);
  if (msgT > 0) msgT--;
  var i;
  for (i = deb.length - 1; i >= 0; i--) { var d = deb[i]; d.vy += 0.25; d.y += d.vy; if (d.y + cam > 90) deb.splice(i, 1); }
  var target = Math.max(0, (off + rows.length) * BH - 44);
  cam += (target - cam) * 0.12;
  if (S == 0) { if (btnp()) { reset(); S = 1; T = 0; } return; }
  if (S == 2) { if (T > 30 && btnp()) { reset(); S = 0; T = 0; } return; }
  cur.x += dir * sp;
  if (cur.x > 160 - cur.w + 14) { cur.x = 160 - cur.w + 14; dir = -1; }
  if (cur.x < -14) { cur.x = -14; dir = 1; }
  if (btnp()) drop();
}

function block(x, y, w, c) {
  x = x | 0; y = y | 0; w = Math.round(w);
  rect(x, y, w, BH - 1, c); rect(x, y, w, 1, 0xffffff); rect(x, y + BH - 2, w, 1, dark(c));
}
function draw() {
  cls(0x05031a);
  var i, c = cam | 0;
  for (i = 0; i < stars.length; i++) pset(stars[i][0], ((stars[i][1] + c / 3) | 0) % 86, stars[i][2] ? 5 : 6);
  rect(0, BASE + BH + c, 160, 20, 0x1a0a3a); line(0, BASE + BH + c, 159, BASE + BH + c, 14);
  for (i = 0; i < 16; i++) line(i * 10 + 2, BASE + BH + c + 2, i * 10 - 2, BASE + BH + c + 6, 0x5a1a7a);
  for (i = 0; i < rows.length; i++) {
    var y = BASE - (off + i) * BH + c;
    if (y > 86) continue;
    if (y < -BH) break;
    block(rows[i].x, y, rows[i].w, hue(off + i));
  }
  if (cur) block(cur.x, BASE - (off + rows.length) * BH + c, cur.w, hue(off + rows.length));
  for (i = 0; i < deb.length; i++) block(deb[i].x, deb[i].y + c, deb[i].w, dark(deb[i].c) * 2);
  if (S == 1) {
    text('' + score, 4, 4, 7, 2);
    if (msgT > 0) ct(msg, 26, msgT & 4 ? 10 : 7);
  }
  if (S == 0) {
    rect(14, 10, 132, 44, 0); box(14, 10, 132, 44, 15);
    ct('NEON STACK', 15, 15, 2);
    ct('TAP TO DROP THE BLOCK', 33, 6);
    ct('BEST ' + best, 43, 10);
    if ((T >> 4) & 1) ct('TAP TO START', 60, 7);
  }
  if (S == 2) {
    rect(20, 12, 120, 50, 0); box(20, 12, 120, 50, 8);
    ct('MISSED!', 17, 8, 2);
    ct('HEIGHT ' + score, 36, 7);
    ct('BEST ' + best, 46, 10);
    if (T > 30 && (T >> 3) & 1) ct('TAP', 54, 6);
  }
}
