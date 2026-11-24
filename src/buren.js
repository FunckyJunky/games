// BUREN (neighbours) for FUNDA WATCH: the houses within 150 m of a watched listing, with their WOZ values.
//
//   GET /api/funda/<id>/buren?n=20     the house + the n nearest addresses (n = 1 ... 100), their WOZ values,
//                                      and the outlines of every building within 150 m (for the 3D map)
//   GET /api/funda/<id>/buren?debug=1  also shows a sample of what each source answered
//   GET /api/funda/<id>/forsale        houses for sale on Funda around it (asking prices), kept 6 hours
//
// Sources (public, no key):
//   PDOK Locatieserver (free + reverse)        the address, its point, the addresses around it
//   PDOK BAG OGC API (pand, verblijfsobject)    building outlines + build year, living area (m²)
//   Kadaster WOZ-waardeloket API                WOZ values per address
// The area is kept 90 days, WOZ values 60 days (new ones come out each February). At most 30 WOZ
// lookups happen per request, so the page asks again until it has what it wants ("pending").
// WOZ values are looked up only for the houses someone asked for, never in bulk.

export const RADIUS = 150;
const MAX_N = 100;
const PER_CALL = 30;
const AREA_TTL = 90 * 86400000;
const WOZ_TTL = 60 * 86400000;
const UA = "error.blue FUNDA WATCH (personal house tracker)";

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" },
  });
}

let ready = null;
function schema(env) {
  if (!ready) {
    ready = env.DB.batch([
      env.DB.prepare(`CREATE TABLE IF NOT EXISTS funda_area (id TEXT PRIMARY KEY, data TEXT NOT NULL, fetched INTEGER NOT NULL)`),
      env.DB.prepare(`CREATE TABLE IF NOT EXISTS funda_woz (nid TEXT PRIMARY KEY, data TEXT NOT NULL, fetched INTEGER NOT NULL)`),
      env.DB.prepare(`CREATE TABLE IF NOT EXISTS funda_forsale (id TEXT PRIMARY KEY, data TEXT NOT NULL, fetched INTEGER NOT NULL)`),
    ]).catch((e) => { ready = null; throw e; });
  }
  return ready;
}

const plain = (id) => String(id ?? "").replace(/^0+/, "");
const ll = (s) => { const m = String(s || "").match(/POINT\s*\(\s*([-\d.]+)\s+([-\d.]+)\s*\)/i); return m ? [+m[1], +m[2]] : null; }; // [lon, lat]
function meters(a, b) { // [lon, lat] -> m
  const R = 6371000, r = Math.PI / 180;
  const dLat = (b[1] - a[1]) * r, dLon = (b[0] - a[0]) * r;
  const h = Math.sin(dLat / 2) ** 2 + Math.cos(a[1] * r) * Math.cos(b[1] * r) * Math.sin(dLon / 2) ** 2;
  return 2 * R * Math.asin(Math.sqrt(h));
}
const r6 = (v) => Math.round(v * 1e6) / 1e6;

async function getJson(url, what) {
  const res = await fetch(url, { headers: { accept: "application/json", "user-agent": UA } });
  if (!res.ok) { const e = new Error(`${what} answered ${res.status}`); e.status = res.status; throw e; }
  return res.json();
}
async function pdok(path, params) {
  const u = new URL("https://api.pdok.nl/bzk/locatieserver/search/v3_1/" + path);
  for (const [k, v] of params) u.searchParams.append(k, v);
  const d = await getJson(u, "PDOK " + path);
  return (d.response && d.response.docs) || [];
}

// "Rozengaard 11 4" / "Dorpsstraat 12 A" / "Laan 3-II" -> parts
export function splitAddress(s) {
  const m = String(s || "").trim().match(/^(.*?)\s+(\d+)\s*([A-Za-z](?![A-Za-z]))?(?:\s*[-\s]\s*([0-9A-Za-z]+))?$/);
  return m ? { street: m[1], nr: m[2], letter: (m[3] || "").toUpperCase(), toev: (m[4] || "").toUpperCase() } : null;
}

