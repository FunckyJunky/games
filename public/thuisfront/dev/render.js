// HOME FRONT screen renderer. The browser device (error.blue/thuisfront/dev) and the phone's preview
// (error.blue/thuisfront) both draw with this, and the ESP32 firmware (esp32/HomeFront/screen.cpp) does
// the same things the same way, so the preview shows what the board shows.
//
// The screen is 320x172. Every pixel is an index into a 64-colour table:
//   0-15 text levels, 16-31 video levels, 32-47 ERROR-DOS green (menus), 48 black, 49 red, 50 white,
//   255 = "see the colour picture underneath" (a photo, or colour video).

export const W = 320, H = 172;
export const T = 0, V = 16, U = 32, BLACK = 48, RED = 49, WHITE = 50, PLANE = 255;
export const NOFILTER = 4;                 // vpal / ppal 4 = no filter, real colours

// the four DOS looks, for the camera filter and for the text
export const PALS = [
  // lo/fg/hi: the camera picture from dark to light; bg: the "dark tint" background; tx: the letters
  { id: 'green',  name: 'DOS GREEN',  lo: [2, 12, 5],  bg: [3, 18, 8],   fg: [57, 255, 122],  hi: [190, 255, 210], tx: [57, 255, 122] },
  { id: 'yellow', name: 'DOS YELLOW', lo: [12, 8, 1],  bg: [20, 13, 2],  fg: [255, 196, 48],  hi: [255, 240, 190], tx: [255, 200, 52] },
  { id: 'blue',   name: 'DOS BLUE',   lo: [0, 6, 52],  bg: [0, 25, 166], fg: [110, 140, 255], hi: [242, 242, 255], tx: [242, 242, 255] },
  { id: 'gray',   name: 'DOS GRAY',   lo: [3, 3, 3],   bg: [12, 12, 12], fg: [192, 192, 192], hi: [255, 255, 255], tx: [225, 225, 225] },
];
export const ANIMS = ['static', 'scroll', 'rise', 'type', 'blink', 'wave', 'glitch', 'pulse'];
export const STILL_T = 5.3;                // effects off: the idle screen stands still at this moment (no glitch, cursor on)

const lerp = (a, b, f) => [a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f];
function level(p, from, v) { return v <= 12 ? lerp(from, p.fg, v / 12) : lerp(p.fg, p.hi, (v - 12) / 3); }
// 16 colours of the text: bg 0 black, 1 the look's dark colour, 2 inverted; k = brightness (pulse)
export function textRamp(pal, bg, k = 1) {
  const p = PALS[pal] || PALS[0], out = [];
  for (let v = 0; v < 16; v++) {
    let c;
    if (bg === 2) c = lerp(p.tx, p.lo, (v / 15) * k);
    else c = lerp(bg === 1 ? p.bg : [0, 0, 0], p.tx, (v / 15) * k);
    out.push(c);
  }
  return out;
}
export function videoRamp(pal) { const p = PALS[pal] || PALS[0], out = []; for (let v = 0; v < 16; v++) out.push(level(p, p.lo, v)); return out; }
// a photo through a DOS filter: its brightness (0-255) mapped onto the look's colours, smoothly
export function photoRamp(pal) {
  const p = PALS[pal] || PALS[0], out = new Uint32Array(256);
  for (let y = 0; y < 256; y++) {
    const c = level(p, p.lo, (y * 15) / 255);
    out[y] = 0xff000000 | (Math.round(c[2]) << 16) | (Math.round(c[1]) << 8) | Math.round(c[0]);
  }
  return out;
}
// RGBA pixels (w x h, any size) -> a 320x172 plane of colours, through a filter (NOFILTER = as they are)
export function filterPlane(rgba, w, h, pal) {
  const out = new Uint32Array(W * H), ramp = pal === NOFILTER ? null : photoRamp(pal);
  for (let y = 0; y < H; y++) {
    const sy = ((y * h) / H) | 0;
    for (let x = 0; x < W; x++) {
      const j = (sy * w + (((x * w) / W) | 0)) * 4, r = rgba[j], g = rgba[j + 1], b = rgba[j + 2];
      out[y * W + x] = ramp ? ramp[(r * 77 + g * 150 + b * 29) >> 8] : 0xff000000 | (b << 16) | (g << 8) | r;
    }
  }
  return out;
}
export function uiRamp() { const p = PALS[0], out = []; for (let v = 0; v < 16; v++) out.push(level(p, p.bg, v)); return out; }

