// SAVERS: screensavers for the ERROR-CUBE.
// 16 animated pictures in 3 groups (GEOMETRY, ALGORITHMS, NATURE & SPACE). Each has its own settings (TUNE),
// shown with a live preview and a line that says what each setting does. OPTIONS change all of them.
//
// MENU     tap a picture to start it (- / + choose, hold + = start). SLIDESHOW plays them all. OPTIONS.
// PLAYING  tap = the bar (< MENU TUNE >). Swipe left / right = next / previous, up = TUNE, down = MENU.
//          Drag to play with it: LIFE draws cells, FLOW and BOIDS flee your finger, ANTS: new ants.
//          - / + = previous / next, hold + = TUNE, hold - = MENU.
// TUNE     the picture runs at the top, its settings below: tap - / +, or slide along a value.
//
// The cube runs this file itself (Duktape: ES5 only). Try it in a browser: run.html?app=savers
// Firmware 1.5.2+: keep(true) keeps the picture between frames (trails, drawings that build up) and
// overlay() draws the menus on top without keeping them. Older firmware: it runs, without trails.
//
// How it's built:
//   helpers + PALETTES   col(t) goes round a palette, colv(v) runs it from dark (0) to bright (1)
//   SAVERS               each: { name, cat, info, params, init(s), step(s, dt), touch(s, type, x, y) }
//                        they draw inside the view V (the whole screen, or the preview strip in TUNE)
//   PANEL                the settings list (used by TUNE and OPTIONS)
//   MENU / RUN / TUNE / OPTIONS, and the cube's functions init, update, draw, overlay, touch, swipe, key

var TAU = Math.PI * 2;
var KEEP = false;                  // trails possible (firmware 1.5.2+)
var NOW = 0, DT = 33, SPD = 1;     // ms now, ms since the last frame, OPTIONS speed
var V = { x: 0, y: 0, w: 480, h: 480, cx: 240, cy: 240, r: 240 };   // where the picture is drawn
var P = {}, S = {}, CUR = 0;       // the running picture's settings, its state, its number

// ======================================================================== helpers
function rnd(a, b) { return a + Math.random() * (b - a); }
function irnd(n) { return Math.floor(Math.random() * n); }
function clamp(v, a, b) { return v < a ? a : v > b ? b : v; }
function lerpC(a, b, t) {
  var r = (a >> 16) & 255, g = (a >> 8) & 255, bl = a & 255;
  r += (((b >> 16) & 255) - r) * t; g += (((b >> 8) & 255) - g) * t; bl += ((b & 255) - bl) * t;
  return ((r & 255) << 16) | ((g & 255) << 8) | (bl & 255);
}
function setView(x, y, w, h) { V.x = x; V.y = y; V.w = w; V.h = h; V.cx = x + w / 2; V.cy = y + h / 2; V.r = Math.min(w, h) / 2; }
function wipe() { rect(V.x, V.y, V.w, V.h, BLACK); }
// trails: 0 = a clean picture every frame ... 10 = very long trails (fade() dims by 1/32 steps on the cube)
var trailN = 0, TRAIL_A = [0, 16, 9, 5, 3, 2, 1, 1, 1, 1, 1], TRAIL_EVERY = [1, 1, 1, 1, 1, 1, 1, 2, 3, 5, 8];
function trail(level) {
  level = level | 0;
  if (!KEEP || level <= 0) { wipe(); return; }
  if (level > 10) level = 10;
  if (++trailN % TRAIL_EVERY[level]) return;
  fade(V.x, V.y, V.w, V.h, BLACK, TRAIL_A[level] / 32 + 0.001);
}
// a pen for drawings that build up. With keep() each line is drawn once; without it the last 360 are redrawn
var PEN = { lx: null, ly: 0, seg: [] };
function penReset() { PEN.lx = null; PEN.seg = []; }
function penTo(x, y, c) {
  if (PEN.lx !== null) {
    if (KEEP) line(PEN.lx, PEN.ly, x, y, c);
    else { PEN.seg.push(PEN.lx, PEN.ly, x, y, c); if (PEN.seg.length > 1800) PEN.seg.splice(0, 5); }
  }
  PEN.lx = x; PEN.ly = y;
}
function penShow() { if (KEEP) return; var q = PEN.seg; for (var i = 0; i < q.length; i += 5) line(q[i], q[i + 1], q[i + 2], q[i + 3], q[i + 4]); }
function gcd(a, b) { while (b) { var t = a % b; a = b; b = t; } return a; }
function wrapText(s, n) {      // into lines of at most n letters
  var w = String(s).split(' '), out = [], l = '';
  for (var i = 0; i < w.length; i++) {
    if (l.length && l.length + 1 + w[i].length > n) { out.push(l); l = w[i]; }
    else l = l.length ? l + ' ' + w[i] : w[i];
  }
  if (l.length) out.push(l);
  return out;
}

// ======================================================================== PALETTES
// [name, colour stops, round?]. Round palettes go straight round; the others go up and back down
var PALS = [
  ['CUBE', null],
  ['RAINBOW', [0xff3050, 0xff9a1f, 0xffe633, 0x3cff6e, 0x2ad4ff, 0x5a5cff, 0xd447ff], 1],
  ['FIRE', [0x1a0000, 0x8a0d00, 0xe83a00, 0xff8c00, 0xffd23a, 0xfff6c0]],
  ['ICE', [0x00142e, 0x004a8f, 0x1e90ff, 0x7fd8ff, 0xe8fbff]],
  ['NEON', [0xff1fa8, 0x8a2bff, 0x1fd6ff, 0x1fff9c], 1],
  ['SUNSET', [0x2a0845, 0x7b1fa2, 0xe91e63, 0xff7043, 0xffd54f]],
  ['AURORA', [0x00ff99, 0x00c8ff, 0x7a5cff, 0xff4fd8], 1],
  ['OCEAN', [0x001820, 0x00566b, 0x00a3a3, 0x5fffd7, 0xe0fff8]],
  ['TOXIC', [0x0b1a00, 0x3d7a00, 0x9be000, 0xe5ff3a, 0xffffcc]],
  ['FOREST', [0x3a2410, 0x6b4423, 0x2e7d32, 0x66bb3a, 0xc5e86c]],
  ['CANDY', [0xff6fb5, 0xffd36e, 0x7dffcf, 0x8fa8ff], 1],
  ['MONO', [0x262626, 0x6e6e6e, 0xc4c4c4, 0xffffff]]
];
var PALNAMES = [], PT = [], PV = [], CT = null, CV = null;
for (var pi = 0; pi < PALS.length; pi++) PALNAMES.push(PALS[pi][0]);
function rampAt(st, t) { var n = st.length - 1, f = t * n, i = Math.floor(f); if (i >= n) return st[n]; return lerpC(st[i], st[i + 1], f - i); }
function buildPals() {
  for (var p = 0; p < PALS.length; p++) {
    var st = PALS[p][1] || [lerpC(BLACK, FG, 0.16), DIM, FG, lerpC(FG, WHITE, 0.6)], round = PALS[p][2], t = [], v = [], i;
    for (i = 0; i < 64; i++) v.push(rampAt(st, i / 63));
    if (round) { var st2 = st.concat([st[0]]); for (i = 0; i < 64; i++) t.push(rampAt(st2, i / 64)); }
    else { for (i = 0; i < 32; i++) t.push(rampAt(st, i / 31)); for (i = 0; i < 32; i++) t.push(rampAt(st, 1 - i / 31)); }
    PT.push(t); PV.push(v);
  }
}
function usePal(i) { CT = PT[i] || PT[0]; CV = PV[i] || PV[0]; }
function col(t) { return CT[((t * 64) | 0) & 63]; }                             // round and round
function colv(v) { return CV[v <= 0 ? 0 : v >= 1 ? 63 : (v * 63) | 0]; }       // dark -> bright

// ======================================================================== settings: how they're described
function ext(p, o) { if (o) for (var i in o) p[i] = o[i]; return p; }
function NUM(k, name, min, max, def, help, o) { return ext({ k: k, name: name, min: min, max: max, def: def, step: 1, help: help }, o); }
function CHO(k, name, opts, def, help, o) { return ext({ k: k, name: name, opts: opts, min: 0, max: opts.length - 1, def: def, step: 1, help: help }, o); }
function PALP(def) { return CHO('pal', 'COLOURS', PALNAMES, def, 'Which colours (OPTIONS > COLOURS can set one for all pictures)', { live: 1 }); }
function pval(p, v) { return p.opts ? p.opts[v] : String(v) + (p.u || ''); }

// ======================================================================== SAVERS
var CATS = ['GEOMETRY', 'ALGORITHMS', 'NATURE & SPACE'];
var SAVERS = [];
function saver(o) { SAVERS.push(o); }

// ------------------------------------------------------------------ GEOMETRY
saver({
  id: 'harmono', name: 'HARMONOGRAPH', cat: 0, info: 'pendulum pen',
  params: [
    NUM('fx', 'SWING X', 1, 7, 2, 'How often the left-right pendulum swings in one round'),
    NUM('fy', 'SWING Y', 1, 7, 3, 'How often the up-down pendulum swings in one round'),
    NUM('det', 'DETUNE', 0, 20, 6, 'A tiny mistuning: the figure slowly turns while it is drawn'),
    NUM('damp', 'DAMPING', 1, 10, 4, 'How fast the swing dies out. Then the next drawing starts'),
    NUM('speed', 'SPEED', 1, 10, 5, 'How fast the pen moves', { live: 1 }),
    PALP(1)
  ],
  init: function (s) {
    var d = P.det * 0.003;
    s.f = [P.fx + rnd(-d, d), P.fx + rnd(-d, d), P.fy + rnd(-d, d), P.fy + rnd(-d, d)];
    s.p = [rnd(0, TAU), rnd(0, TAU), rnd(0, TAU), rnd(0, TAU)];
    s.t = 0; s.hue = Math.random(); s.end = 0;
    penReset(); wipe();
  },
  step: function (s) {
    if (s.end) { trail(2); penShow(); if (NOW > s.end) this.init(s); return; }   // fading away, then a new one
    var n = Math.round((3 + P.speed * 3) * SPD), R = V.r * 0.45, damp = P.damp * 0.0011;
    for (var i = 0; i < n; i++) {
      var t = (s.t += 0.03), e = Math.exp(-damp * t);
      var x = V.cx + R * e * (Math.sin(s.f[0] * t + s.p[0]) + Math.sin(s.f[1] * t + s.p[1]));
      var y = V.cy + R * e * (Math.sin(s.f[2] * t + s.p[2]) + Math.sin(s.f[3] * t + s.p[3]));
      penTo(x, y, col(s.hue + t * 0.003));
      if (e < 0.05) { s.end = NOW + 1600; break; }
    }
    penShow();
  }
});

saver({
  id: 'spiro', name: 'SPIROGRAPH', cat: 0, info: 'wheel in wheel',
  params: [
    NUM('ring', 'RING', 40, 150, 105, 'Teeth of the big ring that stays still', { step: 5 }),
    NUM('wheel', 'WHEEL', 10, 120, 36, 'Teeth of the small wheel that rolls inside the ring'),
    NUM('pen', 'PEN HOLE', 10, 100, 80, 'How far out on the wheel the pen sits', { u: '%', step: 5 }),
    NUM('speed', 'SPEED', 1, 10, 5, 'How fast the wheel rolls', { live: 1 }),
    CHO('vary', 'NEXT ONE', ['SAME', 'NEW WHEELS'], 1, 'When a drawing is done: draw it again, or try new wheels'),
    PALP(4)
  ],
  init: function (s) { s.R = P.ring; s.r = Math.min(P.wheel, P.ring - 5); s.d = P.pen / 100; this.begin(s); },
  begin: function (s) {
    var R = s.R, r = s.r;
    s.k = (R - r) / r; s.d2 = s.d * r;
    s.sc = V.r * 0.94 / ((R - r) + s.d2);
    s.total = TAU * r / gcd(R, r); s.th = 0; s.hue = Math.random(); s.end = 0; s.hold = 0;
    penReset(); wipe();
  },
  step: function (s) {
    if (s.end) {
      trail(2); penShow();
      if (NOW > s.end) {
        if (P.vary) { s.R = 60 + irnd(91); s.r = 10 + irnd(s.R - 15); s.d = rnd(0.35, 1); }
        this.begin(s);
      }
      return;
    }
    if (s.hold) { penShow(); if (NOW > s.hold) s.end = NOW + 1400; return; }
    var n = Math.round((4 + P.speed * 5) * SPD), R = s.R, r = s.r;
    for (var i = 0; i < n; i++) {
      var t = (s.th += 0.025);
      var x = V.cx + s.sc * ((R - r) * Math.cos(t) + s.d2 * Math.cos(s.k * t));
      var y = V.cy + s.sc * ((R - r) * Math.sin(t) - s.d2 * Math.sin(s.k * t));
      penTo(x, y, col(s.hue + t / s.total));
      if (t >= s.total) { s.hold = NOW + 2500; break; }
    }
    penShow();
  }
});

