// Small MD5 for Uint8Array (RFC 1321). Returns lowercase hex.
const S = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21];
const K = new Int32Array(64);
for (let i = 0; i < 64; i++) K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296) | 0;

export function md5(bytes) {
  const n = bytes.length;
  const total = ((n + 8) >>> 6) + 1 << 6;          // padded length in bytes
  const tail = new Uint8Array(total - (n & ~63));    // last full blocks incl. padding
  tail.set(bytes.subarray(n & ~63));
  tail[n & 63] = 0x80;
  const bits = n * 8;
  const dv = new DataView(tail.buffer);
  dv.setUint32(tail.length - 8, bits >>> 0, true);
  dv.setUint32(tail.length - 4, Math.floor(bits / 4294967296), true);

  let a0 = 0x67452301, b0 = 0xefcdab89 | 0, c0 = 0x98badcfe | 0, d0 = 0x10325476;
  const M = new Int32Array(16);
  const block = (src, off) => {
    for (let j = 0; j < 16; j++) {
      const o = off + j * 4;
      M[j] = src[o] | src[o + 1] << 8 | src[o + 2] << 16 | src[o + 3] << 24;
    }
    let A = a0, B = b0, C = c0, D = d0;
    for (let i = 0; i < 64; i++) {
      let F, g;
      if (i < 16) { F = (B & C) | (~B & D); g = i; }
      else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) & 15; }
      else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) & 15; }
      else { F = C ^ (B | ~D); g = (7 * i) & 15; }
      F = (F + A + K[i] + M[g]) | 0;
      A = D; D = C; C = B;
      const s = S[(i >> 4) * 4 + (i & 3)];
      B = (B + (F << s | F >>> (32 - s))) | 0;
    }
    a0 = (a0 + A) | 0; b0 = (b0 + B) | 0; c0 = (c0 + C) | 0; d0 = (d0 + D) | 0;
  };
  const full = n & ~63;
  for (let off = 0; off < full; off += 64) block(bytes, off);
  for (let off = 0; off < tail.length; off += 64) block(tail, off);
  let hex = '';
  for (const v of [a0, b0, c0, d0]) for (let k = 0; k < 4; k++) hex += ((v >>> (8 * k)) & 255).toString(16).padStart(2, '0');
  return hex;
}