// ---------------- the 8x8 font (public domain, Daniel Hepper's font8x8_basic, characters 32-126) ----------------
const FONT_HEX = '0000000000000000183c3c1818001800363600000000000036367f367f3636000c3e031e301f0c00006333180c6663001c361c6e3b336e000606030000000000180c0606060c1800060c1818180c060000663cff3c660000000c0c3f0c0c000000000000000c0c060000003f0000000000000000000c0c006030180c060301003e63737b6f673e000c0e0c0c0c0c3f001e33301c06333f001e33301c30331e00383c36337f3078003f031f3030331e001c06031f33331e003f3330180c0c0c001e33331e33331e001e33333e30180e00000c0c00000c0c00000c0c00000c0c06180c0603060c180000003f00003f0000060c1830180c06001e3330180c000c003e637b7b7b031e000c1e33333f3333003f66663e66663f003c66030303663c001f36666666361f007f46161e16467f007f46161e16060f003c66030373667c003333333f333333001e0c0c0c0c0c1e007830303033331e006766361e366667000f06060646667f0063777f7f6b63630063676f7b736363001c36636363361c003f66663e06060f001e3333333b1e38003f66663e366667001e33070e38331e003f2d0c0c0c0c1e003333333333333f0033333333331e0c006363636b7f7763006363361c1c3663003333331e0c0c1e007f6331184c667f001e06060606061e0003060c18306040001e18181818181e00081c36630000000000000000000000ff0c0c18000000000000001e303e336e000706063e66663b0000001e3303331e003830303e33336e0000001e333f031e001c36060f06060f0000006e33333e301f0706366e666667000c000e0c0c0c1e00300030303033331e070666361e3667000e0c0c0c0c0c1e000000337f7f6b630000001f333333330000001e3333331e0000003b66663e060f00006e33333e307800003b6e66060f0000003e031e301f00080c3e0c0c2c18000000333333336e0000003333331e0c000000636b7f7f3600000063361c36630000003333333e301f00003f190c263f00380c0c070c0c38001818180018181800070c0c380c0c07006e3b000000000000';
const FONT = new Uint8Array(FONT_HEX.length / 2);
for (let i = 0; i < FONT.length; i++) FONT[i] = parseInt(FONT_HEX.substr(i * 2, 2), 16);