saver({
  id: 'rose', name: 'MAURER ROSE', cat: 0, info: 'rose and star',
  params: [
    NUM('n', 'PETALS', 1, 12, 6, 'The rose r = sin(n a): an odd n gives n petals, an even n gives 2n'),
    NUM('d', 'STEP', 1, 179, 71, 'Degrees from one corner of the star lines to the next', { u: ' DEG' }),
    NUM('morph', 'MORPH', 0, 10, 3, 'How fast the step changes: the lines flow into new stars', { live: 1 }),
    CHO('pts', 'LINES', ['120', '180', '360'], 1, 'How many star lines. 360 = the classic rose (slower)'),
    CHO('curve', 'ROSE LINE', ['OFF', 'ON'], 1, 'Draw the rose curve itself on top', { live: 1 }),
    PALP(6)
  ],
  init: function (s) { s.d = P.d; s.hue = Math.random(); s.N = [120, 180, 360][P.pts]; },
  step: function (s) {
    wipe();
    s.d += P.morph * 0.004 * SPD; s.hue += 0.0015 * SPD;
    var N = s.N, R = V.r * 0.94, n = P.n, d = s.d * Math.PI / 180 * (360 / N), px = 0, py = 0, k, a, r, x, y;
    for (k = 0; k <= N; k++) {
      a = k * d; r = R * Math.sin(n * a);
      x = V.cx + r * Math.cos(a); y = V.cy + r * Math.sin(a);
      if (k) line(px, py, x, y, col(s.hue + k / N));
      px = x; py = y;
    }
    if (P.curve) {
      var c = lerpC(col(s.hue + 0.5), WHITE, 0.5);
      for (k = 0; k <= 96; k++) {
        a = k * TAU / 96; r = R * Math.sin(n * a);
        x = V.cx + r * Math.cos(a); y = V.cy + r * Math.sin(a);
        if (k) thick(px, py, x, y, 2, c);
        px = x; py = y;
      }
    }
    var lbl = 'n ' + n + '  d ' + (Math.round(s.d * 10) / 10);
    if (V.h > 300) text(lbl, V.x + 8, V.y + V.h - 36, DIM, 1, 0, false);
  }
});

saver({
  id: 'tunnel', name: 'TUNNEL', cat: 0, info: 'fly through it',
  params: [
    NUM('sides', 'SIDES', 3, 10, 6, 'Corners of each ring (10 is nearly round)'),
    NUM('layers', 'RINGS', 6, 30, 16, 'How many rings are in sight'),
    NUM('twist', 'TWIST', 0, 30, 9, 'How far each ring is turned against the one in front', { live: 1, u: ' DEG' }),
    NUM('speed', 'SPEED', 1, 10, 5, 'How fast you fly', { live: 1 }),
    CHO('web', 'WEB', ['OFF', 'ON'], 1, 'Lines from corner to corner between the rings', { live: 1 }),
    PALP(4)
  ],
  init: function (s) { s.z = 0; s.rot = 0; s.wob = Math.random() * 10; s.vx = []; s.vy = []; },
  step: function (s, dt) {
    trail(2);
    var k = dt * SPD;
    s.z += P.speed * 0.00035 * k; s.rot += 0.0003 * k; s.wob += 0.0006 * k;
    var L = P.layers, n = P.sides, tw = P.twist * Math.PI / 180, base = Math.floor(s.z), fr = s.z - base;
    var bx = Math.sin(s.wob) * V.r * 0.25, by = Math.cos(s.wob * 0.8) * V.r * 0.18;   // the tunnel bends
    var pvx = null, pvy = null;
    for (var j = L - 1; j >= 0; j--) {          // far to near
      var u = (j + 1 - fr) / L, id = base + j;
      var rad = V.r * 0.16 / u, cx = V.cx + bx * u, cy = V.cy + by * u;
      if (rad > V.r * 2.2) { pvx = null; continue; }                         // flown past: off the screen
      var a0 = id * tw + s.rot, c = lerpC(BLACK, col(id * 0.05), clamp(1.25 - u, 0.12, 1)), w = 1 + ((1 - u) * 3 | 0);
      var vx = [], vy = [];
      for (var i = 0; i <= n; i++) { var a = a0 + i * TAU / n; vx.push(cx + Math.cos(a) * rad); vy.push(cy + Math.sin(a) * rad); }
      for (i = 0; i < n; i++) {
        thick(vx[i], vy[i], vx[i + 1], vy[i + 1], w, c);
        if (P.web && pvx) line(vx[i], vy[i], pvx[i], pvy[i], c);
      }
      pvx = vx; pvy = vy;
    }
  }
});

var PHI = (1 + Math.sqrt(5)) / 2;
function solid(kind) {
  var v = [], e = [], i, j, iP = 1 / PHI;
  function addAll(list) { for (var q = 0; q < list.length; q++) v.push(list[q]); }
  function signs(a, b, c) { for (var m = 0; m < 8; m++) { var p = [a * (m & 1 ? -1 : 1), b * (m & 2 ? -1 : 1), c * (m & 4 ? -1 : 1)], dup = false; for (var q = 0; q < v.length; q++) if (v[q][0] == p[0] && v[q][1] == p[1] && v[q][2] == p[2]) dup = true; if (!dup) v.push(p); } }
  if (kind == 0) addAll([[1, 1, 1], [1, -1, -1], [-1, 1, -1], [-1, -1, 1]]);
  else if (kind == 1) signs(1, 1, 1);
  else if (kind == 2) addAll([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]]);
  else if (kind == 3) { signs(1, 1, 1); signs(0, iP, PHI); signs(iP, PHI, 0); signs(PHI, 0, iP); }
  else if (kind == 4) { signs(0, 1, PHI); signs(1, PHI, 0); signs(PHI, 0, 1); }
  else if (kind == 5) {      // two tetrahedra through each other (the edges of each one only)
    addAll([[1, 1, 1], [1, -1, -1], [-1, 1, -1], [-1, -1, 1], [-1, -1, -1], [-1, 1, 1], [1, -1, 1], [1, 1, -1]]);
    for (i = 0; i < 4; i++) for (j = i + 1; j < 4; j++) { e.push([i, j]); e.push([i + 4, j + 4]); }
  } else {                   // a torus
    var U = 16, W2 = 7;
    for (i = 0; i < U; i++) for (j = 0; j < W2; j++) {
      var a = i * TAU / U, b = j * TAU / W2;
      v.push([(1 + 0.42 * Math.cos(b)) * Math.cos(a), 0.42 * Math.sin(b), (1 + 0.42 * Math.cos(b)) * Math.sin(a)]);
      e.push([i * W2 + j, ((i + 1) % U) * W2 + j]); e.push([i * W2 + j, i * W2 + (j + 1) % W2]);
    }
  }
  if (!e.length) {           // edges: the pairs of corners closest together
    var best = 1e9, d;
    for (i = 0; i < v.length; i++) for (j = i + 1; j < v.length; j++) { d = dist3(v[i], v[j]); if (d < best) best = d; }
    for (i = 0; i < v.length; i++) for (j = i + 1; j < v.length; j++) if (dist3(v[i], v[j]) < best * 1.01) e.push([i, j]);
  }
  var mx = 0; for (i = 0; i < v.length; i++) mx = Math.max(mx, Math.sqrt(v[i][0] * v[i][0] + v[i][1] * v[i][1] + v[i][2] * v[i][2]));
  for (i = 0; i < v.length; i++) { v[i][0] /= mx; v[i][1] /= mx; v[i][2] /= mx; }
  return { v: v, e: e };
}
function dist3(a, b) { var x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2]; return Math.sqrt(x * x + y * y + z * z); }
saver({
  id: 'solids', name: 'SOLIDS 3D', cat: 0, info: 'platonic solids',
  params: [
    CHO('shape', 'SHAPE', ['TETRA', 'CUBE', 'OCTA', 'DODECA', 'ICOSA', 'STAR', 'TORUS', 'ALL'], 7, 'The solid. ALL shows a new one every 10 seconds'),
    NUM('sx', 'SPIN X', 0, 10, 3, 'Turning over, front to back', { live: 1 }),
    NUM('sy', 'SPIN Y', 0, 10, 5, 'Turning round, left to right', { live: 1 }),
    NUM('size', 'SIZE', 30, 100, 75, 'How big', { live: 1, u: '%', step: 5 }),
    NUM('ghost', 'GHOSTS', 0, 10, 6, 'Old edges stay a while: the solid paints its own trail', { live: 1 }),
    PALP(3)
  ],
  init: function (s) { s.kind = P.shape == 7 ? irnd(7) : P.shape; s.m = solid(s.kind); s.ax = 0.4; s.ay = 0.3; s.next = NOW + 10000; s.hue = Math.random(); wipe(); },
  step: function (s, dt) {
    trail(P.ghost);
    var k = dt * SPD;
    s.ax += P.sx * 0.00011 * k; s.ay += P.sy * 0.00014 * k; s.hue += 0.00003 * k;
    if (OPT.tilt) { var g = tilt(); s.ay += g[0] * 0.04; s.ax += g[2] * 0.04; }
    if (P.shape == 7 && NOW > s.next) { s.kind = (s.kind + 1) % 7; s.m = solid(s.kind); s.next = NOW + 10000; }
    var v = s.m.v, e = s.m.e, ca = Math.cos(s.ax), sa = Math.sin(s.ax), cb = Math.cos(s.ay), sb = Math.sin(s.ay);
    var sc = V.r * 2.7 * P.size / 100, px = [], py = [], pz = [], i;
    for (i = 0; i < v.length; i++) {
      var x = v[i][0], y = v[i][1], z = v[i][2];
      var x1 = x * cb + z * sb, z1 = z * cb - x * sb;          // round (y axis)
      var y2 = y * ca - z1 * sa, z2 = y * sa + z1 * ca;        // over (x axis)
      var f = sc / (3.2 + z2);
      px.push(V.cx + x1 * f); py.push(V.cy + y2 * f); pz.push(z2);
    }
    for (i = 0; i < e.length; i++) {
      var a = e[i][0], b = e[i][1], depth = (pz[a] + pz[b]) / 2;     // -1 near .. 1 far
      var c = colv(clamp(0.92 - depth * 0.42, 0.35, 1));               // near = bright, far = dark
      thick(px[a], py[a], px[b], py[b], depth < 0 ? 3 : 2, c);
    }
  }
});

// ------------------------------------------------------------------ ALGORITHMS
var LIFE_RULES = [['LIFE', '3', '23'], ['HIGHLIFE', '36', '23'], ['DAY+NIGHT', '3678', '34678'], ['SEEDS', '2', ''],
  ['MAZE', '3', '12345'], ['CORAL', '3', '45678'], ['2X2', '36', '125'], ['AMOEBA', '357', '1358']];
