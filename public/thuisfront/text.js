// HOME FRONT: turns the text, font, size and style into the HFT1 picture the screen animates.
// The phone draws the letters (any web font works), the screen only moves the picture around.
import { W, H } from './dev/render.js';

export const FONTS = [
  { id: 'vt323',     name: 'DOS',        css: '"VT323"',          wt: 400, lh: 0.95 },
  { id: 'press',     name: 'PIXEL',      css: '"Press Start 2P"', wt: 400, lh: 1.25 },
  { id: 'pixelify',  name: 'PIXEL SANS', css: '"Pixelify Sans"',  wt: 600, lh: 1.1 },
  { id: 'ptmono',    name: 'MONO',       css: '"PT Mono"',        wt: 400, lh: 1.1 },
  { id: 'russo',     name: 'ARMY',       css: '"Russo One"',      wt: 400, lh: 1.1 },
  { id: 'blackops',  name: 'STENCIL',    css: '"Black Ops One"',  wt: 400, lh: 1.1 },
  { id: 'oswald',    name: 'TALL',       css: '"Oswald"',         wt: 500, lh: 1.15 },
  { id: 'rubikmono', name: 'HEAVY',      css: '"Rubik Mono One"', wt: 400, lh: 1.15 },
  { id: 'unbounded', name: 'ROUND',      css: '"Unbounded"',      wt: 600, lh: 1.15 },
  { id: 'orbitron',  name: 'SPACE',      css: '"Orbitron"',       wt: 700, lh: 1.15 },
  { id: 'caveat',    name: 'HAND',       css: '"Caveat"',         wt: 600, lh: 1.0 },
  { id: 'pacifico',  name: 'SCRIPT',     css: '"Pacifico"',       wt: 400, lh: 1.3 },
  { id: 'ptserif',   name: 'SERIF',      css: '"PT Serif"',       wt: 700, lh: 1.1 },
  { id: 'sans',      name: 'SANS',       css: 'system-ui, Arial, sans-serif', wt: 700, lh: 1.1 },
];
export const FONTS_CSS = 'https://fonts.googleapis.com/css2?family=VT323&family=Press+Start+2P&family=Pixelify+Sans:wght@600&family=PT+Mono&family=Russo+One&family=Black+Ops+One&family=Oswald:wght@500&family=Rubik+Mono+One&family=Unbounded:wght@600&family=Orbitron:wght@700&family=Caveat:wght@600&family=Pacifico&family=PT+Serif:wght@700&display=swap';
export const fontOf = (id) => FONTS.find((f) => f.id === id) || FONTS[0];
export const fontSpec = (F, size) => `${F.wt} ${size}px ${F.css}`;

const M = 6;                         // margin around the text on the screen
const MAX_PX = 1500000, MAX_W = 16000;

function wrapLines(ctx, para, maxW) {
  const out = [];
  const fits = (s) => ctx.measureText(s).width <= maxW;
  let cur = '';
  const pushLong = (word) => {        // a word wider than the screen: break it between letters
    let piece = '';
    for (const ch of word) {
      if (piece && !fits(piece + ch)) { out.push(piece); piece = ch; } else piece += ch;
    }
    return piece;
  };
  for (const part of para.split(/(\s+)/)) {
    if (!part) continue;
    const test = cur + part;
    if (fits(test)) { cur = test; continue; }
    if (/^\s+$/.test(part)) { out.push(cur); cur = ''; continue; }
    if (cur.trim()) out.push(cur.trimEnd());
    cur = fits(part) ? part : pushLong(part);
  }
  out.push(cur.trimEnd());
  return out;
}

function layout(ctx, text, F, size, mode) {
  ctx.font = fontSpec(F, size);
  const lh = Math.max(1, Math.ceil(size * F.lh));
  const maxW = W - 2 * M;
  let lines;
  if (mode === 'line') lines = [text.replace(/\s*\n\s*/g, '   ')];
  else { lines = []; for (const p of text.split('\n')) lines.push(...wrapLines(ctx, p, maxW)); }
  const widths = lines.map((l) => Math.ceil(ctx.measureText(l).width));
  return { lines, widths, lh, cw: Math.max(1, ...widths), ch: lines.length * lh };
}

