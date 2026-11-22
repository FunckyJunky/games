// pocket.DOS MENU - the apps on a 3D carousel, ERROR-DOS style: green on black, glitches, wireframe icons.
// TAP = next, HOLD = open (keyboard: arrows + ENTER). The board and error.blue/pocket both run this file.
var A = [], N = 0, sel = 0, pos = 0, T = 0, S = null;
var downAt = -1, holdDone = false, keyAt = -99, launch = -1, launchT = 0;
var gl = 0, nextGl = 90, boot = 0;
var BG = 0x031208, FG = 0x39ff7a, DIM = 0x1b7a3c, HI = 0xb8ffd0, DARK = 0x0a2a14, DEEP = 0x0e3a1c;
// the camera and the carousel
var F = 70, D = 96, R = 46, CYS = 37, SC = 12;
var ocol = FG;                                   // the colour of the icon being drawn
// 3D lines are drawn by the board itself (cam3/xf3/line3/proj3); older boards: the same in JavaScript
if (typeof line3 != 'function') {
  var P3 = { x: 0, y: 0, z: 0, cy: 1, sy: 0, cp: 1, sp: 0, cr: 1, sr: 0, f: 80, cx: 80, cyy: 43, ox: 0, oy: 0, oz: 0, oc: 1, os: 0, ok: 1, X: 0, Y: 0, Z: 0 };
  cam3 = function (x, y, z, yaw, pitch, roll, f, cx, cy) {
    P3.x = x; P3.y = y; P3.z = z; P3.cy = Math.cos(yaw || 0); P3.sy = Math.sin(yaw || 0); P3.cp = Math.cos(pitch || 0); P3.sp = Math.sin(pitch || 0);
    P3.cr = Math.cos(roll || 0); P3.sr = Math.sin(roll || 0); P3.f = f || 80; P3.cx = cx === undefined ? 80 : cx; P3.cyy = cy === undefined ? 43 : cy;
    P3.ox = P3.oy = P3.oz = 0; P3.oc = 1; P3.os = 0; P3.ok = 1;
  };
  xf3 = function (x, y, z, a, k) { P3.ox = x || 0; P3.oy = y || 0; P3.oz = z || 0; P3.oc = Math.cos(a || 0); P3.os = Math.sin(a || 0); P3.ok = k === undefined ? 1 : k; };
  var tc3 = function (x, y, z) {
    var lx = x * P3.oc + z * P3.os, lz = z * P3.oc - x * P3.os;
    var dx = P3.ox + lx * P3.ok - P3.x, dy = P3.oy + y * P3.ok - P3.y, dz = P3.oz + lz * P3.ok - P3.z;
    var x1 = dx * P3.cy - dz * P3.sy, z1 = dx * P3.sy + dz * P3.cy, y2 = dy * P3.cp - z1 * P3.sp;
    P3.Z = dy * P3.sp + z1 * P3.cp; P3.X = x1 * P3.cr - y2 * P3.sr; P3.Y = x1 * P3.sr + y2 * P3.cr;
  };
  line3 = function (ax, ay, az, bx, by, bz, c) {
    tc3(ax, ay, az); var X0 = P3.X, Y0 = P3.Y, Z0 = P3.Z; tc3(bx, by, bz); var X1 = P3.X, Y1 = P3.Y, Z1 = P3.Z, f;
    if (Z0 < 1 && Z1 < 1) return;
    if (Z0 < 1) { f = (1 - Z0) / (Z1 - Z0); X0 += (X1 - X0) * f; Y0 += (Y1 - Y0) * f; Z0 = 1; }
    else if (Z1 < 1) { f = (1 - Z1) / (Z0 - Z1); X1 += (X0 - X1) * f; Y1 += (Y0 - Y1) * f; Z1 = 1; }
    line(P3.cx + X0 * P3.f / Z0, P3.cyy - Y0 * P3.f / Z0, P3.cx + X1 * P3.f / Z1, P3.cyy - Y1 * P3.f / Z1, c);
  };
  proj3 = function (x, y, z) { tc3(x, y, z); return P3.Z < 0.05 ? null : [P3.cx + P3.X * P3.f / P3.Z, P3.cyy - P3.Y * P3.f / P3.Z, P3.Z]; };
}