var LIFE_NAMES = []; for (pi = 0; pi < LIFE_RULES.length; pi++) LIFE_NAMES.push(LIFE_RULES[pi][0]);
saver({
  id: 'life', name: 'GAME OF LIFE', cat: 1, info: 'cells live, die',
  params: [
    CHO('rule', 'RULE', LIFE_NAMES, 0, function (v) { var r = LIFE_RULES[v]; return 'B' + r[1] + '/S' + r[2] + ': a cell is born with ' + r[1].split('').join(',') + ' neighbours, stays with ' + (r[2].length ? r[2].split('').join(',') : 'none'); }),
    NUM('cell', 'CELL SIZE', 6, 24, 12, 'Bigger cells = fewer of them, and faster', { u: ' PX', step: 2 }),
    NUM('dens', 'START FILL', 5, 60, 30, 'How full the field is when it starts', { u: '%', step: 5 }),
    NUM('speed', 'SPEED', 1, 30, 12, 'Generations per second', { live: 1, u: '/S' }),
    NUM('ghost', 'GHOSTS', 0, 10, 5, 'Dead cells fade out slowly instead of vanishing', { live: 1 }),
    PALP(0)
  ],
  init: function (s) {
    var cs = P.cell, gw = Math.floor(V.w / cs), gh = Math.floor(V.h / cs), n = gw * gh, r = LIFE_RULES[P.rule], i;
    s.cs = cs; s.gw = gw; s.gh = gh; s.n = n;
    s.ox = V.x + ((V.w - gw * cs) >> 1); s.oy = V.y + ((V.h - gh * cs) >> 1);
    s.a = new Uint8Array(n); s.b = new Uint8Array(n); s.age = new Uint8Array(n); s.gh_ = new Uint8Array(n);
    s.B = []; s.S = []; for (i = 0; i < 9; i++) { s.B.push(r[1].indexOf(String(i)) >= 0 ? 1 : 0); s.S.push(r[2].indexOf(String(i)) >= 0 ? 1 : 0); }
    s.acc = 0; s.hist = []; s.hue = Math.random(); s.wait = 0;
    wipe(); this.seed(s);
  },
  seed: function (s) {
    var d = P.dens / 100;
    for (var i = 0; i < s.n; i++) { s.a[i] = Math.random() < d ? 1 : 0; s.age[i] = 0; s.gh_[i] = 0; if (s.a[i]) this.cell(s, i, col(s.hue)); }
    s.hist = [];
  },
  cell: function (s, i, c) { var x = i % s.gw, y = (i / s.gw) | 0; rect(s.ox + x * s.cs, s.oy + y * s.cs, s.cs - 1, s.cs - 1, c); },
  gen: function (s) {
    var a = s.a, b = s.b, age = s.age, gh = s.gh_, gw = s.gw, ghh = s.gh, B = s.B, S2 = s.S, G = P.ghost * 2, pop = 0;
    var AC = [], GC = [], k;
    for (k = 0; k < 16; k++) AC.push(col(s.hue + k * 0.035));
    var gbase = colv(0.4);
    for (k = 0; k <= G; k++) GC.push(lerpC(BLACK, gbase, G ? k / (G + 1) : 0));
    for (var y = 0; y < ghh; y++) {
      var row = y * gw, up = ((y + ghh - 1) % ghh) * gw, dn = ((y + 1) % ghh) * gw;
      for (var x = 0; x < gw; x++) {
        var xl = x ? x - 1 : gw - 1, xr = x < gw - 1 ? x + 1 : 0, i = row + x;
        var n = a[up + xl] + a[up + x] + a[up + xr] + a[row + xl] + a[row + xr] + a[dn + xl] + a[dn + x] + a[dn + xr];
        var was = a[i], now = was ? S2[n] : B[n];
        b[i] = now;
        if (now) {
          pop++;
          if (!was) { age[i] = 0; gh[i] = 0; this.cell(s, i, AC[0]); }
          else if (age[i] < 15) { age[i]++; if (!(age[i] & 3)) this.cell(s, i, AC[age[i]]); }
        } else if (was) { gh[i] = G; this.cell(s, i, G ? GC[G] : BLACK); }
        else if (gh[i]) { gh[i]--; this.cell(s, i, GC[gh[i]]); }
      }
    }
    s.a = b; s.b = a; s.hue += 0.002;
    // stuck (nothing left, or nothing changes)? a new start
    s.hist.push(pop); if (s.hist.length > 60) s.hist.shift();
    var mn = 1e9, mx = 0; for (k = 0; k < s.hist.length; k++) { mn = Math.min(mn, s.hist[k]); mx = Math.max(mx, s.hist[k]); }
    if (pop == 0 || (s.hist.length >= 60 && mx - mn <= 3)) s.wait = NOW + 1200;
  },
  step: function (s, dt) {
    if (s.wait) { if (KEEP) fade(V.x, V.y, V.w, V.h, BLACK, 0.16); if (NOW > s.wait) { s.wait = 0; wipe(); this.seed(s); } return; }
    s.acc += dt * SPD * P.speed / 1000;
    if (s.acc > 2) s.acc = 2;
    while (s.acc >= 1) { s.acc -= 1; this.gen(s); }
    if (!KEEP) { wipe(); for (var i = 0; i < s.n; i++) if (s.a[i]) this.cell(s, i, col(s.hue + s.age[i] * 0.035)); }
  },
  touch: function (s, t, x, y) {            // drag: draw living cells
    if (t != 'move' && t != 'down') return false;
    var cx = Math.floor((x - s.ox) / s.cs), cy = Math.floor((y - s.oy) / s.cs);
    for (var dy = -1; dy <= 1; dy++) for (var dx = -1; dx <= 1; dx++) {
      var xx = cx + dx, yy = cy + dy;
      if (xx < 0 || yy < 0 || xx >= s.gw || yy >= s.gh || Math.random() < 0.35) continue;
      var i = yy * s.gw + xx; s.a[i] = 1; s.age[i] = 0; this.cell(s, i, WHITE);
    }
    s.hist = [];
    return t == 'move';
  }
});

var ANT_RULES = ['RL', 'RLR', 'LLRR', 'LRRRRRLLR', 'LLRRRLRLRLLR', 'RRLLLRLLLRRR', 'LRRRRLLLRRR', 'RANDOM'];
var ANT_STEPS = [4, 8, 15, 30, 60, 100, 160, 250, 400, 650];
saver({
  id: 'ants', name: 'LANGTON ANTS', cat: 1, info: 'ants + colours',
  params: [
    CHO('rule', 'RULE', ANT_RULES, 0, 'A letter per colour: on it the ant turns R or L, paints the next colour. RL = Langton'),
    NUM('ants', 'ANTS', 1, 8, 1, 'How many ants walk at the same time (drag to add one)'),
    NUM('speed', 'SPEED', 1, 10, 6, 'Steps per picture', { live: 1 }),
    NUM('cell', 'CELL SIZE', 2, 10, 4, 'Size of one square (small = more room, the patterns grow longer)', { u: ' PX' }),
    CHO('start', 'START', ['MIDDLE', 'SCATTERED'], 0, 'Where the ants start'),
    PALP(1)
  ],
  init: function (s) {
    var cs = P.cell; s.cs = cs; s.gw = Math.floor(V.w / cs); s.gh = Math.floor(V.h / cs);
    s.ox = V.x + ((V.w - s.gw * cs) >> 1); s.oy = V.y + ((V.h - s.gh * cs) >> 1);
    s.g = new Uint8Array(s.gw * s.gh);
    var rule = ANT_RULES[P.rule];
    if (rule == 'RANDOM') { rule = ''; var len = 2 + irnd(10); for (var i = 0; i < len; i++) rule += Math.random() < 0.5 ? 'L' : 'R'; if (rule.indexOf('L') < 0 || rule.indexOf('R') < 0) rule = 'RLLR'; }
    s.rule = rule; s.nc = rule.length; s.turn = [];
    for (i = 0; i < s.nc; i++) s.turn.push(rule.charAt(i) == 'R' ? 1 : 3);
    s.hue = Math.random(); s.pc = [BLACK];
    for (i = 1; i < s.nc; i++) s.pc.push(col(s.hue + i / s.nc * 0.9));
    s.ants = [];
    for (i = 0; i < P.ants; i++) {
      if (P.start) s.ants.push({ x: irnd(s.gw), y: irnd(s.gh), d: irnd(4) });
      else s.ants.push({ x: (s.gw >> 1) + (i - (P.ants >> 1)) * 6, y: s.gh >> 1, d: i & 3 });
    }
    s.steps = 0; s.limit = s.gw * s.gh * 30; s.lastAdd = 0;
    wipe();
  },
  step: function (s) {
    if (s.ct !== CT) { s.ct = CT; s.pc = [BLACK]; for (var c = 1; c < s.nc; c++) s.pc.push(col(s.hue + c / s.nc * 0.9)); }   // new colours
    var n = Math.round(ANT_STEPS[P.speed - 1] * SPD), g = s.g, gw = s.gw, gh = s.gh, cs = s.cs, ox = s.ox, oy = s.oy;
    var DX = [0, 1, 0, -1], DY = [-1, 0, 1, 0];
    for (var k = 0; k < n; k++) {
      for (var j = 0; j < s.ants.length; j++) {
        var a = s.ants[j], i = a.y * gw + a.x, st = g[i];
        a.d = (a.d + s.turn[st]) & 3;
        st = st + 1 < s.nc ? st + 1 : 0; g[i] = st;
        rect(ox + a.x * cs, oy + a.y * cs, cs, cs, s.pc[st]);
        a.x += DX[a.d]; a.y += DY[a.d];
        if (a.x < 0) a.x = gw - 1; else if (a.x >= gw) a.x = 0;
        if (a.y < 0) a.y = gh - 1; else if (a.y >= gh) a.y = 0;
      }
    }
    s.steps += n * s.ants.length;
    if (s.steps > s.limit) this.init(s);
    if (V.h > 300) { rect(V.x + 4, V.y + V.h - 34, 15 * (s.rule.length + 1) + 8, 32, BLACK); text(s.rule, V.x + 8, V.y + V.h - 34, DIM, 1, 0, false); }
  },
  touch: function (s, t, x, y) {            // drag: a new ant where the finger is
    if (t != 'move' || NOW - s.lastAdd < 250 || s.ants.length >= 16) return t == 'move';
    var ax = Math.floor((x - s.ox) / s.cs), ay = Math.floor((y - s.oy) / s.cs);
    if (ax >= 0 && ay >= 0 && ax < s.gw && ay < s.gh) { s.ants.push({ x: ax, y: ay, d: irnd(4) }); s.lastAdd = NOW; }
    return true;
  }
});