// 1. the house itself: strict first, then broader searches; the best match by street, number, letter, addition
async function findSelf(info, trace = []) {
  const a = splitAddress(info.address);
  if (!a || !info.postcode) throw new Error("no address for this listing yet");
  const pc = String(info.postcode).replace(/\s+/g, "").toUpperCase();
  const norm = (x) => String(x || "").toUpperCase().replace(/[^A-Z0-9]/g, "");
  const full = norm(info.address);
  const score = (d) => {
    let sc = 0;
    // street names can end in a number ("Rozengaard 11" + house 4 = "Rozengaard 11 4"): compare the whole thing too
    if (norm(`${d.straatnaam || ""}${d.huis_nlt || d.huisnummer || ""}`) === full) sc += 10;
    if (norm(d.postcode) === pc) sc += 4;
    if (norm(d.straatnaam) === norm(a.street)) sc += 3;
    if (String(d.huisnummer) === a.nr) sc += 4;
    if (norm(d.huisletter) === a.letter) sc += 1;
    if (norm(d.huisnummertoevoeging) === a.toev) sc += 2;
    if (norm(d.huis_nlt) === norm(a.nr + a.letter + a.toev)) sc += 3;
    if (!a.toev && !a.letter && norm(d.huis_nlt) === a.nr) sc += 1;
    return sc;
  };
  const tries = [
    ["postcode + number", [["q", `${a.street} ${a.nr}`], ["fq", "type:adres"], ["fq", "postcode:" + pc], ["fq", "huisnummer:" + a.nr], ["rows", "50"], ["fl", "*"]]],
    ["address text", [["q", `${info.address} ${pc} ${info.city || ""}`.trim()], ["fq", "type:adres"], ["rows", "20"], ["fl", "*"]]],
    ["postcode", [["q", pc], ["fq", "type:adres"], ["fq", "postcode:" + pc], ["rows", "100"], ["fl", "*"]]],
    ["street + city", [["q", `${a.street} ${info.city || ""}`.trim()], ["fq", "type:adres"], ["rows", "100"], ["fl", "*"]]],
  ];
  let best = null, bestScore = -1;
  for (const [name, params] of tries) {
    let docs = [];
    try { docs = await pdok("free", params); } catch (e) { trace.push(`search by ${name}: ${e.message || e}`); continue; }
    for (const d of docs) { const sc = score(d); if (sc > bestScore) { best = d; bestScore = sc; } }
    trace.push(`search by ${name}: ${docs.length} found` + (docs[0] ? `, e.g. ${docs[0].weergavenaam}` : "") + (best ? ` · best so far ${best.weergavenaam} (${bestScore})` : ""));
    if (bestScore >= 13) break; // postcode + street + number + the addition all match
  }
  if (!best || bestScore < 7) throw new Error(`PDOK doesn't know ${info.address} ${pc}` + (best ? ` (closest: ${best.weergavenaam})` : ""));
  return { doc: best, point: ll(best.centroide_ll) };
}

// 2. every address within 150 m (nearest first)
async function around(point) {
  let docs;
  try {
    docs = await pdok("reverse", [["lat", point[1]], ["lon", point[0]], ["type", "adres"], ["rows", "100"], ["fl", "*"]]);
  } catch (e) {
    docs = await pdok("reverse", [["lat", point[1]], ["lon", point[0]], ["type", "adres"], ["rows", "100"]]);
  }
  return docs;
}

// 3. building outlines and living areas from the BAG
function bbox(point, m) {
  const dLat = m / 111320, dLon = m / (111320 * Math.cos(point[1] * Math.PI / 180));
  return [point[0] - dLon, point[1] - dLat, point[0] + dLon, point[1] + dLat].map(r6).join(",");
}
async function bag(collection, point) {
  const u = `https://api.pdok.nl/kadaster/bag/ogc/v2/collections/${collection}/items?f=json&limit=1000&bbox=${bbox(point, RADIUS + 25)}`;
  const d = await getJson(u, "BAG " + collection);
  return Array.isArray(d.features) ? d.features : [];
}
function rings(geom) {
  if (!geom) return [];
  if (geom.type === "Polygon") return [geom.coordinates[0]];
  if (geom.type === "MultiPolygon") return geom.coordinates.map((p) => p[0]);
  return [];
}
function inside(pt, ring) {
  let c = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i], [xj, yj] = ring[j];
    if ((yi > pt[1]) !== (yj > pt[1]) && pt[0] < ((xj - xi) * (pt[1] - yi)) / (yj - yi) + xi) c = !c;
  }
  return c;
}
const centroid = (ring) => { let x = 0, y = 0; for (const p of ring) { x += p[0]; y += p[1]; } return [x / ring.length, y / ring.length]; };