function init() {
  holdexit(false);
  var L = apps();
  for (var i = 0; i < L.length; i++) A.push(L[i]);
  A.push({ id: 'setup', name: 'SETUP', info: 'WIFI + BLUETOOTH', icon: 'setup', cached: true, sys: true });
  N = A.length;
  S = sys();
  if (S.last >= 0 && S.last < N) sel = S.last;
  pos = sel;
}

// ------------------------------------------------------------------ 3D
function seg(ax, ay, az, bx, by, bz) { line3(ax, ay, az, bx, by, bz, ocol); }
function box(x, y, z, w, h, d) {
  var a = x - w, b = x + w, c = y - h, e = y + h, f = z - d, g = z + d;
  seg(a, c, f, b, c, f); seg(b, c, f, b, e, f); seg(b, e, f, a, e, f); seg(a, e, f, a, c, f);
  seg(a, c, g, b, c, g); seg(b, c, g, b, e, g); seg(b, e, g, a, e, g); seg(a, e, g, a, c, g);
  seg(a, c, f, a, c, g); seg(b, c, f, b, c, g); seg(b, e, f, b, e, g); seg(a, e, f, a, e, g);
}
function poly(cx, cy, cz, r, n, ax, a0) {      // a ring: ax 0 = flat (in x-z), 1 = standing (x-y)
  var px = 0, py = 0, pz = 0;
  for (var i = 0; i <= n; i++) {
    var a = a0 + i * 6.2832 / n, c = Math.cos(a) * r, s = Math.sin(a) * r;
    var x = cx + c, y = ax ? cy + s : cy, z = ax ? cz : cz + s;
    if (i) seg(px, py, pz, x, y, z);
    px = x; py = y; pz = z;
  }
}

