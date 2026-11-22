// NEON FLIGHT for pocket.DOS - fly through the 8 rings around the neon city before the time runs out.
// With a motion sensor (the board, a phone): tilt to steer, HOLD = gas. Calibrate once on the title screen.
// In a browser: the arrow keys tilt, SPACE = gas. No sensor at all: the plane steers itself, HOLD = climb.
var MK = typeof motion == 'function' ? motion() : 0;     // 0 none, 1 keys, 2 sensor
var ST = 0, T = 0, sel = 0, items = [];
var x = 0, y = 30, z = 0, yaw = 0, pit = 0, rol = 0, spd = 1, thr = 0.5;
var ringI = 0, rings = 0, left = 0, best = 0, flash = 0, crashT = 0, side = 0, lap = 0, msg = '', msgT = 0;
var inv = 1, calOk = false, calStep = 0, calWarn = 0;
var CN = [0, 0, 1], CR = [1, 0, 0], CD = [0, 1, 0];       // the calibration: level, right, nose down
var N = [0, 0, 1], RR = [1, 0, 0], DD = [0, 1, 0];        // the same, around how you hold it now
var sx = 0, sy = 0;
// the camera
var F = 80, camX = 0, camY = 0, camZ = 0, cyw = 1, syw = 0, cp = 1, sp = 0, cr = 1, sr = 0, qx = 0, qy = 0, qz = 0;
// the world
var RINGS = [], TOW = [], STARS = [], MNT = [], NR = 8, RR0 = 135, RAD = 9;

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

function h2(i, j) { var s = Math.sin(i * 127.1 + j * 311.7) * 43758.5453; return s - Math.floor(s); }

function init() {
  holdexit(false);
  best = load('best', 0);
  inv = load('inv', 1);
  calOk = load('cal', 0) == 1;
  if (calOk) for (var i = 0; i < 3; i++) { CN[i] = load('n' + i, CN[i]); CR[i] = load('r' + i, CR[i]); CD[i] = load('d' + i, CD[i]); }
  // 8 rings on a circle around the city, each standing across the way round
  for (i = 0; i < NR; i++) {
    var a = i * 6.2832 / NR, cx = Math.sin(a) * RR0, cz = Math.cos(a) * RR0, cy = 18 + (i % 3) * 12 + (i % 2) * 6;
    var nx = Math.cos(a), nz = -Math.sin(a);              // the way round (the ring faces it)
    var pts = [];
    for (var k = 0; k < 12; k++) {
      var b = k * 6.2832 / 12, u = Math.cos(b) * RAD, v = Math.sin(b) * RAD;
      pts.push(cx + nz * v, cy + u, cz - nx * v);
    }
    RINGS.push({ x: cx, y: cy, z: cz, nx: nx, nz: nz, p: pts });
  }
  // the city: towers on a 7 x 7 grid
  for (i = -3; i <= 3; i++) for (var j = -3; j <= 3; j++) {
    if (h2(i, j) > 0.55 || (i == 0 && j == 0)) continue;
    var w = 4 + h2(j, i) * 5, ht = 10 + h2(i + 7, j - 3) * 34;
    TOW.push({ x: i * 30, z: j * 30, w: w, h: ht, c: [12, 14, 15, 13][(h2(i * 3, j * 5) * 4) | 0] });
  }
  for (i = 0; i < 22; i++) { var az = h2(i, 1) * 6.2832, el = 0.06 + h2(1, i) * 1.1; STARS.push(Math.cos(el) * Math.sin(az), Math.sin(el), Math.cos(el) * Math.cos(az)); }
  for (i = 0; i < 32; i++) { var ma = i * 6.2832 / 32, mh = i % 2 ? 0.02 + h2(i, 9) * 0.03 : 0.06 + h2(9, i) * 0.09; MNT.push(Math.sin(ma), mh, Math.cos(ma)); }
  menu();
  reset();
}
function menu() {
  items = ['FLY'];
  if (MK == 2) items.push('CALIBRATE');
  if (MK > 0) items.push(inv > 0 ? 'PITCH: NORMAL' : 'PITCH: INVERTED');
  items.push('EXIT');
  if (sel >= items.length) sel = 0;
}
function reset() {
  var a = -0.55, r = RR0;                                // start a little before ring 1
  x = Math.sin(a) * r; z = Math.cos(a) * r; y = 26; yaw = a + 1.5708; pit = 0; rol = 0; spd = 1; thr = 0.5;
  ringI = 0; rings = 0; lap = 0; left = 45 * 30; side = sideOf(RINGS[0]);
}