async function buildArea(info, trace = []) {
  const sample = {};
  trace.push("find the house: " + info.address + " " + info.postcode);
  const { doc, point } = await findSelf(info, trace);
  trace.push("found: " + (doc && doc.weergavenaam) + " at " + JSON.stringify(point));
  if (!point) throw new Error("PDOK gave no point for the house");
  sample.self = doc;
  const docs = await around(point);
  trace.push("addresses around it: " + docs.length);
  sample.reverse = docs[0] || null;
  const addrs = [];
  for (const d of docs) {
    const p = ll(d.centroide_ll);
    const dist = d.afstand != null ? +d.afstand : p ? meters(point, p) : null;
    if (dist != null && dist > RADIUS) continue;
    const nid = d.nummeraanduiding_id || null;
    addrs.push({
      nid, aoid: d.adresseerbaarobject_id || null, name: d.weergavenaam ? String(d.weergavenaam).replace(/,\s*\d{4}\s?[A-Z]{2}\s+.*$/, "") : d.id,
      street: d.straatnaam || null, nr: d.huisnummer ?? null, pc: d.postcode || null,
      dist: dist == null ? null : Math.round(dist * 10) / 10, lon: p ? r6(p[0]) : null, lat: p ? r6(p[1]) : null,
    });
  }
  const selfNid = doc.nummeraanduiding_id;
  let self = addrs.find((x) => x.nid && x.nid === selfNid);
  if (!self) {
    self = { nid: selfNid, aoid: doc.adresseerbaarobject_id, name: String(doc.weergavenaam || info.address).replace(/,\s*\d{4}\s?[A-Z]{2}\s+.*$/, ""), dist: 0, lon: r6(point[0]), lat: r6(point[1]) };
    addrs.unshift(self);
  }
  self.self = true; self.dist = 0;

  // building outlines (pand) + living areas (verblijfsobject); either may fail without stopping the rest
  let panden = [], errors = [];
  try {
    const feats = await bag("pand", point);
    sample.pand = feats[0] ? feats[0].properties : null;
    for (const f of feats) {
      const p = f.properties || {};
      for (const ring of rings(f.geometry)) {
        if (ring.length < 4 || meters(point, centroid(ring)) > RADIUS + 20) continue;
        panden.push({ id: plain(p.identificatie || f.id), built: p.bouwjaar ? +String(p.bouwjaar).slice(0, 4) : null, status: p.status || null, ring: ring.map((c) => [r6(c[0]), r6(c[1])]) });
      }
    }
    trace.push("building outlines: " + panden.length);
  } catch (e) { errors.push(String(e.message || e)); trace.push("outlines failed: " + (e.message || e)); }
  try {
    const feats = await bag("verblijfsobject", point);
    trace.push("verblijfsobjecten: " + feats.length);
    sample.verblijfsobject = feats[0] ? feats[0].properties : null;
    const area = new Map();
    for (const f of feats) {
      const p = f.properties || {};
      const id = plain(p.identificatie || f.id);
      if (id) area.set(id, { m2: p.oppervlakte != null ? +p.oppervlakte : null, use: p.gebruiksdoel || null, built: p.bouwjaar ? +String(p.bouwjaar).slice(0, 4) : null });
    }
    for (const a of addrs) {
      const v = area.get(plain(a.aoid));
      if (v) { a.area = v.m2; a.use = Array.isArray(v.use) ? v.use.join(", ") : v.use; if (v.built) a.built = v.built; }
    }
  } catch (e) { errors.push(String(e.message || e)); }
  // which building each address is in
  for (const a of addrs) {
    if (a.lon == null) continue;
    const pd = panden.find((p) => inside([a.lon, a.lat], p.ring));
    if (pd) { a.pand = pd.id; if (!a.built && pd.built) a.built = pd.built; }
  }
  return { center: [r6(point[0]), r6(point[1])], addrs, panden, errors, sample, total: docs.length };
}

