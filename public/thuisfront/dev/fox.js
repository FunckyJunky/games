// FOX for HOME FRONT: a small 3D fox living in a box (the screen), DOS green on black.
// The screen with a motion sensor runs foxStep() with it (tilt, turns, jolts), draws drawFox(), and sends
// foxPack(pose) to the phones 12 times a second while one watches; they draw the same fox (foxLerp between packets).
// The board's fox.cpp does the same (its mesh tables are made from this file by tools/hf-foxmesh.mjs).
import { T, fill, line, text8, textw } from './render.js';

const SW = 320, SH = 172;

// ---------------- the fox's shape: parts made of rings (lofts), in the part's own coordinates ----------------
// x = forward (nose), y = up, z = the fox's left. Each triangle: [a, b, c, edges (bit 0 = a-b, 1 = b-c, 2 = c-a), shade]
function ringAt(cx, cy, cz, hw, hh, n, a0) {        // n corners in the y-z plane around (cx, cy, cz)
  const out = [];
  for (let k = 0; k < n; k++) { const a = a0 + (k * 2 * Math.PI) / n; out.push([cx, cy + hh * Math.sin(a), cz + hw * Math.cos(a)]); }
  return out;
}
function ringY(cx, cy, cz, hx, hz, n, a0) {         // n corners in the x-z plane (for legs, which go down)
  const out = [];
  for (let k = 0; k < n; k++) { const a = a0 + (k * 2 * Math.PI) / n; out.push([cx + hx * Math.cos(a), cy, cz + hz * Math.sin(a)]); }
  return out;
}
const avg = (pts) => pts.reduce((s, p) => [s[0] + p[0] / pts.length, s[1] + p[1] / pts.length, s[2] + p[2] / pts.length], [0, 0, 0]);
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
// rings -> a closed tube; shades[i] = shade of the band after ring i; triangles turned to face outwards
function loft(rings, shades, capA = true, capB = true, capShade = [0, 0]) {
  const v = [], t = [], n = rings[0].length;
  for (const r of rings) v.push(...r);
  const add = (a, b, c, e, s, inside) => {
    const nrm = cross(sub(v[b], v[a]), sub(v[c], v[a])), ctr = avg([v[a], v[b], v[c]]);
    if (dot(nrm, sub(ctr, inside)) < 0) t.push([a, c, b, ((e & 1) << 2) | (e & 2) | ((e >> 2) & 1), s]);
    else t.push([a, b, c, e, s]);
  };
  for (let i = 0; i + 1 < rings.length; i++) {
    const inside = avg([...rings[i], ...rings[i + 1]]);
    for (let k = 0; k < n; k++) {
      const a = i * n + k, b = i * n + ((k + 1) % n), c = (i + 1) * n + ((k + 1) % n), d = (i + 1) * n + k;
      add(a, b, c, 0b011, shades[i], inside);          // a-b (ring) and b-c (side) outlined, c-a (diagonal) not
      add(a, c, d, 0b110, shades[i], inside);          // c-d (ring) and d-a (side)
    }
  }
  const cap = (ri, nextRi, s) => {
    const c = v.length; v.push(avg(rings[ri]));
    const inside = avg(rings[nextRi]);
    for (let k = 0; k < n; k++) add(ri * n + k, ri * n + ((k + 1) % n), c, 0b001, s, inside);
  };
  if (capA) cap(0, 1, capShade[0]);
  if (capB) cap(rings.length - 1, rings.length - 2, capShade[1]);
  return { v, t };
}
function tetra(base, tip, shade) {                   // an ear: 3 base corners and a tip
  const v = [...base, tip], t = [], inside = avg(v);
  for (const [a, b] of [[0, 1], [1, 2], [2, 0]]) {
    const nrm = cross(sub(v[b], v[a]), sub(v[3], v[a]));
    if (dot(nrm, sub(avg([v[a], v[b], v[3]]), inside)) < 0) t.push([a, 3, b, 0b111, shade]); else t.push([a, b, 3, 0b111, shade]);
  }
  return { v, t };
}
const HEX = Math.PI / 2;                            // hexagon rings start at the top
const BODY = loft([
  ringAt(-0.62, 0.54, 0, 0.12, 0.12, 6, HEX),
  ringAt(-0.45, 0.56, 0, 0.23, 0.21, 6, HEX),
  ringAt(0.05, 0.57, 0, 0.24, 0.22, 6, HEX),
  ringAt(0.40, 0.62, 0, 0.21, 0.24, 6, HEX),
  ringAt(0.56, 0.73, 0, 0.13, 0.16, 6, HEX),
], [0, 0, 1, 3], true, true, [0, 3]);               // chest lighter
const HS = 1.25;                                    // the head, a bit big (cute)
const HEAD = loft([
  ringAt(-0.08 * HS, 0.06 * HS, 0, 0.12 * HS, 0.12 * HS, 6, HEX),
  ringAt(0.06 * HS, 0.07 * HS, 0, 0.17 * HS, 0.14 * HS, 6, HEX),        // cheeks
  ringAt(0.20 * HS, 0.02 * HS, 0, 0.10 * HS, 0.08 * HS, 6, HEX),
  ringAt(0.33 * HS, -0.01 * HS, 0, 0.04 * HS, 0.04 * HS, 6, HEX),       // nose
], [0, 2, 1], true, true, [0, -5]);
const EAR_PIVOT = [0.02 * HS, 0.17 * HS, 0.10 * HS];
const EAR = tetra([[0.08, 0, -0.035], [-0.06, 0, -0.025], [0.0, -0.01, 0.08]], [0.0, 0.29, 0.045], 0);   // relative to its pivot (left ear)
const LEG = loft([
  ringY(0, 0, 0, 0.075, 0.068, 4, Math.PI / 4),
  ringY(0, -0.22, 0, 0.05, 0.048, 4, Math.PI / 4),
  ringY(0.01, -0.36, 0, 0.042, 0.042, 4, Math.PI / 4),
  ringY(0.045, -0.44, 0, 0.065, 0.052, 4, Math.PI / 4),   // paw
], [-1, -2, -3], true, true, [0, -3]);
const HIPS = [[0.36, 0.46, 0.11], [0.36, 0.46, -0.11], [-0.38, 0.48, 0.12], [-0.38, 0.48, -0.12]];   // FL FR BL BR
const LEG_PH = [0, Math.PI, Math.PI, 0];
const HEAD_PIVOT = [0.52, 0.86, 0];
const TAIL_PIVOT = [-0.60, 0.60, 0];
const TAIL_R = [[0.06, 0.06], [0.14, 0.13], [0.19, 0.17], [0.16, 0.15], [0.05, 0.05]];   // bushy, then the tip
const TAIL_L = [0.18, 0.25, 0.25, 0.22];
const TAIL_SHADE = [0, 0, 1, 4];                    // white tip
export const FOX_MESH = { BODY, HEAD, EAR, LEG, HIPS, LEG_PH, HEAD_PIVOT, TAIL_PIVOT, TAIL_R, TAIL_L, TAIL_SHADE, EAR_PIVOT, HS };

