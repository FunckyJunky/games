// A small GIF decoder for HOME FRONT: an (animated) GIF -> its frames as whole pictures, the way a browser
// shows them (transparency, disposal, interlace). Only the frames you ask for are copied out, so long GIFs
// can be thinned without keeping every frame.
//
//   const g = gifParse(bytes);            // {w, h, frames: [{delay}]} or null when it is not a GIF
//   gifDecode(bytes, g, keep, (i, rgba) => ...)   // keep(i) = true for the frames you want; rgba = w*h*4

export function gifParse(u8) {
  if (u8.length < 13 || String.fromCharCode(u8[0], u8[1], u8[2]) !== 'GIF') return null;
  const w = u8[6] | (u8[7] << 8), h = u8[8] | (u8[9] << 8), flags = u8[10];
  let p = 13;
  if (flags & 0x80) p += 3 * (2 << (flags & 7));
  const frames = [];
  let delay = 0;
  while (p < u8.length) {
    const b = u8[p++];
    if (b === 0x3b) break;                                   // the end
    if (b === 0x21) {                                        // an extension
      const label = u8[p++];
      if (label === 0xf9 && u8[p] >= 4) delay = (u8[p + 2] | (u8[p + 3] << 8)) * 10;
      while (p < u8.length && u8[p]) p += u8[p] + 1;
      p++;
      continue;
    }
    if (b !== 0x2c) break;                                   // not an image: broken, stop here
    const f = u8[p + 8];
    p += 9;
    if (f & 0x80) p += 3 * (2 << (f & 7));
    p++;                                                     // LZW code size
    while (p < u8.length && u8[p]) p += u8[p] + 1;
    p++;
    frames.push({ delay: delay < 20 ? 100 : delay });        // 0 or 10 ms: browsers show it 100 ms
    delay = 0;
  }
  return w && h && frames.length ? { w, h, frames } : null;
}

function lzw(data, minSize, out) {
  const clear = 1 << minSize, eoi = clear + 1, npix = out.length;
  const prefix = new Uint16Array(4096), suffix = new Uint8Array(4096), stack = new Uint8Array(4097);
  for (let i = 0; i < clear; i++) suffix[i] = i;
  let size = minSize + 1, mask = (1 << size) - 1, next = eoi + 1, old = -1, first = 0, cur = 0, bits = 0, ip = 0, op = 0;
  while (op < npix) {
    while (bits < size) { if (ip >= data.length) return op; cur |= data[ip++] << bits; bits += 8; }
    const code = cur & mask; cur >>>= size; bits -= size;
    if (code === clear) { size = minSize + 1; mask = (1 << size) - 1; next = eoi + 1; old = -1; continue; }
    if (code === eoi) break;
    if (old < 0) { if (code >= clear) break; out[op++] = code; old = first = code; continue; }
    let c = code, sp = 0;
    if (c >= next) { if (c > next) break; stack[sp++] = first; c = old; }   // the code being made right now
    while (c >= clear) { stack[sp++] = suffix[c]; c = prefix[c]; }
    first = c; stack[sp++] = c;
    while (sp && op < npix) out[op++] = stack[--sp];
    if (next < 4096) { prefix[next] = old; suffix[next] = first; next++; if (next > mask && size < 12) { size++; mask = (1 << size) - 1; } }
    old = code;
  }
  return op;
}

export function gifDecode(u8, g, keep, onFrame) {
  const W = g.w, H = g.h, flags = u8[10];
  const canvas = new Uint8ClampedArray(W * H * 4);           // starts transparent
  let p = 13, gct = null;
  if (flags & 0x80) { const n = 2 << (flags & 7); gct = u8.subarray(p, p + 3 * n); p += 3 * n; }
  let trans = -1, disposal = 0, idx = 0;
  while (p < u8.length && idx < g.frames.length) {
    const b = u8[p++];
    if (b === 0x3b) break;
    if (b === 0x21) {
      const label = u8[p++];
      if (label === 0xf9 && u8[p] >= 4) { const pf = u8[p + 1]; disposal = (pf >> 2) & 7; trans = pf & 1 ? u8[p + 4] : -1; }
      while (p < u8.length && u8[p]) p += u8[p] + 1;
      p++;
      continue;
    }
    if (b !== 0x2c) break;
    const fx = u8[p] | (u8[p + 1] << 8), fy = u8[p + 2] | (u8[p + 3] << 8), fw = u8[p + 4] | (u8[p + 5] << 8), fh = u8[p + 6] | (u8[p + 7] << 8), f = u8[p + 8];
    p += 9;
    let ct = gct;
    if (f & 0x80) { const n = 2 << (f & 7); ct = u8.subarray(p, p + 3 * n); p += 3 * n; }
    const minSize = Math.min(11, Math.max(2, u8[p++]));
    let len = 0;
    for (let q = p; q < u8.length && u8[q]; q += u8[q] + 1) len += u8[q];
    const data = new Uint8Array(len);
    let o = 0;
    while (p < u8.length && u8[p]) { const n = u8[p]; data.set(u8.subarray(p + 1, p + 1 + n), o); o += n; p += n + 1; }
    p++;
    const pix = new Uint8Array(fw * fh);
    lzw(data, minSize, pix);
    const rows = new Int32Array(fh);                         // which canvas row each decoded row goes to
    if (f & 0x40) { let r = 0; for (const [s, st] of [[0, 8], [4, 8], [2, 4], [1, 2]]) for (let y = s; y < fh; y += st) rows[r++] = y; }
    else for (let y = 0; y < fh; y++) rows[y] = y;
    const saved = disposal === 3 ? canvas.slice() : null;
    if (ct) {
      for (let r = 0; r < fh; r++) {
        const y = fy + rows[r];
        if (y >= H) continue;
        for (let x = 0; x < fw; x++) {
          const ci = pix[r * fw + x];
          if (ci === trans || fx + x >= W || ci * 3 + 2 >= ct.length) continue;
          const d = (y * W + fx + x) * 4;
          canvas[d] = ct[ci * 3]; canvas[d + 1] = ct[ci * 3 + 1]; canvas[d + 2] = ct[ci * 3 + 2]; canvas[d + 3] = 255;
        }
      }
    }
    if (keep(idx)) onFrame(idx, canvas.slice());
    if (disposal === 2) {                                    // clear its rectangle
      for (let y = fy; y < Math.min(H, fy + fh); y++) canvas.fill(0, (y * W + fx) * 4, (y * W + Math.min(W, fx + fw)) * 4);
    } else if (disposal === 3 && saved) canvas.set(saved);   // back to what was there before
    trans = -1; disposal = 0; idx++;
  }
  return idx;
}