// 4. WOZ values of one address
async function woz(nid) {
  const d = await getJson(`https://api.kadaster.nl/lvwoz/wozwaardeloket-api/v1/wozwaarde/nummeraanduiding/${encodeURIComponent(nid)}`, "WOZ-waardeloket");
  const list = Array.isArray(d.wozWaarden) ? d.wozWaarden : Array.isArray(d) ? d : [];
  const byYear = new Map();
  for (const w of list) {
    const v = w.vastgesteldeWaarde ?? w.wozwaarde;
    const y = parseInt(String(w.peildatum || w.waardepeildatum || "").slice(0, 4), 10);
    if (v != null && y) byYear.set(y, +v);
  }
  const o = d.wozObject || {};
  return { v: [...byYear.entries()].sort((a, b) => a[0] - b[0]), ground: o.grondoppervlakte != null ? +o.grondoppervlakte : null, raw: d };
}

export async function handleBuren(request, env, url, id, info) {
  const trace = [];
  try {
    return await buren(request, env, url, id, info, trace);
  } catch (e) {
    console.error("buren", e);
    const debug = url.searchParams.get("debug") === "1";
    return json({ error: "Couldn't load the neighbours: " + (e.message || e), trace: debug ? trace : undefined, stack: debug ? String(e.stack || "") : undefined }, debug ? 200 : 500);
  }
}