// ---------------- 3x4 matrices ----------------
const mI = () => [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0];
const mT = (x, y, z) => [1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z];
const mRx = (a) => { const c = Math.cos(a), s = Math.sin(a); return [1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0]; };
const mRy = (a) => { const c = Math.cos(a), s = Math.sin(a); return [c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0]; };
const mRz = (a) => { const c = Math.cos(a), s = Math.sin(a); return [c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0]; };
function mMul(a, b) {
  const o = new Array(12);
  for (let r = 0; r < 3; r++) {
    for (let c = 0; c < 4; c++) o[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c] + (c === 3 ? a[r * 4 + 3] : 0);
  }
  return o;
}
const mAp = (m, p) => [m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3], m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7], m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11]];

// ---------------- the camera: in front of the box, a bit from above ----------------
const CAM = [0, 1.9, 5.0], LOOK = [0, 0.42, -0.1], FOC = 370, CX = 160, CY = 100;
const CF = (() => { const f = sub(LOOK, CAM), l = Math.hypot(...f); return f.map((x) => x / l); })();
const CR = (() => { const r = cross(CF, [0, 1, 0]), l = Math.hypot(...r); return r.map((x) => x / l); })();
const CU = cross(CR, CF);
const LIGHT = (() => { const l = [-0.45, 0.85, 0.55], n = Math.hypot(...l); return l.map((x) => x / n); })();
function proj(p) {                                  // world -> [screen x, screen y, depth]
  const d = sub(p, CAM), z = dot(d, CF);
  return [CX + (FOC * dot(d, CR)) / z, CY - (FOC * dot(d, CU)) / z, z];
}
export const BOX = { x: 2.3, zb: -1.5, zf: 1.2, h: 2.2 };   // the floor: x -2.3..2.3, z -1.5 (back) .. 1.2 (front)
export const FOX_CAM = { CAM, CF, CR, CU, LIGHT, FOC, CX, CY, BOX };   // for tools/hf-foxmesh.mjs

