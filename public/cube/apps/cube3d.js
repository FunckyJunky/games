// CUBE3D: a wireframe cube in 3D. Tilt the ERROR-CUBE and it rolls; tap for the next shape, drag to spin.
var ax = 0, ay = 0, vx = 0.4, vy = 0.7, shape = 0, gx = 0, gy = 0, lx = -1;
var SH = ['CUBE', 'PYRAMID', 'DIAMOND', 'GRID'];

function init() { fps(30); }

function update(dt) {
  var g = tilt(), s = dt / 1000;
  gx = gx * 0.85 + g[0] * 0.15; gy = gy * 0.85 + (g[1] - 1) * 0.15;
  vx += gy * 3 * s; vy += gx * 3 * s;              // the cube falls the way you tilt it
  vx *= 0.995; vy *= 0.995;
  ax += vx * s; ay += vy * s;
}

function seg(a, b) { line3(a[0], a[1], a[2], b[0], b[1], b[2], FG); }
function draw() {
  cls();
  cam3(0, 0, -5, 0, 0, 0, 380, 240, 240);
  xf3(0, 0, 0, ay, 1);
  var c = Math.cos(ax), s = Math.sin(ax);
  function p(x, y, z) { return [x, y * c - z * s, y * s + z * c]; }
  if (shape == 0) {
    var v = [];
    for (var i = 0; i < 8; i++) v.push(p(i & 1 ? 1 : -1, i & 2 ? 1 : -1, i & 4 ? 1 : -1));
    var E = [0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 2, 6, 3, 7];
    for (var k = 0; k < E.length; k += 2) seg(v[E[k]], v[E[k + 1]]);
  } else if (shape == 1) {
    var top = p(0, 1.4, 0), b = [p(-1, -1, -1), p(1, -1, -1), p(1, -1, 1), p(-1, -1, 1)];
    for (var i = 0; i < 4; i++) { seg(b[i], b[(i + 1) % 4]); seg(b[i], top); }
  } else if (shape == 2) {
    var u = p(0, 1.6, 0), d = p(0, -1.6, 0), r = [p(1.2, 0, 0), p(0, 0, 1.2), p(-1.2, 0, 0), p(0, 0, -1.2)];
    for (var i = 0; i < 4; i++) { seg(r[i], r[(i + 1) % 4]); seg(r[i], u); seg(r[i], d); }
  } else {
    for (var i = -2; i <= 2; i++) { seg(p(i * 0.6, 0, -1.2), p(i * 0.6, 0, 1.2)); seg(p(-1.2, 0, i * 0.6), p(1.2, 0, i * 0.6)); }
  }
  text(SH[shape], 240, 30, FG, 1, 1);
  text('TILT  /  DRAG  /  TAP', 240, 420, DIM, 1, 1, false);
}

function touch(type, x, y) {
  if (type == 'tap') { shape = (shape + 1) % SH.length; beep('move'); }
  if (type == 'down') lx = x;
  if (type == 'move' && lx >= 0) { vy += (x - lx) * 0.05; lx = x; }
  if (type == 'up') lx = -1;
}
function key(k) { if (k == 'next' || k == 'prev') { shape = (shape + (k == 'next' ? 1 : SH.length - 1)) % SH.length; return true; } return false; }