async function buren(request, env, url, id, info, trace) {
  await schema(env);
  const n = Math.max(1, Math.min(MAX_N, parseInt(url.searchParams.get("n") || "10", 10) || 10));
  const debug = url.searchParams.get("debug") === "1";
  const now = Date.now();

  // the area: kept for 90 days (fresh=1 builds it again)
  let row = await env.DB.prepare(`SELECT data, fetched FROM funda_area WHERE id = ?`).bind(id).first();
  let area = null;
  try { area = row ? JSON.parse(row.data) : null; } catch {}
  if (!area || now - row.fetched > AREA_TTL || (url.searchParams.get("fresh") === "1" && now - row.fetched > 600000)) {
    try {
      area = await buildArea(info, trace);
      await env.DB.prepare(`INSERT OR REPLACE INTO funda_area (id, data, fetched) VALUES (?, ?, ?)`).bind(id, JSON.stringify(area), now).run();
    } catch (e) {
      trace.push("area failed: " + (e.message || e));
      if (!area) return json({ error: "Couldn't find the houses around it: " + (e.message || e), trace: debug ? trace : undefined }, debug ? 200 : 502);
    }
  }

  const self = area.addrs.find((a) => a.self);
  const others = area.addrs.filter((a) => !a.self && a.nid).sort((a, b) => (a.dist ?? 1e9) - (b.dist ?? 1e9));
  const chosen = [self, ...others.slice(0, n)].filter(Boolean);

  // WOZ values we already have
  const have = new Map();
  const ids = chosen.map((a) => a.nid).filter(Boolean);
  for (let i = 0; i < ids.length; i += 50) {
    const part = ids.slice(i, i + 50);
    const { results } = await env.DB.prepare(`SELECT nid, data, fetched FROM funda_woz WHERE nid IN (${part.map(() => "?").join(",")})`).bind(...part).all();
    for (const r of results || []) { try { have.set(r.nid, { ...JSON.parse(r.data), fetched: r.fetched }); } catch {} }
  }
  // look up what is missing or old: at most PER_CALL now, in groups of 4
  const todo = chosen.filter((a) => a.nid && (!have.has(a.nid) || now - have.get(a.nid).fetched > WOZ_TTL));
  const errors = [...(area.errors || [])];
  let looked = 0, stop = false, rawSample = null;
  const batch = todo.slice(0, PER_CALL);
  for (let i = 0; i < batch.length && !stop; i += 4) {
    await Promise.all(batch.slice(i, i + 4).map(async (a) => {
      try {
        const w = await woz(a.nid);
        if (!rawSample) rawSample = w.raw;
        const data = { v: w.v, ground: w.ground };
        await env.DB.prepare(`INSERT OR REPLACE INTO funda_woz (nid, data, fetched) VALUES (?, ?, ?)`).bind(a.nid, JSON.stringify(data), now).run();
        have.set(a.nid, { ...data, fetched: now });
        looked++;
      } catch (e) {
        if (e.status === 404) { // no WOZ value for this address (a garage, a shop...): remember that too
          await env.DB.prepare(`INSERT OR REPLACE INTO funda_woz (nid, data, fetched) VALUES (?, ?, ?)`).bind(a.nid, JSON.stringify({ v: [], none: true }), now).run();
          have.set(a.nid, { v: [], none: true, fetched: now });
        } else {
          errors.push(`${a.name}: ${e.message || e}`);
          if (e.status === 429 || e.status === 403) stop = true; // being asked to slow down: stop for now
        }
      }
    }));
  }
  const pending = chosen.filter((a) => a.nid && !have.has(a.nid)).length;

  const out = (a) => {
    const w = a.nid ? have.get(a.nid) : null;
    return { ...a, woz: w ? w.v : null, ground: w ? w.ground : null };
  };
  const body = {
    radius: RADIUS, center: area.center, n, all: others.length,
    self: self ? out(self) : null,
    houses: chosen.filter((a) => !a.self).map(out),
    others: others.slice(n).map((a) => ({ nid: a.nid, name: a.name, dist: a.dist, lon: a.lon, lat: a.lat, pand: a.pand, area: a.area })),
    panden: area.panden,
    pending, looked, stopped: stop, errors: errors.slice(0, 8),
    areaFetched: row ? row.fetched : now,
    sources: ["PDOK Locatieserver", "BAG (Kadaster, via PDOK)", "WOZ-waardeloket (Kadaster)"],
  };
  if (debug) body.debug = { trace, sample: area.sample, woz: rawSample, total: area.total };
  return json(body);
}