var SORT_NAMES = ['TAKE TURNS', 'BUBBLE', 'COCKTAIL', 'INSERTION', 'SELECTION', 'SHELL', 'COMB', 'QUICK', 'HEAP', 'MERGE', 'RADIX'];
var SORT_STEPS = [1, 2, 3, 5, 8, 12, 20, 32, 50, 80];
function sortOps(kind, w) {      // runs the sort on w and writes down every step: [what, a, b] (0 compare, 1 swap, 2 write)
  var ops = [], n = w.length, i, j;
  function cmp(a, b) { ops.push(0, a, b); return w[a] > w[b]; }
  function swp(a, b) { var t = w[a]; w[a] = w[b]; w[b] = t; ops.push(1, a, b); }
  function put(a, v) { w[a] = v; ops.push(2, a, v); }
  if (kind == 1) { for (i = n - 1; i > 0; i--) { var sw = false; for (j = 0; j < i; j++) if (cmp(j, j + 1)) { swp(j, j + 1); sw = true; } if (!sw) break; } }
  else if (kind == 2) {
    var lo = 0, hi = n - 1;
    while (lo < hi) {
      var sw2 = false;
      for (j = lo; j < hi; j++) if (cmp(j, j + 1)) { swp(j, j + 1); sw2 = true; }
      hi--;
      for (j = hi; j > lo; j--) if (cmp(j - 1, j)) { swp(j - 1, j); sw2 = true; }
      lo++;
      if (!sw2) break;
    }
  } else if (kind == 3) { for (i = 1; i < n; i++) { j = i; while (j > 0 && cmp(j - 1, j)) { swp(j - 1, j); j--; } } }
  else if (kind == 4) { for (i = 0; i < n - 1; i++) { var m = i; for (j = i + 1; j < n; j++) if (cmp(m, j)) m = j; if (m != i) swp(i, m); } }
  else if (kind == 5) {
    var gaps = [701, 301, 132, 57, 23, 10, 4, 1];
    for (var gi = 0; gi < gaps.length; gi++) { var gp = gaps[gi]; if (gp >= n) continue; for (i = gp; i < n; i++) { j = i; while (j >= gp && cmp(j - gp, j)) { swp(j - gp, j); j -= gp; } } }
  } else if (kind == 6) {
    var gap = n, done = false;
    while (!done) { gap = Math.floor(gap / 1.3); if (gap <= 1) { gap = 1; done = true; } for (i = 0; i + gap < n; i++) if (cmp(i, i + gap)) { swp(i, i + gap); done = false; } }
  } else if (kind == 7) {
    var st = [0, n - 1];
    while (st.length) {
      var h = st.pop(), l = st.pop();
      if (l >= h) continue;
      var p = l + irnd(h - l + 1); if (p != h) swp(p, h);
      var q = l;
      for (j = l; j < h; j++) if (!cmp(j, h)) { if (q != j) swp(q, j); q++; }
      if (q != h) swp(q, h);
      st.push(l, q - 1, q + 1, h);
    }
  } else if (kind == 8) {
    var sift = function (a, m) { for (;;) { var c = 2 * a + 1; if (c >= m) return; if (c + 1 < m && cmp(c + 1, c)) c++; if (cmp(c, a)) { swp(a, c); a = c; } else return; } };
    for (i = (n >> 1) - 1; i >= 0; i--) sift(i, n);
    for (i = n - 1; i > 0; i--) { swp(0, i); sift(0, i); }
  } else if (kind == 9) {
    for (var wd = 1; wd < n; wd *= 2) for (var a0 = 0; a0 < n - wd; a0 += 2 * wd) {
      var b0 = Math.min(a0 + 2 * wd, n), aux = w.slice(a0, b0), li = 0, ri = wd, ln = b0 - a0;
      for (var kk = a0; kk < b0; kk++) {
        if (li < wd && ri < ln) ops.push(0, a0 + li, a0 + ri);
        if (li < wd && (ri >= ln || aux[li] <= aux[ri])) put(kk, aux[li++]); else put(kk, aux[ri++]);
      }
    }
  } else if (kind == 10) {
    for (var e = 1; e < n; e *= 4) {
      var cnt = [0, 0, 0, 0], out = [], d;
      for (i = 0; i < n; i++) { ops.push(0, i, i); cnt[Math.floor(w[i] / e) % 4]++; }
      for (d = 1; d < 4; d++) cnt[d] += cnt[d - 1];
      for (i = n - 1; i >= 0; i--) { d = Math.floor(w[i] / e) % 4; out[--cnt[d]] = w[i]; }
      for (i = 0; i < n; i++) put(i, out[i]);
    }
  }
  return ops;
}
saver({
  id: 'sort', name: 'SORTING', cat: 1, info: '10 algorithms',
  params: [
    CHO('algo', 'ALGORITHM', SORT_NAMES, 0, 'How the bars are put in order. TAKE TURNS shows them all, one after the other'),
    NUM('bars', 'BARS', 16, 128, 64, 'How many things to sort (slow ones like BUBBLE take long with many)', { step: 8 }),
    NUM('speed', 'SPEED', 1, 10, 5, 'Steps per picture', { live: 1 }),
    CHO('style', 'STYLE', ['BARS', 'MIRROR', 'DOTS', 'CIRCLE', 'DISTANCE'], 0, 'How it looks. DISTANCE: dots move to the ring as they reach their place', { live: 1 }),
    CHO('sound', 'SOUND', ['OFF', 'ON'], 0, 'Hear it: the higher the bar being moved, the higher the beep', { live: 1 }),
    PALP(1)
  ],
  init: function (s) { s.turn = 0; s.hue = Math.random(); this.begin(s); },
  begin: function (s) {
    var n = P.bars, a = [], i;
    for (i = 0; i < n; i++) a.push(i);
    for (i = n - 1; i > 0; i--) { var j = irnd(i + 1), t = a[i]; a[i] = a[j]; a[j] = t; }
    s.kind = P.algo ? P.algo : 1 + (s.turn++ % (SORT_NAMES.length - 1));
    s.arr = a.slice(); s.ops = sortOps(s.kind, a.slice()); s.pc = 0;
    s.ha = -1; s.hb = -1; s.hs = false; s.ncmp = 0; s.nswp = 0; s.phase = 0; s.check = 0; s.wait = 0;
  },
  step: function (s, dt) {
    wipe();
    var n = P.bars, arr = s.arr, k, i;
    if (s.phase == 0) {
      var m = Math.round(SORT_STEPS[P.speed - 1] * SPD) || 1, beepV = -1;
      s.ha = s.hb = -1;
      for (k = 0; k < m && s.pc < s.ops.length; k++) {
        var o = s.ops[s.pc], a = s.ops[s.pc + 1], b = s.ops[s.pc + 2]; s.pc += 3;
        if (o == 0) { s.ncmp++; s.ha = a; s.hb = b; s.hs = false; }
        else if (o == 1) { var t = arr[a]; arr[a] = arr[b]; arr[b] = t; s.nswp++; s.ha = a; s.hb = b; s.hs = true; beepV = arr[a]; }
        else { arr[a] = b; s.nswp++; s.ha = a; s.hb = -1; s.hs = true; beepV = b; }
      }
      if (P.sound && beepV >= 0) tone(160 + beepV / n * 900, 24, 22);
      if (s.pc >= s.ops.length) { s.phase = 1; s.check = 0; }
    } else if (s.phase == 1) {          // the green sweep: all in order
      s.check += Math.max(1, n / 24) * SPD;
      if (P.sound && s.check < n) tone(160 + s.check / n * 900, 18, 16);
      if (s.check >= n) { s.phase = 2; s.wait = NOW + 1600; }
    } else if (NOW > s.wait) this.begin(s);
    // draw
    var top = V.y + 36, H = V.h - 40, bw = V.w / n, gap = bw > 4 ? 1 : 0, base = V.y + V.h - 2, R = Math.min(V.w, V.h - 36) / 2 - 4;
    var ccx = V.cx, ccy = V.y + 36 + (V.h - 36) / 2;
    for (i = 0; i < n; i++) {
      var v = arr[i], c = col(s.hue + v / n * 0.85);
      if (i == s.ha || i == s.hb) c = s.hs ? RED : WHITE;
      else if (s.phase >= 1 && i < s.check) c = lerpC(c, WHITE, 0.55);
      var hh = (v + 1) / n * H, x = V.x + i * bw;
      if (P.style == 0) rect(x, base - hh, bw - gap, hh, c);
      else if (P.style == 1) rect(x, ccy - hh / 2, bw - gap, hh, c);
      else if (P.style == 2) rect(x, base - hh, Math.max(3, bw - gap), 4, c);
      else if (P.style == 3) { var an = i / n * TAU - Math.PI / 2, rr = R * 0.2 + R * 0.8 * (v + 1) / n; thick(ccx + Math.cos(an) * R * 0.18, ccy + Math.sin(an) * R * 0.18, ccx + Math.cos(an) * rr, ccy + Math.sin(an) * rr, Math.max(1, bw * 0.5), c); }
      else { var an2 = i / n * TAU - Math.PI / 2, rd = R * (1 - Math.abs(v - i) / n); rect(ccx + Math.cos(an2) * rd - 2, ccy + Math.sin(an2) * rd - 2, 5, 5, c); }
    }
    text(SORT_NAMES[s.kind] + '  ' + s.ncmp + ' CMP  ' + s.nswp + ' MOVES', V.x + 6, V.y + 2, DIM, 1, 0, false);
  }
});

var MAZE_NAMES = ['TAKE TURNS', 'BACKTRACK', 'PRIM', 'BINARY TREE'];
var MAZE_STEPS = [1, 2, 3, 5, 8, 12, 18, 26, 40, 60];
saver({
  id: 'maze', name: 'MAZE', cat: 1, info: 'built + solved',
  params: [
    CHO('algo', 'BUILDER', MAZE_NAMES, 0, 'BACKTRACK: long winding paths. PRIM: grows like moss. BINARY TREE: fast, diagonal'),
    NUM('cell', 'CELL SIZE', 10, 40, 20, 'Size of one room of the maze', { u: ' PX', step: 2 }),
    NUM('wall', 'WALLS', 2, 10, 4, 'How thick the walls are', { u: ' PX' }),
    NUM('speed', 'SPEED', 1, 10, 5, 'How fast it is dug and solved', { live: 1 }),
    CHO('solve', 'SOLVE IT', ['OFF', 'ON'], 1, 'Then flood it from the top left and draw the way to the bottom right', { live: 1 }),
    PALP(4)
  ],
  init: function (s) { s.turn = s.turn || 0; this.begin(s); },
  begin: function (s) {
    var cs = P.cell, wl = Math.min(P.wall, Math.floor(cs / 3)), gw = Math.floor((V.w - wl) / cs), gh = Math.floor((V.h - wl) / cs), n = gw * gh;
    s.cs = cs; s.wl = wl; s.gw = gw; s.gh = gh; s.n = n;
    s.ox = V.x + Math.floor((V.w - (gw * cs + wl)) / 2); s.oy = V.y + Math.floor((V.h - (gh * cs + wl)) / 2);
    s.R = new Uint8Array(n); s.D = new Uint8Array(n); s.vis = new Uint8Array(n); s.ord = 0; s.hue = Math.random();
    s.kind = P.algo ? P.algo : 1 + (s.turn++ % 3);
    s.phase = 0; s.wait = 0;
    wipe();
    var st = irnd(n);
    if (s.kind == 1) { s.stack = [st]; s.vis[st] = 1; this.room(s, st, WHITE); }
    else if (s.kind == 2) { s.front = []; s.inF = new Uint8Array(n); s.vis[st] = 1; this.room(s, st, this.c(s)); this.addF(s, st); }
    else s.bi = 0;
  },
  c: function (s) { return col(s.hue + (s.ord++) * 0.0025); },
  room: function (s, i, c) { var x = i % s.gw, y = (i / s.gw) | 0; rect(s.ox + x * s.cs + s.wl, s.oy + y * s.cs + s.wl, s.cs - s.wl, s.cs - s.wl, c); },
  link: function (s, a, b, c) {           // the opening between two rooms next to each other
    if (b < a) { var t = a; a = b; b = t; }
    var x = a % s.gw, y = (a / s.gw) | 0;
    if (b == a + 1) { s.R[a] = 1; rect(s.ox + (x + 1) * s.cs, s.oy + y * s.cs + s.wl, s.wl, s.cs - s.wl, c); }
    else { s.D[a] = 1; rect(s.ox + x * s.cs + s.wl, s.oy + (y + 1) * s.cs, s.cs - s.wl, s.wl, c); }
  },
  nb: function (s, i) { var x = i % s.gw, y = (i / s.gw) | 0, r = []; if (x > 0) r.push(i - 1); if (x < s.gw - 1) r.push(i + 1); if (y > 0) r.push(i - s.gw); if (y < s.gh - 1) r.push(i + s.gw); return r; },
  addF: function (s, i) { var nb = this.nb(s, i); for (var k = 0; k < nb.length; k++) if (!s.vis[nb[k]] && !s.inF[nb[k]]) { s.inF[nb[k]] = 1; s.front.push(nb[k]); } },
  open: function (s, u) {                 // the rooms reachable from u
    var x = u % s.gw, r = [];
    if (x < s.gw - 1 && s.R[u]) r.push(u + 1); if (x > 0 && s.R[u - 1]) r.push(u - 1);
    if (u + s.gw < s.n && s.D[u]) r.push(u + s.gw); if (u - s.gw >= 0 && s.D[u - s.gw]) r.push(u - s.gw);
    return r;
  },
  build: function (s) {                   // one step of digging; false = finished
    var nb, k, c;
    if (s.kind == 1) {
      if (!s.stack.length) return false;
      var top = s.stack[s.stack.length - 1], free = [];
      nb = this.nb(s, top); for (k = 0; k < nb.length; k++) if (!s.vis[nb[k]]) free.push(nb[k]);
      if (free.length) { var nx = free[irnd(free.length)]; c = this.c(s); this.room(s, top, c); this.link(s, top, nx, c); s.vis[nx] = 1; this.room(s, nx, WHITE); s.stack.push(nx); }
      else { s.stack.pop(); this.room(s, top, col(s.hue + s.ord * 0.0025)); if (s.stack.length) this.room(s, s.stack[s.stack.length - 1], WHITE); }
      return true;
    }
    if (s.kind == 2) {
      if (!s.front.length) return false;
      var fi = irnd(s.front.length), f = s.front[fi]; s.front[fi] = s.front[s.front.length - 1]; s.front.pop();
      var done = []; nb = this.nb(s, f); for (k = 0; k < nb.length; k++) if (s.vis[nb[k]]) done.push(nb[k]);
      c = this.c(s); s.vis[f] = 1; this.room(s, f, c); this.link(s, f, done[irnd(done.length)], c); this.addF(s, f);
      return true;
    }
    if (s.bi >= s.n) return false;
    var i = s.bi++, x = i % s.gw, y = (i / s.gw) | 0, opt = [];
    if (y > 0) opt.push(i - s.gw); if (x > 0) opt.push(i - 1);
    c = this.c(s); this.room(s, i, c); if (opt.length) this.link(s, i, opt[irnd(opt.length)], c);
    return true;
  },
  step: function (s) {
    var m = Math.round(MAZE_STEPS[P.speed - 1] * SPD) || 1, k;
    if (s.phase == 0) {
      for (k = 0; k < m; k++) if (!this.build(s)) {
        if (!P.solve) { s.phase = 3; s.wait = NOW + 2500; break; }
        s.phase = 1; s.q = [0]; s.qi = 0; s.dist = new Int32Array(s.n); s.par = new Int32Array(s.n);
        for (var i = 0; i < s.n; i++) s.dist[i] = -1;
        s.dist[0] = 0; s.par[0] = -1;
        break;
      }
    } else if (s.phase == 1) {            // the flood: every room gets its distance from the start
      for (k = 0; k < m * 2 && s.qi < s.q.length; k++) {
        var u = s.q[s.qi++], nb = this.open(s, u);
        for (var j = 0; j < nb.length; j++) {
          var v = nb[j]; if (s.dist[v] >= 0) continue;
          s.dist[v] = s.dist[u] + 1; s.par[v] = u; s.q.push(v);
          var c = lerpC(BLACK, col(s.hue + 0.5 + s.dist[v] * 0.006), 0.6);
          this.room(s, v, c); this.link(s, u, v, c);
          if (v == s.n - 1) { s.phase = 2; s.pu = v; }
        }
        if (s.phase == 2) break;
      }
      if (s.qi >= s.q.length && s.phase == 1) { s.phase = 3; s.wait = NOW + 2500; }
    } else if (s.phase == 2) {            // the way back from the end, as a bright line
      for (k = 0; k < m && s.pu > 0; k++) {
        var p = s.par[s.pu], half = s.cs / 2 + s.wl / 2;
        var x1 = s.ox + (s.pu % s.gw) * s.cs + half, y1 = s.oy + ((s.pu / s.gw) | 0) * s.cs + half;
        var x2 = s.ox + (p % s.gw) * s.cs + half, y2 = s.oy + ((p / s.gw) | 0) * s.cs + half;
        thick(x1, y1, x2, y2, Math.max(2, (s.cs - s.wl) / 3), WHITE);
        s.pu = p;
      }
      if (s.pu <= 0) { s.phase = 3; s.wait = NOW + 3000; }
    } else if (NOW > s.wait) this.begin(s);
  }
});