// c = {text, font, size (0 = fit), align l/c/r, valign t/m/b, style, caps, anim}
// returns {w, h, x, y, boxes, px} for encodeText()
export async function makeText(c) {
  const F = fontOf(c.font);
  let text = String(c.text || '');
  if (c.caps) text = text.toUpperCase();
  text = text.replace(/\r/g, '').replace(/\t/g, '    ');
  const mode = c.anim === 'scroll' ? 'line' : c.anim === 'rise' ? 'tall' : 'block';
  if (!text.trim()) return { w: 1, h: 1, x: 0, y: 0, boxes: [], px: new Uint8Array(1) };
  try { await document.fonts.load(fontSpec(F, 48), text.slice(0, 200)); } catch { /* use what we have */ }

  const cv = document.createElement('canvas');
  const ctx = cv.getContext('2d', { willReadFrequently: true });
  let size = c.size | 0, L;
  if (!size) {
    if (mode === 'line') size = Math.floor((H - 2 * M) / F.lh);
    else if (mode === 'tall') size = 34;
    else {                                  // the biggest size that fits the screen
      let lo = 8, hi = 172;
      while (lo < hi) {
        const mid = (lo + hi + 1) >> 1, t = layout(ctx, text, F, mid, mode);
        if (t.ch <= H - 2 * M && t.cw <= W - 2 * M + 1) lo = mid; else hi = mid - 1;
      }
      size = lo;
    }
  }
  for (;;) {
    L = layout(ctx, text, F, size, mode);
    const padX = Math.ceil(size * 0.12), padY = Math.ceil(size * 0.18);
    const bw = L.cw + 2 * padX, bh = (mode === 'block' ? Math.min(L.ch, H * 3) : L.ch) + 2 * padY;
    if ((bw * bh <= MAX_PX && bw <= MAX_W) || size <= 8) { L.padX = padX; L.padY = padY; L.bw = Math.min(bw, MAX_W); L.bh = bh; break; }
    size = Math.max(8, Math.floor(size * 0.85));
  }
  const { padX, padY, bw, bh, lh } = L;
  cv.width = bw; cv.height = bh;
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, bw, bh);
  ctx.font = fontSpec(F, size);
  ctx.textBaseline = 'middle'; ctx.lineJoin = 'round';
  const al = c.align || 'c';
  const boxes = [];
  L.lines.forEach((line, i) => {
    const lw = L.widths[i];
    const lx = padX + (al === 'l' ? 0 : al === 'r' ? L.cw - lw : Math.round((L.cw - lw) / 2));
    const ly = padY + i * lh + lh / 2;
    if (c.style === 'shadow') { const d = Math.max(1, Math.round(size / 22)); ctx.fillStyle = '#5a5a5a'; ctx.fillText(line, lx + d, ly + d); }
    if (c.style === 'outline') {
      ctx.strokeStyle = '#fff'; ctx.lineWidth = Math.max(1.5, size / 14); ctx.strokeText(line, lx, ly);
      ctx.fillStyle = '#000'; ctx.fillText(line, lx, ly);
    } else {
      if (c.style === 'bold') { ctx.strokeStyle = '#fff'; ctx.lineWidth = Math.max(1, size / 20); ctx.strokeText(line, lx, ly); }
      ctx.fillStyle = '#fff'; ctx.fillText(line, lx, ly);
    }
    if (mode !== 'line' && boxes.length < 2000) {            // the letter boxes, for the typewriter
      let pre = '', px0 = 0;
      for (const ch of line) {
        const w1 = ctx.measureText(pre + ch).width;
        const x0 = Math.floor(lx + px0), x1 = Math.ceil(lx + w1);
        const y0 = i === 0 ? 0 : padY + i * lh, y1 = i === L.lines.length - 1 ? bh : padY + (i + 1) * lh;
        if (boxes.length < 2000) boxes.push([Math.max(0, x0 - (pre ? 0 : padX)), y0, Math.max(1, Math.min(bw, x1 + (ch === ' ' ? 0 : 1)) - Math.max(0, x0 - (pre ? 0 : padX))), y1 - y0]);
        pre += ch; px0 = w1;
      }
    }
  });
  const img = ctx.getImageData(0, 0, bw, bh).data, px = new Uint8Array(bw * bh);
  for (let i = 0, j = 0; i < px.length; i++, j += 4) px[i] = Math.round((img[j] * 15) / 255);

  const va = c.valign || 'm';
  let x = 0, y = 0;
  const bx = al === 'l' ? M : al === 'r' ? W - M - L.cw : Math.round((W - L.cw) / 2);
  const blockH = mode === 'line' ? lh : L.ch;
  const by = va === 't' ? M : va === 'b' ? H - M - blockH : Math.round((H - blockH) / 2);
  if (mode === 'line') { x = 0; y = by - padY; }
  else if (mode === 'tall') { x = bx - padX; y = 0; }
  else { x = bx - padX; y = by - padY; }
  return { w: bw, h: bh, x, y, boxes, px, size };
}