// ------------------------------------------------------------------ the icons (unit size, animated)
var CP = [-0.35, -1, -1, 0.35, -1, -1, 0.35, 1, -1, -0.35, 1, -1, -0.35, -1, 1, 0.35, -1, 1, 0.35, 1, 1, -0.35, 1, 1];
var CE = [0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7], QY = [], QZ = [];
var pc = 1, ps = 0, pb = 0;
function pp(x, y, z, x2, y2, z2) { seg(x, pb + y * pc - z * ps, y * ps + z * pc, x2, pb + y2 * pc - z2 * ps, y2 * ps + z2 * pc); }
var ICONS = {
  bird: function (t) {
    var w = Math.sin(t * 9) * 0.7, b = Math.sin(t * 4) * 0.12;
    seg(-0.7, b, 0, 0.6, b, 0); seg(0.6, b, 0, 0.95, b - 0.12, 0);
    seg(0.2, b, 0, -0.15, b + w, 0.95); seg(-0.15, b + w, 0.95, -0.4, b, 0);
    seg(0.2, b, 0, -0.15, b + w, -0.95); seg(-0.15, b + w, -0.95, -0.4, b, 0);
    seg(-0.7, b, 0, -1, b + 0.25, 0.25); seg(-0.7, b, 0, -1, b + 0.25, -0.25);
    poly(0.45, b + 0.12, 0, 0.18, 5, 1, t);
  },
  run: function (t) {
    var p = t * 8, s = Math.sin(p), c = Math.cos(p), b = Math.abs(c) * 0.1;
    poly(0, 0.82 + b, 0, 0.17, 6, 1, 0);
    seg(0, 0.65 + b, 0, 0, b, 0);
    seg(0, b, 0, s * 0.3, -0.4 + b, 0.1); seg(s * 0.3, -0.4 + b, 0.1, s * 0.55, -0.85, 0.1);
    seg(0, b, 0, -s * 0.3, -0.4 + b, -0.1); seg(-s * 0.3, -0.4 + b, -0.1, -s * 0.55, -0.85, -0.1);
    seg(0, 0.5 + b, 0, -s * 0.35, 0.15 + b, 0.15); seg(0, 0.5 + b, 0, s * 0.35, 0.15 + b, -0.15);
  },
  stack: function (t) {
    box(0, -0.75, 0, 0.75, 0.18, 0.5);
    box(0.08, -0.35, 0, 0.62, 0.18, 0.5);
    box(Math.sin(t * 2.2) * 0.45, 0.05, 0, 0.5, 0.18, 0.5);
    var y = 0.95 - ((t * 0.7) % 1) * 0.7;
    seg(-0.3, y, 0, 0.3, y, 0);
  },
  flip: function (t) {
    var a = t * 3, c = Math.cos(a) * 0.35, s = Math.sin(a) * 0.35, y = Math.sin(t * 1.5) * 0.45;
    // a cube turning over (around x)
    for (var i = 0; i < 8; i++) { QY[i] = y + CP[i * 3 + 1] * c - CP[i * 3 + 2] * s; QZ[i] = CP[i * 3 + 1] * s + CP[i * 3 + 2] * c; }
    for (i = 0; i < 24; i += 2) { var a0 = CE[i], b0 = CE[i + 1]; seg(CP[a0 * 3], QY[a0], QZ[a0], CP[b0 * 3], QY[b0], QZ[b0]); }
    for (i = -2; i < 2; i++) {
      seg(i * 0.4, -1, 0, i * 0.4 + 0.2, -0.75, 0); seg(i * 0.4 + 0.2, -0.75, 0, i * 0.4 + 0.4, -1, 0);
      seg(i * 0.4, 1, 0, i * 0.4 + 0.2, 0.75, 0); seg(i * 0.4 + 0.2, 0.75, 0, i * 0.4 + 0.4, 1, 0);
    }
  },
  orbit: function (t) {
    var r = 0.3;
    seg(0, r, 0, r, 0, 0); seg(r, 0, 0, 0, -r, 0); seg(0, -r, 0, -r, 0, 0); seg(-r, 0, 0, 0, r, 0);
    seg(0, r, 0, 0, 0, r); seg(0, 0, r, 0, -r, 0); seg(0, r, 0, 0, 0, -r); seg(0, 0, -r, 0, -r, 0);
    poly(0, 0, 0, 0.95, 12, 0, 0);
    poly(0, 0, 0, 0.6, 8, 0, 0);
    var a = t * 2.5;
    box(Math.cos(a) * 0.95, 0, Math.sin(a) * 0.95, 0.12, 0.12, 0.12);
  },
  plane: function (t) {
    var r = Math.sin(t * 1.3) * 0.5, p = pp;
    pc = Math.cos(r); ps = Math.sin(r); pb = Math.sin(t * 2) * 0.1;      // the plane rolls (around x)
    p(-1, 0, 0, 1, 0, 0); p(1, 0, 0, 0.75, 0.12, 0);
    p(0.3, 0, 0, -0.15, 0, 1); p(-0.15, 0, 1, -0.35, 0, 1); p(-0.35, 0, 1, -0.2, 0, 0);
    p(0.3, 0, 0, -0.15, 0, -1); p(-0.15, 0, -1, -0.35, 0, -1); p(-0.35, 0, -1, -0.2, 0, 0);
    p(-0.8, 0, 0, -1, 0.45, 0); p(-1, 0.45, 0, -1, 0, 0);
    p(-0.85, 0, 0, -1, 0, 0.35); p(-0.85, 0, 0, -1, 0, -0.35);
  },
  chat: function (t) {
    var a = Math.sin(t * 3) * 0.08, b = Math.sin(t * 3 + 2) * 0.08;
    // two speech bubbles, one behind the other
    var y = 0.25 + a;
    seg(-0.9, y, 0.2, 0.3, y, 0.2); seg(0.3, y, 0.2, 0.3, y + 0.6, 0.2); seg(0.3, y + 0.6, 0.2, -0.9, y + 0.6, 0.2);
    seg(-0.9, y + 0.6, 0.2, -0.9, y, 0.2); seg(-0.6, y, 0.2, -0.75, y - 0.3, 0.2); seg(-0.75, y - 0.3, 0.2, -0.4, y, 0.2);
    y = -0.55 + b;
    seg(-0.3, y, -0.2, 0.9, y, -0.2); seg(0.9, y, -0.2, 0.9, y + 0.6, -0.2); seg(0.9, y + 0.6, -0.2, -0.3, y + 0.6, -0.2);
    seg(-0.3, y + 0.6, -0.2, -0.3, y, -0.2); seg(0.6, y, -0.2, 0.75, y - 0.3, -0.2); seg(0.75, y - 0.3, -0.2, 0.4, y, -0.2);
    var k = (t * 3 | 0) % 4;
    for (var i = 0; i < 3; i++) if (i < k) seg(-0.65 + i * 0.25, 0.55 + a, 0.2, -0.55 + i * 0.25, 0.55 + a, 0.2);
  },
  setup: function (t) {
    var k = (t * 3 | 0) % 4;
    box(0, -0.75, 0, 0.1, 0.1, 0.1);
    for (var i = 1; i <= 3; i++) {
      if (i > k) continue;
      var r = i * 0.38, px = 0, py = 0;
      for (var j = 0; j <= 6; j++) {
        var a = 0.8 + j * 0.26, x = Math.cos(a) * r, y = -0.75 + Math.sin(a) * r;
        if (j) seg(-px, py, 0, -x, y, 0);
        px = x; py = y;
      }
    }
  },
  cube: function (t) { box(0, Math.sin(t * 2) * 0.15, 0, 0.6, 0.6, 0.6); }
};

