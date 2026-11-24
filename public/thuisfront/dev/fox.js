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
const CAM = [0, 1.5, 5.6], LOOK = [0, 0.62, -0.15], FOC = 330, CX = 160, CY = 94;   // the whole box in view
const CF = (() => { const f = sub(LOOK, CAM), l = Math.hypot(...f); return f.map((x) => x / l); })();
const CR = (() => { const r = cross(CF, [0, 1, 0]), l = Math.hypot(...r); return r.map((x) => x / l); })();
const CU = cross(CR, CF);
const LIGHT = (() => { const l = [-0.45, 0.85, 0.55], n = Math.hypot(...l); return l.map((x) => x / n); })();
function proj(p) {                                  // world -> [screen x, screen y, depth]
  const d = sub(p, CAM), z = dot(d, CF);
  return [CX + (FOC * dot(d, CR)) / z, CY - (FOC * dot(d, CU)) / z, z];
}
export const BOX = { x: 2.15, zb: -1.35, zf: 1.05, h: 1.75 };   // the box: x -2.15..2.15, z -1.35 (back) .. 1.05 (front), 1.75 high
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
// cr: crouch (stalking, before a jump); gx gz: the spot it was sent to (tapped), gm: 1 = show it;
// sc: scratching an ear with a hind leg (+ the leg on its +z side, - the other); fp: flipping (pitch, a backflip or a nose dive);
// bw: a play bow (front down, bottom up); dd: playing dead (flat on its side, legs stiff)
export const FOX_KEYS = ['px', 'pz', 'hop', 'hd', 'rl', 'pt', 'tb', 'lf', 'hy', 'hp', 'el', 'er', 'ta', 'tu', 'wp', 'wa', 'ey', 'md', 'ov', 'cr', 'gx', 'gz', 'gm',
  'sc', 'fp', 'bw', 'dd'];
export const FOX_MOODS = ['CHILLING', 'CURIOUS', 'WALKING', 'SITTING', 'ZZZ...', 'HUH?', 'EEK!', 'WHOA!', 'HOLD ON!', 'HELP!', 'DIZZY...', 'OOF!', 'HELLO!', 'WHEEE!',
  'UPSIDE DOWN!', 'SNIFF SNIFF', 'YAAAWN', 'HMM?', 'STALKING...', 'BOING!', 'ROLLING!', 'FOUND IT!', 'POUNCE!',
  'SCRITCH SCRATCH', 'ROUND N ROUND', 'COMFY', 'TAIL! TAIL!', 'TA-DA!', 'PLAY DEAD', 'PLAY WITH ME!', 'ZOOMIES!', 'MOUSING...', 'MISSED!'];