// ------------------------------------------------------------------ the stick
function dot(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
function norm(a) { var l = Math.sqrt(dot(a, a)) || 1; a[0] /= l; a[1] /= l; a[2] /= l; return a; }
function flat(v, n) { var d = dot(v, n); return norm([v[0] - n[0] * d, v[1] - n[1] * d, v[2] - n[2] * d]); }
function up() { return [ax(), ay(), az()]; }
function level() {                                       // how you hold it now = level
  N = norm(up());
  RR = flat(CR, N);
  var d = flat(CD, N), k = dot(d, RR);
  DD = norm([d[0] - RR[0] * k, d[1] - RR[1] * k, d[2] - RR[2] * k]);
}
function dz(v) { v = v > 0 ? Math.max(0, v - 0.06) : Math.min(0, v + 0.06); return Math.max(-1, Math.min(1, v / 0.94)); }
function stick() {
  if (MK == 0) return;
  var g = up();
  sx = dz(dot(g, RR) / 0.5);
  sy = dz(-dot(g, DD) / 0.45) * inv;
}

// ------------------------------------------------------------------ flying
function sideOf(r) { return (x - r.x) * r.nx + (z - r.z) * r.nz; }
function update() {
  T++;
  if (flash > 0 && --flash == 0) led(0);
  if (msgT > 0) msgT--;
  if (ST == 0) { title(); return; }
  if (ST == 1) { calib(); return; }
  if (ST == 3) { crashT++; if (crashT > 30 && btnp()) { ST = 0; reset(); } return; }
  stick();
  var gas = btn();
  if (MK == 0) {                                          // one button: the plane steers to the ring, HOLD = climb
    var r = RINGS[ringI], want = Math.atan2(r.x - x, r.z - z), e = want - yaw;
    e = Math.atan2(Math.sin(e), Math.cos(e));
    sx = Math.max(-1, Math.min(1, e * 2.5));
    sy = gas ? 0.8 : -0.35;
    gas = true;
  }
  thr += ((gas ? 1 : 0.12) - thr) * 0.06;
  spd += (0.55 + thr * 1.35 - spd) * 0.025 - Math.sin(pit) * 0.035;
  if (spd < 0.3) spd = 0.3;
  var stall = spd < 0.5;
  rol += (sx * 1.05 - rol) * 0.12;
  if (stall) pit += (-0.5 - pit) * 0.05;
  else pit += (sy * 0.6 - pit) * 0.08;
  yaw += Math.sin(rol) * 0.05 * Math.min(1.3, spd);
  var c = Math.cos(pit);
  x += Math.sin(yaw) * c * spd; z += Math.cos(yaw) * c * spd; y += Math.sin(pit) * spd;
  if (y > 150) { y = 150; if (pit > 0) pit *= 0.5; }
  if (MK == 0 && y < 8) sy = 0.8;                         // the autopilot doesn't fly into the ground
  // the ring: through it = time + score, then the next one
  var r0 = RINGS[ringI], s = sideOf(r0);
  if (side < 0 && s >= 0) {
    var dx = x - r0.x, dy = y - r0.y, dzz = z - r0.z;
    if (dx * dx + dy * dy + dzz * dzz - s * s < RAD * RAD) {
      rings++; left += 8 * 30; led(10); flash = 6; msg = 'RING ' + rings; msgT = 30;
      ringI = (ringI + 1) % NR; if (ringI == 0) { lap++; msg = 'LAP ' + (lap + 1); }
    }
  }
  side = sideOf(RINGS[ringI]);
  // the ground and the towers
  if (y < 0.8) return crash('HIT THE GROUND');
  for (var i = 0; i < TOW.length; i++) {
    var t = TOW[i];
    if (y < t.h && Math.abs(x - t.x) < t.w && Math.abs(z - t.z) < t.w) return crash('HIT A TOWER');
  }
  if (--left <= 0) return crash('TIME IS UP');
}
function crash(why) {
  ST = 3; crashT = 0; msg = why; led(8); flash = 20;
  if (rings > best) { best = rings; save('best', best); }
}

// ------------------------------------------------------------------ title + calibration
function title() {
  // the camera circles the city
  var a = T * 0.006; x = Math.sin(a) * 175; z = Math.cos(a) * 175; y = 45 + Math.sin(T * 0.01) * 10;
  yaw = a + 1.5708 + 0.5; pit = -0.12; rol = 0.25;
  if (btnp()) { downT = T; held = false; }
  if (btn() && downT >= 0 && !held && T - downT > 18) { held = true; pick(); }
  if (btnr()) { if (downT >= 0 && !held) { sel = (sel + 1) % items.length; } downT = -1; }
  var k = key();
  if (k == 'DOWN' || k == 'RIGHT') sel = (sel + 1) % items.length;
  if (k == 'UP' || k == 'LEFT') sel = (sel + items.length - 1) % items.length;
  if (k == 'ENTER') pick();
}
var downT = -1, held = false;
function pick() {
  var it = items[sel];
  if (it == 'FLY') {
    if (MK == 2 && !calOk) { ST = 1; calStep = 0; return; }
    reset(); level(); ST = 2;
  } else if (it == 'CALIBRATE') { ST = 1; calStep = 0; }
  else if (it == 'EXIT') exit();
  else { inv = -inv; save('inv', inv); menu(); }
}
function calib() {
  var g = norm(up());
  if (calWarn > 0) calWarn--;
  if (btnp() || key() == 'ENTER') {
    if (calStep == 0) { CN = g; calStep = 1; }
    else {
      var d = flat(g, CN), dd = dot(g, CN);
      if (dd > 0.97) { calWarn = 40; return; }             // not tilted enough
      if (calStep == 1) { CR = d; calStep = 2; }
      else {
        var k = dot(d, CR); CD = norm([d[0] - CR[0] * k, d[1] - CR[1] * k, d[2] - CR[2] * k]);
        for (var i = 0; i < 3; i++) { save('n' + i, CN[i]); save('r' + i, CR[i]); save('d' + i, CD[i]); }
        save('cal', 1); calOk = true; ST = 0; msg = 'CALIBRATED'; msgT = 60;
      }
    }
  }
}

// ------------------------------------------------------------------ drawing
function cam() {
  // behind the plane and a bit above; it banks with the plane, but only partly
  var c = Math.cos(pit);
  camX = x - Math.sin(yaw) * c * 12; camZ = z - Math.cos(yaw) * c * 12; camY = y - Math.sin(pit) * 12 + 2.5;
  cyw = Math.cos(yaw); syw = Math.sin(yaw); cp = Math.cos(pit * 0.8); sp = Math.sin(pit * 0.8);
  cr = Math.cos(rol * 0.6); sr = Math.sin(rol * 0.6);
  cam3(camX, camY, camZ, yaw, pit * 0.8, rol * 0.6, F, 80, 43);
}
function tc(px, py, pz) {                                // world -> camera (qx, qy, qz)
  var dx = px - camX, dy = py - camY, dz2 = pz - camZ;
  var x1 = dx * cyw - dz2 * syw, z1 = dx * syw + dz2 * cyw;
  var y2 = dy * cp - z1 * sp; qz = dy * sp + z1 * cp;
  qx = x1 * cr - y2 * sr; qy = x1 * sr + y2 * cr;
}
function gf(X, Y) { var u = (X - 80) / F, v = (43 - Y) / F; return (-sr * u + cr * v) * cp + sp; }   // < 0: ground
function ground() {
  var C = [0, 0, 159, 0, 159, 85, 0, 85], P = [];
  for (var i = 0; i < 4; i++) {
    var ax1 = C[i * 2], ay1 = C[i * 2 + 1], bx = C[(i * 2 + 2) % 8], by = C[(i * 2 + 3) % 8];
    var fa = gf(ax1, ay1), fb = gf(bx, by);
    if (fa <= 0) P.push(ax1, ay1);
    if ((fa < 0) != (fb < 0)) { var k = fa / (fa - fb); P.push(ax1 + (bx - ax1) * k, ay1 + (by - ay1) * k); }
  }
  for (i = 2; i + 3 < P.length; i += 2) tri(P[0], P[1], P[i], P[i + 1], P[i + 2], P[i + 3], 0x12042a);
  // the horizon glows
  if (P.length >= 4) for (i = 0; i < P.length; i += 2) {
    var j = (i + 2) % P.length;
    if (Math.abs(gf(P[i], P[i + 1])) < 0.002 && Math.abs(gf(P[j], P[j + 1])) < 0.002) line(P[i], P[i + 1], P[j], P[j + 1], 14);
  }
}
function draw() {
  cam();
  cls(0x06021a);
  var i, k;
  // far away things: stars, the sun, mountains all around (always 600 away)
  for (i = 0; i < STARS.length; i += 3) { var sx3 = camX + STARS[i] * 600, sy3 = camY + STARS[i + 1] * 600, sz3 = camZ + STARS[i + 2] * 600; line3(sx3, sy3, sz3, sx3, sy3, sz3, i % 9 ? 5 : 6); }
  var sun = proj3(camX, camY + 0.13 * 600, camZ + 0.99 * 600);
  if (sun) { circ(sun[0], sun[1], 15, 0xff3a8a); circ(sun[0], sun[1], 12, 9); circ(sun[0], sun[1] - 2, 8, 10); }
  for (i = 0; i < MNT.length; i += 3) {
    k = (i + 3) % MNT.length;
    line3(camX + MNT[i] * 600, camY + MNT[i + 1] * 600, camZ + MNT[i + 2] * 600, camX + MNT[k] * 600, camY + MNT[k + 1] * 600, camZ + MNT[k + 2] * 600, 0x8a1a8a);
  }
  ground();
  // the grid on the ground, around the plane
  var gx = Math.round(x / 30) * 30, gz = Math.round(z / 30) * 30;
  for (i = -5; i <= 5; i++) {
    line3(gx + i * 30, 0, gz - 150, gx + i * 30, 0, gz + 150, 0x4a0a6a);
    line3(gx - 150, 0, gz + i * 30, gx + 150, 0, gz + i * 30, 0x4a0a6a);
  }
  // the towers: each corner turned once, then the 4 sides and the roof
  for (i = 0; i < TOW.length; i++) {
    var t = TOW[i], ddx = t.x - camX, ddz = t.z - camZ;
    var fwd = ddx * syw + ddz * cyw, lat = ddx * cyw - ddz * syw;     // ahead / to the side
    if (fwd < -t.w || fwd > 150 || Math.abs(lat) > fwd * 1.3 + t.w * 2) continue;
    var a = t.x - t.w, b = t.x + t.w, c = t.z - t.w, d = t.z + t.w, h = t.h, tcol = t.c;
    line3(a, 0, c, a, h, c, tcol); line3(b, 0, c, b, h, c, tcol); line3(b, 0, d, b, h, d, tcol); line3(a, 0, d, a, h, d, tcol);
    line3(a, h, c, b, h, c, tcol); line3(b, h, c, b, h, d, tcol); line3(b, h, d, a, h, d, tcol); line3(a, h, d, a, h, c, tcol);
  }
  // the next two rings
  for (k = 1; k >= 0; k--) {
    var r = RINGS[(ringI + k) % NR], p = r.p, col = k ? 0x5a4a10 : ((T >> 2) & 1 ? 10 : 9);
    if (ST == 0) col = 0x5a4a10;
    for (i = 0; i < 36; i += 3) { var j = (i + 3) % 36; line3(p[i], p[i + 1], p[i + 2], p[j], p[j + 1], p[j + 2], col); }
  }
  if (ST == 0) { drawTitle(); return; }
  if (ST == 1) { drawCalib(); return; }
  plane();
  hud();
  if (ST == 3) {
    glitch(crashT < 10 ? 9 - crashT : 0);
    rect(18, 20, 124, 46, 0); box(18, 20, 124, 46, 8);
    ct(msg, 25, 8, msg.length <= 10 ? 2 : 1);
    ct('RINGS ' + rings + '   BEST ' + best, 44, 7);
    if (crashT > 30 && (T >> 3) & 1) ct('TAP', 55, 6);
  }
}
function ct(s, yy, c, k) { k = k || 1; text(s, (W - textw(s, k)) >> 1, yy, c, k); }
function plane() {
  // the plane from behind: it banks the part the camera doesn't
  var b = rol * 0.4, c = Math.cos(b), s = Math.sin(b), px = 80, py = 60 - pit * 10;
  function P(u, v) { return [px + u * c - v * s, py + u * s + v * c]; }
  var L = P(-16, 1), R = P(16, 1), M = P(0, -2), U = P(0, -9), B = P(0, 2), TL = P(-6, -1), TR = P(6, -1);
  tri(L[0], L[1], R[0], R[1], M[0], M[1], 0x0a5a6a);
  line(L[0], L[1], M[0], M[1], 12); line(M[0], M[1], R[0], R[1], 12); line(L[0], L[1], R[0], R[1], 13);
  line(B[0], B[1], U[0], U[1], 12); line(TL[0], TL[1], TR[0], TR[1], 12);
  circ(B[0], B[1], 1 + (thr > 0.6 && (T & 1) ? 1 : 0), thr > 0.6 ? 10 : 9);
}
function hud() {
  text('RINGS ' + rings, 2, 2, 10);
  var s = (left / 30) | 0, tl = 'TIME ' + s;
  text(tl, W - 2 - textw(tl), 2, s < 10 && (T >> 3) & 1 ? 8 : 7);
  // speed (left) and height (right)
  rect(2, 30, 3, 40, 0x1a0a3a); var sh = Math.min(40, spd * 20) | 0; rect(2, 70 - sh, 3, sh, spd < 0.5 ? 8 : 11);
  text(String(Math.round(y)), W - 2 - textw(String(Math.round(y))), 76, 6);
  if (spd < 0.5 && (T >> 2) & 1) ct('STALL', 12, 8);
  if (msgT > 0) ct(msg, 14, 10);
  if (MK == 0 && T < 150) ct('HOLD = CLIMB', 76, 6);
  // where's the ring? an arrow at the edge when it's off the screen
  var r = RINGS[ringI];
  tc(r.x, r.y, r.z);
  var X = 80 + qx * F / Math.max(qz, 0.1), Y = 43 - qy * F / Math.max(qz, 0.1);
  if (qz < 1 || X < 0 || X > 159 || Y < 0 || Y > 85) {
    var a = Math.atan2(-qy, qx), ex = 80 + Math.cos(a) * 70, ey = 43 + Math.sin(a) * 34;
    tri(ex + Math.cos(a) * 5, ey + Math.sin(a) * 5, ex + Math.cos(a + 2.4) * 4, ey + Math.sin(a + 2.4) * 4, ex + Math.cos(a - 2.4) * 4, ey + Math.sin(a - 2.4) * 4, 10);
  }
}
function drawTitle() {
  rect(14, 8, 132, 22, 0); box(14, 8, 132, 22, 14);
  ct('NEON FLIGHT', 12, 12, 2);
  var hint = MK == 2 ? (calOk ? 'TILT TO FLY, HOLD = GAS' : 'CALIBRATE, THEN FLY') : MK == 1 ? 'ARROWS FLY, SPACE = GAS' : 'NO TILT: HOLD = CLIMB';
  ct(hint, 33, 6);
  for (var i = 0; i < items.length; i++) {
    var yy = 44 + i * 9, on = i == sel;
    if (on) rect(30, yy - 1, 100, 9, 14);
    ct(items[i], yy, on ? 0 : 7);
  }
  ct('BEST ' + best + '  TAP NEXT HOLD OK', 79, 5);
  if (msgT > 0) ct(msg, 34, 11);
}
function drawCalib() {
  rect(6, 8, 148, 72, 0); box(6, 8, 148, 72, 12);
  ct('CALIBRATE ' + (calStep + 1) + '/3', 12, 12);
  var t1 = ['HOLD IT LIKE A PLANE,', 'TILT IT TO THE RIGHT', 'TILT THE TOP AWAY'][calStep];
  var t2 = ['LEVEL', '(RIGHT WING DOWN)', '(NOSE DOWN)'][calStep];
  ct(t1, 26, 7); ct(t2, 36, 7);
  // which way is it tilted now (from the level you set)
  var g = norm(up());
  ct(calWarn > 0 ? 'TILT IT MORE!' : 'THEN TAP', 52, calWarn > 0 ? 8 : 10);
  var tl = Math.acos(Math.max(-1, Math.min(1, calStep ? dot(g, CN) : 1))) * 57.3 | 0;
  if (calStep > 0) ct('TILTED ' + tl + ' DEGREES', 64, 6);
}
