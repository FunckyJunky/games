// HOME FRONT screen renderer. The browser device (error.blue/thuisfront/dev) and the phone's preview
// (error.blue/thuisfront) both draw with this, and the ESP32 firmware (esp32/HomeFront/screen.cpp) does
// the same things the same way, so the preview shows what the board shows.
//
// The screen is 320x172. Every pixel is an index into a 64-colour table:
//   0-15 text levels, 16-31 video levels, 32-47 ERROR-DOS green (menus), 48 black, 49 red, 50 white,
//   255 = "see the colour picture underneath" (a photo, or colour video).

export const W = 320, H = 172;
export const T = 0, V = 16, U = 32, BLACK = 48, RED = 49, WHITE = 50, PLANE = 255;
// the ETA train scene (real colours, whatever the DOS look): yellow and blue train, glass, rails, dark grey, brick, lamps, platform
export const NSY = 51, NSB = 52, GLASS = 53, RAIL = 54, DARK = 55, BRICK = 56, LAMP = 57, PLAT = 58, ROOF = 59, LEAF = 60, LEAFD = 61, TRUNK = 62, CREAM = 63;
export const SCENE = [[255, 201, 23], [0, 48, 130], [24, 34, 52], [150, 150, 160], [52, 52, 58], [156, 70, 48], [255, 236, 160], [112, 112, 118],
  [150, 52, 36], [56, 158, 64], [28, 100, 46], [110, 74, 44], [232, 220, 190]];
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
// HFA1, an animated photo (a GIF made 320x172 on the phone): "HFA1", u16 n frames, u16 0,
// n x (u16 delay in ms, u32 bytes), then the n JPEGs one after the other. All numbers little-endian.
export const ANIM_FRAMES = 48, ANIM_BYTES = 1000000;
export function animParse(u8) {
  if (!u8 || u8.length < 8 || String.fromCharCode(u8[0], u8[1], u8[2], u8[3]) !== 'HFA1') return null;
  const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength), n = dv.getUint16(4, true);
  if (n < 1 || n > 64) return null;
  let o = 8 + 6 * n;
  if (o > u8.length) return null;
  const out = [];
  for (let i = 0; i < n; i++) {
    const delay = dv.getUint16(8 + 6 * i, true), len = dv.getUint32(10 + 6 * i, true);
    if (o + len > u8.length) return null;
    out.push({ jpg: u8.subarray(o, o + len), delay: Math.min(10000, Math.max(20, delay)) });
    o += len;
  }
  return out;
}
export function animBuild(frames) {             // frames = [{jpg (Uint8Array), delay (ms)}]
  const n = frames.length, size = 8 + 6 * n + frames.reduce((a, f) => a + f.jpg.length, 0);
  const u8 = new Uint8Array(size), dv = new DataView(u8.buffer);
  u8.set([72, 70, 65, 49]); dv.setUint16(4, n, true);
  let o = 8 + 6 * n;
  frames.forEach((f, i) => { dv.setUint16(8 + 6 * i, Math.min(10000, Math.max(20, Math.round(f.delay))), true); dv.setUint32(10 + 6 * i, f.jpg.length, true); u8.set(f.jpg, o); o += f.jpg.length; });
  return u8;
}
// which frame of an animation shows t ms after it started (frames = [{delay}], it loops)
export function animFrameAt(frames, t) {
  const total = frames.reduce((a, f) => a + f.delay, 0);
  if (frames.length < 2 || !total) return 0;
  t = ((t % total) + total) % total;
  let i = 0;
  while (i < frames.length - 1 && t >= frames[i].delay) { t -= frames[i].delay; i++; }
  return i;
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
// Cyrillic capitals in the same style (А-Я, then Ё Є І Ї Ґ); small letters use the capitals
const CYR = new Uint8Array('0c1e33333f3333007f46063e66663f003f66663e66663f007f46060606060f007c666666667f63007f46161e16467f006b6b3e1c3e6b6b001e33301c30331e006363736b676363001c63736b676363006766361e36666700786c6c6c6c66630063777f7f6b6363003333333f333333001c36636363361c007f636363636363003f66663e06060f003c66030303663c003f2d0c0c0c0c1e003333333e30331e00083e6b6b3e081c006363361c1c36630033333333337f60003333333e303078006b6b6b6b6b6b7f006b6b6b6b6bff80000706063e66663e006363636f6b6b6f000606063e66663e001e33303c30331e00334b4b4f4b4b33007e63637e786c670012007f461e467f003c66031f03663c001e0c0c0c0c0c1e0012001e0c0c0c1e00607f060606060f00'.match(/../g).map((h) => parseInt(h, 16)));
// a character (code point) -> its glyph: 32-126 the 8x8 font, 0x100 + n Cyrillic glyph n, else '?'
export function glyphOf(cp) {
  if (cp >= 32 && cp <= 126) return cp;
  if (cp >= 0x410 && cp <= 0x42f) return 0x100 + cp - 0x410;
  if (cp >= 0x430 && cp <= 0x44f) return 0x100 + cp - 0x430;
  switch (cp) {
    case 0x401: case 0x451: return 0x100 + 32;   // Ё
    case 0x404: case 0x454: return 0x100 + 33;   // Є
    case 0x406: case 0x456: return 0x100 + 34;   // І
    case 0x407: case 0x457: return 0x100 + 35;   // Ї
    case 0x490: case 0x491: return 0x100 + 36;   // Ґ
    case 0x2018: case 0x2019: case 0x2bc: return 39;
    case 0xab: case 0xbb: case 0x201c: case 0x201d: case 0x201e: return 34;
    case 0x2013: case 0x2014: return 45;
  }
  return 63;
}
export function textw(s, k = 1) { return Array.from(String(s)).length * 8 * k; }
export function text8(fb, s, x, y, k, c) {
  let i = 0;
  for (const ch of String(s)) {
    const g = glyphOf(ch.codePointAt(0));
    for (let r = 0; r < 8; r++) {
      const bits = g >= 0x100 ? CYR[(g - 0x100) * 8 + r] : FONT[(g - 32) * 8 + r];
      if (!bits) continue;
      for (let b = 0; b < 8; b++) if (bits & (1 << b)) fill(fb, x + (i * 8 + b) * k, y + r * k, k, k, c);
    }
    i++;
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
    SCENE.forEach((c, i) => this.lutSet(NSY + i, c));
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
  setPhoto(p) { this.photo = p; this.photoKey = ''; this.photoPlanes = []; this.photoT0 = performance.now(); }   // p = {w, h, data, frames?: [{data, delay}]} or null
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
    else if (s.app === 'eta') {
      const e = s.eta || {}, tz = (s.clock || {}).tz, at = (sec) => (sec ? clockNow(tz, sec * 1000) : null);
      drawEta(fb, e, this.nowMs ? this.nowMs() : Date.now(), true, at(e.to), at(e.from), fx);
      this.setLut(T, textRamp(e.pal || 0, 0, 1));
      scan = fx && !!s.scan;
    }
    else if (s.app === 'clock') {
      const k = s.clock || {};
      drawClock(fb, this.timeNow ? this.timeNow() : clockNow(k.tz), k, fx);
      this.setLut(T, textRamp(k.pal || 0, k.bg ? 1 : 0, 1));
      scan = fx && !!s.scan;
    }
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
      const pal = c.ppal == null ? NOFILTER : c.ppal, P = this.photo;
      if (P) {                                // a photo, or an animation: the frame for now (it keeps playing, also with effects off)
        if (this.photoKey !== String(pal)) { this.photoKey = String(pal); this.photoPlanes = []; }
        const i = P.frames ? animFrameAt(P.frames, now - this.photoT0) : 0;
        let pl = this.photoPlanes[i];
        if (!pl) pl = this.photoPlanes[i] = filterPlane(P.frames ? P.frames[i].data : P.data, P.w, P.h, pal);
        fb.fill(PLANE); this.plane = pl;
      }
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

// ---------------- CLOCK: big dot-matrix digits ----------------
// 5x7 glyphs, one byte per row, bit 4 = the left column. ':' is one column wide.
const GLYPH = {
  0: [0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E], 1: [0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E],
  2: [0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F], 3: [0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E],
  4: [0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02], 5: [0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E],
  6: [0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E], 7: [0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08],
  8: [0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E], 9: [0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C],
  '-': [0, 0, 0, 0x1F, 0, 0, 0], ' ': [0, 0, 0, 0, 0, 0, 0], '+': [0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0],
};
const COLON = [0, 0, 1, 0, 1, 0, 0];
const WDAY = ['SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT'];
const MONTH = ['JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC'];
// one round-ish dot of size d at x, y
export function dot(fb, x, y, d, c) {
  for (let r = 0; r < d; r++) {
    const e = r === 0 || r === d - 1, n = r === 1 || r === d - 2;
    const inset = d >= 6 ? (e ? 2 : n ? 1 : 0) : d >= 4 ? (e ? 1 : 0) : 0;
    fill(fb, x + inset, y + r, d - 2 * inset, 1, c);
  }
}
// a string of digits, ' ', '-' and ':' as a dot matrix; unlit dots are drawn dim (like a real LED matrix)
function matrix(fb, str, x, y, p, d, lit, ghost, colonOn) {
  for (const ch of str) {
    const colon = ch === ':', w = colon ? 1 : 5, rows = colon ? null : GLYPH[ch] || GLYPH[' '];
    for (let r = 0; r < 7; r++) for (let c = 0; c < w; c++) {
      const on = colon ? colonOn && COLON[r] : (rows[r] >> (4 - c)) & 1;
      dot(fb, x + c * p, y + r * p, d, on ? lit : ghost);
    }
    x += (w + 1) * p;
  }
}
// t = {h, m, s, wd, d, mo, y, ms} or null (no time yet); k = the clock settings
export function drawClock(fb, t, k, fx) {
  fb.fill(T);
  const big = k.sec === 2, h24 = k.h24 !== 0, pad = (v) => (v < 10 ? '0' : '') + v;
  let str, line = '';
  if (!t) { str = big ? '--:--:--' : '--:--'; line = 'NO TIME YET'; }
  else {
    let h = t.h;
    if (!h24) { h = h % 12 || 12; }
    str = (h24 ? pad(h) : (h < 10 ? ' ' : '') + h) + ':' + pad(t.m) + (big ? ':' + pad(t.s) : '');
    if (k.date !== 0) line = WDAY[t.wd] + ' ' + t.d + ' ' + MONTH[t.mo] + ' ' + t.y;
    if (!h24) line += (line ? '  ' : '') + (t.h < 12 ? 'AM' : 'PM');
  }
  const p = big ? 8 : 12, d = p - 2, cols = big ? 39 : 25;
  const tw = cols * p - (p - d), th = 7 * p - (p - d);
  const row = !big && k.sec !== 0, total = th + (row ? 13 : 0) + (line ? 28 : 0);
  let y = Math.round((H - total) / 2);
  const x = Math.round((W - tw) / 2);
  matrix(fb, str, x, y, p, d, T + 15, T + 2, !fx || !t || t.ms < 500);
  y += th;
  if (row) {                                   // the seconds: a row of 60 dots filling up
    y += 10;
    for (let i = 0; i < 60; i++) dot(fb, x + i * 5, y, 3, t && i <= t.s ? T + 15 : T + 2);
    y += 3;
  }
  if (line) { y += 12; text8(fb, line, ((W - textw(line, 2)) / 2) | 0, y, 2, T + 11); }
}
// ---------------- ETA: when I arrive, with a description and a progress bar (the same as scrEta in screen.cpp) ----------------
// word wrap for the 8x8 font: lines of at most max letters; a word longer than that is cut
export function wrapWords(s, max) {
  const out = [];
  let line = '';
  for (let w of String(s || '').split(' ')) {
    if (!w) continue;
    while (w.length > max) { if (line) { out.push(line); line = ''; } out.push(w.slice(0, max)); w = w.slice(max); }
    if (!w) continue;
    if (!line) line = w; else if (line.length + 1 + w.length <= max) line += ' ' + w; else { out.push(line); line = w; }
  }
  if (line) out.push(line);
  return out;
}
const hm = (t) => (t.h < 10 ? '0' : '') + t.h + ':' + (t.m < 10 ? '0' : '') + t.m;
// a description about a train (English, Dutch, Ukrainian, Russian) -> the progress bar is a train going to the station
// a description about a train or a car -> the progress bar is a little scene: 1 = a train going to the station,
// 2 = a car driving home, 3 = a taxi driving home. Words are compared whole ('CAR') or by their start ('TRAIN*');
// the first word that matches decides. The same lists as SCENE_WORDS in screen.cpp.
export const SCENE_WORDS = [
  [1, 'TRAIN*', 'TREIN*', 'SPRINTER*', 'INTERCITY*', 'ПОТЯГ*', 'ПОТЯЗ*', 'ПОЇЗД*', 'ПОЕЗД*', 'ЕЛЕКТРИЧК*', 'ЭЛЕКТРИЧК*'],
  [3, 'TAXI*', 'ТАКСІ*', 'ТАКСИ*'],
  [2, 'CAR', 'CARS', 'AUTO', 'AUTOS', 'AUTOMOBIL*', 'АВТО', 'АВТОМОБІЛ*', 'АВТОМОБИЛ*', 'АВТІВК*', 'МАШИН*'],
];
const glyphUp = (cp) => { const g = glyphOf(cp); return g >= 97 && g <= 122 ? g - 32 : g; };
const isLetter = (g) => (g >= 65 && g <= 90) || g >= 0x100;
export function sceneOf(text) {
  const g = Array.from(String(text || '')).map((c) => glyphUp(c.codePointAt(0)));
  for (let i = 0; i < g.length;) {
    if (!isLetter(g[i])) { i++; continue; }
    let j = i; while (j < g.length && isLetter(g[j])) j++;
    for (const [kind, ...words] of SCENE_WORDS) for (const w of words) {
      const pre = w.endsWith('*'), k = Array.from(pre ? w.slice(0, -1) : w).map((c) => glyphUp(c.codePointAt(0)));
      if (pre ? j - i < k.length : j - i !== k.length) continue;
      if (k.every((v, n) => g[i + n] === v)) return kind;
    }
    i = j;
  }
  return 0;
}
export const isTrain = (text) => sceneOf(text) === 1;
// one car of the train, x = its left end; the cab (front) car has a sloped nose and a pantograph
function trainCar(fb, x, y, len, cab, wheel) {
  fill(fb, x + 1, y + 8, len - 2, 1, DARK);                       // roof
  fill(fb, x, y + 9, len, 12, NSY);                               // yellow body
  for (let wx = x + 3; wx + 5 <= x + len - (cab ? 10 : 3); wx += 7) fill(fb, wx, y + 11, 5, 4, GLASS);
  fill(fb, x, y + 17, len, 2, NSB);                               // blue band
  const door = x + (cab ? 12 : (len >> 1) - 2);
  fill(fb, door, y + 11, 4, 9, NSB);                              // blue door
  fill(fb, door + 1, y + 12, 2, 3, GLASS);
  fill(fb, x + 2, y + 21, len - 4, 1, DARK);                      // under the floor
  for (const wx of [x + 4, x + 9, x + len - 10, x + len - 5]) {   // wheels
    fill(fb, wx - 1, y + 21, 3, 3, BLACK);
    fill(fb, wx - 1 + wheel, y + 22, 1, 1, RAIL);
  }
  if (cab) {
    for (let r = 0; r < 4; r++) fill(fb, x + len - 4 + r, y + 9 + r, 4 - r, 1, T);   // sloped nose
    fill(fb, x + len - 9, y + 11, 5, 4, GLASS);                   // windscreen
    fill(fb, x + len - 3, y + 15, 2, 2, LAMP);                    // headlight
    fill(fb, x + 10, y + 7, 8, 1, RAIL);                          // pantograph
    fill(fb, x + 12, y + 6, 1, 1, RAIL); fill(fb, x + 13, y + 5, 1, 1, RAIL); fill(fb, x + 14, y + 4, 1, 1, RAIL);
    fill(fb, x + 13, y + 3, 1, 1, RAIL); fill(fb, x + 12, y + 2, 1, 1, RAIL);
    fill(fb, x + 9, y + 1, 7, 1, RAIL);
  }
}
// overhead wire, a platform under a canopy with a clock and a sign, the station with its clock tower, a buffer stop,
// and the train: at p = 0 on the left, at p = 1 at the buffer stop (28 pixels high)
export function drawTrainScene(fb, y, p, wheel) {
  fill(fb, 0, y, 196, 1, RAIL);                                   // overhead wire and its masts
  for (const mx of [40, 104, 168]) { fill(fb, mx, y, 1, 22, PLAT); fill(fb, mx - 3, y + 1, 7, 1, PLAT); }
  fill(fb, 190, y + 20, 94, 3, PLAT);                             // the platform
  for (const cx of [198, 230, 262]) fill(fb, cx, y + 4, 2, 16, PLAT);   // canopy columns
  const bx = 284;                                                  // the station building
  fill(fb, bx, y + 8, W - bx, 16, BRICK);
  fill(fb, bx + 1, y + 5, W - bx - 1, 1, DARK); fill(fb, bx - 1, y + 6, W - bx + 1, 2, DARK);
  fill(fb, bx, y + 8, W - bx, 1, CREAM);
  fill(fb, bx + 4, y + 10, 28, 3, NSB);                           // the blue name board
  for (let x = bx + 6; x < bx + 30; x += 4) fill(fb, x, y + 11, 3, 1, WHITE);
  for (const wx of [bx + 3, bx + 9, bx + 23, bx + 29]) { fill(fb, wx + 1, y + 14, 1, 1, LAMP); fill(fb, wx, y + 15, 3, 6, LAMP); }
  fill(fb, bx + 15, y + 15, 6, 1, CREAM); fill(fb, bx + 15, y + 16, 6, 8, GLASS);   // the door
  fill(fb, bx + 13, y + 1, 10, 7, BRICK);                         // clock tower
  fill(fb, bx + 15, y, 6, 1, DARK); fill(fb, bx + 12, y + 1, 12, 1, DARK);
  fill(fb, bx + 16, y + 2, 4, 1, WHITE); fill(fb, bx + 15, y + 3, 6, 3, WHITE); fill(fb, bx + 16, y + 6, 4, 1, WHITE);
  fill(fb, bx + 17, y + 3, 1, 2, BLACK); fill(fb, bx + 18, y + 4, 2, 1, BLACK);
  fill(fb, 279, y + 14, 5, 5, RED);                               // buffer stop
  fill(fb, 279, y + 15, 5, 1, WHITE); fill(fb, 279, y + 17, 5, 1, WHITE);
  fill(fb, 280, y + 19, 2, 5, DARK);
  fill(fb, 0, y + 24, bx, 1, RAIL);                               // rails and sleepers
  for (let x = 2; x < bx; x += 6) fill(fb, x, y + 25, 3, 2, DARK);
  const L = 66, front = 4 + L + Math.floor(p * (278 - 4 - L));
  trainCar(fb, front - L, y, 30, false, wheel);
  fill(fb, front - L + 30, y + 15, 2, 2, DARK);                   // the coupling
  trainCar(fb, front - 34, y, 34, true, wheel);
  fill(fb, 194, y + 1, 90, 2, DARK); fill(fb, 194, y + 3, 90, 1, PLAT);   // the canopy (in front of the pantograph)
  fill(fb, 214, y + 4, 1, 1, RAIL);                               // the platform clock
  fill(fb, 213, y + 5, 3, 1, WHITE); fill(fb, 212, y + 6, 5, 2, WHITE); fill(fb, 213, y + 8, 3, 1, WHITE);
  fill(fb, 214, y + 6, 1, 2, BLACK);
  fill(fb, 244, y + 4, 1, 1, RAIL); fill(fb, 239, y + 5, 11, 3, NSB); fill(fb, 241, y + 6, 7, 1, WHITE);   // platform sign
}
// a car (or a taxi) seen from the side, x = its left end, 32 long, driving to the right; phase -1 = standing still
function car(fb, x, y, taxi, phase) {
  const body = taxi ? NSY : RED;
  fill(fb, x + 10, y + 9, 12, 1, body);                           // the roof and the cabin
  fill(fb, x + 9, y + 10, 15, 1, body); fill(fb, x + 8, y + 11, 17, 1, body); fill(fb, x + 7, y + 12, 19, 1, body);
  fill(fb, x + 10, y + 10, 6, 1, GLASS); fill(fb, x + 17, y + 10, 6, 1, GLASS);
  fill(fb, x + 9, y + 11, 7, 2, GLASS); fill(fb, x + 17, y + 11, 7, 2, GLASS); fill(fb, x + 24, y + 12, 1, 1, GLASS);
  fill(fb, x + 1, y + 13, 30, 1, body); fill(fb, x, y + 14, 32, 2, body);   // the body, with arches over the wheels
  fill(fb, x, y + 16, 4, 3, body); fill(fb, x + 11, y + 16, 11, 3, body); fill(fb, x + 29, y + 16, 3, 3, body);
  if (taxi) {
    fill(fb, x + 13, y + 7, 6, 2, WHITE); fill(fb, x + 14, y + 8, 4, 1, BLACK);   // the TAXI sign
    for (let i = 0; i < 13; i++) fill(fb, x + 3 + i * 2, y + 14 + (i & 1), 2, 1, BLACK);   // checkers
  }
  fill(fb, x + 16, y + 13, 1, 5, DARK);                           // door line
  fill(fb, x + 13, y + 15, 2, 1, DARK); fill(fb, x + 19, y + 15, 2, 1, DARK);   // handles
  fill(fb, x + 30, y + 14, 2, 2, LAMP);                           // headlight
  fill(fb, x, y + 14, 1, 2, taxi ? RED : ROOF);                   // rear light
  fill(fb, x + 1, y + 19, 3, 1, DARK); fill(fb, x + 11, y + 19, 11, 1, DARK); fill(fb, x + 29, y + 19, 2, 1, DARK);   // sills, bumpers
  for (const cx of [x + 7, x + 25]) {                             // wheels: grey tyre, rim, a spoke that turns
    fill(fb, cx - 1, y + 16, 3, 1, DARK); fill(fb, cx - 2, y + 17, 5, 3, DARK); fill(fb, cx - 1, y + 20, 3, 1, DARK);
    fill(fb, cx - 1, y + 17, 3, 3, PLAT); fill(fb, cx, y + 18, 1, 1, RAIL);
    const sp = phase > 0 ? 1 : 0;
    fill(fb, sp ? cx + 1 : cx - 1, y + 17, 1, 1, DARK); fill(fb, sp ? cx - 1 : cx + 1, y + 19, 1, 1, DARK);
  }
  if (phase >= 0) { fill(fb, x - 3 - phase * 2, y + 18, 2, 1, RAIL); fill(fb, x - 7 - phase * 3, y + 16, 2, 2, PLAT); }   // exhaust
}
// a road with lamp posts and a tree, a house with lit windows on the right, and the car: at p = 0 on the left,
// at p = 1 in front of the house (28 pixels high)
export function drawCarScene(fb, y, p, phase, taxi) {
  for (const lx of [66, 186]) {                                   // lamp posts
    fill(fb, lx, y + 4, 1, 18, PLAT); fill(fb, lx, y + 3, 4, 1, PLAT); fill(fb, lx + 2, y + 4, 3, 1, LAMP);
  }
  fill(fb, 125, y + 12, 3, 10, TRUNK);                            // a tree
  fill(fb, 122, y + 1, 9, 1, LEAF); fill(fb, 120, y + 2, 13, 2, LEAF); fill(fb, 119, y + 4, 15, 6, LEAF);
  fill(fb, 120, y + 10, 13, 2, LEAF); fill(fb, 122, y + 12, 9, 1, LEAF);
  fill(fb, 128, y + 6, 5, 4, LEAFD); fill(fb, 126, y + 10, 6, 2, LEAFD); fill(fb, 121, y + 3, 2, 1, LEAFD);
  const hx = 284;                                                  // the house
  fill(fb, hx + 25, y, 3, 6, BRICK); fill(fb, hx + 24, y, 5, 1, DARK);   // chimney
  for (let r = 0; r < 9; r++) fill(fb, hx + 15 - 2 * r, y + r, 4 + 4 * r, 1, ROOF);
  fill(fb, hx + 1, y + 9, 32, 13, BRICK);
  fill(fb, hx + 15, y + 4, 4, 3, LAMP); fill(fb, hx + 17, y + 4, 1, 3, DARK);   // attic window
  for (const wx of [hx + 3, hx + 24]) { fill(fb, wx, y + 12, 7, 5, LAMP); fill(fb, wx + 3, y + 12, 1, 5, DARK); fill(fb, wx, y + 14, 7, 1, DARK); fill(fb, wx, y + 17, 7, 1, CREAM); }
  fill(fb, hx + 14, y + 13, 6, 9, LEAFD); fill(fb, hx + 18, y + 17, 1, 1, LAMP);   // the green door
  fill(fb, 0, y + 22, W, 1, PLAT);                                // the road
  fill(fb, 0, y + 23, W, 4, DARK);
  for (let x = 4; x < W; x += 14) fill(fb, x, y + 24, 7, 1, CREAM);
  car(fb, 4 + Math.floor(p * (279 - 4 - 32)), y + 1, taxi, phase);
}
// the words on the ETA screen: 0 = English, 1 = Ukrainian (the same as ETA_TXT in screen.cpp)
export const ETA_TXT = [
  ['ETA', 'ARRIVES ', 'ARRIVED ', 'ARRIVED', 'ON MY WAY SINCE ', 'ON MY WAY', 'ON THE WAY SINCE ', 'ARRIVING NOW', 'LATE', 'NO ARRIVAL SET', 'WAITING FOR THE TIME'],
  ['ПРИЇЗД', 'БУДУ О ', 'НА МІСЦІ О ', 'НА МІСЦІ', 'В ДОРОЗІ З ', 'В ДОРОЗІ', 'В ДОРОЗІ З ', 'ОСЬ-ОСЬ БУДУ', 'ЗАПІЗНЮЮСЬ', 'ЧАС НЕ ЗАДАНО', 'ЧЕКАЮ НА ЧАС'],
];
// e = {from, to (unix seconds), text, pal, done, lang}; now in ms; toT / fromT = local {h, m} of to / from
export function drawEta(fb, e, now, known, toT, fromT, fx) {
  fb.fill(T);
  const ms = ((now % 1000) + 1000) % 1000, blink = !fx || ms < 500;
  fill(fb, 0, 0, W, 12, T + 12);
  const L = ETA_TXT[e.lang === 1 ? 1 : 0];
  text8(fb, L[0], 4, 2, 1, T + 0);
  const top = !e.to || !known || !toT ? '' : (e.done ? L[2] : L[1]) + hm(toT);
  if (top) text8(fb, top, W - 4 - textw(top, 1), 2, 1, T + 0);
  let k = 2, lines = wrapWords(e.text, 19);
  if (lines.length > 2) { k = 1; lines = wrapWords(e.text, 38).slice(0, 3); }
  const lh = k === 2 ? 18 : 11, descH = lines.length * lh, gap = lines.length ? 6 : 0;
  const scene = sceneOf(e.text), barH = scene ? 28 : 12;
  const total = descH + gap + 54 + 10 + barH + 12 + 8;
  let y = 12 + Math.max(4, Math.round((160 - total) / 2));
  lines.forEach((l, i) => text8(fb, l, ((W - textw(l, k)) / 2) | 0, y + i * lh, k, T + 15));
  y += descH + gap;
  // the big digits (or ARRIVED)
  let str = '--:--', colon = false, bottom = L[9], p = 0, running = false;
  if (e.to && !known) bottom = L[10];
  else if (e.to && e.done) { str = ''; p = 1; bottom = fromT ? L[6] + hm(fromT) : ''; }
  else if (e.to) {
    const span = (e.to - e.from) * 1000, left = e.to * 1000 - now;
    p = span > 0 ? Math.min(1, Math.max(0, (now - e.from * 1000) / span)) : 1;
    const pad = (v) => (v < 10 ? '0' : '') + v;
    const fmt = (secs) => { const h = Math.floor(secs / 3600), m = Math.floor((secs % 3600) / 60), s = secs % 60; return (h ? h + ':' + pad(m) : pad(m)) + ':' + pad(s); };
    if (left > 0) { str = fmt(Math.ceil(left / 1000)); colon = true; running = true; bottom = fromT ? L[4] + hm(fromT) : L[5]; }
    else {
      const late = Math.floor(-left / 1000);
      if (late < 60) { str = '00:00'; bottom = L[7]; } else { str = '+' + fmt(late); bottom = L[8]; }
    }
  }
  if (str) {
    let cols = -1;
    for (const ch of str) cols += (ch === ':' ? 1 : 5) + 1;
    matrix(fb, str, Math.round((W - (cols * 8 - 2)) / 2), y, 8, 6, T + 15, T + 2, colon ? blink : true);
  } else text8(fb, L[3], ((W - textw(L[3], 4)) / 2) | 0, y + 11, 4, T + 15);
  y += 54 + 10;
  // the progress bar: 50 blocks, or a train on its way to the station, or a car (taxi) on its way home
  const phase = running && fx ? Math.floor(ms / 250) % 2 : -1;
  if (scene === 1) drawTrainScene(fb, y, e.to ? p : 0, phase < 0 ? 0 : phase);
  else if (scene) drawCarScene(fb, y, e.to ? p : 0, phase, scene === 3);
  else {
    const filled = Math.floor(p * 50);
    for (let i = 0; i < 50; i++) {
      const c = i < filled ? T + 15 : i === filled && running && fx ? (ms < 500 ? T + 15 : T + 6) : T + 2;
      fill(fb, 10 + i * 6, y, 5, 12, c);
    }
  }
  y += barH + 12;
  if (bottom) text8(fb, bottom, 10, y, 1, T + 11);
  if (e.to && known) { const pc = Math.floor(p * 100) + '%'; text8(fb, pc, W - 10 - textw(pc, 1), y, 1, T + 11); }
}

// the time in a time zone (IANA name; "" = this device's own), via Intl
const zoneFmt = {};
export function clockNow(tz, at) {
  const now = at == null ? new Date() : new Date(at), ms = now.getMilliseconds();
  let f = zoneFmt[tz || ''];
  if (f === undefined) {
    try { f = tz ? new Intl.DateTimeFormat('en-US', { timeZone: tz, hourCycle: 'h23', year: 'numeric', month: 'numeric', day: 'numeric', weekday: 'short', hour: 'numeric', minute: 'numeric', second: 'numeric' }) : null; } catch { f = null; }
    zoneFmt[tz || ''] = f;
  }
  if (!f) return { h: now.getHours(), m: now.getMinutes(), s: now.getSeconds(), wd: now.getDay(), d: now.getDate(), mo: now.getMonth(), y: now.getFullYear(), ms };
  const q = {};
  for (const x of f.formatToParts(now)) q[x.type] = x.value;
  return { h: +q.hour % 24, m: +q.minute, s: +q.second, wd: ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'].indexOf(q.weekday), d: +q.day, mo: +q.month - 1, y: +q.year, ms };
}
// an IANA time zone as a POSIX TZ string (for the board), e.g. Europe/Amsterdam -> STD-1DST-2,M3.5.0/2,M10.5.0/3
export function posixTZ(tz) {
  let f;
  try { f = new Intl.DateTimeFormat('en-US', { timeZone: tz, hourCycle: 'h23', year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' }); } catch { return ''; }
  const off = (ms) => {                         // minutes east of UTC at that moment
    const q = {}; for (const x of f.formatToParts(new Date(ms))) q[x.type] = +x.value;
    return Math.round((Date.UTC(q.year, q.month - 1, q.day, q.hour % 24, q.minute, q.second) - Math.floor(ms / 1000) * 1000) / 60000);
  };
  const hm = (m) => { const a = Math.abs(m), s = (m < 0 ? '-' : '') + Math.floor(a / 60); return a % 60 ? s + ':' + String(a % 60).padStart(2, '0') : s; };
  const y = new Date().getUTCFullYear(), t0 = Date.UTC(y, 0, 1), t1 = Date.UTC(y + 1, 0, 1), DAY = 86400000;
  const changes = [];
  let prev = off(t0);
  for (let t = t0 + DAY; t <= t1; t += DAY) {
    const o = off(t);
    if (o === prev) continue;
    let lo = t - DAY, hi = t;                   // find the minute it changed
    while (hi - lo > 60000) { const mid = lo + Math.floor((hi - lo) / 120000) * 60000; if (off(mid) === prev) lo = mid; else hi = mid; }
    changes.push({ at: hi, from: prev, to: o });
    prev = o;
  }
  const offs = [off(t0), off(Date.UTC(y, 6, 1))], std = Math.min(...offs), dst = Math.max(...offs);
  if (changes.length !== 2 || std === dst) return 'STD' + hm(-off(Date.now()));
  const rule = (c) => {                         // the moment, in the local time just before it, as Mm.w.d/h[:mm]
    const l = new Date(c.at + c.from * 60000), day = l.getUTCDate(), dim = new Date(Date.UTC(l.getUTCFullYear(), l.getUTCMonth() + 1, 0)).getUTCDate();
    const wk = day + 7 > dim ? 5 : Math.ceil(day / 7), mins = l.getUTCHours() * 60 + l.getUTCMinutes();
    return 'M' + (l.getUTCMonth() + 1) + '.' + wk + '.' + l.getUTCDay() + '/' + hm(mins);
  };
  const on = changes.find((c) => c.to === dst), offc = changes.find((c) => c.to === std);
  return 'STD' + hm(-std) + 'DST' + hm(-dst) + ',' + rule(on) + ',' + rule(offc);
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