var ATTR_NAMES = ['CLIFFORD', 'DE JONG', 'SVENSSON', 'LORENZ'];
var ATTR_SETS = [
  [[-1.4, 1.6, 1.0, 0.7], [1.7, 1.7, 0.6, 1.2], [-1.7, 1.3, -0.1, -1.2], [-1.8, -2.0, -0.5, -0.9], [1.5, -1.8, 1.6, 0.9], [-1.32, -1.65, 0.74, 1.81]],
  [[1.4, -2.3, 2.4, -2.1], [2.01, -2.53, 1.61, -0.33], [-2.7, -0.09, -0.86, -2.2], [-2.24, 0.43, -0.65, -2.43], [-2.0, -2.0, -1.2, 2.0], [1.641, 1.902, 0.316, 1.525]],
  [[1.4, 1.56, 1.4, -6.56], [-2.337, -2.337, 0.533, 1.378], [1.5, -1.8, 1.6, 0.9], [-1.78, 1.29, -0.09, -1.18], [-0.91, -1.29, -1.97, -1.56], [2.0, 1.5, 1.3, 1.6]],
  [[10, 28, 2.667], [10, 35, 2.667], [10, 45, 2.667], [10, 99.96, 2.667], [14, 28, 2.667], [10, 24.5, 2.667]]
];
saver({
  id: 'attr', name: 'ATTRACTORS', cat: 1, info: 'order in chaos',
  params: [
    CHO('type', 'TYPE', ATTR_NAMES, 0, 'The formula. The first three jump point to point, LORENZ is a flowing line'),
    NUM('set', 'SHAPE', 1, 6, 1, 'Six good sets of numbers for each formula'),
    NUM('pts', 'POINTS', 100, 1200, 400, 'Points per picture: more = faster drawing, slower cube', { live: 1, step: 100 }),
    NUM('morph', 'MORPH', 0, 10, 0, 'Let the numbers drift: the shape slowly changes (and old points fade)', { live: 1 }),
    CHO('by', 'COLOUR BY', ['SPEED', 'PLACE', 'TIME'], 0, 'SPEED: how far each jump goes, PLACE: where it lands, TIME: slowly changing', { live: 1 }),
    PALP(2)
  ],
  init: function (s) {
    var t = P.type, k = ATTR_SETS[t][P.set - 1];
    s.k = k.slice(); s.k0 = k.slice(); s.x = 0.1; s.y = 0.1; s.z = 0.1; s.T = 0; s.hue = Math.random();
    // run it a while unseen, to learn how big it is
    var mnx = 1e9, mxx = -1e9, mny = 1e9, mxy = -1e9;
    for (var i = 0; i < 3000; i++) { this.it(s); var p = this.pr(s); if (i > 100) { mnx = Math.min(mnx, p[0]); mxx = Math.max(mxx, p[0]); mny = Math.min(mny, p[1]); mxy = Math.max(mxy, p[1]); } }
    s.bx = (mnx + mxx) / 2; s.by = (mny + mxy) / 2;
    s.sc = Math.min(V.w * 0.46 / Math.max(0.01, (mxx - mnx) / 2), V.h * 0.46 / Math.max(0.01, (mxy - mny) / 2));
    s.lx = null; wipe();
  },
  it: function (s) {                        // one step of the formula
    var a = s.k[0], b = s.k[1], c = s.k[2], d = s.k[3], x = s.x, y = s.y;
    if (P.type == 0) { s.x = Math.sin(a * y) + c * Math.cos(a * x); s.y = Math.sin(b * x) + d * Math.cos(b * y); }
    else if (P.type == 1) { s.x = Math.sin(a * y) - Math.cos(b * x); s.y = Math.sin(c * x) - Math.cos(d * y); }
    else if (P.type == 2) { s.x = d * Math.sin(a * x) - Math.sin(b * y); s.y = c * Math.cos(a * x) + Math.cos(b * y); }
    else { var h = 0.006, z = s.z; s.x = x + h * a * (y - x); s.y = y + h * (x * (b - z) - y); s.z = z + h * (x * y - c * z); }
  },
  pr: function (s) { return P.type == 3 ? [s.x, -s.z] : [s.x, -s.y]; },     // what goes on the screen (Lorenz: from the side)
  step: function (s, dt) {
    if (P.morph) {
      s.T += dt * 0.00004 * P.morph * SPD;
      for (var q = 0; q < (P.type == 3 ? 0 : 4); q++) s.k[q] = s.k0[q] + Math.sin(s.T * (1 + q * 0.37) + q) * 0.12;
      trail(P.type == 3 ? 6 : 8);
    } else if (P.type == 3) trail(9);
    var n = Math.round(P.pts * SPD * (P.type == 3 ? 0.25 : 1)), cx = V.cx, cy = V.cy, sc = s.sc;
    s.hue += 0.0002 * dt;
    for (var i = 0; i < n; i++) {
      var ox = s.x, oy = s.y;
      this.it(s);
      var p = this.pr(s), X = cx + (p[0] - s.bx) * sc, Y = cy + (p[1] - s.by) * sc, c;
      if (P.by == 0) c = col(s.hue + (Math.abs(s.x - ox) + Math.abs(s.y - oy)) * 0.18);
      else if (P.by == 1) c = col(s.hue + (X - V.x) / V.w * 0.5 + (Y - V.y) / V.h * 0.3);
      else c = col(s.hue);
      if (P.type == 3) { if (s.lx !== null) line(s.lx, s.ly, X, Y, c); s.lx = X; s.ly = Y; }
      else pset(X, Y, c);
    }
  }
});

saver({
  id: 'plasma', name: 'PLASMA', cat: 1, info: 'old demo effect',
  params: [
    NUM('cells', 'BLOCKS', 8, 32, 18, 'Blocks across the screen: more = finer and slower', { step: 2 }),
    NUM('speed', 'SPEED', 1, 10, 4, 'How fast the waves move', { live: 1 }),
    NUM('zoom', 'WAVE SIZE', 1, 10, 4, 'Small = big soft waves, big = many small ones', { live: 1 }),
    NUM('waves', 'WAVES', 2, 5, 3, 'How many waves are mixed together', { live: 1 }),
    CHO('shape', 'SHAPE', ['SQUARES', 'DOTS', 'DIAMONDS'], 0, 'How each block is drawn', { live: 1 }),
    PALP(1)
  ],
  init: function (s) { s.T = Math.random() * 100; },
  step: function (s, dt) {
    if (P.shape) wipe();
    s.T += dt * 0.0006 * P.speed * SPD;
    var n = P.cells, cs = V.w / n, rows = Math.ceil(V.h / cs), z = P.zoom * 0.09, T = s.T, W = P.waves;
    var mx = Math.sin(T * 0.37) * 6 + n / 2, my = Math.cos(T * 0.29) * 6 + rows / 2, c2 = Math.ceil(cs);
    for (var j = 0; j < rows; j++) for (var i = 0; i < n; i++) {
      var v = Math.sin(i * z * 2 + T) + Math.sin(j * z * 1.7 - T * 1.2);
      if (W > 2) v += Math.sin((i + j) * z * 1.2 + T * 0.7);
      if (W > 3) { var dx = i - mx, dy = j - my; v += Math.sin(Math.sqrt(dx * dx + dy * dy) * z * 2.4 - T * 1.6); }
      if (W > 4) v += Math.sin(i * z - j * z * 1.6 + T * 0.4);
      var c = col(v * 0.11 + T * 0.03), x = V.x + i * cs, y = V.y + j * cs;
      if (P.shape == 0) rect(x, y, c2, c2, c);
      else if (P.shape == 1) circ(x + cs / 2, y + cs / 2, cs * (0.25 + 0.2 * (v / W + 1)), c);
      else { var q = cs * (0.25 + 0.3 * (v / W + 1)); rect(x + (cs - q) / 2, y + (cs - q) / 2, q, q, c); }
    }
  }
});

// ------------------------------------------------------------------ NATURE & SPACE
saver({
  id: 'warp', name: 'STAR WARP', cat: 2, info: 'to light speed',
  params: [
    NUM('count', 'STARS', 40, 400, 180, 'How many stars', { step: 20 }),
    NUM('speed', 'SPEED', 1, 10, 5, 'How fast you fly', { live: 1 }),
    NUM('spin', 'SPIN', -10, 10, 2, 'The ship rolls left (-) or right (+)', { live: 1 }),
    NUM('streak', 'STREAKS', 0, 10, 4, 'How long the stars are pulled into lines', { live: 1 }),
    CHO('steer', 'STEER', ['MIDDLE', 'WANDER', 'TILT'], 1, 'Where you fly: straight, wandering, or where you tilt the cube', { live: 1 }),
    PALP(3)
  ],
  init: function (s) {
    var n = P.count; s.n = n; s.x = new Float32Array(n); s.y = new Float32Array(n); s.z = new Float32Array(n);
    for (var i = 0; i < n; i++) { s.x[i] = rnd(-1, 1); s.y[i] = rnd(-1, 1); s.z[i] = rnd(0.05, 1); }
    s.T = 0; s.cx = V.cx; s.cy = V.cy;
  },
  step: function (s, dt) {
    wipe();
    var k = dt * SPD, dz = P.speed * 0.00009 * k, a = P.spin * 0.00003 * k, ca = Math.cos(a), sa = Math.sin(a);
    s.T += k * 0.0004;
    var tx = V.cx, ty = V.cy;
    if (P.steer == 1) { tx += Math.sin(s.T * 0.9) * V.w * 0.18; ty += Math.cos(s.T * 0.7) * V.h * 0.15; }
    else if (P.steer == 2) { var g = tilt(); tx -= g[0] * V.w * 0.4; ty -= g[2] * V.h * 0.4; }
    s.cx += (tx - s.cx) * 0.05; s.cy += (ty - s.cy) * 0.05;
    var f = V.r * 0.7, tail = 1 + P.streak * 2.5;
    for (var i = 0; i < s.n; i++) {
      var x = s.x[i] * ca - s.y[i] * sa, y = s.x[i] * sa + s.y[i] * ca, z = s.z[i] - dz;
      if (z < 0.02) { x = rnd(-1, 1); y = rnd(-1, 1); z = 1; }
      s.x[i] = x; s.y[i] = y; s.z[i] = z;
      var sx = s.cx + x / z * f, sy = s.cy + y / z * f;
      if (sx < V.x || sy < V.y || sx >= V.x + V.w || sy >= V.y + V.h) { s.z[i] = 1; s.x[i] = rnd(-1, 1); s.y[i] = rnd(-1, 1); continue; }
      var z2 = z + dz * tail, c = colv(1.05 - z);
      if (z < 0.25) thick(s.cx + x / z2 * f, s.cy + y / z2 * f, sx, sy, 2, c);
      else line(s.cx + x / z2 * f, s.cy + y / z2 * f, sx, sy, c);
    }
  }
});