// ---------------- small drawing tools on a 320x172 index buffer ----------------
export function fill(fb, x, y, w, h, c) {
  const x0 = Math.max(0, x | 0), y0 = Math.max(0, y | 0), x1 = Math.min(W, (x + w) | 0), y1 = Math.min(H, (y + h) | 0);
  for (let yy = y0; yy < y1; yy++) fb.fill(c, yy * W + x0, yy * W + x1);
}
export function textw(s, k = 1) { return s.length * 8 * k; }
export function text8(fb, s, x, y, k, c) {
  for (let i = 0; i < s.length; i++) {
    let ch = s.charCodeAt(i);
    if (ch < 32 || ch > 126) ch = 63;
    const g = (ch - 32) * 8;
    for (let r = 0; r < 8; r++) {
      const bits = FONT[g + r];
      if (!bits) continue;
      for (let b = 0; b < 8; b++) if (bits & (1 << b)) fill(fb, x + (i * 8 + b) * k, y + r * k, k, k, c);
    }
  }
}
export function center8(fb, s, y, k, c) { text8(fb, s, ((W - textw(s, k)) / 2) | 0, y, k, c); }
export function line(fb, x0, y0, x1, y1, c) {
  x0 |= 0; y0 |= 0; x1 |= 0; y1 |= 0;
  const dx = Math.abs(x1 - x0), dy = -Math.abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  let err = dx + dy, n = 0;
  for (;;) {
    if (x0 >= 0 && y0 >= 0 && x0 < W && y0 < H) fb[y0 * W + x0] = c;
    if ((x0 === x1 && y0 === y1) || ++n > 2000) break;
    const e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
// the same random numbers on the board (mulberry32)
export function rng(seed) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6D2B79F5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// ---------------- HFT1 text pictures and HFV1 video frames ----------------
export function encodeText(t) {             // t = {w, h, x, y, boxes:[[x,y,w,h]], px: Uint8Array levels}
  const head = 16 + t.boxes.length * 8;
  const out = new Uint8Array(head + t.px.length * 2 + 16);
  const dv = new DataView(out.buffer);
  out.set([72, 70, 84, 49]);
  dv.setUint16(4, t.w, true); dv.setUint16(6, t.h, true); dv.setInt16(8, t.x, true); dv.setInt16(10, t.y, true);
  dv.setUint16(12, t.boxes.length, true); dv.setUint16(14, 0, true);
  t.boxes.forEach((b, i) => { for (let k = 0; k < 4; k++) dv.setUint16(16 + i * 8 + k * 2, b[k], true); });
  let o = head, i = 0;
  const px = t.px, n = px.length;
  while (i < n) {
    const v = px[i];
    let run = 1;
    while (i + run < n && px[i + run] === v && run < 271) run++;
    if (run < 16) out[o++] = ((run - 1) << 4) | v;
    else { out[o++] = 0xf0 | v; out[o++] = run - 16; }
    i += run;
  }
  return out.slice(0, o);
}
export function decodeText(u8) {
  if (u8.length < 16 || u8[0] !== 72 || u8[1] !== 70 || u8[2] !== 84 || u8[3] !== 49) return null;
  const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength);
  const w = dv.getUint16(4, true), h = dv.getUint16(6, true), x = dv.getInt16(8, true), y = dv.getInt16(10, true), n = dv.getUint16(12, true);
  if (!w || !h || w * h > 1600000) return null;
  const boxes = [];
  let o = 16;
  for (let i = 0; i < n && o + 8 <= u8.length; i++, o += 8) boxes.push([dv.getUint16(o, true), dv.getUint16(o + 2, true), dv.getUint16(o + 4, true), dv.getUint16(o + 6, true)]);
  const px = new Uint8Array(w * h);
  let p = 0;
  while (o < u8.length && p < px.length) {
    const b = u8[o++], v = b & 15;
    let run = (b >> 4) + 1;
    if (run === 16) run = 16 + (u8[o++] || 0);
    px.fill(v, p, Math.min(px.length, p + run));
    p += run;
  }
  return { w, h, x, y, boxes, px };
}
export function encodeFrame(w, h, seq, lv) {  // lv = Uint8Array of w*h levels 0-15
  const out = new Uint8Array(12 + ((w * h + 1) >> 1));
  const dv = new DataView(out.buffer);
  out.set([72, 70, 86, 49]);
  dv.setUint16(4, w, true); dv.setUint16(6, h, true); dv.setUint32(8, seq >>> 0, true);
  for (let i = 0, o = 12; i < w * h; i += 2, o++) out[o] = (lv[i] << 4) | (lv[i + 1] || 0);
  return out;
}
export function decodeFrame(u8) {
  if (u8.length < 12 || u8[0] !== 72 || u8[1] !== 70 || u8[2] !== 86 || u8[3] !== 49) return null;
  const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength);
  const w = dv.getUint16(4, true), h = dv.getUint16(6, true), seq = dv.getUint32(8, true);
  if (!w || !h || 12 + ((w * h + 1) >> 1) > u8.length) return null;
  const px = new Uint8Array(w * h);
  for (let i = 0, o = 12; i < w * h; i += 2, o++) { px[i] = u8[o] >> 4; if (i + 1 < w * h) px[i + 1] = u8[o] & 15; }
  return { w, h, seq, px };
}

// ---------------- drawing the text picture ----------------
// draw the part (sx, sy, sw, sh) of the picture at (dx, dy); level 0 is see-through
function blit(fb, t, dx, dy, sx = 0, sy = 0, sw = t.w, sh = t.h) {
  const ya = Math.max(0, -dy), yb = Math.min(sh, H - dy), xa = Math.max(0, -dx), xb = Math.min(sw, W - dx);
  for (let yy = ya; yy < yb; yy++) {
    const row = (sy + yy) * t.w + sx, orow = (dy + yy) * W + dx;
    for (let xx = xa; xx < xb; xx++) { const v = t.px[row + xx]; if (v) fb[orow + xx] = T + v; }
  }
}