// ---------------- filling ----------------
function tri(fb, a, b, c, col) {
  const ymin = Math.min(a[1], b[1], c[1]), ymax = Math.max(a[1], b[1], c[1]);
  const y0 = Math.max(0, Math.ceil(ymin - 0.5)), y1 = Math.min(SH - 1, Math.floor(ymax - 0.5));
  const E = [[a, b], [b, c], [c, a]];
  for (let y = y0; y <= y1; y++) {
    const yc = y + 0.5;
    let xl = 1e9, xr = -1e9;
    for (const [p, q] of E) {
      if ((p[1] <= yc && q[1] > yc) || (q[1] <= yc && p[1] > yc)) {
        const x = p[0] + ((yc - p[1]) * (q[0] - p[0])) / (q[1] - p[1]);
        if (x < xl) xl = x;
        if (x > xr) xr = x;
      }
    }
    if (xl > xr) continue;
    const x0 = Math.max(0, Math.ceil(xl - 0.5)), x1 = Math.min(SW - 1, Math.floor(xr - 0.5));
    if (x1 >= x0) fb.fill(col, y * SW + x0, y * SW + x1 + 1);
  }
}
const seg = (fb, p, q, c) => line(fb, Math.round(p[0]), Math.round(p[1]), Math.round(q[0]), Math.round(q[1]), c);

// ---------------- the pose (what the phone gets) ----------------
// px pz: where on the floor; hop: up; hd: which way it faces (0 = right, -pi/2 = to you); rl pt: lean (roll, pitch);
// tb: tumbling; lf: legs folded (0 stand, 1 sit, 2 lie); hy hp: head turn / nod; el er: ears (0 up, - back);
// ta tu: tail swing / raise; wp wa: walking (leg phase, stride); ey: eyes 0 open 1 shut 2 dizzy 3 wide;
// md: mood (FOX_MOODS); ov: 0, 1 zzz, 2 !, 3 stars
export const FOX_KEYS = ['px', 'pz', 'hop', 'hd', 'rl', 'pt', 'tb', 'lf', 'hy', 'hp', 'el', 'er', 'ta', 'tu', 'wp', 'wa', 'ey', 'md', 'ov'];
export const FOX_MOODS = ['CHILLING', 'CURIOUS', 'WALKING', 'SITTING', 'ZZZ...', 'HUH?', 'EEK!', 'WHOA!', 'HOLD ON!', 'HELP!', 'DIZZY...', 'OOF!', 'HELLO!'];
const ANGLES = new Set(['hd', 'tb', 'wp']);
const INTS = new Set(['ey', 'md', 'ov']);
export const foxPack = (p) => FOX_KEYS.map((k) => (INTS.has(k) ? p[k] | 0 : Math.round(p[k] * 100) / 100));
export function foxUnpack(a) {
  if (!Array.isArray(a) || a.length < FOX_KEYS.length) return null;
  const p = {};
  FOX_KEYS.forEach((k, i) => { const v = Number(a[i]); p[k] = Number.isFinite(v) ? v : 0; });
  return p;
}
const wrap = (a) => a - 2 * Math.PI * Math.round(a / (2 * Math.PI));
export function foxLerp(a, b, u) {                  // between two poses (angles the short way round)
  const p = {};
  for (const k of FOX_KEYS) p[k] = INTS.has(k) ? b[k] : ANGLES.has(k) ? a[k] + wrap(b[k] - a[k]) * u : a[k] + (b[k] - a[k]) * u;
  return p;
}

// ---------------- CENTRE: the screen's position now counts as level and still ----------------
// zero = [up x, y, z, gyro x, y, z] in screen directions, as the screen felt them when CENTRE was pressed.
// Turned into a rotation that brings that "up" to straight up (Rodrigues), and the gyro's resting value to take off.
export function foxZero(zero) {
  if (!Array.isArray(zero) || zero.length < 6) return null;
  const l = Math.hypot(zero[0], zero[1], zero[2]);
  if (!(l > 0.3)) return null;
  const a = [zero[0] / l, zero[1] / l, zero[2] / l], c = a[1];
  let R;
  if (c > 0.99999) R = [1, 0, 0, 0, 1, 0, 0, 0, 1];
  else if (c < -0.99999) R = [1, 0, 0, 0, -1, 0, 0, 0, -1];
  else {
    const v = [-a[2], 0, a[0]], k = (1 - c) / (1 - c * c);          // a x (0, 1, 0)
    const K = [0, -v[2], v[1], v[2], 0, -v[0], -v[1], v[0], 0];
    R = [];
    for (let r = 0; r < 3; r++) for (let q = 0; q < 3; q++) {
      let k2 = 0;
      for (let m = 0; m < 3; m++) k2 += K[r * 3 + m] * K[m * 3 + q];
      R.push((r === q ? 1 : 0) + K[r * 3 + q] + k2 * k);
    }
  }
  return { R, b: [+zero[3] || 0, +zero[4] || 0, +zero[5] || 0] };
}
export function foxZeroApply(Z, f, w) {
  if (!Z) return [f, w];
  const R = Z.R, ap = (v) => [R[0] * v[0] + R[1] * v[1] + R[2] * v[2], R[3] * v[0] + R[4] * v[1] + R[5] * v[2], R[6] * v[0] + R[7] * v[1] + R[8] * v[2]];
  return [ap(f), ap([w[0] - Z.b[0], w[1] - Z.b[1], w[2] - Z.b[2]])];
}