saver({
  id: 'flow', name: 'FLOW FIELD', cat: 2, info: 'drifting dust',
  params: [
    NUM('count', 'PARTICLES', 50, 400, 175, 'How many particles drift', { step: 25 }),
    NUM('speed', 'SPEED', 1, 10, 5, 'How fast they drift', { live: 1 }),
    NUM('scale', 'SWIRLS', 1, 10, 4, 'Small = a few big slow currents, big = many small swirls', { live: 1 }),
    NUM('change', 'CHANGE', 0, 10, 3, 'How fast the currents themselves change', { live: 1 }),
    NUM('trails', 'TRAILS', 1, 10, 7, 'How long the trails stay', { live: 1 }),
    PALP(6)
  ],
  init: function (s) {
    var n = P.count; s.n = n; s.x = new Float32Array(n); s.y = new Float32Array(n); s.l = new Float32Array(n);
    for (var i = 0; i < n; i++) this.spawn(s, i);
    s.T = Math.random() * 100; s.hue = Math.random(); s.fx = -1; wipe();
  },
  spawn: function (s, i) { s.x[i] = V.x + Math.random() * V.w; s.y[i] = V.y + Math.random() * V.h; s.l[i] = 40 + Math.random() * 160; },
  step: function (s, dt) {
    trail(P.trails);
    var k = dt * SPD / 33, sp = (0.6 + P.speed * 0.35) * k, f = P.scale * 0.0028, T, gx = 0, gy = 0;
    s.T += P.change * 0.0005 * dt * SPD; T = s.T;
    if (OPT.tilt) { var g = tilt(); gx = g[0] * 1.5 * k; gy = g[2] * 1.5 * k; }
    for (var i = 0; i < s.n; i++) {
      var x = s.x[i], y = s.y[i];
      var a = (Math.sin(x * f + T) + Math.sin(y * f * 1.3 - T * 0.8) + Math.sin((x + y) * f * 0.7 + T * 0.5)) * 1.7;
      if (s.fx >= 0) { var dx = x - s.fx, dy = y - s.fy; if (dx * dx + dy * dy < 8100) a = Math.atan2(dy, dx) + 1.9; }   // a whirl round the finger
      var nx = x + Math.cos(a) * sp + gx, ny = y + Math.sin(a) * sp + gy;
      line(x, y, nx, ny, col(s.hue + a * 0.08));
      s.x[i] = nx; s.y[i] = ny; s.l[i] -= k;
      if (s.l[i] <= 0 || nx < V.x || ny < V.y || nx >= V.x + V.w || ny >= V.y + V.h) this.spawn(s, i);
    }
  },
  touch: function (s, t, x, y) { if (t == 'up') s.fx = -1; else { s.fx = x; s.fy = y; } return t == 'move'; }
});

saver({
  id: 'boids', name: 'BOIDS', cat: 2, info: 'flocking birds',
  params: [
    NUM('count', 'BIRDS', 10, 80, 40, 'How many birds. Each one looks at all the others: many = slower', { step: 5 }),
    NUM('sep', 'SEPARATION', 0, 10, 5, 'How much they keep apart', { live: 1 }),
    NUM('ali', 'ALIGNMENT', 0, 10, 5, 'How much they fly the same way as their neighbours', { live: 1 }),
    NUM('coh', 'COHESION', 0, 10, 4, 'How much they pull together into a flock', { live: 1 }),
    NUM('speed', 'SPEED', 1, 10, 5, 'How fast they fly', { live: 1 }),
    NUM('trails', 'TRAILS', 0, 10, 4, 'Lines where they flew', { live: 1 }),
    CHO('shape', 'SHAPE', ['ARROWS', 'DOTS', 'LINES'], 0, 'How a bird is drawn', { live: 1 }),
    PALP(4)
  ],
  init: function (s) {
    var n = P.count; s.n = n; s.x = new Float32Array(n); s.y = new Float32Array(n); s.vx = new Float32Array(n); s.vy = new Float32Array(n);
    for (var i = 0; i < n; i++) { s.x[i] = V.x + Math.random() * V.w; s.y[i] = V.y + Math.random() * V.h; var a = Math.random() * TAU; s.vx[i] = Math.cos(a) * 2; s.vy[i] = Math.sin(a) * 2; }
    s.fx = -1; s.hue = Math.random(); wipe();
  },
  step: function (s, dt) {
    trail(P.trails);
    var n = s.n, X = s.x, Y = s.y, VX = s.vx, VY = s.vy, R = Math.max(30, V.r * 0.25), R2 = R * R, SEP2 = R2 * 0.18;
    var vmax = (1 + P.speed * 0.45) * SPD, k = dt / 33, wsep = P.sep * 0.05, wali = P.ali * 0.006, wcoh = P.coh * 0.0005, i, j;
    if (OPT.tilt) { var g = tilt(); var gx = g[0] * 0.08, gy = g[2] * 0.08; }
    for (i = 0; i < n; i++) {
      var ax = 0, ay = 0, cx = 0, cy = 0, sx = 0, sy = 0, cnt = 0, xi = X[i], yi = Y[i];
      for (j = 0; j < n; j++) {
        if (j == i) continue;
        var dx = X[j] - xi, dy = Y[j] - yi, d2 = dx * dx + dy * dy;
        if (d2 < R2) { cnt++; ax += VX[j]; ay += VY[j]; cx += dx; cy += dy; if (d2 < SEP2) { sx -= dx / (d2 + 1) * 6; sy -= dy / (d2 + 1) * 6; } }
      }
      var vx = VX[i], vy = VY[i];
      if (cnt) { vx += (ax / cnt - vx) * wali + cx / cnt * wcoh; vy += (ay / cnt - vy) * wali + cy / cnt * wcoh; }
      vx += sx * wsep; vy += sy * wsep;
      if (s.fx >= 0) { var fx = xi - s.fx, fy = yi - s.fy, fd = fx * fx + fy * fy; if (fd < 14400) { vx += fx / (fd + 1) * 40; vy += fy / (fd + 1) * 40; } }   // fleeing the finger
      if (OPT.tilt) { vx += gx; vy += gy; }
      var sp = Math.sqrt(vx * vx + vy * vy) || 1;
      if (sp > vmax) { vx = vx / sp * vmax; vy = vy / sp * vmax; } else if (sp < vmax * 0.4) { vx = vx / sp * vmax * 0.4; vy = vy / sp * vmax * 0.4; }
      VX[i] = vx; VY[i] = vy;
    }
    for (i = 0; i < n; i++) {
      var x = X[i] + VX[i] * k, y = Y[i] + VY[i] * k;
      if (x < V.x) x += V.w; else if (x >= V.x + V.w) x -= V.w;
      if (y < V.y) y += V.h; else if (y >= V.y + V.h) y -= V.h;
      X[i] = x; Y[i] = y;
      var s2 = Math.sqrt(VX[i] * VX[i] + VY[i] * VY[i]) || 1, ux = VX[i] / s2, uy = VY[i] / s2, c = col(s.hue + Math.atan2(uy, ux) / TAU);
      if (P.shape == 0) {
        var hx = x + ux * 8, hy = y + uy * 8, lx = x - ux * 5 - uy * 5, ly = y - uy * 5 + ux * 5, rx = x - ux * 5 + uy * 5, ry = y - uy * 5 - ux * 5;
        line(hx, hy, lx, ly, c); line(hx, hy, rx, ry, c); line(lx, ly, rx, ry, c);
      } else if (P.shape == 1) circ(x, y, 3, c);
      else thick(x - ux * 9, y - uy * 9, x + ux * 4, y + uy * 4, 2, c);
    }
  },
  touch: function (s, t, x, y) { if (t == 'up') s.fx = -1; else { s.fx = x; s.fy = y; } return t == 'move'; }
});

saver({
  id: 'tree', name: 'FRACTAL TREE', cat: 2, info: 'tree in wind',
  params: [
    NUM('depth', 'DEPTH', 5, 9, 8, 'How often the branches split. 9 = 511 branches (slower)'),
    NUM('angle', 'ANGLE', 10, 60, 24, 'The angle between two branches', { live: 1, u: ' DEG' }),
    NUM('shrink', 'SHRINK', 60, 80, 72, 'Each branch is this long compared to the one it grows from', { live: 1, u: '%' }),
    NUM('wind', 'WIND', 0, 10, 4, 'How hard the wind blows', { live: 1 }),
    CHO('leaves', 'LEAVES', ['OFF', 'LEAVES', 'BLOSSOM'], 1, 'What grows on the twigs', { live: 1 }),
    PALP(9)
  ],
  init: function (s) { s.T = 0; s.grow = 0; s.R = []; for (var i = 0; i < 1024; i++) s.R.push(rnd(0.82, 1.15)); s.hue = Math.random(); },
  step: function (s, dt) {
    wipe();
    s.T += dt * 0.001 * SPD; s.grow = Math.min(P.depth + 1, s.grow + dt * 0.0022 * SPD);
    var base = V.y + V.h - 3;
    rect(V.x, base, V.w, 3, colv(0.22));
    this.br(s, V.cx, base, V.h * 0.27, -Math.PI / 2, 0, 1);
  },
  br: function (s, x, y, len, ang, lvl, id) {
    var g = s.grow - lvl; if (g <= 0) return;
    var l = len * (g < 1 ? g : 1) * s.R[id & 1023], x2 = x + Math.cos(ang) * l, y2 = y + Math.sin(ang) * l;
    var w = Math.max(1, (P.depth - lvl) * V.r / 240 * 1.6);
    thick(x, y, x2, y2, w, colv(0.12 + 0.6 * lvl / P.depth));
    if (lvl + 1 >= P.depth) {
      if (P.leaves && g >= 1) circ(x2, y2, Math.max(2, V.r / 80), P.leaves == 2 ? lerpC(0xffffff, 0xff7fbf, s.R[(id * 7) & 1023] - 0.8) : colv(0.75 + 0.25 * s.R[(id * 3) & 1023] - 0.2));
      return;
    }
    if (g < 1) return;
    var sway = (Math.sin(s.T * 1.6 + lvl * 0.8 + id * 0.37) * 0.6 + Math.sin(s.T * 0.7)) * P.wind * 0.011 * (lvl + 1);
    var a = P.angle * Math.PI / 180 * s.R[(id * 5) & 1023], sh = P.shrink / 100;
    this.br(s, x2, y2, len * sh, ang - a + sway, lvl + 1, id * 2);
    this.br(s, x2, y2, len * sh, ang + a + sway, lvl + 1, id * 2 + 1);
  }
});