// draws the text with its animation at tA seconds; returns the text colour brightness (pulse) 0-1
export function drawText(fb, t, anim, sp, tA) {
  if (!t) return 1;
  switch (anim) {
    case 'scroll': {
      const pps = 15 + sp * 15, total = t.w + W;
      blit(fb, t, Math.round(W - ((tA * pps) % total)), t.y);
      return 1;
    }
    case 'rise': {
      const pps = 8 + sp * 7, total = t.h + H;
      blit(fb, t, t.x, Math.round(H - ((tA * pps) % total)));
      return 1;
    }
    case 'type': {
      const n = t.boxes.length;
      if (!n) { blit(fb, t, t.x, t.y); return 1; }
      const cps = 2 + sp * 2, steps = n + Math.ceil(cps * 3);
      const k = Math.floor(tA * cps) % steps, shown = Math.min(k, n);
      for (let i = 0; i < shown; i++) { const b = t.boxes[i]; blit(fb, t, t.x + b[0], t.y + b[1], b[0], b[1], b[2], b[3]); }
      const b = t.boxes[Math.max(0, shown - 1)];
      if (Math.floor(tA * 2.5) % 2 === 0 || shown < n) {
        const cx = shown ? b[0] + b[2] : b[0], ch = Math.max(4, Math.round(b[3] * 0.55));
        fill(fb, t.x + cx + 1, t.y + b[1] + Math.round((b[3] - ch) / 2), Math.max(2, Math.round(b[3] * 0.3)), ch, T + 15);
      }
      return 1;
    }
    case 'blink': {
      const hz = 0.5 + sp * 0.25;
      if (Math.floor(tA * hz * 2) % 2 === 0) blit(fb, t, t.x, t.y);
      return 1;
    }
    case 'wave': {
      const amp = 7, w = 1.5 + sp * 0.5;
      for (let i = 0; i < t.w; i++) {
        const dy = Math.round(amp * Math.sin((t.x + i) * 0.055 - tA * w));
        blit(fb, t, t.x + i, t.y + dy, i, 0, 1, t.h);
      }
      return 1;
    }
    case 'glitch': {
      blit(fb, t, t.x, t.y);
      glitchRows(fb, Math.floor(tA * 12) + 1, 0.12 + sp * 0.045, 6 + sp * 4);
      return 1;
    }
    case 'pulse': {
      blit(fb, t, t.x, t.y);
      return 0.3 + 0.7 * (0.5 + 0.5 * Math.sin(tA * (1.2 + sp * 0.45)));
    }
    default:
      blit(fb, t, t.x, t.y);
      return 1;
  }
}
// pull some rows sideways and draw a noise line (same numbers on the board for the same seed)
export function glitchRows(fb, seed, chance, maxShift) {
  const r = rng(seed);
  if (r() < chance) {
    const n = 1 + Math.floor(r() * 3);
    for (let s = 0; s < n; s++) {
      const sy = Math.floor(r() * H), sh = 2 + Math.floor(r() * 10), dx = Math.round((r() - 0.5) * 2 * maxShift);
      for (let y = sy; y < Math.min(H, sy + sh); y++) {
        const row = fb.slice(y * W, y * W + W);
        for (let x = 0; x < W; x++) { const sx = x - dx; fb[y * W + x] = sx >= 0 && sx < W ? row[sx] : row[0]; }
      }
    }
    if (r() < 0.5) {
      const y = Math.floor(r() * H);
      for (let x = 0; x < W; x++) if (r() < 0.3) fb[y * W + x] = T + 3 + Math.floor(r() * 6);
    }
  }
}