// ---------------- what the fox does ----------------
export function foxNew() {
  return {
    px: -0.2, pz: -0.1, vx: 0, vz: 0, hop: 0, vy: 0, hd: -0.65, rl: 0, pt: 0, tb: 0, tbv: 0, lf: 0, hy: 0, hp: 0, el: 0, er: 0,
    ta: 0, tu: 0.15, wp: 0, wa: 0, ey: 0, md: 12, ov: 0, t: 0, gl: [0, 1, 0], still: 0, startle: 0, dizzy: 0, wake: 0, hello: 2.5,
    blink: 2, look: 3, lookY: 0, lookP: 0, calm: 0, wander: 5, tx: 0, tz: 0, walking: false, falling: false, twitch: 4, twitchE: 0, bump: 0,
    sensor: false,
  };
}
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const ease = (cur, to, rate, dt) => cur + (to - cur) * (1 - Math.exp(-rate * dt));
const easeA = (cur, to, rate, dt) => cur + wrap(to - cur) * (1 - Math.exp(-rate * dt));
const headingTo = (dx, dz) => Math.atan2(-dz, dx);
// f: what the accelerometer feels, in g, in screen directions (x right, y up, z out of the screen), 1 g up when still;
// w: turning in rad/s (same directions); null = no sensor. rnd: random 0..1. dt: seconds.
export function foxStep(S, f, w, dt, rnd = Math.random) {
  dt = clamp(dt, 0, 0.1);
  S.t += dt;
  const has = !!f;
  if (has) S.sensor = true;
  f = f || [0, 1, 0]; w = w || [0, 0, 0];
  for (let i = 0; i < 3; i++) S.gl[i] = ease(S.gl[i], f[i], 6, dt);
  const gn = Math.hypot(...S.gl) || 1, up = S.gl.map((x) => x / gn);
  const jolt = Math.hypot(f[0] - S.gl[0], f[1] - S.gl[1], f[2] - S.gl[2]), spin = Math.hypot(...w);
  const dx = -up[0], dz = -up[2], slope = Math.hypot(dx, dz);   // downhill on the floor, and how steep (sin)
  // calm / sleep / wake
  const busy = jolt > 0.05 || spin > 0.35;
  if (busy) { if (S.lf > 1.5 && S.still > 2) { S.wake = 2; S.hello = 0; } S.still = 0; } else S.still += dt;
  const sleepy = S.sensor && S.still > 45;
  // dizzy from spinning and shaking
  S.dizzy = clamp(S.dizzy + (spin > 3 ? (spin - 3) * dt * 0.45 : 0) + (jolt > 0.8 ? dt * 1.3 : 0) - dt * 0.22, 0, 3);
  // startled by a knock or a jerk
  S.startle = Math.max(0, S.startle - dt); S.wake = Math.max(0, S.wake - dt); S.hello = Math.max(0, S.hello - dt); S.bump = Math.max(0, S.bump - dt);
  if (jolt > 0.55 && S.startle < 0.6) {
    S.startle = 1.6;
    if (S.hop < 0.05) S.vy = 1.6 + Math.min(1.5, jolt);
    S.vx -= (f[0] - S.gl[0]) * 2.2; S.vz -= (f[2] - S.gl[2]) * 2.2;   // it stays where it was: the box moves under it
    if (S.lf > 1.5) S.wake = 2;
  }
  // falling: the box on its side or upside down
  const wasFalling = S.falling;
  S.falling = up[1] < (S.falling ? 0.5 : 0.3);
  if (S.falling) {
    S.tbv = Math.min(9, S.tbv + dt * 10); S.tb += S.tbv * dt;
    S.vx += dx * 9.8 * dt * 0.8; S.vz += dz * 9.8 * dt * 0.8;
    S.hop = ease(S.hop, 0.35 + 0.15 * Math.sin(S.t * 7), 8, dt);
  } else if (wasFalling) {                          // it lands
    S.tbv = 0; S.dizzy = Math.min(3, S.dizzy + 1.3); S.vy = 1.8; S.bump = 0.6;
  }
  if (!S.falling) { S.tb = ease(S.tb, 2 * Math.PI * Math.round(S.tb / (2 * Math.PI)), 6, dt); }
  // moving over the floor: sliding down a slope, or walking about when calm
  const sliding = !S.falling && slope > 0.24;
  if (!S.falling) {
    if (sliding) { const k = (9.8 * (slope - 0.2)) / slope; S.vx += dx * k * dt; S.vz += dz * k * dt; }
    S.wander -= dt;
    const canWalk = !sliding && S.dizzy < 1 && S.startle <= 0 && !sleepy && S.lf < 0.5 && S.wake <= 0 && slope < 0.15;
    if (S.walking) {
      const ex = S.tx - S.px, ez = S.tz - S.pz, d = Math.hypot(ex, ez);
      if (!canWalk || d < 0.12) S.walking = false;
      else { S.vx = ease(S.vx, (ex / d) * 0.55, 4, dt); S.vz = ease(S.vz, (ez / d) * 0.55, 4, dt); }
    } else if (canWalk && S.wander <= 0 && S.calm < 8) {
      S.walking = true; S.tx = -1.6 + rnd() * 3.2; S.tz = -1.0 + rnd() * 1.6; S.wander = 6 + rnd() * 10;
    }
    if (!S.walking) { const fr = sliding ? 1.2 : 4; S.vx *= Math.exp(-fr * dt); S.vz *= Math.exp(-fr * dt); }
  }
  S.px += S.vx * dt; S.pz += S.vz * dt;
  const xl = BOX.x - 0.55, zb = BOX.zb + 0.45, zf = BOX.zf - 0.45;
  const hit = (v) => { if (Math.abs(v) > 1.2 && S.bump <= 0) { S.bump = 0.7; S.vy = Math.max(S.vy, 0.9); } return 0; };
  if (S.px < -xl) { S.px = -xl; S.vx = S.vx < 0 ? hit(S.vx) : S.vx; }
  if (S.px > xl) { S.px = xl; S.vx = S.vx > 0 ? hit(S.vx) : S.vx; }
  if (S.pz < zb) { S.pz = zb; S.vz = S.vz < 0 ? hit(S.vz) : S.vz; }
  if (S.pz > zf) { S.pz = zf; S.vz = S.vz > 0 ? hit(S.vz) : S.vz; }
  // hopping
  if (!S.falling) {
    S.vy -= 9.8 * dt; S.hop += S.vy * dt;
    if (S.hop <= 0) { S.hop = 0; S.vy = 0; }
  }
  const speed = Math.hypot(S.vx, S.vz);
  // sitting and sleeping
  if (!sliding && !S.walking && speed < 0.15 && S.startle <= 0 && !S.falling) S.calm += dt; else S.calm = 0;
  const lfTo = sleepy ? 2 : S.calm > 7 && S.dizzy < 1 ? 1 : 0;
  S.lf = ease(S.lf, lfTo, lfTo > S.lf ? 1.2 : 5, dt);
  // which way it faces: up the slope when sliding, where it walks, else turned a bit to you
  let hdTo;
  if (sliding) hdTo = headingTo(-dx, -dz);
  else if (S.walking && speed > 0.1) hdTo = headingTo(S.vx, S.vz);
  else hdTo = S.px > 0.4 ? -2.45 : S.px < -0.4 ? -0.7 : S.hd;
  S.hd -= w[1] * dt * 0.85;                         // the box turns under it
  if (!S.falling) S.hd = easeA(S.hd, hdTo, sliding ? 3 : S.walking ? 5 : 1.5, dt);
  S.hd = wrap(S.hd);
  // leaning against the slope (in its own directions)
  const fx = Math.cos(S.hd), fz = -Math.sin(S.hd);
  const along = dx * fx + dz * fz, side = dx * fz - dz * fx;
  S.pt = ease(S.pt, clamp(-along * 1.3, -0.5, 0.5), 6, dt);
  S.rl = ease(S.rl, clamp(side * 1.1, -0.5, 0.5) + (S.dizzy > 1 ? Math.sin(S.t * 3) * 0.12 : 0), 6, dt);
  // legs
  const waTo = S.falling ? 0.9 : sliding ? 0.75 : S.lf > 0.5 ? 0 : clamp(speed * 1.1, 0, 0.6);
  S.wa = ease(S.wa, waTo, 8, dt);
  S.wp = wrap(S.wp + dt * (sliding || S.falling ? 15 : 3 + speed * 12));
  // head: looks at you mostly, sometimes around; down when asleep
  S.look -= dt;
  if (S.look <= 0) { S.look = 2 + rnd() * 4; S.lookY = rnd() < 0.55 ? 0 : (rnd() - 0.5) * 1.6; S.lookP = (rnd() - 0.5) * 0.4; }
  const toCam = clamp(wrap(headingTo(CAM[0] - S.px, CAM[2] - S.pz) - S.hd), -1.1, 1.1);
  let hyTo = S.lookY === 0 ? toCam : S.lookY, hpTo = S.lookY === 0 ? 0.12 : S.lookP;
  if (sliding || S.falling) { hyTo = 0; hpTo = 0.25; }
  if (S.startle > 0 || S.wake > 0 || S.hello > 0) { hyTo = toCam; hpTo = 0.2; }
  if (S.lf > 1.2) { hyTo = 0.5; hpTo = -0.45; }
  if (S.dizzy > 1) hyTo += Math.sin(S.t * 2.6) * 0.3;
  S.hy = ease(S.hy, hyTo, 5, dt); S.hp = ease(S.hp, hpTo, 5, dt);
  // ears
  S.twitch -= dt;
  if (S.twitch <= 0) { S.twitch = 2 + rnd() * 5; S.twitchE = rnd() < 0.5 ? 1 : 2; }
  const earBase = S.startle > 0 ? -0.15 : sliding || S.falling || S.dizzy > 1 || S.bump > 0 ? -0.7 : S.lf > 1.2 ? -0.45 : 0;
  const tw = S.twitch < 0.25 ? -0.5 : 0;           // a quick flick just before the next one is picked
  S.el = ease(S.el, earBase + (S.twitchE === 1 ? tw : 0), 14, dt); S.er = ease(S.er, earBase + (S.twitchE === 2 ? tw : 0), 14, dt);
  // tail
  const taTo = S.lf > 1.2 ? 1.5 : S.walking ? Math.sin(S.wp) * 0.3 : S.hello > 0 || S.wake > 0 ? Math.sin(S.t * 12) * 0.5 : Math.sin(S.t * 1.4) * 0.35;
  const tuTo = S.startle > 0 ? 0.85 : sliding || S.falling ? 0.6 : S.lf > 1.2 ? -0.25 : S.lf > 0.5 ? -0.1 : 0.15;
  S.ta = ease(S.ta, taTo, 6, dt); S.tu = ease(S.tu, tuTo, 5, dt);
  // eyes
  S.blink -= dt;
  if (S.blink <= 0) S.blink = 2 + rnd() * 3.5;
  S.ey = S.lf > 1.5 ? 1 : S.dizzy > 1 ? 2 : S.startle > 0.6 ? 3 : S.blink < 0.13 ? 1 : 0;
  // the words in the top bar, and what floats above it
  S.md = S.falling ? 9 : S.bump > 0 ? 11 : S.startle > 0.4 ? 6 : S.dizzy > 1 ? 10 : sliding ? (slope > 0.45 ? 7 : 8) : S.wake > 0 ? 5
    : S.hello > 0 ? 12 : S.lf > 1.5 ? 4 : S.lf > 0.6 ? 3 : S.walking ? 2 : S.lookY === 0 ? 1 : 0;
  S.ov = S.lf > 1.5 ? 1 : S.startle > 0.6 || S.wake > 0 ? 2 : S.dizzy > 1 ? 3 : 0;
  return S;
}
export const foxPose = (S) => { const p = {}; for (const k of FOX_KEYS) p[k] = S[k]; return p; };

