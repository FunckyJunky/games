// Radio tracks for the games: every SoundRooms folder of a game (public/games/<game>/ or public/<game>/) gets a tracks.json that
// lists its .mp3 and .m4a files and how long each one is (the game reads it to find its channels).
//
// It runs by itself before every deploy (wrangler.jsonc "build"), so adding a track is only:
// put the file in the SoundRooms folder and push. Each file can be up to 25 MB (Cloudflare's limit
// for one file); a bigger one is left off the site and the list, with a warning in the build log.
//
//   node tools/soundrooms.mjs
import { readdirSync, readFileSync, writeFileSync, statSync, existsSync } from 'node:fs';
import { join } from 'node:path';

const PUBLIC = 'public';
const MAX = 25 * 1024 * 1024;
const AUDIO = /\.(mp3|m4a|m4b|mp4)$/i;

// ---------------------------------------------------------------- how long a track is (seconds)
const BR1 = [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320];
const BR2 = [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160];
const SR = { 3: [44100, 48000, 32000], 2: [22050, 24000, 16000], 0: [11025, 12000, 8000] };

function header(b, p) {           // an MPEG audio layer III frame header at p, or null
  if (b[p] !== 0xff || (b[p + 1] & 0xe0) !== 0xe0) return null;
  const ver = (b[p + 1] >> 3) & 3, layer = (b[p + 1] >> 1) & 3, bri = b[p + 2] >> 4, sri = (b[p + 2] >> 2) & 3;
  if (ver === 1 || layer !== 1 || bri === 0 || bri === 15 || sri === 3) return null;
  const br = (ver === 3 ? BR1 : BR2)[bri], sr = SR[ver][sri], pad = (b[p + 2] >> 1) & 1, mono = (b[p + 3] >> 6) === 3;
  const len = Math.floor((ver === 3 ? 144 : 72) * br * 1000 / sr) + pad;
  return { ver, br, sr, len, mono, spf: ver === 3 ? 1152 : 576 };
}
function mp3Seconds(b) {
  let p = 0, end = b.length;
  if (end > 10 && b.toString('latin1', 0, 3) === 'ID3')
    p = 10 + ((b[6] & 127) << 21 | (b[7] & 127) << 14 | (b[8] & 127) << 7 | (b[9] & 127)) + (b[5] & 0x10 ? 10 : 0);
  if (end > 128 && b.toString('latin1', end - 128, end - 125) === 'TAG') end -= 128;
  let h = null;
  for (; p + 4 < end && p < 1 << 20; p++) {      // the first frame: a header with another right after it
    const a = header(b, p);
    if (a && header(b, p + a.len)) { h = a; break; }
  }
  if (!h) return 0;
  const side = h.ver === 3 ? (h.mono ? 17 : 32) : (h.mono ? 9 : 17);
  const x = p + 4 + side, tag = b.toString('latin1', x, x + 4);
  if ((tag === 'Xing' || tag === 'Info') && (b.readUInt32BE(x + 4) & 1)) return b.readUInt32BE(x + 8) * h.spf / h.sr;
  if (b.toString('latin1', p + 36, p + 40) === 'VBRI') return b.readUInt32BE(p + 36 + 14) * h.spf / h.sr;
  return (end - p) * 8 / (h.br * 1000);
}
function boxSeconds(b, i) {        // an mvhd or mdhd box at i (its name): duration / timescale
  const v = b[i + 4];
  const ts = b.readUInt32BE(i + (v ? 24 : 16)), du = v ? Number(b.readBigUInt64BE(i + 28)) : b.readUInt32BE(i + 20);
  return ts ? du / ts : 0;
}
function m4aSeconds(b) {
  // the sound track's own length (its mdhd, the one whose hdlr says 'soun'); else the whole movie's
  for (let i = b.indexOf('mdhd', 0, 'latin1'); i >= 0; i = b.indexOf('mdhd', i + 4, 'latin1')) {
    const h = b.indexOf('hdlr', i, 'latin1');
    if (h > 0 && h - i < 200 && b.toString('latin1', h + 12, h + 16) === 'soun') return boxSeconds(b, i);
  }
  const i = b.indexOf('mvhd', 0, 'latin1');
  return i < 0 ? 0 : boxSeconds(b, i);
}
function seconds(path) {
  try {
    const b = readFileSync(path);
    return /\.mp3$/i.test(path) ? mp3Seconds(b) : m4aSeconds(b);
  } catch (e) { return 0; }
}

// ---------------------------------------------------------------- every game's SoundRooms
// (a game in public/games/<game>/ or at its own short address, public/<game>/, like error.blue/voyage)
const tooBig = [];
const games = [];
for (const base of ['games', '.']) {
  let names = [];
  try { names = readdirSync(join(PUBLIC, base)); } catch (e) {}
  for (const n of names) games.push(base === '.' ? n : `games/${n}`);
}
for (const g of games) {
  const dir = join(PUBLIC, g, 'SoundRooms');
  if (!existsSync(dir) || !statSync(dir).isDirectory()) continue;
  const tracks = [];
  for (const f of readdirSync(dir).sort()) {
    if (!AUDIO.test(f) || f.startsWith('.')) continue;
    const path = join(dir, f), size = statSync(path).size;
    if (size > MAX) {
      tooBig.push(`${g}/SoundRooms/${f}`);
      console.warn(`soundrooms: ${g}/${f} is ${(size / 1048576).toFixed(1)} MB, over 25 MB: left off the site`);
      continue;
    }
    const dur = Math.round(seconds(path) * 100) / 100;
    tracks.push(dur > 1 ? { file: f, dur } : { file: f });
    console.log(`soundrooms: ${g}/${f} ${dur > 1 ? dur.toFixed(1) + ' s' : '(length: the browser finds it)'}`);
  }
  writeFileSync(join(dir, 'tracks.json'), JSON.stringify({ tracks }, null, 2) + '\n');
}

// files too big to serve: kept out of the upload (or the whole deploy would fail)
const ign = join(PUBLIC, '.assetsignore'), START = '# soundrooms: over 25 MB', STOP = '# soundrooms end';
let keep = existsSync(ign) ? readFileSync(ign, 'utf8') : '';
const a = keep.indexOf(START), z = keep.indexOf(STOP);
if (a >= 0 && z > a) keep = keep.slice(0, a) + keep.slice(z + STOP.length + 1);
if (tooBig.length) keep += `${START}\n${tooBig.join('\n')}\n${STOP}\n`;
if (keep.trim() || existsSync(ign)) writeFileSync(ign, keep);