// ---------------- the screen ----------------
export class Screen {
  constructor(canvas) {
    this.cv = canvas; canvas.width = W; canvas.height = H;
    this.cx = canvas.getContext('2d');
    this.img = this.cx.createImageData(W, H);
    this.out = new Uint32Array(this.img.data.buffer);
    this.fb = new Uint8Array(W * H);
    this.lut = new Uint32Array(64); this.dim = new Uint32Array(64);
    this.state = null; this.text = null; this.frame = null; this.frameAt = 0;
    this.photo = null; this.photoPlane = null; this.photoKey = '';     // photo: {w, h, data RGBA}
    this.color = null; this.colorAt = 0;                               // colour video: a plane
    this.plane = null;
    this.t0 = performance.now(); this.textKey = '';
    this.phase = 'connecting'; this.phaseAt = performance.now();
    this.info = { name: '', wifi: '', ip: '', online: false, id: '' };
    this.offline = false;
    this.lastPal = '';
    this.setLut(U, uiRamp());
    this.lutSet(BLACK, [0, 0, 0]); this.lutSet(RED, [255, 40, 40]); this.lutSet(WHITE, [255, 255, 255]);
  }
  lutSet(i, c) {
    const r = Math.round(c[0]), g = Math.round(c[1]), b = Math.round(c[2]);
    this.lut[i] = 0xff000000 | (b << 16) | (g << 8) | r;
    this.dim[i] = 0xff000000 | (((b * 0.55) | 0) << 16) | (((g * 0.55) | 0) << 8) | ((r * 0.55) | 0);
  }
  setLut(base, ramp) { ramp.forEach((c, i) => this.lutSet(base + i, c)); }
  setPhase(p) { if (this.phase !== p) { this.phase = p; this.phaseAt = performance.now(); } }
  setState(s) {
    this.state = s;
    const c = s && s.comm;
    const key = c ? [c.tb, c.anim, c.speed].join('|') : '';
    if (key !== this.textKey) { this.textKey = key; this.t0 = performance.now(); }
  }
  setText(t) { this.text = t; this.t0 = performance.now(); }
  setFrame(f) { this.frame = f; this.frameAt = performance.now(); }
  setPhoto(p) { this.photo = p; this.photoKey = ''; }                 // p = {w, h, data} or null
  setColorFrame(f) { this.color = filterPlane(f.data, f.w, f.h, NOFILTER); this.colorAt = performance.now(); }
  restart() { this.t0 = performance.now(); }

  render(now = performance.now()) {
    const fb = this.fb, s = this.state;
    const t = (now - this.phaseAt) / 1000;
    let scan = false;
    const fx = !s || s.fx !== 0;                // fx 0: all effects off (still text, no scanlines, no glitches, no blinking)
    if (this.phase === 'connecting') drawConnecting(fb, t, this.info);
    else if (this.phase === 'connected') drawConnected(fb, t, this.info);
    else if (!s || !s.screen) fb.fill(BLACK);
    else if (s.app === 'comm') { this.drawComm(now, fx); scan = fx && !!s.scan; }
    else drawIdle(fb, fx ? now / 1000 : STILL_T, this.info);
    if (this.offline && s && s.screen && this.phase === 'run' && (!fx || Math.floor(now / 600) % 2 === 0)) fill(fb, W - 7, 2, 5, 5, RED);
    const out = this.out, fbl = this.fb, P = s && s.app === 'comm' && s.screen && this.phase === 'run' ? this.plane : null;
    for (let y = 0; y < H; y++) {
      const dimRow = scan && (y & 1), L = dimRow ? this.dim : this.lut, o = y * W;
      if (!P) { for (let x = 0; x < W; x++) out[o + x] = L[fbl[o + x]]; continue; }
      for (let x = 0; x < W; x++) {
        const i = fbl[o + x];
        if (i !== PLANE) { out[o + x] = L[i]; continue; }
        const c = P[o + x];
        out[o + x] = dimRow ? 0xff000000 | ((c >> 1) & 0x7f7f7f) : c;
      }
    }
    this.cx.putImageData(this.img, 0, 0);
  }