// ------------------------------------------------------------------ input
function next(d) { sel = (sel + d + N) % N; gl = 2; }
function open() {
  if (launch >= 0) return;
  launch = sel; launchT = 0;
}

function update() {
  T++;
  if (boot < 30) boot++;
  if (T % 45 == 0) S = sys();
  var k = key();
  while (k) {
    keyAt = T;
    if (k == 'RIGHT' || k == 'DOWN' || k == 'TAB') next(1);
    else if (k == 'LEFT' || k == 'UP') next(-1);
    else if (k == 'ENTER') open();
    k = key();
  }
  if (launch >= 0) {
    if (++launchT > 14) { var idx = launch; launch = -1; if (A[idx].sys) setup(); else run(idx); }
  } else if (T - keyAt > 8) {                  // the button (a keyboard key also "presses" it: skip those)
    if (btnp()) { downAt = T; holdDone = false; }
    if (btn() && downAt >= 0 && !holdDone && T - downAt > 20) { holdDone = true; open(); }
    if (btnr()) { if (downAt >= 0 && !holdDone && T - downAt < 14) next(1); downAt = -1; }
  }
  // the carousel turns the short way round to the chosen app
  var d = sel - pos;
  if (d > N / 2) d -= N;
  if (d < -N / 2) d += N;
  pos += d * 0.22;
  if (Math.abs(d) < 0.01) pos = sel;
  if (pos < 0) pos += N;
  if (pos >= N) pos -= N;
  if (T >= nextGl) { gl = 3 + irnd(3); nextGl = T + 90 + irnd(150); }
}

// ------------------------------------------------------------------ drawing
function wifiBars(x, y) {
  var n = !S.online ? 0 : S.rssi > -60 ? 4 : S.rssi > -70 ? 3 : S.rssi > -80 ? 2 : 1;
  for (var i = 0; i < 4; i++) rect(x + i * 2, y + 6 - (i + 1) * 1.5 - 1 | 0, 1, (i + 1) * 1.5 + 1 | 0, i < n ? BG : 0x2a8a4c);
}
function ctext(s, y, c, k) { k = k || 1; text(s, (W - textw(s, k)) >> 1, y, c, k); }