// ---------------- drawing ----------------
const MESHES = { BODY, HEAD, EAR, LEG };
function partTris(out, mesh, m, extraShade = 0) {   // the mesh through matrix m -> world triangles
  const wv = mesh.v.map((p) => mAp(m, p));
  for (const [a, b, c, e, s] of mesh.t) out.push([wv[a], wv[b], wv[c], e, s + extraShade]);
}
function tailTris(out, mb, ta, tu) {
  // the tail: rings along a bent line from the pivot backwards; it swings (ta) and rises (tu), more towards the tip
  const rings = [];
  let p = [...TAIL_PIVOT], yaw = 0, el = tu;
  for (let i = 0; i < TAIL_R.length; i++) {
    const side = [Math.sin(yaw), 0, Math.cos(yaw)];
    const r = [];
    for (let k = 0; k < 6; k++) {
      const a = HEX + (k * Math.PI) / 3, hw = TAIL_R[i][0] * Math.cos(a), hh = TAIL_R[i][1] * Math.sin(a);
      r.push([p[0] + side[0] * hw, p[1] + hh, p[2] + side[2] * hw]);
    }
    rings.push(r);
    if (i < TAIL_L.length) {
      yaw += ta * 0.35; el += tu * 0.15 - 0.12;
      const L = TAIL_L[i];
      p = [p[0] - Math.cos(el) * Math.cos(yaw) * L, p[1] + Math.sin(el) * L, p[2] + Math.cos(el) * Math.sin(yaw) * L];
    }
  }
  partTris(out, loft(rings, TAIL_SHADE, false, true, [0, 4]), mb);
}
export function foxTris(P) {                         // the pose -> all triangles, in world coordinates
  const lf = P.lf, sit = Math.min(1, lf), lie = Math.max(0, lf - 1);
  const drop = sit * 0.13 + lie * 0.3;
  const pitch = P.pt + sit * 0.42 * (1 - lie) - lie * 0.05;
  let mb = mT(P.px, P.hop - drop, P.pz);
  mb = mMul(mb, mRy(P.hd));
  mb = mMul(mb, mT(0, 0.62, 0)); mb = mMul(mb, mRx(P.rl)); mb = mMul(mb, mRz(pitch)); mb = mMul(mb, mRx(P.tb)); mb = mMul(mb, mT(0, -0.62, 0));
  const out = [];
  partTris(out, BODY, mb);
  let mh = mMul(mb, mT(...HEAD_PIVOT));
  mh = mMul(mh, mRy(P.hy)); mh = mMul(mh, mRz(P.hp - pitch * 0.6));
  partTris(out, HEAD, mh);
  for (const sd of [1, -1]) {                       // ears (the right one mirrored)
    const e = sd > 0 ? P.el : P.er;
    let me = mMul(mh, mT(EAR_PIVOT[0], EAR_PIVOT[1], EAR_PIVOT[2] * sd));
    me = mMul(me, mRz(e)); me = mMul(me, mRx(-0.15 * sd));
    if (sd < 0) me = mMul(me, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0]);
    const mesh = sd < 0 ? { v: EAR.v, t: EAR.t.map(([a, b, c, f, s]) => [a, c, b, ((f & 1) << 2) | (f & 2) | ((f >> 2) & 1), s]) } : EAR;
    partTris(out, mesh, me);
  }
  for (let i = 0; i < 4; i++) {                     // legs: walking swing, folded when sitting / lying
    const front = i < 2, hp = HIPS[i];
    let sw = Math.sin(P.wp + LEG_PH[i]) * P.wa;
    if (front) sw += lie * 1.35 - sit * (1 - lie) * 0.42; else sw += sit * 1.25 + lie * 0.25;
    let ml = mMul(mb, mT(...hp)); ml = mMul(ml, mRz(sw));
    partTris(out, LEG, ml);
  }
  tailTris(out, mb, P.ta, P.tu);
  return { tris: out, mb, mh };
}
// k: {pal}; t: seconds (for the floating zzz / stars); fx: effects on
export function drawFox(fb, P, t = 0, fx = true) {
  fb.fill(T);
  // the box: a grid on the floor, the back wall's edges
  const G = (x, y, z) => proj([x, y, z]);
  for (let i = 0; i <= 12; i++) { const x = -BOX.x + (i * BOX.x) / 6; seg(fb, G(x, 0, BOX.zb), G(x, 0, BOX.zf), T + 3); }
  for (let i = 0; i <= 6; i++) { const z = BOX.zb + (i * (BOX.zf - BOX.zb)) / 6; seg(fb, G(-BOX.x, 0, z), G(BOX.x, 0, z), T + 3); }
  for (const x of [-BOX.x, BOX.x]) { seg(fb, G(x, 0, BOX.zb), G(x, BOX.h, BOX.zb), T + 5); seg(fb, G(x, 0, BOX.zf), G(x, 0, BOX.zb), T + 6); }
  seg(fb, G(-BOX.x, BOX.h, BOX.zb), G(BOX.x, BOX.h, BOX.zb), T + 5);
  seg(fb, G(-BOX.x, 0, BOX.zf), G(BOX.x, 0, BOX.zf), T + 6);
  seg(fb, G(-BOX.x, 0, BOX.zb), G(BOX.x, 0, BOX.zb), T + 5);
  // its shadow
  const sh = 1 / (1 + P.hop * 1.6), hd = P.hd, pts = [];
  for (let i = 0; i < 14; i++) {
    const a = (i * 2 * Math.PI) / 14, lx = Math.cos(a) * 0.78 * sh, lz = Math.sin(a) * 0.3 * sh;
    pts.push(G(P.px + lx * Math.cos(hd) + lz * Math.sin(hd), 0, P.pz - lx * Math.sin(hd) + lz * Math.cos(hd)));
  }
  for (let i = 1; i + 1 < pts.length; i++) tri(fb, pts[0], pts[i], pts[i + 1], T + 1);
  // the fox: far triangles first, back faces left out, lit from the top left
  const { tris, mh } = foxTris(P);
  const list = [];
  for (const [a, b, c, e, s] of tris) {
    const pa = proj(a), pb = proj(b), pc = proj(c);
    const area = (pb[0] - pa[0]) * (pc[1] - pa[1]) - (pb[1] - pa[1]) * (pc[0] - pa[0]);
    if (area >= 0) continue;                        // facing away (screen y goes down)
    const n = cross(sub(b, a), sub(c, a)), nl = Math.hypot(...n) || 1;
    const lit = Math.max(0, dot(n, LIGHT) / nl);
    const lvl = clamp(Math.round(3 + 9 * lit) + s, 1, 13);
    list.push([(pa[2] + pb[2] + pc[2]) / 3, pa, pb, pc, e, lvl]);
  }
  list.sort((x, y) => y[0] - x[0]);
  for (const [, pa, pb, pc, e, lvl] of list) {
    tri(fb, pa, pb, pc, T + lvl);
    const ec = T + Math.min(15, lvl + 3);
    if (e & 1) seg(fb, pa, pb, ec);
    if (e & 2) seg(fb, pb, pc, ec);
    if (e & 4) seg(fb, pc, pa, ec);
  }
  // eyes
  for (const sd of [1, -1]) {
    const pw = mAp(mh, [0.15 * HS, 0.11 * HS, 0.085 * HS * sd]), nw = sub(mAp(mh, [0.75, 0.25, 0.62 * sd]), mAp(mh, [0, 0, 0]));
    if (dot(nw, sub(CAM, pw)) <= 0) continue;
    const q = proj(pw), x = Math.round(q[0]), y = Math.round(q[1]);
    if (P.ey === 1) fill(fb, x - 1, y, 3, 1, T + 0);
    else if (P.ey === 2) { fill(fb, x - 1, y - 1, 1, 1, T + 0); fill(fb, x + 1, y - 1, 1, 1, T + 0); fill(fb, x, y, 1, 1, T + 0); fill(fb, x - 1, y + 1, 1, 1, T + 0); fill(fb, x + 1, y + 1, 1, 1, T + 0); }
    else if (P.ey === 3) { fill(fb, x - 1, y - 1, 3, 3, T + 0); fill(fb, x - 1, y - 1, 1, 1, T + 15); }
    else { fill(fb, x - 1, y - 1, 2, 2, T + 0); fill(fb, x - 1, y - 1, 1, 1, T + 15); }
  }
  // above its head: zzz, !, or stars going round
  const top = proj(mAp(mh, [0.05, 0.52, 0])), hx = Math.round(top[0]), hy = Math.round(top[1]);
  if (P.ov === 1) {
    const u = fx ? (t * 0.6) % 1 : 0.5;
    text8(fb, 'z', hx + 6 + Math.round(u * 10), hy - 4 - Math.round(u * 14), 1, T + 10);
    text8(fb, 'Z', hx + 14 + Math.round(u * 10), hy - 18 - Math.round(u * 14), 2, T + 13);
  } else if (P.ov === 2) text8(fb, '!', hx - 7, hy - 22, 2, T + 15);
  else if (P.ov === 3) {
    for (let i = 0; i < 3; i++) {
      const a = (fx ? t * 4 : 0) + (i * 2 * Math.PI) / 3;
      text8(fb, '*', hx - 4 + Math.round(Math.cos(a) * 16), hy - 10 + Math.round(Math.sin(a) * 5), 1, T + 14);
    }
  }
  // the top bar
  fill(fb, 0, 0, SW, 11, T + 12);
  text8(fb, 'FOX', 3, 2, 1, T + 0);
  const m = FOX_MOODS[P.md] || '';
  text8(fb, m, SW - 3 - textw(m, 1), 2, 1, T + 0);
}