  drawComm(now, fx = true) {
    const fb = this.fb, c = this.state.comm, tA = (now - this.t0) / 1000;
    const vpal = c.vpal === NOFILTER ? 0 : c.vpal, vkey = 'v' + vpal;
    if (this.lastPal !== vkey) { this.setLut(V, videoRamp(vpal)); this.lastPal = vkey; }
    this.plane = null;
    if (c.mode === 'photo') {
      const pal = c.ppal == null ? NOFILTER : c.ppal, key = pal + '|' + (this.photo ? this.photo.data.length : 0);
      if (this.photo && this.photoKey !== key) { this.photoPlane = filterPlane(this.photo.data, this.photo.w, this.photo.h, pal); this.photoKey = key; }
      if (this.photo) { fb.fill(PLANE); this.plane = this.photoPlane; }
      else { fb.fill(T); text8(fb, 'NO PHOTO YET', 4, 4, 1, U + 6); }
    } else if (c.mode === 'video' || c.mode === 'both') {
      const grey = this.frame && now - this.frameAt < 3000 ? this.frameAt : 0, col = this.color && now - this.colorAt < 3000 ? this.colorAt : 0;
      if (col && col >= grey) { fb.fill(PLANE); this.plane = this.color; }
      else if (grey) drawVideo(fb, this.frame);
      else if (c.mode === 'video') drawNoSignal(fb, fx ? now : 0);
      else { fb.fill(T); text8(fb, 'NO VIDEO', 4, 4, 1, U + 6); }
    } else fb.fill(T);
    let k = 1;
    if (c.mode !== 'video') {
      if (this.text) k = drawText(fb, this.text, fx ? c.anim : 'static', c.speed, tA);
      else if (c.mode === 'text') center8(fb, 'NO TEXT YET', 82, 1, U + 5);
    }
    this.setLut(T, textRamp(c.tpal, c.mode === 'text' ? c.bg : 0, k));
  }
}

export function drawVideo(fb, f) {
  const px = f.px;
  if (f.w === W && f.h === H) { for (let i = 0; i < W * H; i++) fb[i] = V + px[i]; return; }
  if (f.w * 2 === W && f.h * 2 === H) {
    for (let y = 0; y < f.h; y++) for (let x = 0; x < f.w; x++) {
      const v = V + px[y * f.w + x], o = y * 2 * W + x * 2;
      fb[o] = fb[o + 1] = fb[o + W] = fb[o + W + 1] = v;
    }
    return;
  }
  for (let y = 0; y < H; y++) { const sy = ((y * f.h) / H) | 0; for (let x = 0; x < W; x++) fb[y * W + x] = V + px[sy * f.w + (((x * f.w) / W) | 0)]; }
}
export function drawNoSignal(fb, now) {
  const r = rng(Math.floor(now / 50));
  for (let y = 0; y < H; y += 2) for (let x = 0; x < W; x += 2) {
    const v = V + Math.floor(r() * 7), o = y * W + x;
    fb[o] = fb[o + 1] = fb[o + W] = fb[o + W + 1] = v;
  }
  fill(fb, 80, 70, 160, 32, BLACK);
  center8(fb, 'NO SIGNAL', 78, 2, U + 15);
}

// ---------------- the menu screens ----------------
function bars(fb, left, right) {
  fill(fb, 0, 0, W, 12, U + 12);
  text8(fb, left, 4, 2, 1, U + 0);
  if (right) text8(fb, right, W - 4 - textw(right), 2, 1, U + 0);
}
// a little 3D house, Home Front's logo
const HOUSE_V = [[-1, 0, -0.8], [1, 0, -0.8], [1, 1.2, -0.8], [-1, 1.2, -0.8], [-1, 0, 0.8], [1, 0, 0.8], [1, 1.2, 0.8], [-1, 1.2, 0.8],
  [-1, 2.05, 0], [1, 2.05, 0], [-0.25, 0, -0.8], [-0.25, 0.7, -0.8], [0.25, 0.7, -0.8], [0.25, 0, -0.8],
  [0.42, 0.5, -0.8], [0.8, 0.5, -0.8], [0.8, 0.88, -0.8], [0.42, 0.88, -0.8], [0.55, 1.62, -0.25], [0.55, 2.35, -0.25]];
const HOUSE_E = [[0, 1], [1, 2], [2, 3], [3, 0], [4, 5], [5, 6], [6, 7], [7, 4], [0, 4], [1, 5], [2, 6], [3, 7],
  [3, 8], [7, 8], [2, 9], [6, 9], [8, 9], [10, 11], [11, 12], [12, 13], [14, 15], [15, 16], [16, 17], [17, 14], [18, 19]];
