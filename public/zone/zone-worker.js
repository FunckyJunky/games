/* ZONE map worker (module worker).
   Loads map data around the player, turns it into low-poly geometry in 64 m chunks and sends the
   finished vertex buffers to the page, so the page itself only uploads them.

   Map sources, tried in this order (the first that answers is used):
     ofm    OpenFreeMap vector tiles (OpenMapTiles schema, zoom 14, about 1.5 km a tile). No key, CDN.
     ovp    Overpass API (raw OpenStreetMap), zoom-16 cells, if OpenFreeMap can't be reached.
     synth  a made-up neon city, if neither answers (offline). Retries OpenFreeMap now and then.
   Force one with ?src=ofm|ovp|synth on the page.

   Page -> worker
     {t:'init', lat, lon, hScale, src}       origin of the local metre grid (x = east, z = south)
     {t:'want', keys:['cx,cz',..], x, z, vx, vz}   chunks to build, nearest first; player and speed
     {t:'drop', keys}                         page threw these chunks away
   Worker -> page
     {t:'chunk', key, geo:{pos,nrm,k,uv}, add:{...}, roads, foot, footIdx, names, pois}
     {t:'status', src, loading, loaded, failed, note}
     {t:'invalidate', keys}                   build these again (better data came in)
     {t:'reset', src}                         the source changed: drop everything */

import { ShapeUtils, Vector2 } from './three.module.min.js';

const CH = 64;                       // chunk size in metres
let O = null;                        // origin
let SRC = 'ofm', forced = null;
let hScale = 1.6;
const tiles = new Map();             // 'z/x/y' -> tile
let want = [];                       // chunk keys the page wants, nearest first
const built = new Map();             // chunk key -> rect (built and sent)
let player = { x: 0, z: 0, vx: 0, vz: 0 };
let ofmUrl = null, ofmOk = false, ofmFails = 0, ovpOk = false, ovpFails = 0, lastProbe = 0, gen = 0;
let busy = false;

const zoomOf = () => (SRC === 'ofm' ? 14 : 16);