saver({
  id: 'fire', name: 'FIREWORKS', cat: 2, info: 'bang + sparkle',
  params: [
    NUM('rate', 'ROCKETS', 1, 10, 4, 'How often a rocket goes up', { live: 1 }),
    NUM('sparks', 'SPARKS', 20, 120, 60, 'Sparks in each burst', { live: 1, step: 10 }),
    NUM('grav', 'GRAVITY', 0, 10, 4, 'How fast the sparks fall', { live: 1 }),
    NUM('trails', 'TRAILS', 1, 10, 6, 'How long the sparks glow behind them', { live: 1 }),
    CHO('style', 'BURSTS', ['MIXED', 'BALL', 'RING', 'PALM', 'DOUBLE'], 0, 'The shape of the bursts', { live: 1 }),
    CHO('sound', 'SOUND', ['OFF', 'ON'], 0, 'A pop for every burst', { live: 1 }),
    PALP(1)
  ],
  init: function (s) { s.p = []; s.next = 0; wipe(); },
  burst: function (s, x, y) {
    var st = P.style ? P.style : 1 + irnd(4), n = P.sparks, c = col(Math.random()), c2 = col(Math.random()), sp = V.r / 240 * 3.2, i, a, v;
    for (i = 0; i < n; i++) {
      a = Math.random() * TAU; v = sp;
      if (st == 1) v = sp * Math.sqrt(Math.random());
      else if (st == 2) v = sp * rnd(0.95, 1.05);
      else if (st == 3) { a = (i % 7) * TAU / 7 + rnd(-0.06, 0.06); v = sp * rnd(0.3, 1); }
      else if (st == 4 && i & 1) v = sp * 0.5;
      var life = rnd(40, 70);
      if (s.p.length < 700) s.p.push({ x: x, y: y, vx: Math.cos(a) * v, vy: Math.sin(a) * v, l: life, m: life, c: st == 4 && i & 1 ? c2 : c, r: 0 });
    }
    if (P.sound) tone(110 + irnd(90), 70, 40);
  },
  step: function (s, dt) {
    trail(P.trails);
    var k = dt * SPD / 33, g = (0.02 + P.grav * 0.012) * k, i;
    if (NOW > s.next) {                   // a new rocket
      var h = V.h * rnd(0.45, 0.8);
      s.p.push({ x: V.x + V.w * rnd(0.15, 0.85), y: V.y + V.h, vx: rnd(-0.4, 0.4), vy: -Math.sqrt(2 * 0.28 * h), l: 999, m: 999, c: WHITE, r: 1 });
      s.next = NOW + (2400 - P.rate * 210) * rnd(0.5, 1.3) / SPD;
    }
    var q = s.p;
    for (i = q.length - 1; i >= 0; i--) {
      var p = q[i];
      p.x += p.vx * k; p.y += p.vy * k;
      if (p.r) {                            // a rocket: up, a little trail, bang at the top
        p.vy += 0.28 * k;
        rect(p.x, p.y, 2, 3, 0xffe0a0);
        if (p.vy >= -0.6) { this.burst(s, p.x, p.y); q[i] = q[q.length - 1]; q.pop(); }
        continue;
      }
      p.vy += g; p.vx *= 0.985; p.vy *= 0.985; p.l -= k;
      if (p.l <= 0 || p.y > V.y + V.h) { q[i] = q[q.length - 1]; q.pop(); continue; }
      var f = p.l / p.m;
      if (f < 0.3 && Math.random() < 0.4) continue;                       // the end: it twinkles
      rect(p.x, p.y, f > 0.6 ? 3 : 2, f > 0.6 ? 3 : 2, lerpC(BLACK, f > 0.85 ? lerpC(p.c, WHITE, 0.5) : p.c, Math.min(1, f * 1.4)));
    }
  }
});

// ======================================================================== OPTIONS (for all pictures)
var AUTO_S = [0, 20, 60, 120, 300, 600];
var SPEEDS = [0.25, 0.5, 0.75, 1, 1.5, 2, 3];
var FPSS = [15, 20, 30];
var OPTS = [
  CHO('auto', 'AUTO NEXT', ['OFF', '20 S', '1 MIN', '2 MIN', '5 MIN', '10 MIN'], 0, 'Go on to the next picture by itself after this long (SLIDESHOW: 1 MIN if OFF)'),
  CHO('order', 'ORDER', ['IN TURN', 'SHUFFLE'], 0, 'Which picture comes next: the next in the list, or any'),
  CHO('name', 'SHOW NAME', ['OFF', 'ON'], 1, 'Show the name for a moment when a picture starts'),
  CHO('speed', 'SPEED', ['25%', '50%', '75%', '100%', '150%', '200%', '300%'], 3, 'The speed of all pictures, on top of their own SPEED'),
  CHO('pal', 'COLOURS', ['OWN'].concat(PALNAMES), 0, 'OWN: each picture its own palette. Or one palette for all of them'),
  CHO('fps', 'FRAME RATE', ['15', '20', '30'], 2, 'Pictures per second. Lower uses less battery; heavy pictures may not reach 30'),
  CHO('tilt', 'TILT', ['OFF', 'ON'], 1, 'SOLIDS, FLOW and BOIDS lean the way you tilt the cube'),
  CHO('stats', 'SHOW FPS', ['OFF', 'ON'], 0, 'Show how many pictures per second the cube really draws')
];
var OPT = {};
function applyOpts() { SPD = SPEEDS[OPT.speed]; fps(FPSS[OPT.fps]); applyPal(); }
function applyPal() { usePal(OPT.pal > 0 ? OPT.pal - 1 : P.pal); }

// ======================================================================== the settings of each picture
var ALLP = {};
function paramsOf(i) {
  var sv = SAVERS[i], q = ALLP[sv.id] || {}, out = {};
  for (var k = 0; k < sv.params.length; k++) { var p = sv.params[k], v = q[p.k]; out[p.k] = (typeof v == 'number' && v >= p.min && v <= p.max) ? v : p.def; }
  return out;
}
function startSaver(i, view) {             // view: 0 = the whole screen, 1 = the TUNE preview strip
  CUR = i; P = paramsOf(i);
  if (view) setView(0, 0, 480, 140); else setView(0, 0, 480, 480);
  S = {}; applyPal();
  SAVERS[i].init(S);
  runStart = NOW;
}

// ======================================================================== PANEL: a list of settings
// One row per setting: its name, - value +, a bar showing where the value is between its smallest and biggest.
// Under the list: what the selected setting does. Tap - / + (or the left / right of a row), slide along the
// value, or - / + on the cube (hold + = next value).
var PN = { list: null, vals: null, sel: 0, top: 0, y0: 0, rh: 38, rows: 6, helpY: 0, change: null, drag: null };
function panelOpen(list, vals, y0, rh, rows, helpY, change) { PN.list = list; PN.vals = vals; PN.sel = 0; PN.top = 0; PN.y0 = y0; PN.rh = rh; PN.rows = rows; PN.helpY = helpY; PN.change = change; PN.drag = null; }
function panelSet(i, v) {
  var p = PN.list[i]; v = clamp(Math.round((v - p.min) / p.step) * p.step + p.min, p.min, p.max);
  if (v === PN.vals[p.k]) return;
  PN.vals[p.k] = v; beep('key'); PN.change(p);
}
function panelShow(i) { if (i < PN.top) PN.top = i; if (i >= PN.top + PN.rows) PN.top = i - PN.rows + 1; }
function panelDraw() {
  var L = PN.list, hiBg = lerpC(BLACK, FG, 0.16);
  for (var r = 0; r < PN.rows; r++) {
    var i = PN.top + r; if (i >= L.length) break;
    var p = L[i], v = PN.vals[p.k], y = PN.y0 + r * PN.rh, on = i == PN.sel;
    if (on) { rect(2, y, 476, PN.rh - 2, hiBg); frame(2, y, 476, PN.rh - 2, FG); }
    text(p.name, 12, y + (PN.rh - 32) / 2 - 2, on ? WHITE : FG, 1, 0, on);
    frame(204, y + 3, 40, PN.rh - 10, v > p.min ? FG : DIM); text('-', 224, y + (PN.rh - 32) / 2 - 3, v > p.min ? FG : DIM, 1, 1, false);
    frame(434, y + 3, 40, PN.rh - 10, v < p.max ? FG : DIM); text('+', 454, y + (PN.rh - 32) / 2 - 3, v < p.max ? FG : DIM, 1, 1, false);
    text(pval(p, v), 339, y + (PN.rh - 32) / 2 - 3, on ? WHITE : FG, 1, 1, false);
    var f = p.max > p.min ? (v - p.min) / (p.max - p.min) : 0;
    rect(252, y + PN.rh - 6, 174, 2, DIM); rect(252, y + PN.rh - 6, 174 * f, 2, FG);
  }
  if (L.length > PN.rows) {                 // more than fits: a little slider on the right
    var th = PN.rows / L.length * PN.rows * PN.rh, ty = PN.y0 + PN.top / L.length * PN.rows * PN.rh;
    rect(478, PN.y0, 2, PN.rows * PN.rh, DIM); rect(477, ty, 3, th, FG);
  }
  var hp = L[PN.sel], h = typeof hp.help == 'function' ? hp.help(PN.vals[hp.k]) : hp.help, lines = wrapText(h, 31);
  for (var k = 0; k < lines.length && k < 3; k++) text(lines[k], 8, PN.helpY + k * 27, lerpC(FG, WHITE, 0.3), 1, 0, false);
}
function panelTouch(t, x, y) {
  var L = PN.list, inside = y >= PN.y0 && y < PN.y0 + PN.rows * PN.rh, r = Math.floor((y - PN.y0) / PN.rh), i = PN.top + r;
  if (t == 'down') { PN.drag = inside ? { x: x, y: y, i: i, top: PN.top, mode: null } : null; return inside; }
  if (t == 'move' && PN.drag) {
    var d = PN.drag;
    if (!d.mode) d.mode = (Math.abs(x - d.x) > Math.abs(y - d.y) && d.x >= 200) ? 'slide' : 'scroll';
    if (d.mode == 'slide' && d.i < L.length) {          // slide along the value
      var p = L[d.i]; PN.sel = d.i;
      panelSet(d.i, p.min + clamp((x - 252) / 174, 0, 1) * (p.max - p.min));
    } else if (L.length > PN.rows) PN.top = clamp(d.top - Math.round((y - d.y) / PN.rh), 0, L.length - PN.rows);
    return true;
  }
  if (t == 'tap' && inside && i < L.length) {
    PN.sel = i;
    var q = L[i], v = PN.vals[q.k];
    if (x >= 196 && x < 252) panelSet(i, v - q.step);
    else if (x >= 426) panelSet(i, v + q.step);
    else beep('move');
    return true;
  }
  if (t == 'up') PN.drag = null;
  return inside;
}
function panelKey(k) {
  var L = PN.list;
  if (k == 'next') { PN.sel = (PN.sel + 1) % L.length; panelShow(PN.sel); beep('move'); return true; }
  if (k == 'prev') { PN.sel = (PN.sel + L.length - 1) % L.length; panelShow(PN.sel); beep('move'); return true; }
  if (k == 'ok') { var p = L[PN.sel], v = PN.vals[p.k]; panelSet(PN.sel, v >= p.max ? p.min : v + p.step); return true; }
  return false;
}