export function drawHouse(fb, t, cx, cy, f, c) {
  const yaw = t * 0.7, pitch = 0.12, cy0 = Math.cos(yaw), sy0 = Math.sin(yaw), cp = Math.cos(pitch), sp = Math.sin(pitch);
  const P = HOUSE_V.map(([x, y, z]) => {
    y -= 1.0;
    const x1 = x * cy0 + z * sy0, z1 = -x * sy0 + z * cy0;
    const y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp + 5.5;
    return [cx + (x1 * f) / z2, cy - (y2 * f) / z2];
  });
  for (const [a, b] of HOUSE_E) line(fb, P[a][0], P[a][1], P[b][0], P[b][1], c);
  const ph = (t * 1.2) % 1;                                    // the antenna sends
  for (let k = 0; k < 2; k++) {
    const r = 3 + ((ph + k * 0.5) % 1) * 12, ax = P[19][0], ay = P[19][1];
    for (let a = -0.9; a < 0.9; a += 0.15) { const x = ax + Math.cos(a - Math.PI / 2) * r, y = ay + Math.sin(a - Math.PI / 2) * r; if (x >= 0 && y >= 0 && x < W && y < H) fb[(y | 0) * W + (x | 0)] = U + 9; }
  }
}
export function drawIdle(fb, t, info) {
  fb.fill(U);
  bars(fb, 'HOME FRONT', info.name || info.id || '');
  drawHouse(fb, t, 66, 92, 150, U + 12);
  text8(fb, 'HOME FRONT', 128, 40, 2, U + 15);
  text8(fb, 'WAITING FOR A WEB APP', 128, 66, 1, U + 12);
  if (Math.floor(t * 1.9) % 2 === 0) fill(fb, 128 + textw('WAITING FOR A WEB APP') + 2, 66, 7, 8, U + 12);
  text8(fb, 'CONTROL IT FROM', 128, 92, 1, U + 6);
  text8(fb, 'error.blue/thuisfront', 128, 104, 1, U + 13);
  fill(fb, 0, 158, W, 1, U + 4);
  const left = info.wifi ? info.wifi + ' ' + (info.ip || '') : (info.ip || '');
  text8(fb, left.slice(0, 26), 4, 162, 1, U + 7);
  const st = info.online ? 'ONLINE' : 'OFFLINE';
  text8(fb, st, W - 4 - textw(st), 162, 1, info.online ? U + 11 : RED);
  if (Math.floor(t / 4) % 3 === 0 && (t % 4) < 0.12) glitchRows(fb, Math.floor(t * 30), 1, 14);
}
export function drawConnected(fb, t, info) {
  fb.fill(U);
  bars(fb, 'HOME FRONT', info.name || '');
  const k = 3, s = 'CONNECTED', w = textw(s, k) + 34, x = ((W - w) / 2) | 0;
  // a tick in a box
  fill(fb, x, 44, 26, 26, U + 12); fill(fb, x + 3, 47, 20, 20, U + 0);
  line(fb, x + 7, 57, x + 11, 62, U + 15); line(fb, x + 8, 57, x + 12, 62, U + 15);
  line(fb, x + 11, 62, x + 19, 50, U + 15); line(fb, x + 12, 62, x + 20, 50, U + 15);
  text8(fb, s, x + 34, 45, k, U + 15);
  if (info.wifi) center8(fb, 'WIFI ' + info.wifi, 90, 1, U + 12);
  if (info.ip) center8(fb, 'IP ' + info.ip, 104, 1, U + 8);
  center8(fb, 'LOADING error.blue/thuisfront/dev', 124, 1, U + 7);
  const left = Math.max(0, 1 - t / 3);
  fill(fb, 40, 142, 240, 6, U + 3); fill(fb, 40, 142, Math.round(240 * left), 6, U + 12);
}
export function drawConnecting(fb, t, info) {
  fb.fill(U);
  bars(fb, 'HOME FRONT', info.name || '');
  drawHouse(fb, t, 160, 70, 120, U + 10);
  center8(fb, 'CONNECTING' + '...'.slice(0, Math.floor(t * 3) % 4), 120, 2, U + 14);
  center8(fb, 'error.blue', 146, 1, U + 7);
}