/* ---------- projection ---------- */
function toLocal(lat, lon) { return [(lon - O.lon) * O.kx, -(lat - O.lat) * O.kz]; }
function toLatLon(x, z) { return [O.lat - z / O.kz, O.lon + x / O.kx]; }
const lon2tx = (lon, z) => (lon + 180) / 360 * 2 ** z;
const lat2ty = (lat, z) => { const r = lat * Math.PI / 180; return (1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2 * 2 ** z; };
const tx2lon = (x, z) => x / 2 ** z * 360 - 180;
const ty2lat = (y, z) => { const n = Math.PI - 2 * Math.PI * y / 2 ** z; return 180 / Math.PI * Math.atan(Math.sinh(n)); };
function tileRect(z, x, y) {
  const [x0] = toLocal(0 + O.lat, tx2lon(x, z)), [x1] = toLocal(O.lat, tx2lon(x + 1, z));
  const z0 = toLocal(ty2lat(y, z), O.lon)[1], z1 = toLocal(ty2lat(y + 1, z), O.lon)[1];
  return { x0, z0, x1, z1 };
}
function tilesForRect(r) {
  const z = zoomOf();
  const [la0, lo0] = toLatLon(r.x0, r.z0), [la1, lo1] = toLatLon(r.x1, r.z1);
  const tx0 = Math.floor(lon2tx(lo0, z)), tx1 = Math.floor(lon2tx(lo1, z));
  const ty0 = Math.floor(lat2ty(la0, z)), ty1 = Math.floor(lat2ty(la1, z));
  const out = [];
  for (let x = tx0; x <= tx1; x++) for (let y = ty0; y <= ty1; y++) out.push(z + '/' + x + '/' + y);
  return out;
}
const chunkRect = (key) => { const [cx, cz] = key.split(',').map(Number); return { x0: cx * CH, z0: cz * CH, x1: cx * CH + CH, z1: cz * CH + CH }; };
const overlap = (a, b) => a.x0 < b.x1 && a.x1 > b.x0 && a.z0 < b.z1 && a.z1 > b.z0;
const touch = (a, b) => a.x0 <= b.x1 && a.x1 >= b.x0 && a.z0 <= b.z1 && a.z1 >= b.z0;   // lines can have a flat bbox

/* ---------- small helpers ---------- */
function hash(a, b) { let h = Math.imul((a | 0) ^ 0x9e3779b9, 0x85ebca6b) ^ Math.imul((b | 0) + 0x632be5ab, 0xc2b2ae35); h ^= h >>> 15; h = Math.imul(h, 0x2c1b3c6d); h ^= h >>> 12; return (h >>> 0) / 4294967296; }
const hp = (x, z) => hash(Math.round(x * 10), Math.round(z * 10));
function ringArea(r) { let a = 0; for (let i = 0, n = r.length; i < n; i += 2) { const j = (i + 2) % n; a += r[i] * r[j + 1] - r[j] * r[i + 1]; } return a / 2; }
function bboxOf(pts, b) { b = b || { x0: 1e9, z0: 1e9, x1: -1e9, z1: -1e9 }; for (let i = 0; i < pts.length; i += 2) { const x = pts[i], z = pts[i + 1]; if (x < b.x0) b.x0 = x; if (x > b.x1) b.x1 = x; if (z < b.z0) b.z0 = z; if (z > b.z1) b.z1 = z; } return b; }
function pip(x, z, r) { let c = false; for (let i = 0, n = r.length, j = n - 2; i < n; j = i, i += 2) { const xi = r[i], zi = r[i + 1], xj = r[j], zj = r[j + 1]; if ((zi > z) !== (zj > z) && x < (xj - xi) * (z - zi) / (zj - zi) + xi) c = !c; } return c; }
function segDist2(px, pz, ax, az, bx, bz) { const dx = bx - ax, dz = bz - az, l = dx * dx + dz * dz; let t = l ? ((px - ax) * dx + (pz - az) * dz) / l : 0; t = t < 0 ? 0 : t > 1 ? 1 : t; const x = ax + dx * t - px, z = az + dz * t - pz; return x * x + z * z; }

/* Sutherland-Hodgman: clip a ring [x,z,...] to a rectangle */
function clipRing(r, R) {
  let pts = r;
  const edges = [[0, R.x0, 1], [0, R.x1, -1], [1, R.z0, 1], [1, R.z1, -1]];
  for (const [ax, v, s] of edges) {
    const out = []; const n = pts.length; if (!n) return out;
    for (let i = 0; i < n; i += 2) {
      const j = (i + n - 2) % n;
      const cx = pts[i], cz = pts[i + 1], px = pts[j], pz = pts[j + 1];
      const cin = ((ax ? cz : cx) - v) * s >= 0, pin = ((ax ? pz : px) - v) * s >= 0;
      if (cin !== pin) { const a = ax ? pz : px, b = ax ? cz : cx, t = (v - a) / (b - a); out.push(px + (cx - px) * t, pz + (cz - pz) * t); }
      if (cin) out.push(cx, cz);
    }
    pts = out;
  }
  return pts;
}
/* clip a polyline with a running distance s per point: [x,z,s,...] -> pieces */
function clipLine(p, R) {
  const pieces = []; let cur = null;
  for (let i = 0; i + 5 < p.length; i += 3) {
    let x0 = p[i], z0 = p[i + 1], s0 = p[i + 2], x1 = p[i + 3], z1 = p[i + 4], s1 = p[i + 5];
    let t0 = 0, t1 = 1; const dx = x1 - x0, dz = z1 - z0; let ok = true;
    for (const [pp, q] of [[-dx, x0 - R.x0], [dx, R.x1 - x0], [-dz, z0 - R.z0], [dz, R.z1 - z0]]) {
      if (pp === 0) { if (q < 0) { ok = false; break; } } else { const t = q / pp; if (pp < 0) { if (t > t1) { ok = false; break; } if (t > t0) t0 = t; } else { if (t < t0) { ok = false; break; } if (t < t1) t1 = t; } }
    }
    if (!ok) { cur = null; continue; }
    const ax = x0 + dx * t0, az = z0 + dz * t0, as = s0 + (s1 - s0) * t0, bx = x0 + dx * t1, bz = z0 + dz * t1, bs = s0 + (s1 - s0) * t1;
    if (!cur || t0 > 0) { cur = [ax, az, as]; pieces.push(cur); }
    cur.push(bx, bz, bs);
    if (t1 < 1) cur = null;
  }
  return pieces.filter((q) => q.length >= 6);
}
function withS(pts) { const o = []; let s = 0; for (let i = 0; i < pts.length; i += 2) { if (i) s += Math.hypot(pts[i] - pts[i - 2], pts[i + 1] - pts[i - 1]); o.push(pts[i], pts[i + 1], s); } return o; }
/* Douglas-Peucker on a closed ring (keeps it low poly) */
function simplify(r, tol) {
  const n = r.length / 2; if (n <= 4) return r;
  const keep = new Uint8Array(n); keep[0] = 1; keep[n - 1] = 1;
  const st = [[0, n - 1]];
  while (st.length) {
    const [a, b] = st.pop(); let md = 0, mi = -1;
    for (let i = a + 1; i < b; i++) { const d = segDist2(r[i * 2], r[i * 2 + 1], r[a * 2], r[a * 2 + 1], r[b * 2], r[b * 2 + 1]); if (d > md) { md = d; mi = i; } }
    if (md > tol * tol) { keep[mi] = 1; st.push([a, mi], [mi, b]); }
  }
  const o = []; for (let i = 0; i < n; i++) if (keep[i]) o.push(r[i * 2], r[i * 2 + 1]);
  return o.length >= 6 ? o : r;
}
function dedupe(r) { const o = []; for (let i = 0; i < r.length; i += 2) { const n = o.length; if (n && Math.abs(o[n - 2] - r[i]) < 0.05 && Math.abs(o[n - 1] - r[i + 1]) < 0.05) continue; o.push(r[i], r[i + 1]); } const n = o.length; if (n > 4 && Math.abs(o[0] - o[n - 2]) < 0.05 && Math.abs(o[1] - o[n - 1]) < 0.05) o.length = n - 2; return o; }

/* ---------- road classes ---------- */
// code: 1 motorway 2 trunk 3 primary 4 secondary 5 tertiary 6 minor 7 service 8 track 9 path 10 cycleway 11 raceway
const ROAD = { motorway: [1, 14], trunk: [2, 12], primary: [3, 10], secondary: [4, 9], tertiary: [5, 8], minor: [6, 6.5], busway: [6, 6], service: [7, 4], track: [8, 3], path: [9, 2], raceway: [11, 9] };
// ground kinds (p1 of kind 0): 1 grass/park 2 wood 3 scrub 4 farmland 5 pitch 6 plaza 7 commercial 8 industrial 9 cemetery 10 wetland/sand
const TREE_EVERY = { 1: 260, 2: 55, 3: 90, 9: 140 };       // m2 per tree

/* ---------- MVT decoding ---------- */
const td = new TextDecoder();
class PB {
  constructor(b, p, e) { this.b = b; this.p = p || 0; this.e = e == null ? b.length : e; }
  v() { const b = this.b; let p = this.p, val = 0, m = 1, x; do { x = b[p++]; val += (x & 0x7f) * m; m *= 128; } while (x & 0x80); this.p = p; return val; }
  skip(w) { if (w === 0) this.v(); else if (w === 1) this.p += 8; else if (w === 2) { const l = this.v(); this.p += l; } else if (w === 5) this.p += 4; }
  str() { const l = this.v(), s = td.decode(this.b.subarray(this.p, this.p + l)); this.p += l; return s; }
  sub() { const l = this.v(), s = new PB(this.b, this.p, this.p + l); this.p += l; return s; }
  packed() { const l = this.v(), e = this.p + l, o = []; while (this.p < e) o.push(this.v()); return o; }
}
const LAYERS = new Set(['building', 'transportation', 'transportation_name', 'landcover', 'landuse', 'park', 'water', 'waterway', 'poi']);
function decodeMVT(buf) {
  const pb = new PB(buf), out = {};
  while (pb.p < pb.e) {
    const tag = pb.v(), f = tag >> 3, w = tag & 7;
    if (f !== 3) { pb.skip(w); continue; }
    const L = pb.sub(); let name = '', ext = 4096; const keys = [], vals = [], feats = [];
    while (L.p < L.e) {
      const t2 = L.v(), f2 = t2 >> 3, w2 = t2 & 7;
      if (f2 === 1) name = L.str();
      else if (f2 === 2) { const l = L.v(); feats.push([L.p, L.p + l]); L.p += l; }
      else if (f2 === 3) keys.push(L.str());
      else if (f2 === 4) {
        const V = L.sub(); let val = null;
        while (V.p < V.e) {
          const t3 = V.v(), f3 = t3 >> 3, w3 = t3 & 7;
          if (f3 === 1) val = V.str();
          else if (f3 === 2) { val = new DataView(V.b.buffer, V.b.byteOffset + V.p, 4).getFloat32(0, true); V.p += 4; }
          else if (f3 === 3) { val = new DataView(V.b.buffer, V.b.byteOffset + V.p, 8).getFloat64(0, true); V.p += 8; }
          else if (f3 === 4 || f3 === 5) val = V.v();
          else if (f3 === 6) { const u = V.v(); val = (u % 2) ? -(u + 1) / 2 : u / 2; }
          else if (f3 === 7) val = !!V.v();
          else V.skip(w3);
        }
        vals.push(val);
      } else if (f2 === 5) ext = L.v();
      else L.skip(w2);
    }
    if (!LAYERS.has(name)) continue;
    const list = [];
    for (const [s, e] of feats) {
      const F = new PB(buf, s, e); let type = 0, tags = null, geom = null;
      while (F.p < F.e) {
        const t4 = F.v(), f4 = t4 >> 3, w4 = t4 & 7;
        if (f4 === 2) tags = F.packed(); else if (f4 === 3) type = F.v(); else if (f4 === 4) geom = F.packed(); else F.skip(w4);
      }
      const props = {};
      if (tags) for (let i = 0; i + 1 < tags.length; i += 2) props[keys[tags[i]]] = vals[tags[i + 1]];
      list.push({ type, props, geom });
    }
    out[name] = { ext, list };
  }
  return out;
}
/* geometry commands -> rings/lines in tile units */
function geomParts(g) {
  const parts = []; let cur = null, x = 0, y = 0, i = 0;
  while (g && i < g.length) {
    const c = g[i++], id = c & 7, n = c >> 3;
    if (id === 1 || id === 2) {
      for (let k = 0; k < n; k++) {
        const dx = g[i++], dy = g[i++]; x += (dx >> 1) ^ (-(dx & 1)); y += (dy >> 1) ^ (-(dy & 1));
        if (id === 1) { cur = [x, y]; parts.push(cur); } else cur.push(x, y);
      }
    } else if (id === 7) { /* close path: rings are closed implicitly */ }
  }
  return parts;
}
function fromMVT(buf, z, x, y) {
  const raw = { B: [], R: [], G: [], WL: [], P: [], N: [], T: [] };
  if (!buf || !buf.length) return raw;
  const L = decodeMVT(buf), R = tileRect(z, x, y);
  const conv = (part, ext) => { const o = new Array(part.length); for (let i = 0; i < part.length; i += 2) { o[i] = R.x0 + (R.x1 - R.x0) * part[i] / ext; o[i + 1] = R.z0 + (R.z1 - R.z0) * part[i + 1] / ext; } return o; };
  const polys = (f, ext) => {          // feature -> [[outer, hole, ...], ...]
    const parts = geomParts(f.geom), out = []; let ref = 0;
    for (const p of parts) {
      if (p.length < 6) continue;
      const a = ringArea(p); if (!a) continue;
      if (!ref) ref = Math.sign(a);
      const ring = conv(p, ext);
      if (Math.sign(a) === ref || !out.length) out.push([ring]); else out[out.length - 1].push(ring);
    }
    return out;
  };
  const lines = (f, ext) => geomParts(f.geom).filter((p) => p.length >= 4).map((p) => conv(p, ext));
  const lay = (n) => L[n] || { ext: 4096, list: [] };

  { const { ext, list } = lay('building');
    for (const f of list) { if (f.type !== 3 || f.props.hide_3d === true) continue; const h = +f.props.render_height || 6, mh = +f.props.render_min_height || 0; for (const rings of polys(f, ext)) raw.B.push({ rings, h, mh }); } }
  { const { ext, list } = lay('transportation');
    for (const f of list) {
      const p = f.props; if (p.brunnel === 'tunnel') continue;
      let c = ROAD[p.class]; if (!c) continue;
      if (p.class === 'path' && p.subclass === 'cycleway') c = [10, 2.6];
      if (f.type === 2) for (const l of lines(f, ext)) raw.R.push({ pts: l, cls: c[0], w: c[1] * (p.ramp ? 0.8 : 1) });
      else if (f.type === 3 && (p.class === 'path' || p.class === 'minor' || p.class === 'service')) for (const rings of polys(f, ext)) raw.G.push({ rings, sub: 6 });
    } }
  { const { ext, list } = lay('transportation_name');
    for (const f of list) { const n = f.props.name || f.props['name:latin'] || f.props.ref; if (!n || f.type !== 2) continue; for (const l of lines(f, ext)) raw.N.push({ pts: l, name: String(n) }); } }
  const LC = { wood: 2, forest: 2, grass: 1, park: 1, garden: 1, meadow: 1, grassland: 1, scrub: 3, heath: 3, farmland: 4, wetland: 10, sand: 10, beach: 10 };
  { const { ext, list } = lay('landcover');
    for (const f of list) { if (f.type !== 3) continue; const sub = LC[f.props.subclass] || LC[f.props.class]; if (!sub) continue; for (const rings of polys(f, ext)) raw.G.push({ rings, sub }); } }
  const LU = { pitch: 5, playground: 5, stadium: 5, commercial: 7, retail: 7, industrial: 8, cemetery: 9, grass: 1, recreation_ground: 1, village_green: 1, allotments: 1 };
  { const { ext, list } = lay('landuse');
    for (const f of list) { if (f.type !== 3) continue; const sub = LU[f.props.class]; if (!sub) continue; for (const rings of polys(f, ext)) raw.G.push({ rings, sub }); } }
  { const { ext, list } = lay('park');
    for (const f of list) { if (f.type !== 3) continue; for (const rings of polys(f, ext)) raw.G.push({ rings, sub: 1 }); } }
  { const { ext, list } = lay('water');
    for (const f of list) { if (f.type !== 3 || f.props.brunnel === 'tunnel') continue; for (const rings of polys(f, ext)) raw.G.push({ rings, sub: 20 }); } }
  const WW = { river: 5, canal: 4, stream: 2, drain: 1.3, ditch: 1.3 };
  { const { ext, list } = lay('waterway');
    for (const f of list) { if (f.type !== 2 || f.props.brunnel === 'tunnel') continue; const w = WW[f.props.class]; if (!w) continue; for (const l of lines(f, ext)) raw.WL.push({ pts: l, w }); } }
  const SIGN = /^(shop|grocery|fast_food|clothing_store|bar|beer|cafe|restaurant|cinema|pharmacy|bank|fuel|alcohol_shop|ice_cream|music|hairdresser|bakery|theatre|museum|lodging|hackerspace|escape_game|art_gallery|nightclub|casino|car|bicycle|laundry|dentist)$/;
  { const { ext, list } = lay('poi');
    for (const f of list) {
      const p = f.props; if (f.type !== 1 || !p.name || p.indoor || (+p.level || 0) < 0 || !SIGN.test(p.class)) continue;
      for (const pt of geomParts(f.geom)) { const [px, pz] = conv(pt, ext); raw.P.push({ x: px, z: pz, name: String(p.name), cls: p.class }); }
    } }
  return raw;
}

/* ---------- Overpass (raw OSM) ---------- */
const OVP = ['https://overpass-api.de/api/interpreter', 'https://overpass.kumi.systems/api/interpreter'];
async function fetchOverpass(z, x, y) {
  const n = ty2lat(y, z), s = ty2lat(y + 1, z), w = tx2lon(x, z), e = tx2lon(x + 1, z), bb = `(${s},${w},${n},${e})`;
  const q = `[out:json][timeout:25];(way["building"]${bb};way["highway"]${bb};way["leisure"~"^(park|garden|pitch|playground|golf_course|nature_reserve)$"]${bb};` +
    `way["landuse"~"^(grass|forest|meadow|recreation_ground|village_green|cemetery|allotments|commercial|retail|industrial|farmland)$"]${bb};` +
    `way["natural"~"^(wood|scrub|grassland|heath|water)$"]${bb};way["waterway"]${bb};node["natural"="tree"]${bb};` +
    `node["name"]["shop"]${bb};node["name"]["amenity"~"^(cafe|restaurant|bar|pub|fast_food|cinema|pharmacy|bank|fuel|nightclub|ice_cream|theatre)$"]${bb};);out geom qt;`;
  let err;
  for (const u of OVP) {
    try {
      const r = await fetch(u, { method: 'POST', body: 'data=' + encodeURIComponent(q), headers: { 'Content-Type': 'application/x-www-form-urlencoded' } });
      if (r.ok) return fromOverpass(await r.json());
      err = new Error('overpass ' + r.status);
    } catch (e2) { err = e2; }
  }
  throw err;
}
function fromOverpass(j) {
  const raw = { B: [], R: [], G: [], WL: [], P: [], N: [], T: [] };
  const HW = { motorway: 'motorway', motorway_link: 'motorway', trunk: 'trunk', trunk_link: 'trunk', primary: 'primary', primary_link: 'primary', secondary: 'secondary', secondary_link: 'secondary',
    tertiary: 'tertiary', tertiary_link: 'tertiary', residential: 'minor', unclassified: 'minor', living_street: 'minor', road: 'minor', busway: 'busway', service: 'service', track: 'track',
    footway: 'path', path: 'path', pedestrian: 'path', steps: 'path', bridleway: 'path', cycleway: 'path', raceway: 'raceway' };
  for (const el of j.elements || []) {
    const t = el.tags || {};
    if (el.type === 'node') {
      const [x, z] = toLocal(el.lat, el.lon);
      if (t.natural === 'tree') raw.T.push({ x, z });
      else if (t.name) raw.P.push({ x, z, name: t.name, cls: t.shop ? 'shop' : (t.amenity || 'shop') });
      continue;
    }
    if (el.type !== 'way' || !el.geometry) continue;
    const pts = []; for (const g of el.geometry) { const [x, z] = toLocal(g.lat, g.lon); pts.push(x, z); }
    const closed = el.nodes && el.nodes.length > 3 && el.nodes[0] === el.nodes[el.nodes.length - 1];
    if (closed) pts.length -= 2;
    if (t.building && closed) {
      const lv = parseFloat(t['building:levels']), hh = parseFloat(t.height);
      const h = hh > 0 ? hh : lv > 0 ? lv * 3.2 + 1 : 5 + Math.floor(hp(pts[0], pts[1]) * 5);
      raw.B.push({ rings: [pts], h, mh: parseFloat(t.min_height) || 0 });
    } else if (t.highway && HW[t.highway] && t.tunnel !== 'yes' && t.area !== 'yes') {
      let c = ROAD[HW[t.highway]]; if (t.highway === 'cycleway') c = [10, 2.6];
      const lanes = parseFloat(t.lanes); raw.R.push({ pts, cls: c[0], w: lanes > 0 && c[0] <= 6 ? Math.max(c[1] * 0.7, lanes * 3.2) : c[1] });
      if (t.name) raw.N.push({ pts, name: t.name });
    } else if (closed) {
      let sub = 0;
      if (t.natural === 'water' || t.waterway === 'riverbank' || t.landuse === 'reservoir' || t.landuse === 'basin') sub = 20;
      else if (t.natural === 'wood' || t.landuse === 'forest') sub = 2;
      else if (t.natural === 'scrub' || t.natural === 'heath') sub = 3;
      else if (t.landuse === 'farmland') sub = 4;
      else if (t.leisure === 'pitch' || t.leisure === 'playground') sub = 5;
      else if (t.landuse === 'commercial' || t.landuse === 'retail') sub = 7;
      else if (t.landuse === 'industrial') sub = 8;
      else if (t.landuse === 'cemetery') sub = 9;
      else if (t.leisure || t.landuse || t.natural) sub = 1;
      if (sub) raw.G.push({ rings: [pts], sub });
    } else if (t.waterway && t.tunnel !== 'yes') {
      const w = { river: 5, canal: 4, stream: 2, drain: 1.3, ditch: 1.3 }[t.waterway]; if (w) raw.WL.push({ pts, w });
    }
  }
  return raw;
}

/* ---------- SIMULATED SECTOR (offline) ---------- */
const SYN_NAMES = ['NEON NOODLE', 'DATA DEN', 'SYNTH BAR', 'CHROME CAFE', 'GLITCH MART', 'VOID RAMEN', 'KILOBYTE', 'PIXEL PHARMA', 'HOLO HOTEL', 'BYTE BURGER', 'NULL CLUB', 'ERROR 404', 'GRID GYM', 'RAM RAMEN', 'CIRCUIT CITY', 'LASER LOUNGE', 'MEGA BANK', 'SOY & SILICON', 'BLUE SCREEN', 'NIGHT MARKET'];
function synthTile(z, x, y) {
  const raw = { B: [], R: [], G: [], WL: [], P: [], N: [], T: [] };
  const R = tileRect(z, x, y), SX = 92, SZ = 76;
  const gx = (i) => i * SX + (hash(i, 7) - 0.5) * 18, gz = (j) => j * SZ + (hash(j, 11) - 0.5) * 14;
  const i0 = Math.floor(R.x0 / SX) - 1, i1 = Math.ceil(R.x1 / SX) + 1, j0 = Math.floor(R.z0 / SZ) - 1, j1 = Math.ceil(R.z1 / SZ) + 1;
  const wOf = (k) => (k % 4 === 0 ? 11 : 7), cOf = (k) => (k % 4 === 0 ? 3 : 6);
  for (let i = i0; i <= i1; i++) { const c = cOf(i); raw.R.push({ pts: [gx(i), gz(j0), gx(i), gz(j1)], cls: c, w: wOf(i) }); raw.N.push({ pts: [gx(i), gz(j0), gx(i), gz(j1)], name: 'AVENUE ' + (((i % 26) + 26) % 26 + 10).toString(36).toUpperCase() + (i % 4 === 0 ? ' BLVD' : '') }); }
  for (let j = j0; j <= j1; j++) { const c = cOf(j); raw.R.push({ pts: [gx(i0), gz(j), gx(i1), gz(j)], cls: c, w: wOf(j) }); raw.N.push({ pts: [gx(i0), gz(j), gx(i1), gz(j)], name: 'SECTOR ' + j + ' ST' }); }
  for (let i = i0; i < i1; i++) for (let j = j0; j < j1; j++) {
    const bx0 = gx(i) + wOf(i) / 2 + 3, bx1 = gx(i + 1) - wOf(i + 1) / 2 - 3, bz0 = gz(j) + wOf(j) / 2 + 3, bz1 = gz(j + 1) - wOf(j + 1) / 2 - 3;
    const cx = (bx0 + bx1) / 2, cz = (bz0 + bz1) / 2; if (cx < R.x0 - 120 || cx > R.x1 + 120 || cz < R.z0 - 120 || cz > R.z1 + 120) continue;
    const kind = hash(i * 31 + 5, j * 17 + 3);
    if (kind < 0.1) { raw.G.push({ rings: [[bx0, bz0, bx1, bz0, bx1, bz1, bx0, bz1]], sub: kind < 0.04 ? 2 : 1 }); continue; }
    if (kind < 0.13) { raw.G.push({ rings: [[bx0 - 2, bz0 - 2, bx1 + 2, bz0 - 2, bx1 + 2, bz1 + 2, bx0 - 2, bz1 + 2]], sub: 20 }); continue; }
    const lots = [[bx0, bz0, bx1, bz1, 0]];
    const done = [];
    while (lots.length) {
      const [a, b, c, d, dep] = lots.pop(); const w = c - a, h = d - b, r = hash(Math.round(a * 3), Math.round(b * 7) + dep);
      if (dep < 3 && (w > 26 || h > 26) && r < 0.85) {
        if (w > h) { const m = a + w * (0.35 + 0.3 * r); lots.push([a, b, m - 1, d, dep + 1], [m + 1, b, c, d, dep + 1]); }
        else { const m = b + h * (0.35 + 0.3 * r); lots.push([a, b, c, m - 1, dep + 1], [a, m + 1, c, d, dep + 1]); }
      } else done.push([a, b, c, d]);
    }
    for (const [a, b, c, d] of done) {
      const r = hash(Math.round(a * 5), Math.round(d * 5)), r2 = hash(Math.round(c * 3), Math.round(b * 9));
      const h = r < 0.06 ? 70 + r2 * 90 : r < 0.3 ? 25 + r2 * 40 : 7 + r2 * 18;
      const inset = r2 < 0.3 ? 1.5 : 0;
      raw.B.push({ rings: [[a + inset, b + inset, c - inset, b + inset, c - inset, d - inset, a + inset, d - inset]], h: h / 1.6, mh: 0 });
      if (r2 > 0.82) raw.P.push({ x: (a + c) / 2, z: b - 2, name: SYN_NAMES[Math.floor(hash(Math.round(a), Math.round(b)) * SYN_NAMES.length)], cls: 'shop' });
    }
  }
  return raw;
}

/* ---------- ingesting a tile: clip to its own rectangle, index it ---------- */
function onEdge(ax, az, bx, bz, R) {
  const e = 0.02;
  return (Math.abs(ax - R.x0) < e && Math.abs(bx - R.x0) < e) || (Math.abs(ax - R.x1) < e && Math.abs(bx - R.x1) < e) ||
         (Math.abs(az - R.z0) < e && Math.abs(bz - R.z0) < e) || (Math.abs(az - R.z1) < e && Math.abs(bz - R.z1) < e);
}
function ingest(T, raw) {
  const R = T.rect, B = new Map(), RD = [], G = [], WL = [], P = [], N = [], TR = [];
  for (const b of raw.B) {
    const rings = [];
    for (let k = 0; k < b.rings.length; k++) {
      let r = dedupe(b.rings[k]); if (r.length < 6) continue;
      const bb = bboxOf(r); if (bb.x1 < R.x0 || bb.x0 > R.x1 || bb.z1 < R.z0 || bb.z0 > R.z1) { if (!k) break; continue; }
      if (bb.x0 < R.x0 || bb.x1 > R.x1 || bb.z0 < R.z0 || bb.z1 > R.z1) r = dedupe(clipRing(r, R));
      if (r.length < 6 || Math.abs(ringArea(r)) < (k ? 2 : 6)) { if (!k) break; continue; }
      rings.push(simplify(r, 0.35));
    }
    if (!rings.length) continue;
    const o = rings[0]; let cx = 0, cz = 0; for (let i = 0; i < o.length; i += 2) { cx += o[i]; cz += o[i + 1]; } cx /= o.length / 2; cz /= o.length / 2;
    if (cx < R.x0 || cx >= R.x1 || cz < R.z0 || cz >= R.z1) { const bb = bboxOf(o); cx = Math.min(Math.max(cx, Math.max(bb.x0, R.x0)), Math.min(bb.x1, R.x1) - 0.01); cz = Math.min(Math.max(cz, Math.max(bb.z0, R.z0)), Math.min(bb.z1, R.z1) - 0.01); }
    const key = Math.floor(cx / CH) + ',' + Math.floor(cz / CH);
    let list = B.get(key); if (!list) B.set(key, list = []);
    list.push({ rings, h: b.h, mh: b.mh, cx, cz, seed: hp(cx, cz), bb: bboxOf(o) });
  }
  const addLine = (arr, l, extra) => { const pts = dedupe(l.pts); if (pts.length < 4) return; for (const p of clipLine(withS(pts), R)) arr.push(Object.assign({ p, bb: bboxOf3(p) }, extra)); };
  for (const r of raw.R) addLine(RD, r, { cls: r.cls, w: r.w, seed: hp(r.pts[0], r.pts[1]) });
  for (const r of raw.WL) addLine(WL, r, { w: r.w });
  for (const r of raw.N) addLine(N, r, { name: r.name });
  for (const g of raw.G) {
    const rings = [];
    for (let k = 0; k < g.rings.length; k++) {
      let r = dedupe(g.rings[k]); if (r.length < 6) continue;
      const bb = bboxOf(r); if (bb.x1 < R.x0 || bb.x0 > R.x1 || bb.z1 < R.z0 || bb.z0 > R.z1) { if (!k) break; continue; }
      r = dedupe(clipRing(r, R)); if (r.length < 6) { if (!k) break; continue; }
      rings.push(r);
    }
    if (rings.length) G.push({ rings, sub: g.sub, bb: bboxOf(rings[0]) });
  }
  for (const p of raw.P) if (p.x >= R.x0 && p.x < R.x1 && p.z >= R.z0 && p.z < R.z1) P.push(p);
  for (const t of raw.T) if (t.x >= R.x0 && t.x < R.x1 && t.z >= R.z0 && t.z < R.z1) TR.push(t);
  T.B = B; T.R = RD; T.G = G; T.WL = WL; T.P = P; T.N = N; T.T = TR;
}
function bboxOf3(p) { const b = { x0: 1e9, z0: 1e9, x1: -1e9, z1: -1e9 }; for (let i = 0; i < p.length; i += 3) { const x = p[i], z = p[i + 1]; if (x < b.x0) b.x0 = x; if (x > b.x1) b.x1 = x; if (z < b.z0) b.z0 = z; if (z > b.z1) b.z1 = z; } return b; }

/* ---------- loading tiles ---------- */
async function gunzip(u8) { const ds = new DecompressionStream('gzip'); const ab = await new Response(new Blob([u8]).stream().pipeThrough(ds)).arrayBuffer(); return new Uint8Array(ab); }
async function fetchOfm(z, x, y) {
  if (!ofmUrl) {
    const r = await fetch('https://tiles.openfreemap.org/planet'); if (!r.ok) throw new Error('tilejson ' + r.status);
    const j = await r.json(); ofmUrl = j.tiles && j.tiles[0]; if (!ofmUrl) throw new Error('no tiles url');
  }
  const r = await fetch(ofmUrl.replace('{z}', z).replace('{x}', x).replace('{y}', y));
  if (r.status === 204 || r.status === 404) return new Uint8Array(0);
  if (!r.ok) throw new Error('tile ' + r.status);
  let b = new Uint8Array(await r.arrayBuffer());
  if (b[0] === 0x1f && b[1] === 0x8b) b = await gunzip(b);
  return b;
}
let inflight = 0;
function loadTile(key) {
  let T = tiles.get(key);
  if (T && (T.state === 'ok' || T.state === 'loading' || T.state === 'queued')) return T;
  if (T && T.state === 'fail' && Date.now() < T.retryAt) return T;
  const [z, x, y] = key.split('/').map(Number);
  if (!T) { T = { key, z, x, y, rect: tileRect(z, x, y), state: 'queued', src: SRC }; tiles.set(key, T); }
  T.state = 'queued';
  pump();
  return T;
}
function pump() {
  const max = SRC === 'ofm' ? 4 : SRC === 'ovp' ? 1 : 8;
  for (const T of tiles.values()) {
    if (inflight >= max) break;
    if (T.state !== 'queued') continue;
    T.state = 'loading'; inflight++;
    const src = SRC, g0 = gen;
    (async () => {
      try {
        let raw;
        if (src === 'ofm') { raw = fromMVT(await fetchOfm(T.z, T.x, T.y), T.z, T.x, T.y); ofmOk = true; ofmFails = 0; }
        else if (src === 'ovp') { raw = await fetchOverpass(T.z, T.x, T.y); ovpOk = true; ovpFails = 0; }
        else raw = synthTile(T.z, T.x, T.y);
        if (src !== SRC) return;
        ingest(T, raw); T.state = 'ok';
        const inv = []; for (const [k, r] of built) if (overlap(r, T.rect) && T.hadFail) inv.push(k);
        if (inv.length) { for (const k of inv) built.delete(k); post({ t: 'invalidate', keys: inv }); }
      } catch (e) {
        if (src !== SRC) return;
        T.state = 'fail'; T.hadFail = true; T.retryAt = Date.now() + 8000; T.B = new Map(); T.R = []; T.G = []; T.WL = []; T.P = []; T.N = []; T.T = [];
        if (src === 'ofm' && !ofmOk && ++ofmFails >= 2 && !forced) switchSrc('ovp', 'OpenFreeMap unreachable');
        else if (src === 'ovp' && !ovpOk && ++ovpFails >= 2 && !forced) switchSrc('synth', 'no map signal');
      } finally { if (g0 === gen) { inflight--; status(); pump(); work(); } }
    })();
  }
}
function switchSrc(s, why) {
  SRC = s; tiles.clear(); built.clear(); inflight = 0; gen++;
  post({ t: 'reset', src: s, note: why }); status();
}
async function probe() {          // offline / fallback: is OpenFreeMap back?
  if (SRC === 'ofm' || forced || Date.now() - lastProbe < 45000) return;
  lastProbe = Date.now();
  try { ofmUrl = null; const b = await fetchOfm(14, Math.floor(lon2tx(O.lon, 14)), Math.floor(lat2ty(O.lat, 14))); if (b) { ofmOk = true; switchSrc('ofm', 'map signal back'); } } catch (e) {}
}
function status() {
  let loading = 0, loaded = 0, failed = 0;
  for (const T of tiles.values()) { if (T.state === 'ok') loaded++; else if (T.state === 'fail') failed++; else loading++; }
  post({ t: 'status', src: SRC, loading, loaded, failed });
}
function post(m, tr) { self.postMessage(m, tr || []); }

/* ---------- building a chunk ---------- */
class Geo {
  constructor() { this.p = []; this.n = []; this.k = []; this.u = []; }
  v(x, y, z, nx, ny, nz, k0, k1, k2, k3, u, w) { this.p.push(x, y, z); this.n.push(nx, ny, nz); this.k.push(k0, k1, k2, k3); this.u.push(u, w); }
  tri(a, b, c, nx, ny, nz, K) {        // a,b,c = [x,y,z,u,v]; turned so its front faces n
    const e1x = b[0] - a[0], e1y = b[1] - a[1], e1z = b[2] - a[2], e2x = c[0] - a[0], e2y = c[1] - a[1], e2z = c[2] - a[2];
    const cx = e1y * e2z - e1z * e2y, cy = e1z * e2x - e1x * e2z, cz = e1x * e2y - e1y * e2x;
    if (cx * nx + cy * ny + cz * nz < 0) { const t = b; b = c; c = t; }
    for (const q of [a, b, c]) this.v(q[0], q[1], q[2], nx, ny, nz, K[0], K[1], K[2], K[3], q[3], q[4]);
  }
  ftri(a, b, c, K) {                   // flat-shaded triangle, normal from its own winding
    const e1x = b[0] - a[0], e1y = b[1] - a[1], e1z = b[2] - a[2], e2x = c[0] - a[0], e2y = c[1] - a[1], e2z = c[2] - a[2];
    let nx = e1y * e2z - e1z * e2y, ny = e1z * e2x - e1x * e2z, nz = e1x * e2y - e1y * e2x; const l = Math.hypot(nx, ny, nz) || 1; nx /= l; ny /= l; nz /= l;
    for (const q of [a, b, c]) this.v(q[0], q[1], q[2], nx, ny, nz, K[0], K[1], K[2], K[3], q[3], q[4]);
  }
  quad(a, b, c, d, nx, ny, nz, K) { this.tri(a, b, c, nx, ny, nz, K); this.tri(a, c, d, nx, ny, nz, K); }
  box(x, y, z, sx, sy, sz, K) {       // axis box, centre bottom at x,y,z
    const X0 = x - sx / 2, X1 = x + sx / 2, Y0 = y, Y1 = y + sy, Z0 = z - sz / 2, Z1 = z + sz / 2;
    const P = (px, py, pz, u, v) => [px, py, pz, u, v];
    this.quad(P(X0, Y0, Z1, 0, 0), P(X1, Y0, Z1, 1, 0), P(X1, Y1, Z1, 1, 1), P(X0, Y1, Z1, 0, 1), 0, 0, 1, K);
    this.quad(P(X1, Y0, Z0, 0, 0), P(X0, Y0, Z0, 1, 0), P(X0, Y1, Z0, 1, 1), P(X1, Y1, Z0, 0, 1), 0, 0, -1, K);
    this.quad(P(X1, Y0, Z1, 0, 0), P(X1, Y0, Z0, 1, 0), P(X1, Y1, Z0, 1, 1), P(X1, Y1, Z1, 0, 1), 1, 0, 0, K);
    this.quad(P(X0, Y0, Z0, 0, 0), P(X0, Y0, Z1, 1, 0), P(X0, Y1, Z1, 1, 1), P(X0, Y1, Z0, 0, 1), -1, 0, 0, K);
    this.quad(P(X0, Y1, Z1, 0, 0), P(X1, Y1, Z1, 1, 0), P(X1, Y1, Z0, 1, 1), P(X0, Y1, Z0, 0, 1), 0, 1, 0, K);
  }
  out() { return { pos: new Float32Array(this.p), nrm: new Float32Array(this.n), k: new Float32Array(this.k), uv: new Float32Array(this.u), count: this.p.length / 3 }; }
}
function triangulate(rings) {
  const contour = [], holes = [];
  const toV = (r) => { const o = []; for (let i = 0; i < r.length; i += 2) o.push(new Vector2(r[i], r[i + 1])); return o; };
  contour.push(...toV(rings[0]));
  for (let i = 1; i < rings.length; i++) holes.push(toV(rings[i]));
  let faces = [];
  try { faces = ShapeUtils.triangulateShape(contour, holes); } catch (e) { faces = []; }
  const all = contour.concat(...holes);
  return { all, faces };
}
const ROAD_Y = (cls) => (cls >= 9 ? 0.07 : 0.085 + (11 - Math.min(cls, 8)) * 0.006);
const GROUND_Y = { 1: 0.03, 2: 0.034, 3: 0.032, 4: 0.026, 5: 0.038, 6: 0.04, 7: 0.02, 8: 0.022, 9: 0.03, 10: 0.028, 20: 0.05 };

function buildChunk(key) {
  const R = chunkRect(key), G = new Geo(), A = new Geo();
  const tks = tilesForRect(R).map((k) => tiles.get(k)).filter((T) => T && (T.state === 'ok' || T.state === 'fail') && T.B);
  const roadsOut = [], foot = [], footIdx = [0], names = [], pois = [];
  const blds = [], roads = [];
  for (const T of tks) {
    const bl = T.B.get(key); if (bl) blds.push(...bl);
    for (const r of T.R) if (touch(r.bb, R)) for (const p of clipLine(r.p, R)) roads.push({ p, cls: r.cls, w: r.w, seed: r.seed });
  }
  /* buildings */
  for (const b of blds) {
    const h = Math.max(3, b.h * hScale), mh = b.mh * hScale, sd = b.seed;
    let longest = null;
    b.rings.forEach((r, ri) => {
      const A0 = ringArea(r), sgn = A0 > 0 ? 1 : -1, inward = ri > 0 ? -1 : 1, n = r.length;
      for (let i = 0; i < n; i += 2) {
        const j = (i + 2) % n, ax = r[i], az = r[i + 1], bx = r[j], bz = r[j + 1];
        const L = Math.hypot(bx - ax, bz - az); if (L < 0.05) continue;
        let nx = (bz - az) / L * sgn * inward, nz = -(bx - ax) / L * sgn * inward;
        if (tks.some((T) => onEdge(ax, az, bx, bz, T.rect))) continue;
        const K = [4, h, L, sd];
        G.quad([ax, mh, az, 0, mh], [bx, mh, bz, L, mh], [bx, h, bz, L, h], [ax, h, az, 0, h], nx, 0, nz, K);
        if (ri === 0 && (!longest || L > longest.L)) longest = { ax, az, bx, bz, L, nx, nz };
      }
    });
    const { all, faces } = triangulate(b.rings);
    for (const f of faces) { const a = all[f[0]], c = all[f[1]], d = all[f[2]]; G.tri([a.x, h, a.y, a.x, a.y], [c.x, h, c.y, c.x, c.y], [d.x, h, d.y, d.x, d.y], 0, 1, 0, [5, h, 0, sd]); }
    // roof spire with a blinking beacon on tall towers
    if (h > 34 && sd < 0.55) {
      const sh = 4 + sd * 18, x = b.cx, z = b.cz;
      if (pip(x, z, b.rings[0])) { G.box(x, h, z, 0.35, sh, 0.35, [7, 0, 0, sd]); G.box(x, h + sh, z, 0.7, 0.5, 0.7, [8, 3, 1, sd]); }
    }
    // holo billboard on a big wall
    if (longest && h >= 11 && longest.L > 6 && sd > 0.62) {
      const { ax, az, bx, bz, L, nx, nz } = longest, w = Math.min(L * 0.7, 14), bh = Math.min(Math.max(h * 0.22, 3), 9);
      const y0 = Math.min(h - bh - 1, Math.max(4, h * 0.5)), m = 0.5 - w / L / 2, ox = nx * 0.35, oz = nz * 0.35;
      const p0x = ax + (bx - ax) * m + ox, p0z = az + (bz - az) * m + oz, p1x = ax + (bx - ax) * (1 - m) + ox, p1z = az + (bz - az) * (1 - m) + oz;
      A.quad([p0x, y0, p0z, 0, 0], [p1x, y0, p1z, 1, 0], [p1x, y0 + bh, p1z, 1, 1], [p0x, y0 + bh, p0z, 0, 1], nx, 0, nz, [9, Math.floor(sd * 1000) % 5, w / bh, sd]);
    }
    const o = b.rings[0]; for (let i = 0; i < o.length; i++) foot.push(o[i]); footIdx.push(foot.length);
  }
  /* roads */
  const lampPts = [];
  for (const r of roads) {
    const p = r.p, n = p.length / 3, hw = r.w / 2, y = ROAD_Y(r.cls) + r.seed * 0.004, kind = r.cls >= 9 && r.cls !== 11 ? 3 : 2;
    const L = [], Rr = [];
    for (let i = 0; i < n; i++) {
      const x = p[i * 3], z = p[i * 3 + 1];
      let dx0 = 0, dz0 = 0, dx1 = 0, dz1 = 0;
      if (i > 0) { dx0 = x - p[i * 3 - 3]; dz0 = z - p[i * 3 - 2]; const l = Math.hypot(dx0, dz0) || 1; dx0 /= l; dz0 /= l; }
      if (i < n - 1) { dx1 = p[i * 3 + 3] - x; dz1 = p[i * 3 + 4] - z; const l = Math.hypot(dx1, dz1) || 1; dx1 /= l; dz1 /= l; }
      if (i === 0) { dx0 = dx1; dz0 = dz1; } if (i === n - 1) { dx1 = dx0; dz1 = dz0; }
      let mx = -(dz0 + dz1), mz = dx0 + dx1; const ml = Math.hypot(mx, mz) || 1; mx /= ml; mz /= ml;
      const cosA = mx * -dz0 + mz * dx0, sc = hw / Math.max(0.45, cosA);
      L.push([x + mx * sc, y, z + mz * sc, p[i * 3 + 2], 1]); Rr.push([x - mx * sc, y, z - mz * sc, p[i * 3 + 2], -1]);
    }
    for (let i = 0; i < n - 1; i++) G.quad(Rr[i], Rr[i + 1], L[i + 1], L[i], 0, 1, 0, [kind, r.cls, r.w, r.seed]);
    // segments for the page (bike, radar, demo)
    for (let i = 0; i < n - 1; i++) roadsOut.push(p[i * 3], p[i * 3 + 1], p[i * 3 + 3], p[i * 3 + 4], r.cls, r.w);
    // street lamps every 32 m (world-fixed phase), both sides on big roads
    if (r.cls <= 7) {
      for (let i = 0; i < n - 1; i++) {
        const s0 = p[i * 3 + 2], s1 = p[i * 3 + 5], step = r.cls <= 4 ? 26 : 34;
        for (let s = Math.ceil(s0 / step) * step; s < s1; s += step) {
          const t = (s - s0) / (s1 - s0), x = p[i * 3] + (p[i * 3 + 3] - p[i * 3]) * t, z = p[i * 3 + 1] + (p[i * 3 + 4] - p[i * 3 + 1]) * t;
          const dx = p[i * 3 + 3] - p[i * 3], dz = p[i * 3 + 4] - p[i * 3 + 1], l = Math.hypot(dx, dz) || 1, side = (Math.round(s / step) % 2 ? 1 : -1);
          const off = hw + 1.3;
          lampPts.push([x - dz / l * off * side, z + dx / l * off * side, r.cls, -dz / l * side, dx / l * side]);
          if (r.cls <= 3) lampPts.push([x + dz / l * off * side, z - dx / l * off * side, r.cls, dz / l * side, -dx / l * side]);
        }
      }
    }
  }
  const inBuilding = (x, z, pad) => { for (const b of blds) { const bb = b.bb; if (x < bb.x0 - pad || x > bb.x1 + pad || z < bb.z0 - pad || z > bb.z1 + pad) continue; if (pip(x, z, b.rings[0])) return true; } return false; };
  const nearRoad = (x, z, pad) => { for (const r of roads) { const p = r.p, hw = r.w / 2 + pad; for (let i = 0; i + 5 < p.length; i += 3) if (segDist2(x, z, p[i], p[i + 1], p[i + 3], p[i + 4]) < hw * hw) return true; } return false; };
  for (const [x, z, cls, nx, nz] of lampPts) {
    if (x < R.x0 || x >= R.x1 || z < R.z0 || z >= R.z1 || inBuilding(x, z, 0.5)) continue;
    const c = cls <= 3 ? 1 : cls <= 5 ? 2 : 0, sd = hp(x, z);
    G.box(x, 0, z, 0.14, 5.6, 0.14, [7, 0, 0, sd]);
    G.box(x - nx * 0.6, 5.6, z - nz * 0.6, 0.18 + Math.abs(nx) * 1.1, 0.12, 0.18 + Math.abs(nz) * 1.1, [8, c, 0, sd]);
    A.quad([x - 3.5, 0.12, z - 3.5, -1, -1], [x + 3.5, 0.12, z - 3.5, 1, -1], [x + 3.5, 0.12, z + 3.5, 1, 1], [x - 3.5, 0.12, z + 3.5, -1, 1], 0, 1, 0, [10, c, 0, sd]);
  }
  /* ground: green, water, plazas, districts */
  const treeAreas = [];
  for (const T of tks) for (const g of T.G) {
    if (!overlap(g.bb, R)) continue;
    const outer = dedupe(clipRing(g.rings[0], R)); if (outer.length < 6 || Math.abs(ringArea(outer)) < 1) continue;
    const rings = [outer];
    for (let i = 1; i < g.rings.length; i++) { const hb = bboxOf(g.rings[i]); if (hb.x0 > R.x0 && hb.x1 < R.x1 && hb.z0 > R.z0 && hb.z1 < R.z1) rings.push(g.rings[i]); }
    const kind = g.sub === 20 ? 1 : 0, y = GROUND_Y[g.sub] || 0.03, sd = hp(g.bb.x0, g.bb.z0);
    const { all, faces } = triangulate(rings);
    for (const f of faces) { const a = all[f[0]], b = all[f[1]], c = all[f[2]]; G.tri([a.x, y, a.y, a.x, a.y], [b.x, y, b.y, b.x, b.y], [c.x, y, c.y, c.x, c.y], 0, 1, 0, [kind, g.sub, 0, sd]); }
    if (TREE_EVERY[g.sub]) treeAreas.push({ rings, sub: g.sub });
  }
  for (const T of tks) for (const w of T.WL) {
    if (!touch(w.bb, R)) continue;
    for (const p of clipLine(w.p, R)) {
      const n = p.length / 3, hw = w.w / 2;
      for (let i = 0; i < n - 1; i++) {
        const ax = p[i * 3], az = p[i * 3 + 1], bx = p[i * 3 + 3], bz = p[i * 3 + 4], l = Math.hypot(bx - ax, bz - az) || 1, nx = -(bz - az) / l * hw, nz = (bx - ax) / l * hw;
        G.quad([ax - nx, 0.055, az - nz, ax, az], [bx - nx, 0.055, bz - nz, bx, bz], [bx + nx, 0.055, bz + nz, bx, bz], [ax + nx, 0.055, az + nz, ax, az], 0, 1, 0, [1, 21, 0, 0]);
      }
    }
  }
  /* trees: low-poly neon pines and blobs */
  let trees = 0; const treeAt = new Set();
  const tree = (x, z, sd, big) => {
    const tk = Math.round(x * 2) + ',' + Math.round(z * 2); if (trees > 320 || treeAt.has(tk)) return; trees++; treeAt.add(tk);
    const H = (big ? 6 : 3.5) + sd * (big ? 7 : 3), r = H * (0.26 + 0.08 * hash(Math.round(x * 7), 3)), seg = 5, t0 = H * 0.22;
    const K = [6, H, t0, sd], rot = sd * 6.283;
    G.box(x, 0, z, 0.3, t0 + 0.4, 0.3, [7, 0, 0, sd]);
    for (const [y0, y1, rr] of [[t0, t0 + (H - t0) * 0.62, r], [t0 + (H - t0) * 0.38, H, r * 0.68]]) {
      for (let i = 0; i < seg; i++) {
        const a0 = rot + i / seg * 6.283, a1 = rot + (i + 1) / seg * 6.283;
        const p0 = [x + Math.cos(a0) * rr, y0, z + Math.sin(a0) * rr, 0, y0 / H], p1 = [x + Math.cos(a1) * rr, y0, z + Math.sin(a1) * rr, 0, y0 / H], top = [x, y1, z, 0, y1 / H];
        G.ftri(p1, p0, top, K);
        G.ftri(p0, p1, [x, y0, z, 0, y0 / H], K);
      }
    }
  };
  for (const ta of treeAreas) {
    const cell = Math.sqrt(TREE_EVERY[ta.sub]), big = ta.sub !== 3;
    const bb = bboxOf(ta.rings[0]);
    for (let gx = Math.floor(bb.x0 / cell) * cell; gx < bb.x1; gx += cell) for (let gz = Math.floor(bb.z0 / cell) * cell; gz < bb.z1; gz += cell) {
      const x = gx + hash(Math.round(gx * 10), Math.round(gz * 10) + 1) * cell, z = gz + hash(Math.round(gx * 10) + 5, Math.round(gz * 10)) * cell;
      if (x < R.x0 || x >= R.x1 || z < R.z0 || z >= R.z1 || !pip(x, z, ta.rings[0])) continue;
      let hole = false; for (let i = 1; i < ta.rings.length; i++) if (pip(x, z, ta.rings[i])) { hole = true; break; } if (hole) continue;
      if (inBuilding(x, z, 1.5) || nearRoad(x, z, 1.2)) continue;
      tree(x, z, hp(x, z), big);
    }
  }
  for (const T of tks) for (const t of T.T) if (t.x >= R.x0 && t.x < R.x1 && t.z >= R.z0 && t.z < R.z1 && !nearRoad(t.x, t.z, 0.5)) tree(t.x, t.z, hp(t.x, t.z), true);
  /* signs and street names */
  for (const T of tks) for (const p of T.P) if (p.x >= R.x0 && p.x < R.x1 && p.z >= R.z0 && p.z < R.z1) pois.push(p);
  for (const T of tks) for (const nm of T.N) { if (!touch(nm.bb, R)) continue; for (const p of clipLine(nm.p, R)) { const s = []; for (let i = 0; i < p.length; i += 3) s.push(p[i], p[i + 1]); names.push({ name: nm.name, pts: new Float32Array(s) }); } }

  const geo = G.out(), add = A.out(), roadsA = new Float32Array(roadsOut), footA = new Float32Array(foot), footI = new Uint32Array(footIdx);
  post({ t: 'chunk', key, geo, add, roads: roadsA, foot: footA, footIdx: footI, names, pois, src: SRC },
    [geo.pos.buffer, geo.nrm.buffer, geo.k.buffer, geo.uv.buffer, add.pos.buffer, add.nrm.buffer, add.k.buffer, add.uv.buffer, roadsA.buffer, footA.buffer, footI.buffer].concat(names.map((n) => n.pts.buffer)));
  built.set(key, R);
}

/* ---------- work loop ---------- */
function work() {
  if (busy || !O) return; busy = true;
  setTimeout(() => {
    busy = false;
    const t0 = performance.now(); let did = 0;
    for (const key of want) {
      if (built.has(key)) continue;
      const R = chunkRect(key), tk = tilesForRect(R);
      let ready = true;
      for (const k of tk) { const T = loadTile(k); if (T.state !== 'ok' && T.state !== 'fail') ready = false; }
      if (!ready) continue;
      try { buildChunk(key); } catch (e) { post({ t: 'err', msg: 'chunk ' + key + ': ' + (e && e.message) }); built.set(key, R); }
      did++;
      if (performance.now() - t0 > 30) break;
    }
    want = want.filter((k) => !built.has(k));
    if (did && want.length) work();
  }, 0);
}
function prefetch() {
  const ax = player.x + player.vx * 18, az = player.z + player.vz * 18, r = 260;
  for (const k of tilesForRect({ x0: ax - r, z0: az - r, x1: ax + r, z1: az + r })) loadTile(k);
  // forget tiles far away
  for (const [k, T] of tiles) { const R = T.rect, cx = Math.max(R.x0, Math.min(player.x, R.x1)), cz = Math.max(R.z0, Math.min(player.z, R.z1)); if (Math.hypot(cx - player.x, cz - player.z) > 2500 && T.state !== 'loading') tiles.delete(k); }
}

self.onmessage = (e) => {
  const m = e.data;
  if (m.t === 'init') {
    O = { lat: m.lat, lon: m.lon, kx: Math.cos(m.lat * Math.PI / 180) * 111320, kz: 110540 };
    if (m.hScale) hScale = m.hScale;
    if (m.src && /^(ofm|ovp|synth)$/.test(m.src)) { SRC = m.src; forced = m.src; }
    tiles.clear(); built.clear(); gen++; inflight = 0; ofmOk = ovpOk = false; ofmFails = ovpFails = 0; status();
  } else if (m.t === 'want') {
    want = m.keys.slice(); player = { x: m.x, z: m.z, vx: m.vx || 0, vz: m.vz || 0 };
    prefetch(); work(); probe();
  } else if (m.t === 'drop') {
    for (const k of m.keys) built.delete(k);
  } else if (m.t === 'scale') {
    hScale = m.hScale; built.clear();
  }
};