// ---------- houses for sale around it (Funda's own search, the one the phone app uses) ----------
const FORSALE_TTL = 6 * 3600000;
const hexs = (n) => Array.from(crypto.getRandomValues(new Uint8Array(n)), (b) => b.toString(16).padStart(2, "0")).join("");
const digs = (n) => Array.from(crypto.getRandomValues(new Uint8Array(n)), (b) => String(b % 10)).join("").replace(/^0/, "1");
async function fundaSearch(params) {
  const trace = digs(19), parent = hexs(7);
  const body = JSON.stringify({ index: "listings-wonen-searcher-alias-prod" }) + "\n" + JSON.stringify({ id: "search_result_20260227", params }) + "\n";
  const res = await fetch("https://listing-search-wonen.funda.io/_msearch/template", {
    method: "POST",
    headers: {
      "user-agent": "Dart/3.11 (dart:io)", "content-type": "application/json", accept: "application/json", referer: "https://www.funda.nl/",
      "x-datadog-sampling-priority": "0", "x-datadog-origin": "rum", "x-datadog-parent-id": trace, tracestate: `dd=s:0;o:rum;p:${parent}`,
      traceparent: `00-${Math.floor(Date.now() / 1000).toString(16)}00000000${trace.slice(0, 16)}-${parent}-00`,
    },
    body,
  });
  if (!res.ok) throw new Error(`Funda search answered ${res.status}`);
  const d = await res.json();
  const r0 = (d.responses || [])[0] || {};
  if (r0.error) throw new Error("Funda search: " + JSON.stringify(r0.error).slice(0, 160));
  return { hits: (r0.hits && r0.hits.hits) || [], total: r0.hits && r0.hits.total ? (r0.hits.total.value ?? r0.hits.total) : null };
}
const first = (v) => (Array.isArray(v) ? v[0] : v);
function hitToHouse(h) {
  const src = h._source || h;
  const a = src.address || {};
  const price = src.price || {};
  const amount = first(price.selling_price);
  const loc = src.location || src.coordinates || src.centroid || src.geo || null;
  let lon = null, lat = null;
  if (loc) {
    const l = first(loc);
    if (l && typeof l === "object") { lat = +(l.lat ?? l.latitude ?? (Array.isArray(l.coordinates) ? l.coordinates[1] : NaN)); lon = +(l.lon ?? l.lng ?? l.longitude ?? (Array.isArray(l.coordinates) ? l.coordinates[0] : NaN)); }
    else if (typeof l === "string" && l.includes(",")) { const [x, y] = l.split(",").map(Number); lat = x; lon = y; }
  }
  const url = src.object_detail_page_relative_url ? "https://www.funda.nl" + src.object_detail_page_relative_url : null;
  return {
    street: a.street_name || null, nr: a.house_number ?? null, suffix: a.house_number_suffix || null, pc: (a.postal_code || "").replace(/\s+/g, "").toUpperCase() || null,
    city: a.city || null, ids: (a.identifiers || []).map((x) => String(x)), price: amount != null ? +amount : null,
    priceType: price.selling_price_condition || null, area: first(src.floor_area) != null ? +first(src.floor_area) : null,
    status: first(src.availability) || null, url, lon: Number.isFinite(lon) ? lon : null, lat: Number.isFinite(lat) ? lat : null,
    tiny: url ? (url.match(/\/(\d{7,9})\/?$/) || [])[1] || null : null,
  };
}
export async function handleForSale(request, env, url, id, info) {
  await schema(env);
  const debug = url.searchParams.get("debug") === "1";
  const now = Date.now();
  const row = await env.DB.prepare(`SELECT data, fetched FROM funda_forsale WHERE id = ?`).bind(id).first();
  if (row && now - row.fetched < FORSALE_TTL && !debug) {
    try { return json({ ...JSON.parse(row.data), cached: true, fetched: row.fetched }); } catch {}
  }
  const pc = String(info.postcode || "").replace(/\s+/g, "").toLowerCase();
  const city = String(info.city || "").toLowerCase();
  const base = { availability: ["available", "negotiations"], type: ["single"], zoning: ["residential"], object_type: ["house", "apartment"],
    publication_date: { no_preference: true }, offering_type: "buy", sort: { field: null, order: null } };
  const trace = [];
  const tries = [
    pc && ["postcode, 1 km", { radius_search: { index: "geo-wonen-alias-prod", id: pc + "-0", path: "area_with_radius.1" } }],
    pc && ["postcode area, 1 km", { radius_search: { index: "geo-wonen-alias-prod", id: pc.slice(0, 4) + "-0", path: "area_with_radius.1" } }],
    pc && ["postcode", { selected_area: [pc] }],
    pc && ["postcode area", { selected_area: [pc.slice(0, 4)] }],
  ].filter(Boolean);
  let list = [], used = null, raw = null;
  for (const [name, where] of tries) {
    try {
      const out = [];
      for (let page = 0; page < 4; page++) {
        const { hits, total } = await fundaSearch({ ...base, ...where, page: { from: page * 15 } });
        if (!raw && hits[0]) raw = hits[0];
        out.push(...hits.map(hitToHouse));
        if (hits.length < 15 || (total != null && out.length >= total)) break;
      }
      trace.push(`${name}: ${out.length} for sale`);
      if (out.length) { list = out; used = name; break; }
    } catch (e) { trace.push(`${name}: ${e.message || e}`); }
  }
  const body = { list, used, count: list.length };
  if (list.length || !row) await env.DB.prepare(`INSERT OR REPLACE INTO funda_forsale (id, data, fetched) VALUES (?, ?, ?)`).bind(id, JSON.stringify(body), now).run();
  return json({ ...body, fetched: now, trace: debug ? trace : undefined, sample: debug ? raw : undefined });
}