function draw() {
  cls(BG);
  var t = T / 30, i;
  // the DOS bar
  rect(0, 0, W, 9, FG);
  text(S.web ? 'POCKET.DOS  WEB' : 'POCKET.DOS', 2, 1, BG);
  if (S.web) { /* the browser: no WiFi to show */ }
  else if (!S.online) text('OFFLINE', W - 12 - textw('OFFLINE'), 1, BG);
  else { var w = S.wifi.length > 12 ? S.wifi.substr(0, 12) : S.wifi; text(w, W - 13 - textw(w), 1, BG); }
  if (!S.web) wifiBars(W - 10, 1);
  // the camera looks at the carousel; its floor is a dotted ring
  cam3(0, 0, -D, 0, 0, 0, F, 80, CYS);
  var rot = -pos * 6.2832 / N;
  for (i = 0; i < 28; i++) {
    var a = rot + i * 6.2832 / 28, x = Math.sin(a) * R, z = -Math.cos(a) * R;
    line3(x, -17, z, x, -17, z, z < 0 ? DIM : DEEP);
  }
  // the icons, the far ones first
  var order = [];
  for (i = 0; i < N; i++) {
    var ang = (i - pos) * 6.2832 / N;
    order.push({ i: i, x: Math.sin(ang) * R, c: Math.cos(ang) });
  }
  order.sort(function (p, q) { return p.c - q.c; });
  for (i = 0; i < order.length; i++) {
    var o = order[i], front = o.i == sel && o.c > 0.9;
    if (o.c < -0.55) continue;                     // the ones at the back are hidden anyway
    var spin = front ? t * 1.3 : t * 0.4 + o.i * 1.7;
    xf3(o.x, 0, -R * o.c, spin, SC * (front ? 1.05 + (launch >= 0 ? launchT * 0.12 : 0) : 1));   // the front is nearest
    ocol = front ? (launch >= 0 && launchT & 2 ? HI : FG) : o.c > 0.3 ? DIM : DEEP;
    var f = ICONS[A[o.i].icon] || ICONS[A[o.i].id] || ICONS.cube;
    f(t + o.i);
  }
  // the chosen one: its name, what it is, the hold bar
  var a0 = A[sel];
  var nm = a0.name, k2 = textw(nm, 2) <= W - 8 ? 2 : 1;
  var ny = k2 == 2 ? 60 : 63;
  text(nm, ((W - textw(nm, k2)) >> 1) + 1, ny + 1, DARK, k2);
  ctext(nm, ny, launch >= 0 ? HI : FG, k2);
  var info = !a0.cached && !S.online ? 'NEEDS WIFI' : a0.info;
  if ((T / 75 | 0) % 3 == 2 && launch < 0) info = 'TAP: NEXT  HOLD: OPEN';
  ctext(String(info).substr(0, 26), 77, DIM);
  if (downAt >= 0 && !holdDone && T - downAt > 4) {
    var p = Math.min(1, (T - downAt - 4) / 16);
    rect(20, 74, (W - 40) * p | 0, 1, HI);
  }
  // little arrows: there's more on both sides
  text('<', 3, 34, (T >> 3) & 1 ? FG : DIM); text('>', W - 8, 34, (T >> 3) & 1 ? FG : DIM);
  // a moving scan line, like an old screen
  var sl = (T * 2) % 120 - 20;
  if (sl > 10 && sl < 72) rect(0, sl, W, 1, DARK);
  // glitches: now and then, when it turns, when an app opens, and when the menu starts
  if (boot < 12) glitch(10 - boot * 0.7 | 0);
  if (launch >= 0) glitch(Math.min(10, 2 + launchT));
  else if (gl > 0) { glitch(gl); gl--; }
}