const ANGLES = new Set(['hd', 'tb', 'wp', 'fp']);
const INTS = new Set(['ey', 'md', 'ov', 'gm']);
export const foxPack = (p) => FOX_KEYS.map((k) => (INTS.has(k) ? p[k] | 0 : Math.round(p[k] * 100) / 100));
export function foxUnpack(a) {
  if (!Array.isArray(a) || a.length < 19) return null;   // an older screen sends only the first 19 (the rest stay 0)
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
    ta: 0, tav: 0, tu: 0.15, tuv: 0, wp: 0, wa: 0, ey: 0, md: 12, ov: 0, t: 0, gl: [0, 1, 0], still: 0, startle: 0, dizzy: 0, wake: 0,
    hello: 2.5, blink: 2, look: 3, lookY: 0, lookP: 0, pause: 1.5, sit: 0, sniff: 0, tx: 0, tz: 0, walking: false, slide: false, on: 1,
    twitch: 4, twitchE: 0, bump: 0, nap: 0, yawn: 0, hdPrev: -0.65, hdv: 0, sensor: false,
    cr: 0, gx: 0, gz: 0, gm: 0, gs: 0, gph: 0, gt: 0, gdur: 0, leap: 0, jumping: false, found: 0, rollDir: 1, pounce: 0,
    sc: 0, fp: 0, bw: 0, dd: 0, act: 0, lastAct: 0, at: 0, aph: 0, adir: 1, a0: 0, acx: 0, acz: 0, ar: 0, an: 0, rest: 0, tada: 0, missed: 0,
  };
}
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const ease = (cur, to, rate, dt) => cur + (to - cur) * (1 - Math.exp(-rate * dt));
const easeA = (cur, to, rate, dt) => cur + wrap(to - cur) * (1 - Math.exp(-rate * dt));
const headingTo = (dx, dz) => Math.atan2(-dz, dx);
export const FOX_CEIL = BOX.h - 1.24;               // its height when it hangs from the ceiling (upside down)
const XL = BOX.x - 0.55, ZB = BOX.zb + 0.45, ZF = BOX.zf - 0.45;
// TAP: it goes to a spot on the floor. First a careful look ("?"), then one of its ways: 1 stalking (low, stop and go,
// a pounce at the end), 2 jumping there, 3 sniffing its way in zig-zags, 4 rolling over the floor
export function foxGo(S, x, z, rnd = Math.random, style = 0) {
  S.gx = clamp(x, -XL, XL); S.gz = clamp(z, ZB, ZF); S.gm = 1;
  S.gs = style >= 1 && style <= 4 ? style : 1 + Math.floor(rnd() * 4); S.gph = 0; S.gt = 0.8 + rnd() * 0.6; S.gdur = 0;
  S.walking = false; S.sit = 0; S.sniff = 0; S.pause = 0; S.act = 0; S.rest = 0;
  if (S.nap > 0) { S.nap = 0; S.wake = 1.2; }
}
// IDLE: little things it does on its own when left alone: 1 scratching an ear, 2 turning round its spot and lying
// down, 3 chasing its tail, 4 a backflip, 5 playing dead, 6 a play bow (sometimes zoomies after), 7 zoomies,
// 8 mousing (a high pounce, nose first into the floor), 9 a yawn with a stretch
export function foxAct(S, rnd = Math.random, force = 0) {
  let a = force;
  if (!(a >= 1 && a <= 9)) { a = 1 + Math.floor(rnd() * 8); if (S.lastAct > 0 && a >= S.lastAct) a++; }   // not the same twice
  S.act = S.lastAct = a; S.at = 0; S.aph = 0; S.adir = rnd() < 0.5 ? 1 : -1;
  S.walking = false; S.sit = 0; S.sniff = 0;
  const fx = Math.cos(S.hd), fz = -Math.sin(S.hd);
  if (a === 1) S.adir = Math.sin(S.hd) * (CAM[0] - S.px) + Math.cos(S.hd) * (CAM[2] - S.pz) >= 0 ? 1 : -1;   // the leg you can see
  if (a === 2) {                                    // a circle with its centre beside it
    const r = 0.22, nx = S.adir * Math.sin(S.hd), nz = S.adir * Math.cos(S.hd);
    S.acx = clamp(S.px + nx * r, -XL + r, XL - r); S.acz = clamp(S.pz + nz * r, ZB + r, ZF - r);
    S.an = Math.atan2(S.pz - S.acz, S.px - S.acx); S.ar = r;
  }
  if (a === 7) S.an = 3 + Math.floor(rnd() * 2);    // zoomies: how many dashes
  if (a === 8) { S.acx = clamp(S.px + fx * 0.6, -XL, XL); S.acz = clamp(S.pz + fz * 0.6, ZB, ZF); }
  if (a === 9) S.yawn = 2.5;
}
// one step of an idle act on the floor; false = it is over
function actStep(S, dt, rnd) {
  const a = S.act, t = S.at;
  if (a === 1) { S.vx = 0; S.vz = 0; return t < 2.8; }
  if (a === 2) {                                    // round and round (one and a half times), then lies down
    const w = 2.0, an = S.an + S.adir * w * dt, r = S.ar;
    S.an = an;
    const qx = S.acx + Math.cos(an) * r, qz = S.acz + Math.sin(an) * r;
    S.vx = -Math.sin(an) * r * w * S.adir + (qx - S.px) * 5; S.vz = Math.cos(an) * r * w * S.adir + (qz - S.pz) * 5;
    if (t > (3 * Math.PI) / w) { S.rest = 4 + rnd() * 5; S.vx = 0; S.vz = 0; return false; }
    return true;
  }
  if (a === 3) {                                    // notices its tail, spins after it, gets dizzy
    if (t > 0.6) S.aph = 1;
    const fx = Math.cos(S.hd), fz = -Math.sin(S.hd);
    S.vx = S.aph ? fx * 0.15 : 0; S.vz = S.aph ? fz * 0.15 : 0;
    if (t > 3.2) { S.dizzy = Math.max(S.dizzy, 1.7); S.vx = 0; S.vz = 0; return false; }
    return true;
  }
  if (a === 4) {                                    // crouch, jump, a backflip in the air, TA-DA
    S.vx = 0; S.vz = 0;
    if (S.aph === 0 && t > 0.4) { S.aph = 1; S.vy = 2.8; S.hop = 0.001; S.jumping = true; S.a0 = t; }
    else if (S.aph === 1 && t - S.a0 > 0.15) { S.aph = 2; S.fp = 0; S.tada = 1.4; return false; }
    return true;
  }
  if (a === 5) {                                    // a wobble, flops on its back, lies still, peeks, jumps up
    S.vx = 0; S.vz = 0;
    S.aph = t < 0.5 ? 0 : t < 4 ? 1 : t < 4.8 ? 2 : 3;
    if (t > 5.2) { S.tada = 1.3; return false; }
    return true;
  }
  if (a === 6) {                                    // play bow, wagging; sometimes zoomies after
    S.vx = 0; S.vz = 0;
    if (t > 1.9) { if (rnd() < 0.5) { S.act = 7; S.at = 0; S.an = 3; S.aph = 0; return true; } return false; }
    return true;
  }
  if (a === 7) {                                    // zoomies: fast dashes to far spots
    let ex = S.acx - S.px, ez = S.acz - S.pz, d = Math.hypot(ex, ez);
    if (S.aph === 0 || d < 0.25) {
      if (S.an <= 0 || t > 7) { S.pause = 2; return false; }
      S.an--; S.aph = 1;
      for (let k = 0; k < 8; k++) { S.acx = (rnd() * 2 - 1) * XL; S.acz = ZB + rnd() * (ZF - ZB); if (Math.hypot(S.acx - S.px, S.acz - S.pz) >= 1.2) break; }
      ex = S.acx - S.px; ez = S.acz - S.pz; d = Math.hypot(ex, ez);
    }
    S.vx = ease(S.vx, (ex / d) * 1.5, 6, dt); S.vz = ease(S.vz, (ez / d) * 1.5, 6, dt);
    return true;
  }
  if (a === 8) {                                    // mousing: frozen, listening; a high jump; nose first into the floor
    S.vx = 0; S.vz = 0;
    if (S.aph === 0 && t > 1.4) {
      const T = 0.56; S.aph = 1; S.a0 = t;
      S.vx = (S.acx - S.px) / T; S.vz = (S.acz - S.pz) / T; S.vy = (9.8 * T) / 2; S.hop = 0.001; S.jumping = true; S.pounce = 1.4;
    } else if (S.aph === 1 && t - S.a0 > 0.15) { S.aph = 2; S.a0 = t; }
    else if (S.aph === 2 && t - S.a0 > 0.7) { S.aph = 3; S.missed = 1.6; return false; }
    return true;
  }
  return t < 1.6;                                   // 9: the yawn's stretch
}
// the floor spot under a point on the screen (x 0-320, y 0-172), kept inside the box
export function foxFloorAt(sx, sy) {
  const d = [0, 1, 2].map((i) => CF[i] + ((sx - CX) / FOC) * CR[i] - ((sy - CY) / FOC) * CU[i]);
  let x, z;
  if (d[1] < -1e-3) { const t = -CAM[1] / d[1]; x = CAM[0] + d[0] * t; z = CAM[2] + d[2] * t; }
  else { x = ((sx - CX) / FOC) * 6; z = BOX.zb; }   // above the floor: the back of the box
  return [clamp(x, -XL, XL), clamp(z, ZB, ZF)];
}
// one step of going to the tapped spot (on the floor, not sliding); sets its velocity, starts its jumps
function goStep(S, dt, rnd) {
  const ex = S.gx - S.px, ez = S.gz - S.pz, d = Math.hypot(ex, ez), ux = d > 1e-6 ? ex / d : 0, uz = d > 1e-6 ? ez / d : 0;
  S.gdur += dt;
  const arrive = () => { S.gs = 0; S.gm = 0; S.found = 1.6; S.pause = 2.5; S.vx = 0; S.vz = 0; };
  if (S.gdur > 20) { S.gs = 0; S.gm = 0; return; }   // gave up
  if (S.gph === 0) {                                // a careful look first
    S.vx *= Math.exp(-8 * dt); S.vz *= Math.exp(-8 * dt);
    S.gt -= dt;
    if (S.gt <= 0) { S.gph = 1; S.gt = 0; S.rollDir = rnd() < 0.5 ? 1 : -1; }
    return;
  }
  S.gt += dt;
  if (S.gs === 1) {                                 // stalking: low, a few steps, freeze, ... then pounce
    if (d < 0.6 && S.pounce <= 0) {
      const T = 0.45; S.vx = ex / T; S.vz = ez / T; S.vy = 4.9 * T * 0.95; S.hop = 0.001; S.jumping = true; S.pounce = 0.9;
      S.gs = 0; S.gm = 0; S.found = 1.8; S.pause = 2.5;
      return;
    }
    const moving = S.gt % 1.3 < 0.8, sp = moving ? 0.28 : 0;
    S.vx = ease(S.vx, ux * sp, 10, dt); S.vz = ease(S.vz, uz * sp, 10, dt);
  } else if (S.gs === 2) {                          // jumping: crouch, leap, land, again
    if (d < 0.15) { arrive(); return; }
    S.vx *= Math.exp(-10 * dt); S.vz *= Math.exp(-10 * dt);
    if (S.leap > 0) {                               // crouching, then the jump
      S.leap -= dt;
      if (S.leap <= 0) {
        const step = Math.min(d, 1.0), T = 0.55;
        S.vx = (ux * step) / T; S.vz = (uz * step) / T; S.vy = (9.8 * T) / 2; S.hop = 0.001; S.jumping = true; S.leap = 0; S.gt = 0;
      }
    } else if (S.gt > 0.25) S.leap = 0.3;
  } else if (S.gs === 3) {                          // sniffing its way: slow zig-zags, nose down, little stops
    if (d < 0.15) { arrive(); return; }
    const a = Math.sin(S.gt * 2.4) * 0.7, stop = S.gt % 2.2 > 1.7, sp = stop ? 0 : 0.3;
    const vx = ux * Math.cos(a) - uz * Math.sin(a), vz = ux * Math.sin(a) + uz * Math.cos(a);
    S.vx = ease(S.vx, vx * sp, 6, dt); S.vz = ease(S.vz, vz * sp, 6, dt);
  } else if (S.gs === 4) {                          // rolling: lie down, roll over, get up and shake
    if (d < 0.12 || S.gt > 8) { arrive(); S.bump = 0; return; }
    const sp = S.gt < 0.4 ? 0 : 0.8;
    S.vx = ease(S.vx, ux * sp, 5, dt); S.vz = ease(S.vz, uz * sp, 5, dt);
  }
}
// f: what the accelerometer feels, in g, in screen directions (x right, y up, z out of the screen), 1 g up when still;
// w: turning in rad/s (same directions); null = no sensor. rnd: random 0..1. dt: seconds.
export function foxStep(S, f, w, dt, rnd = Math.random) {
  dt = clamp(dt, 0, 0.1);
  S.t += dt;
  if (f) S.sensor = true;
  const gyro = !!w;
  f = f || [0, 1, 0]; w = w || [0, 0, 0];
  // which way is "up" in the box: the gyroscope turns it along at once (only tilting changes it), the accelerometer
  // pulls it back slowly, and only when it feels about 1 g (not while the box is being thrown about)
  const g = S.gl, c = [g[1] * w[2] - g[2] * w[1], g[2] * w[0] - g[0] * w[2], g[0] * w[1] - g[1] * w[0]];
  for (let i = 0; i < 3; i++) g[i] += c[i] * dt;
  const fm = Math.hypot(...f), trust = Math.abs(fm - 1) < 0.25 ? 1 : 0.1;
  for (let i = 0; i < 3; i++) g[i] = ease(g[i], f[i] / (fm || 1), (gyro ? 1.5 : 5) * trust, dt);
  const gn = Math.hypot(...g) || 1;
  for (let i = 0; i < 3; i++) g[i] /= gn;
  const lin = [f[0] - g[0], f[1] - g[1], f[2] - g[2]], linM = Math.hypot(...lin), spin = Math.hypot(...w);
  // what it feels in the box (m/s2): gravity, and the box's own jerks the other way: small ones ignored, big ones
  // made bigger, so moving the box a lot throws it about
  const kick = linM > 0.25 ? ((linM - 0.25) * 2.5) / linM : 0;
  const A = [0, 1, 2].map((i) => -9.8 * (g[i] + lin[i] * kick));
  // calm, sleep (only with a sensor: 3 minutes untouched, a nap of a minute or so), waking up
  const busy = linM > 0.06 || spin > 0.35;
  if (busy) { if (S.nap > 0) { S.wake = 2; S.nap = 0; } S.still = 0; } else S.still += dt;
  if (S.sensor && S.still > 180 && S.nap <= 0 && S.on === 1) S.nap = 60 + rnd() * 40;
  if (S.nap > 0) { S.nap -= dt; if (S.nap <= 0) { S.yawn = 2.5; S.still = 0; } }
  const sleeping = S.nap > 0;
  S.dizzy = clamp(S.dizzy + (spin > 3 ? (spin - 3) * dt * 0.45 : 0) + (linM > 1 ? dt * 1.0 : 0) - dt * 0.22, 0, 3);
  for (const k of ['startle', 'wake', 'hello', 'bump', 'yawn', 'sit', 'sniff', 'found', 'pounce', 'rest', 'tada', 'missed']) S[k] = Math.max(0, S[k] - dt);
  if (S.act) S.at += dt;
  if (linM > 0.6 && S.startle < 0.6) { S.startle = 1.4; if (S.nap > 0) { S.wake = 2; S.nap = 0; } if (S.on === 0) S.tbv += (rnd() - 0.5) * 8; S.rest = 0; }
  // ---- moving: on the floor (or upside down on the ceiling) while pressed against it, else flying
  const surf = S.hop <= 0.0005 ? 1 : S.hop >= FOX_CEIL - 0.0005 ? -1 : 0;
  const wasOn = S.on;
  S.on = surf === 1 && A[1] < -2.5 ? 1 : surf === -1 && A[1] > 2.5 ? -1 : 0;
  const vx0 = S.vx, vy0 = S.vy, vz0 = S.vz;
  const ownJump = (S.act === 4 || S.act === 8) && S.aph >= 1 && S.jumping;
  if (S.act && ((S.on !== 1 && !ownJump) || S.slide || S.startle > 0.8 || S.gs > 0)) { S.act = 0; S.walking = false; }
  const dx = A[0] / 9.8, dz = A[2] / 9.8, slope = Math.hypot(dx, dz);     // downhill along the surface
  let speed = Math.hypot(S.vx, S.vz);
  if (S.on) {
    const N = Math.abs(A[1]), T = Math.hypot(A[0], A[2]);
    S.slide = T > 0.27 * N || (S.slide && speed > 0.2 && T > 0.1 * N);
    S.vy = 0;
    if (S.slide) {
      const fr = 0.18 * N, ux = speed > 0.05 ? S.vx / speed : A[0] / (T || 1), uz = speed > 0.05 ? S.vz / speed : A[2] / (T || 1);
      S.vx += (A[0] - fr * ux) * dt; S.vz += (A[2] - fr * uz) * dt;
      S.walking = false;
    } else if (S.on === 1 && S.gs > 0 && S.dizzy < 1.5 && S.startle <= 0.8) {
      goStep(S, dt, rnd);
    } else if (S.on === 1) {
      // its own life on the floor: walk somewhere, stop (sniff, look, sometimes sit), walk again
      const canWalk = S.dizzy < 1 && S.startle <= 0 && !sleeping && S.wake <= 0 && S.yawn <= 0 && S.hello <= 0;
      if (S.act) {
        if (S.slide || !actStep(S, dt, rnd)) { S.act = 0; if (S.pause < 1) S.pause = 1 + rnd() * 2; }
      } else if (S.walking) {
        const ex = S.tx - S.px, ez = S.tz - S.pz, d = Math.hypot(ex, ez);
        if (!canWalk || d < 0.12) {
          S.walking = false; S.pause = 1 + rnd() * 2.5;
          if (rnd() < 0.4) S.sniff = 1.2 + rnd(); else if (rnd() < 0.25) { S.sit = 4 + rnd() * 5; S.pause = S.sit + 0.5; }
        } else { const sp = d > 0.5 ? 0.6 : 0.35; S.vx = ease(S.vx, (ex / d) * sp, 4, dt); S.vz = ease(S.vz, (ez / d) * sp, 4, dt); }
      } else {
        S.pause -= dt;
        if (canWalk && S.pause <= 0 && S.sit <= 0 && S.rest <= 0 && S.tada <= 0 && S.missed <= 0 && rnd() < 0.4) foxAct(S, rnd);
        else if (canWalk && S.pause <= 0 && S.sit <= 0 && S.rest <= 0 && S.tada <= 0 && S.missed <= 0) {
          S.walking = true;
          for (let k = 0; k < 8; k++) { S.tx = (rnd() * 2 - 1) * (XL - 0.1); S.tz = ZB + 0.05 + rnd() * (ZF - ZB - 0.1); if (Math.hypot(S.tx - S.px, S.tz - S.pz) >= 0.7) break; }
        }
      }
      if (!S.walking && !S.act) { S.vx *= Math.exp(-6 * dt); S.vz *= Math.exp(-6 * dt); }
    } else { S.vx *= Math.exp(-6 * dt); S.vz *= Math.exp(-6 * dt); S.walking = false; }
  } else {
    S.vx += A[0] * dt; S.vy += A[1] * dt; S.vz += A[2] * dt;
    S.walking = false; S.slide = false;
  }
  S.px += S.vx * dt; S.pz += S.vz * dt; S.hop += S.vy * dt;
  // the box: walls, floor and ceiling; it bounces (and lands when the hit is soft and it is pressed that way)
  let hit = 0;
  const bounce = (v) => { hit = Math.max(hit, Math.abs(v)); return Math.abs(v) < 0.3 ? 0 : -v * 0.45; };
  if (S.px < -XL) { S.px = -XL; if (S.vx < 0) S.vx = bounce(S.vx); }
  if (S.px > XL) { S.px = XL; if (S.vx > 0) S.vx = bounce(S.vx); }
  if (S.pz < ZB) { S.pz = ZB; if (S.vz < 0) S.vz = bounce(S.vz); }
  if (S.pz > ZF) { S.pz = ZF; if (S.vz > 0) S.vz = bounce(S.vz); }
  if (S.hop < 0) {                                  // its own jumps land softly; being thrown bounces
    S.hop = 0;
    if (S.vy < 0) { if ((-S.vy < 1.6 || S.jumping) && A[1] < -2.5) { hit = Math.max(hit, S.jumping ? 0 : -S.vy * 0.5); S.vy = 0; } else S.vy = bounce(S.vy); }
    S.jumping = false;
  }
  if (S.hop > FOX_CEIL) { S.hop = FOX_CEIL; if (S.vy > 0) { if (S.vy < 1.6 && A[1] > 2.5) { hit = Math.max(hit, S.vy * 0.5); S.vy = 0; } else S.vy = bounce(S.vy); } }
  if (hit > 2 && S.bump <= 0) { S.bump = 0.7; S.dizzy = Math.min(3, S.dizzy + hit * 0.1); S.tbv += (rnd() - 0.5) * hit * 2.5; }
  speed = Math.hypot(S.vx, S.vz);
  // how it is turned: feet on its floor (or ceiling); flying it spins, and turns its feet down like a cat
  const left = [Math.sin(S.hd), 0, Math.cos(S.hd)], am = Math.hypot(...A) || 1;
  if (S.on && S.gs === 4 && S.gph === 1) {        // rolling over the floor: it turns round its long side
    S.tb = wrap(S.tb + S.rollDir * (Math.hypot(S.vx, S.vz) / 0.28) * dt); S.tbv = 0;
  } else if (S.on) {
    const to = S.on === 1 ? (S.act === 5 && S.aph >= 1 && S.aph <= 2 ? S.adir * Math.PI * 0.5 : 0) : Math.PI;
    if (!wasOn && Math.abs(wrap(S.tb - to)) > 1.2 && !S.jumping) S.dizzy = Math.min(3, S.dizzy + 0.8);
    S.tb = easeA(S.tb, to, S.gs === 4 ? 5 : S.act === 5 ? 6 : 8, dt); S.tbv = 0;
  } else {
    const cat = Math.atan2(-(A[0] * left[0] + A[2] * left[2]) / am, -A[1] / am);
    S.tbv *= Math.exp(-0.7 * dt); S.tb = wrap(S.tb + S.tbv * dt);
    S.tb = easeA(S.tb, cat, 3 * Math.max(0, 1 - Math.abs(S.tbv) / 5), dt);
  }
  // which way it faces: up the slope when sliding, where it walks; else as it was (turning the box turns it)
  let hdTo = S.hd;
  const toGoal = headingTo(S.gx - S.px, S.gz - S.pz);
  if (S.slide) hdTo = headingTo(-dx, -dz);
  else if (S.gs > 0 && S.on === 1) hdTo = S.gs === 4 && S.gph === 1 ? toGoal + (S.rollDir > 0 ? -Math.PI / 2 : Math.PI / 2) : S.gph === 0 ? S.hd : toGoal;
  else if ((S.walking || S.act === 2 || S.act === 7) && speed > 0.1) hdTo = headingTo(S.vx, S.vz);
  else if (S.act === 8 && S.aph === 0) hdTo = headingTo(S.acx - S.px, S.acz - S.pz);
  S.hd -= w[1] * dt * 0.85;
  if (S.act === 3 && S.aph === 1 && S.on === 1) { S.hd += S.adir * 6.5 * dt; hdTo = S.hd; }
  if (S.on) S.hd = easeA(S.hd, hdTo, S.slide ? 3 : S.act === 7 ? 8 : S.walking || S.gs > 0 || S.act === 2 ? 5 : 1.5, dt);
  S.hd = wrap(S.hd);
  const hdv = wrap(S.hd - S.hdPrev) / (dt || 1), hda = (hdv - S.hdv) / (dt || 1);
  S.hdPrev = S.hd; S.hdv = hdv;
  // leaning against the slope (in its own directions)
  const fwx = Math.cos(S.hd), fwz = -Math.sin(S.hd), along = dx * fwx + dz * fwz, side = dx * fwz - dz * fwx;
  S.pt = ease(S.pt, S.on === 1 ? clamp(-along * 1.3, -0.5, 0.5) : 0, 6, dt);
  const rlAct = S.act === 1 ? S.adir * 0.12 : S.act === 5 && S.aph === 0 ? Math.sin(S.at * 14) * 0.25 : 0;
  S.rl = ease(S.rl, (S.on === 1 ? clamp(side * 1.1, -0.5, 0.5) : 0) + rlAct + (S.dizzy > 1 ? Math.sin(S.t * 3) * 0.12 : 0), 6, dt);
  // sitting and sleeping
  const rolling = S.gs === 4 && S.gph === 1;
  const deadLie = S.act === 5 && S.aph >= 1 && S.aph <= 2;
  S.dd = ease(S.dd, deadLie ? 1 : 0, 7, dt);
  const lfTo = sleeping || rolling || (S.rest > 0 && S.on === 1) ? 2 : (S.sit > 0 || S.act === 1) && S.on === 1 && !S.slide ? 1 : 0;
  S.lf = ease(S.lf, lfTo, rolling ? 6 : lfTo > S.lf ? 1.5 : 5, dt);
  const crTo = S.gs > 0 && S.on === 1 ? (S.gph === 0 ? 0.35 : S.gs === 1 ? 0.85 : S.gs === 2 && S.leap > 0 ? 1 : S.gs === 3 ? 0.3 : 0)
    : S.act === 4 && S.aph <= 1 ? 1 : S.act === 8 && S.aph === 0 ? 0.5 : 0;
  S.cr = ease(S.cr, crTo, 7, dt);
  // the tricks' own joints: scratching leg, flips, the play bow
  S.sc = ease(S.sc, S.act === 1 && S.at > 0.5 && S.lf > 0.6 ? S.adir : 0, 6, dt);
  const flyT = S.at - S.a0;
  if (S.act === 4 && S.aph === 1) { const u = clamp(flyT / 0.58, 0, 1); S.fp = wrap(2 * Math.PI * u * u * (3 - 2 * u)); }
  else if (S.act === 8 && S.aph === 1) S.fp = 0.6 - 2.0 * clamp(flyT / 0.56, 0, 1);
  else if (S.act === 8 && S.aph === 2) S.fp = ease(S.fp, -1.0, 12, dt);
  else S.fp = easeA(S.fp, 0, 6, dt);
  S.bw = ease(S.bw, S.act === 6 || (S.act === 9 && S.at < 1.4) ? 1 : 0, 6, dt);
  // legs
  const waTo = !S.on ? (S.jumping ? (S.act === 4 ? 0 : 0.5) : 0.9) : S.slide ? 0.75 : S.lf > 0.5 || S.bw > 0.3 ? 0 : S.act === 3 && S.aph === 1 ? 0.6
    : S.gs === 1 && S.gph === 1 ? clamp(speed * 2.2, 0, 0.45) : clamp(speed * 1.1, 0, 0.6);
  S.wa = ease(S.wa, waTo, 8, dt);
  S.wp = wrap(S.wp + dt * (S.slide || !S.on ? 15 : S.act === 3 ? 16 : 3 + speed * 12));
  // head: looks at you mostly, sometimes around; sniffs the floor; down when asleep
  S.look -= dt;
  if (S.look <= 0) { S.look = 2 + rnd() * 4; S.lookY = rnd() < 0.55 ? 0 : (rnd() - 0.5) * 1.6; S.lookP = (rnd() - 0.5) * 0.4; }
  const toCam = clamp(wrap(headingTo(CAM[0] - S.px, CAM[2] - S.pz) - S.hd), -1.1, 1.1);
  let hyTo = S.walking ? (S.lookY === 0 ? toCam * 0.5 : 0) : S.lookY === 0 ? toCam : S.lookY, hpTo = S.lookY === 0 ? 0.12 : S.lookP;
  if (S.sniff > 0 || (S.gs === 3 && S.gph === 1)) { hyTo = Math.sin(S.t * 5) * 0.2; hpTo = -0.55; }
  if (S.gs > 0 && (S.gph === 0 || S.gs === 1)) { hyTo = clamp(wrap(toGoal - S.hd), -1.1, 1.1); hpTo = S.gph === 0 ? -0.1 : -0.2; }
  if (S.found > 0 || S.tada > 0 || S.act === 6) { hyTo = toCam; hpTo = 0.25; }
  if (S.missed > 0) { hyTo = toCam * 0.6; hpTo = -0.15; }
  if (S.act === 1) { hyTo = -S.adir * 0.8; hpTo = -0.35; }
  if (S.act === 3) { hyTo = S.adir * 1.1; hpTo = -0.15; }
  if (S.act === 8 && S.aph === 0) { hyTo = Math.sin(S.at * 2.2) * 0.12; hpTo = -0.4; }
  if (S.act === 2 || S.act === 7) { hyTo = 0; hpTo = 0.05; }
  if (S.slide || !S.on) { hyTo = 0; hpTo = 0.25; }
  if (S.startle > 0 || S.wake > 0 || S.hello > 0) { hyTo = toCam; hpTo = 0.2; }
  if (S.yawn > 0) { hyTo = toCam * 0.5; hpTo = 0.45; }
  if (S.lf > 1.2) { hyTo = 0.5; hpTo = -0.45; }
  if (S.lf > 1.2 && S.rest > 0) { hyTo = toCam * 0.4; hpTo = -0.35; }
  if (deadLie) { hyTo = 0; hpTo = 0.1; }
  if (S.dizzy > 1) hyTo += Math.sin(S.t * 2.6) * 0.3;
  S.hy = ease(S.hy, hyTo, 5, dt); S.hp = ease(S.hp, hpTo, 5, dt);
  // ears
  S.twitch -= dt;
  if (S.twitch <= 0) { S.twitch = 2 + rnd() * 5; S.twitchE = rnd() < 0.5 ? 1 : 2; }
  const earBase = (S.gs > 0 && S.on === 1 && S.gs !== 4) || (S.act === 8 && S.aph === 0) || S.act === 6 ? 0.15 : S.startle > 0 ? -0.15
    : S.act === 7 || S.slide || (!S.on && !S.jumping) || S.dizzy > 1 || S.bump > 0 ? -0.7 : S.lf > 1.2 || deadLie ? -0.45 : 0;
  const tw = S.twitch < 0.25 ? -0.5 : 0, scE = S.act === 1 ? -0.55 : 0;
  S.el = ease(S.el, earBase + (S.twitchE === 1 ? tw : 0) + (S.adir > 0 ? scE : 0), 14, dt);
  S.er = ease(S.er, earBase + (S.twitchE === 2 ? tw : 0) + (S.adir < 0 ? scE : 0), 14, dt);
  // the tail: a springy joint with its own weight. It wants its pose (taTo / tuTo), but is pulled by what it feels:
  // the box's gravity and jerks, minus the body's own acceleration (so it trails behind when the fox runs off,
  // swings out when it turns, bounces with a hop, floats up when flying)
  const wag = S.found > 0 || S.hello > 0 || S.wake > 0 || S.tada > 0 || S.act === 6 || (S.act === 8 && S.aph === 2) || (S.gs === 3 && S.gph === 1);
  const taTo = rolling || deadLie ? 0 : S.act === 3 ? -S.adir * 1.4 : S.lf > 1.2 ? 1.5 : wag ? Math.sin(S.t * 12) * 0.5
    : S.gs === 1 || S.act === 8 ? 0 : S.walking ? Math.sin(S.wp) * 0.25 : Math.sin(S.t * 1.4) * 0.3;
  const tuTo = deadLie ? 0 : S.act === 6 || (S.act === 8 && S.aph === 2) ? 0.9 : (S.gs === 1 && S.gph === 1) || (S.act === 8 && S.aph === 0) ? -0.15
    : S.startle > 0 ? 0.85 : S.slide ? 0.6 : S.found > 0 || S.tada > 0 ? 0.5 : S.lf > 1.2 ? -0.25 : S.lf > 0.5 ? -0.1 : 0.15;
  const ab = [(S.vx - vx0) / (dt || 1), (S.vy - vy0) / (dt || 1), (S.vz - vz0) / (dt || 1)];
  const F = [A[0] - ab[0], A[1] - ab[1], A[2] - ab[2]];   // what pulls on the tail, from the fox's point of view
  const ct = Math.cos(S.tb), st = Math.sin(S.tb);
  const upW = [st * left[0], ct, st * left[2]], leftW = [ct * left[0], -st, ct * left[2]];
  const fUp = F[0] * upW[0] + F[1] * upW[1] + F[2] * upW[2], fLeft = F[0] * leftW[0] + F[1] * leftW[1] + F[2] * leftW[2];
  S.tav += (40 * (taTo - S.ta) - 4 * S.tav + 0.6 * fLeft - 0.35 * clamp(hda, -60, 60)) * dt;
  S.tuv += (40 * (tuTo - S.tu) - 4 * S.tuv + 0.45 * (fUp + 9.8)) * dt;
  S.ta = clamp(S.ta + S.tav * dt, -2, 2); S.tu = clamp(S.tu + S.tuv * dt, -0.9, 1.3);
  // eyes
  S.blink -= dt;
  if (S.blink <= 0) S.blink = 2 + rnd() * 3.5;
  const asleep = sleeping && S.lf > 1.5;
  S.ey = asleep || S.act === 1 ? 1 : deadLie && S.aph === 1 ? 2 : S.dizzy > 1 ? 2 : S.startle > 0.6 ? 3 : S.blink < 0.13 || S.yawn > 1.2 ? 1 : 0;
  // the words in the top bar, and what floats above it
  const flying = !S.on && Math.hypot(S.vx, S.vy, S.vz) > 1.2;
  const goMood = S.gph === 0 ? 17 : [0, S.pounce > 0 ? 22 : 18, 19, 15, 20][S.gs];
  const actMood = [0, 23, 24, 26, 19, 28, 29, 30, S.aph === 0 ? 31 : 22, 16][S.act];
  S.md = S.pounce > 0 && !S.on ? 22 : S.jumping && !S.on ? 19 : flying ? 13 : !S.on ? 9 : S.bump > 0 ? 11 : S.startle > 0.4 ? 6 : S.dizzy > 1 ? 10 : S.on === -1 ? 14
    : S.slide ? (slope > 0.45 ? 7 : 8) : S.gs > 0 ? goMood : S.act ? actMood : S.found > 0 ? 21 : S.tada > 0 ? 27 : S.missed > 0 ? 32 : S.wake > 0 ? 5
    : S.yawn > 0 ? 16 : S.hello > 0 ? 12 : asleep ? 4 : S.lf > 1.5 && S.rest > 0 ? 25 : S.lf > 0.6 ? 3 : S.sniff > 0 ? 15 : S.walking ? 2 : S.lookY === 0 ? 1 : 0;
  S.ov = asleep ? 1 : S.startle > 0.6 || S.wake > 0 ? 2 : S.dizzy > 1 ? 3 : S.gs > 0 && S.gph === 0 ? 4 : 0;
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
export function foxTris(P, t = 0) {                 // the pose -> all triangles, in world coordinates (t: seconds, for fast wiggles)
  const lf = P.lf, sit = Math.min(1, lf), lie = Math.max(0, lf - 1), cr = P.cr || 0, bw = P.bw || 0, sc = P.sc || 0, dd = P.dd || 0;
  const drop = sit * 0.13 + lie * 0.3 + cr * 0.13 + bw * 0.09 + dd * 0.47;
  const pose = P.pt + sit * 0.42 * (1 - lie) - lie * 0.05 - bw * 0.25, pitch = pose + (P.fp || 0);
  let mb = mT(P.px, P.hop - drop, P.pz);
  mb = mMul(mb, mRy(P.hd));
  mb = mMul(mb, mT(0, 0.62, 0)); mb = mMul(mb, mRx(P.rl)); mb = mMul(mb, mRz(pitch)); mb = mMul(mb, mRx(P.tb)); mb = mMul(mb, mT(0, -0.62, 0));
  const out = [];
  partTris(out, BODY, mb);
  let mh = mMul(mb, mT(...HEAD_PIVOT));
  mh = mMul(mh, mRy(P.hy)); mh = mMul(mh, mRz(P.hp - pose * 0.6));
  partTris(out, HEAD, mh);
  for (const sd of [1, -1]) {                       // ears (the right one mirrored)
    const e = sd > 0 ? P.el : P.er;
    let me = mMul(mh, mT(EAR_PIVOT[0], EAR_PIVOT[1], EAR_PIVOT[2] * sd));
    me = mMul(me, mRz(e)); me = mMul(me, mRx(-0.15 * sd));
    if (sd < 0) me = mMul(me, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0]);
    const mesh = sd < 0 ? { v: EAR.v, t: EAR.t.map(([a, b, c, f, s]) => [a, c, b, ((f & 1) << 2) | (f & 2) | ((f >> 2) & 1), s]) } : EAR;
    partTris(out, mesh, me);
  }
  for (let i = 0; i < 4; i++) {                     // legs: walking swing, folded when sitting / lying, bowing, scratching
    const front = i < 2, hp = HIPS[i], sd = hp[2] > 0 ? 1 : -1;
    let sw = Math.sin(P.wp + LEG_PH[i]) * P.wa;
    if (front) sw += lie * 1.35 - sit * (1 - lie) * 0.42 - cr * 0.35 + bw * 1.1 + dd * 0.4; else sw += sit * 1.25 + lie * 0.25 + cr * 0.55 + bw * 0.25 - dd * 0.4;
    let ml = mMul(mb, mT(...hp)), spl = 0;
    if (!front && sc * sd > 0) {                    // the scratching leg: forward to the ear, out to the side, fast
      const a = Math.abs(sc);
      spl = -0.5 * a * sd; sw += a * (1.05 + 0.3 * Math.sin(t * 26));
    }
    ml = mMul(ml, mRz(sw));
    if (spl) ml = mMul(ml, mRx(spl));
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
  for (let i = 1; i < 6; i++) { const x = -BOX.x + (i * BOX.x) / 3; seg(fb, G(x, BOX.h, BOX.zb), G(x, BOX.h, BOX.zf), T + 2); }   // a thin grid on the ceiling
  for (let i = 1; i < 3; i++) { const z = BOX.zb + (i * (BOX.zf - BOX.zb)) / 3; seg(fb, G(-BOX.x, BOX.h, z), G(BOX.x, BOX.h, z), T + 2); }
  for (const x of [-BOX.x, BOX.x]) {                // the box's edges
    seg(fb, G(x, 0, BOX.zb), G(x, BOX.h, BOX.zb), T + 5); seg(fb, G(x, 0, BOX.zf), G(x, 0, BOX.zb), T + 6);
    seg(fb, G(x, BOX.h, BOX.zf), G(x, BOX.h, BOX.zb), T + 5); seg(fb, G(x, 0, BOX.zf), G(x, BOX.h, BOX.zf), T + 6);
  }
  for (const [y, z, c] of [[BOX.h, BOX.zb, 5], [0, BOX.zf, 6], [0, BOX.zb, 5], [BOX.h, BOX.zf, 6]]) seg(fb, G(-BOX.x, y, z), G(BOX.x, y, z), T + c);
  if (P.gm) {                                       // the tapped spot: a ring and a cross, pulsing
    const r = 0.13 + (fx ? 0.04 * Math.sin(t * 8) : 0);
    let q = G(P.gx + r, 0, P.gz);
    for (let i = 1; i <= 12; i++) { const a = (i * Math.PI) / 6, n = G(P.gx + Math.cos(a) * r, 0, P.gz + Math.sin(a) * r); seg(fb, q, n, T + 14); q = n; }
    seg(fb, G(P.gx - 0.07, 0, P.gz), G(P.gx + 0.07, 0, P.gz), T + 14); seg(fb, G(P.gx, 0, P.gz - 0.07), G(P.gx, 0, P.gz + 0.07), T + 14);
  }
  // its shadow
  const sh = 1 / (1 + P.hop * 1.6), hd = P.hd, pts = [];
  for (let i = 0; i < 14; i++) {
    const a = (i * 2 * Math.PI) / 14, lx = Math.cos(a) * 0.78 * sh, lz = Math.sin(a) * 0.3 * sh;
    pts.push(G(P.px + lx * Math.cos(hd) + lz * Math.sin(hd), 0, P.pz - lx * Math.sin(hd) + lz * Math.cos(hd)));
  }
  for (let i = 1; i + 1 < pts.length; i++) tri(fb, pts[0], pts[i], pts[i + 1], T + 1);
  // the fox: far triangles first, back faces left out, lit from the top left
  const { tris, mh } = foxTris(P, fx ? t : 0);
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
  else if (P.ov === 4) text8(fb, '?', hx - 7, hy - 22, 2, T + 15);
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