// ======================================================================== MENU / RUN / TUNE / OPTIONS
var mode = 'menu', sel = 0, slideshow = false;
var runStart = 0, hudUntil = 0, nameUntil = 0, dragUsed = false;
var ROWS = [], RH = 46, LIST_Y = 44, LIST_H = 372, mScroll = 0, mVel = 0, mDrag = null;
var fpsCount = 0, fpsAt = 0, fpsNow = 0;
function buildRows() {
  ROWS = [];
  for (var c = 0; c < CATS.length; c++) {
    ROWS.push({ h: c });
    for (var i = 0; i < SAVERS.length; i++) if (SAVERS[i].cat == c) ROWS.push({ s: i });
  }
}
function rowOf(si) { for (var r = 0; r < ROWS.length; r++) if (ROWS[r].s === si) return r; return 0; }
function menuMax() { return Math.max(0, ROWS.length * RH - LIST_H); }
function menuShowSel() { var y = rowOf(sel) * RH; if (y < mScroll) mScroll = y - (rowOf(sel) > 0 && ROWS[rowOf(sel) - 1].h !== undefined ? RH : 0); if (y + RH > mScroll + LIST_H) mScroll = y + RH - LIST_H; mScroll = clamp(mScroll, 0, menuMax()); }
function go(m) {
  var was = mode; mode = m; hudUntil = 0;
  if (m == 'menu') { slideshow = false; if (was == 'tune') startSaver(sel, 0); menuShowSel(); save('sel', sel); }
  else if (m == 'run') { if (was == 'tune') startSaver(CUR, 0); runStart = NOW; if (OPT.name) nameUntil = NOW + 2600; }
  else if (m == 'tune') { startSaver(CUR, 1); panelOpen(SAVERS[CUR].params, P, 146, 34, 6, 354, tuneChanged); }
  else if (m == 'opts') panelOpen(OPTS, OPT, 46, 36, 8, 340, optChanged);
}
function play(i) { sel = i; if (CUR != i || mode == 'tune') startSaver(i, 0); go('run'); save('sel', sel); }
function nextSaver(d) {
  var i = CUR;
  if (OPT.order == 1 && SAVERS.length > 1) { while (i == CUR) i = irnd(SAVERS.length); }
  else { var r = rowOf(CUR); do { r = (r + d + ROWS.length) % ROWS.length; } while (ROWS[r].s === undefined); i = ROWS[r].s; }
  sel = i; startSaver(i, 0); runStart = NOW; hudUntil = 0;
  if (OPT.name) nameUntil = NOW + 2600;
}
function tuneChanged(p) {
  ALLP[SAVERS[CUR].id] = P;
  if (p.k == 'pal') applyPal();
  else if (!p.live) { S = {}; SAVERS[CUR].init(S); }
}
function optChanged(p) { applyOpts(); }
function tuneDone() { ALLP[SAVERS[CUR].id] = P; save('p', ALLP); go('run'); }
function tuneReset() { var ps = SAVERS[CUR].params; for (var i = 0; i < ps.length; i++) P[ps[i].k] = ps[i].def; ALLP[SAVERS[CUR].id] = P; applyPal(); S = {}; SAVERS[CUR].init(S); beep('ok'); }
function tuneRandom() {
  var ps = SAVERS[CUR].params;
  for (var i = 0; i < ps.length; i++) { var p = ps[i]; P[p.k] = p.min + Math.round(Math.random() * (p.max - p.min) / p.step) * p.step; }
  ALLP[SAVERS[CUR].id] = P; applyPal(); S = {}; SAVERS[CUR].init(S); beep('ok');
}

// ------------------------------------------------------------------ what's drawn on top (not kept)
function drawMenu() {
  fade(0, 0, 480, 480, BLACK, 0.62);         // the selected picture runs behind the list
  for (var r = 0; r < ROWS.length; r++) {
    var y = LIST_Y + r * RH - Math.round(mScroll), row = ROWS[r];
    if (y < LIST_Y - RH || y > LIST_Y + LIST_H) continue;
    if (row.h !== undefined) {
      var hc = lerpC(FG, BLACK, 0.3), tw = textw(CATS[row.h], 1);
      text(CATS[row.h], 240, y + 9, hc, 1, 1, false);
      rect(16, y + 25, 224 - tw / 2 - 26, 2, hc); rect(240 + tw / 2 + 10, y + 25, 224 - tw / 2 - 26, 2, hc);
      continue;
    }
    var sv = SAVERS[row.s], on = row.s == sel;
    if (on) rect(8, y + 3, 458, RH - 6, FG);
    text(sv.name, 20, y + 6, on ? BLACK : FG, 1, 0, !on);
    text(sv.info, 456, y + 6, on ? BLACK : lerpC(FG, BLACK, 0.35), 1, 2, false);
  }
  var mx = menuMax();
  if (mx > 0) { var th = LIST_H * LIST_H / (ROWS.length * RH); rect(472, LIST_Y + (LIST_H - th) * mScroll / mx, 4, th, FG); }
  rect(0, 0, 480, 40, FG); rect(0, 40, 480, 4, BLACK);
  text('SCREEN SAVERS', 12, 4, BLACK, 1, 0, false);
  text(KEEP ? SAVERS.length + ' PICTURES' : 'TRAILS: UPDATE', 468, 4, BLACK, 1, 2, false);
  rect(0, 418, 480, 62, BLACK); rect(0, 418, 480, 2, FG);
  button('SLIDESHOW', 8, 426, 228, 46, false);
  button('OPTIONS', 244, 426, 228, 46, false);
}
function drawRun() {
  var sv = SAVERS[CUR], auto = autoTime();
  if (NOW < hudUntil) {
    fade(0, 0, 480, 44, BLACK, 0.75);
    text(sv.name, 12, 5, FG, 1, 0, true);
    var right = auto ? 'NEXT ' + Math.max(0, Math.ceil((runStart + auto * 1000 - NOW) / 1000)) + 'S' : CATS[sv.cat];
    text(right, 468, 5, DIM, 1, 2, false);
    fade(0, 414, 480, 66, BLACK, 0.75);
    button('<', 8, 422, 84, 50, false); button('MENU', 100, 422, 132, 50, false);
    button('TUNE', 248, 422, 132, 50, false); button('>', 388, 422, 84, 50, false);
  } else if (NOW < nameUntil) {
    var a = Math.min(1, (nameUntil - NOW) / 600);
    fade(60, 384, 360, 74, BLACK, 0.7 * a);
    text(sv.name, 240, 388, lerpC(BLACK, FG, a), 1, 1, a > 0.5);
    text(sv.info + (slideshow ? '  - SLIDESHOW' : ''), 240, 420, lerpC(BLACK, DIM, a), 1, 1, false);
  }
}
function drawTune() {
  var sv = SAVERS[CUR];
  fade(0, 0, 480, 32, BLACK, 0.6);
  text(sv.name, 8, -2, FG, 1, 0, false); text('TUNE', 472, -2, DIM, 1, 2, false);
  rect(0, 140, 480, 340, BLACK); rect(0, 140, 480, 2, FG);
  panelDraw();
  button('RANDOM', 6, 440, 150, 38, false); button('RESET', 165, 440, 150, 38, false); button('DONE', 324, 440, 150, 38, true);
}
function drawOpts() {
  fade(0, 0, 480, 480, BLACK, 0.82);
  rect(0, 0, 480, 40, FG); text('OPTIONS', 12, 4, BLACK, 1, 0, false); text('ALL PICTURES', 468, 4, BLACK, 1, 2, false);
  panelDraw();
  button('RESET ALL', 6, 440, 228, 38, false); button('DONE', 246, 440, 228, 38, true);
}
function autoTime() { var a = AUTO_S[OPT.auto]; if (!a && slideshow) a = 60; return a; }

// ======================================================================== the cube calls these
function init() {
  KEEP = typeof keep == 'function' && keep(true) === true;
  buildPals(); buildRows();
  var o = load('o', null) || {};
  for (var i = 0; i < OPTS.length; i++) { var p = OPTS[i], v = o[p.k]; OPT[p.k] = (typeof v == 'number' && v >= p.min && v <= p.max) ? v : p.def; }
  ALLP = load('p', null) || {};
  sel = clamp(load('sel', 0) | 0, 0, SAVERS.length - 1);
  NOW = ms(); fpsAt = NOW;
  applyOpts();
  startSaver(sel, 0);
  menuShowSel();
}
function update(dt) {
  NOW = ms(); DT = Math.min(dt, 100);
  if (mode == 'menu' && !mDrag && mVel) {     // the list glides on after a flick
    mScroll += mVel * dt; mVel *= Math.pow(0.997, dt);
    if (Math.abs(mVel) < 0.02) mVel = 0;
    var mx = menuMax(); if (mScroll < 0) { mScroll = 0; mVel = 0; } if (mScroll > mx) { mScroll = mx; mVel = 0; }
  }
  var a = autoTime();
  if (mode == 'run' && a && NOW - runStart > a * 1000) nextSaver(1);
  fpsCount++;
  if (NOW - fpsAt >= 1000) { fpsNow = Math.round(fpsCount * 1000 / (NOW - fpsAt)); fpsCount = 0; fpsAt = NOW; }
}
function draw() {
  SAVERS[CUR].step(S, DT);
  if (!KEEP) overlay();                   // older firmware: no overlay() pass, so the menus go here
}
function overlay() {
  if (mode == 'menu') drawMenu();
  else if (mode == 'run') drawRun();
  else if (mode == 'tune') drawTune();
  else drawOpts();
  if (OPT.stats) text(fpsNow + ' FPS', 474, mode == 'run' && NOW >= hudUntil ? 444 : 44, WARN, 1, 2, false);
}
function touch(t, x, y) {
  if (mode == 'menu') {
    if (t == 'down') { mDrag = y < 418 && y >= LIST_Y ? { y: y, s: mScroll, ly: y, lt: NOW, moved: false } : null; mVel = 0; return; }
    if (t == 'move' && mDrag) {
      var dtm = Math.max(1, NOW - mDrag.lt);
      mVel = mVel * 0.4 + ((mDrag.ly - y) / dtm) * 0.6; mDrag.ly = y; mDrag.lt = NOW; mDrag.moved = true;
      mScroll = clamp(mDrag.s - (y - mDrag.y), 0, menuMax());
      return;
    }
    if (t == 'up') { if (mDrag && NOW - mDrag.lt > 90) mVel = 0; mDrag = null; return; }
    if (t == 'tap') {
      if (y >= 418) { if (x < 240) { slideshow = true; play(sel); slideshow = true; } else go('opts'); beep('key'); return; }
      if (y < LIST_Y) return;
      var r = Math.floor((y - LIST_Y + mScroll) / RH);
      if (r >= 0 && r < ROWS.length && ROWS[r].s !== undefined) { beep('key'); play(ROWS[r].s); }
    }
    return;
  }
  if (mode == 'run') {
    if (t == 'down') dragUsed = false;
    var sv = SAVERS[CUR];
    if (sv.touch && (t == 'down' || t == 'move' || t == 'up') && NOW >= hudUntil) { if (sv.touch(S, t, x, y)) dragUsed = true; }
    if (t == 'tap') {
      if (NOW < hudUntil) {
        if (y >= 414) {
          if (x < 96) { nextSaver(-1); hudUntil = NOW + 4000; }
          else if (x < 240) go('menu');
          else if (x < 384) go('tune');
          else { nextSaver(1); hudUntil = NOW + 4000; }
          beep('key');
        } else hudUntil = 0;
      } else hudUntil = NOW + 4000;
    }
    return;
  }
  if (mode == 'tune') {
    if (y >= 436 && t == 'tap') { if (x < 160) tuneRandom(); else if (x < 320) tuneReset(); else tuneDone(); return; }
    panelTouch(t, x, y);
    return;
  }
  if (mode == 'opts') {
    if (y >= 436 && t == 'tap') {
      if (x < 240) { for (var i = 0; i < OPTS.length; i++) OPT[OPTS[i].k] = OPTS[i].def; ALLP = {}; save('p', ALLP); applyOpts(); startSaver(CUR, 0); beep('ok'); }
      else { save('o', OPT); go('menu'); }
      return;
    }
    panelTouch(t, x, y);
  }
}
function swipe(d) {
  if (mode != 'run' || dragUsed) return;
  if (d == 'left') nextSaver(1);
  else if (d == 'right') nextSaver(-1);
  else if (d == 'up') go('tune');
  else if (d == 'down') go('menu');
}
function key(k) {
  if (mode == 'menu') {
    if (k == 'next' || k == 'prev') {
      var r = rowOf(sel);
      do { r = (r + (k == 'next' ? 1 : ROWS.length - 1)) % ROWS.length; } while (ROWS[r].s === undefined);
      sel = ROWS[r].s; startSaver(sel, 0); menuShowSel(); beep('move'); return true;
    }
    if (k == 'ok') { play(sel); return true; }
    return false;                           // BACK: leave the app
  }
  if (mode == 'run') {
    if (k == 'next') { nextSaver(1); return true; }
    if (k == 'prev') { nextSaver(-1); return true; }
    if (k == 'ok') { go('tune'); return true; }
    if (k == 'back') { go('menu'); return true; }
    return false;
  }
  if (mode == 'tune') { if (k == 'back') { tuneDone(); return true; } return panelKey(k); }
  if (mode == 'opts') { if (k == 'back') { save('o', OPT); go('menu'); return true; } return panelKey(k); }
  return false;
}
