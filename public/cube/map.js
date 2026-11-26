// map.js: a piece of OpenStreetMap -> the ERROR-CUBE's .map file (MAP.EXE shows it in 3D).
// The format is described in esp32/ErrorCube/src/apps/apps_map.cpp.
//   CubeMap.fetch(lat, lon, sizeM, prog) -> Promise<osm json>   (Overpass API)
//   CubeMap.pack(osm, lat, lon, sizeM, name) -> { bytes: Uint8Array, stats }
(function () {
  'use strict';
  const SERVERS = ['https://overpass-api.de/api/interpreter', 'https://overpass.kumi.systems/api/interpreter', 'https://maps.mail.ru/osm/tools/overpass/api/interpreter'];

  function bbox(lat, lon, size) {
    const h = size / 2, dLat = h / 110540, dLon = h / (111320 * Math.cos(lat * Math.PI / 180));
    return [lat - dLat, lon - dLon, lat + dLat, lon + dLon];
  }
  async function fetchOsm(lat, lon, size, prog) {
    const b = bbox(lat, lon, size).map(v => v.toFixed(6)).join(',');
    const q = `[out:json][timeout:90];(
      way["building"](${b}); relation["building"]["type"="multipolygon"](${b});
      way["highway"](${b});
      way["railway"~"^(rail|light_rail|tram|subway|narrow_gauge)$"](${b});
      way["waterway"~"^(river|canal|stream)$"](${b});
      way["natural"~"^(water|wood|scrub|grassland|heath|beach|sand|wetland)$"](${b});
      relation["natural"~"^(water|wood|scrub|wetland)$"](${b});
      way["water"](${b});
      way["landuse"~"^(forest|grass|meadow|farmland|recreation_ground|village_green|cemetery|orchard|vineyard|allotments|basin|reservoir|flowerbed)$"](${b});
      relation["landuse"~"^(forest|grass|meadow|farmland|basin|reservoir)$"](${b});
      way["leisure"~"^(park|garden|pitch|playground|golf_course|nature_reserve|common)$"](${b});
      relation["leisure"~"^(park|nature_reserve|garden)$"](${b});
      way["amenity"="parking"](${b});
      node["natural"="tree"](${b});
    );out geom qt;`;
    let last = '';
    for (const url of SERVERS) {
      try {
        prog && prog('ASKING OPENSTREETMAP (' + new URL(url).hostname + ')...');
        const r = await fetch(url, { method: 'POST', body: 'data=' + encodeURIComponent(q), headers: { 'Content-Type': 'application/x-www-form-urlencoded' } });
        if (!r.ok) { last = 'HTTP ' + r.status; continue; }
        prog && prog('DOWNLOADING...');
        return await r.json();
      } catch (e) { last = e.message; }
    }
    throw new Error('OpenStreetMap did not answer (' + last + '). Try again in a minute, or a smaller map.');
  }

  // ------------------------------------------------------------ geometry helpers (metres, x east, y north)
  const area2 = p => { let a = 0; for (let i = 0, j = p.length - 1; i < p.length; j = i++) a += p[j][0] * p[i][1] - p[i][0] * p[j][1]; return a / 2; };
  function clipPoly(p, h) {             // Sutherland-Hodgman against the square [-h, h]
    const edges = [[0, -h, 1], [0, h, -1], [1, -h, 1], [1, h, -1]];
    for (const [ax, v, s] of edges) {
      if (!p.length) return p;
      const out = [], inside = q => (q[ax] - v) * s >= 0;
      for (let i = 0; i < p.length; i++) {
        const a = p[i], b = p[(i + 1) % p.length], ai = inside(a), bi = inside(b);
        if (ai) out.push(a);
        if (ai !== bi) { const t = (v - a[ax]) / (b[ax] - a[ax]); out.push([a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t]); }
      }
      p = out;
    }
    return p;
  }
  function clipLine(p, h) {             // a polyline cut to the square: maybe several pieces
    const parts = []; let cur = [];
    const ins = q => Math.abs(q[0]) <= h && Math.abs(q[1]) <= h;
    for (let i = 0; i + 1 < p.length; i++) {
      let a = p[i], b = p[i + 1];
      // Liang-Barsky
      let t0 = 0, t1 = 1; const dx = b[0] - a[0], dy = b[1] - a[1];
      const P = [-dx, dx, -dy, dy], Q = [a[0] + h, h - a[0], a[1] + h, h - a[1]];
      let ok = true;
      for (let k = 0; k < 4; k++) {
        if (P[k] === 0) { if (Q[k] < 0) { ok = false; break; } continue; }
        const r = Q[k] / P[k];
        if (P[k] < 0) { if (r > t1) { ok = false; break; } if (r > t0) t0 = r; }
        else { if (r < t0) { ok = false; break; } if (r < t1) t1 = r; }
      }
      if (!ok) { if (cur.length > 1) parts.push(cur); cur = []; continue; }
      const A = [a[0] + dx * t0, a[1] + dy * t0], B = [a[0] + dx * t1, a[1] + dy * t1];
      if (!cur.length) cur.push(A);
      cur.push(B);
      if (t1 < 1) { parts.push(cur); cur = []; }
    }
    if (cur.length > 1) parts.push(cur);
    return parts;
  }
  function simplify(p, tol, closed) {   // Douglas-Peucker
    if (p.length < 4) return p;
    const keep = new Uint8Array(p.length); keep[0] = keep[p.length - 1] = 1;
    const st = [[0, p.length - 1]];
    while (st.length) {
      const [a, b] = st.pop(); let md = 0, mi = -1;
      const ax = p[a][0], ay = p[a][1], bx = p[b][0], by = p[b][1], dx = bx - ax, dy = by - ay, l = Math.hypot(dx, dy) || 1e-9;
      for (let i = a + 1; i < b; i++) { const d = Math.abs((p[i][0] - ax) * dy - (p[i][1] - ay) * dx) / l; if (d > md) { md = d; mi = i; } }
      if (md > tol) { keep[mi] = 1; st.push([a, mi], [mi, b]); }
    }
    const out = p.filter((_, i) => keep[i]);
    return closed && out.length < 3 ? p.slice(0, 3) : out;
  }
  function fit(p, closed, tol) {        // at most 240 points (what the cube draws in one go)
    let t = tol, q = simplify(p, t, closed);
    while (q.length > 240) { t *= 1.8; q = simplify(p, t, closed); }
    return q;
  }
  function joinRings(segs) {            // multipolygon outer ways -> closed rings
    const rings = [], open = segs.filter(s => s.length > 1).map(s => s.slice());
    const same = (a, b) => Math.abs(a[0] - b[0]) < 0.01 && Math.abs(a[1] - b[1]) < 0.01;
    while (open.length) {
      let r = open.shift(), grew = true;
      while (!same(r[0], r[r.length - 1]) && grew) {
        grew = false;
        for (let i = 0; i < open.length; i++) {
          const s = open[i], e = r[r.length - 1];
          if (same(e, s[0])) r = r.concat(s.slice(1));
          else if (same(e, s[s.length - 1])) r = r.concat(s.slice(0, -1).reverse());
          else if (same(r[0], s[s.length - 1])) r = s.slice(0, -1).concat(r);
          else if (same(r[0], s[0])) r = s.slice(1).reverse().concat(r);
          else continue;
          open.splice(i, 1); grew = true; break;
        }
      }
      if (r.length > 3) rings.push(r);
    }
    return rings;
  }
  function inPoly(x, y, p) { let c = false; for (let i = 0, j = p.length - 1; i < p.length; j = i++) if ((p[i][1] > y) !== (p[j][1] > y) && x < (p[j][0] - p[i][0]) * (y - p[i][1]) / (p[j][1] - p[i][1]) + p[i][0]) c = !c; return c; }

  // ------------------------------------------------------------ what's what
  function areaKind(t) {
    if (t.natural === 'water' || t.water || t.landuse === 'basin' || t.landuse === 'reservoir') return 1;
    if (t.natural === 'wood' || t.landuse === 'forest') return 2;
    if (t.leisure === 'pitch') return 7;
    if (t.amenity === 'parking') return 6;
    if (t.natural === 'beach' || t.natural === 'sand') return 5;
    if (['farmland', 'orchard', 'vineyard'].includes(t.landuse)) return 4;
    if (t.landuse || t.leisure || t.natural) return 3;
    return 0;
  }
  function buildKind(b) {
    if (['house', 'detached', 'semidetached_house', 'terrace', 'bungalow', 'farm', 'cabin'].includes(b)) return 1;
    if (['apartments', 'residential', 'dormitory', 'hotel'].includes(b)) return 2;
    if (['commercial', 'retail', 'office', 'supermarket', 'kiosk'].includes(b)) return 3;
    if (['industrial', 'warehouse', 'factory', 'manufacture', 'hangar'].includes(b)) return 4;
    if (['school', 'public', 'hospital', 'civic', 'university', 'government', 'train_station', 'college', 'kindergarten'].includes(b)) return 5;
    if (['church', 'cathedral', 'chapel', 'mosque', 'synagogue', 'temple'].includes(b)) return 6;
    return 0;
  }
  function buildHeight(t) {
    const h = parseFloat(String(t.height || '').replace(',', '.'));
    if (h > 0) return h;
    const lv = parseFloat(t['building:levels']);
    if (lv > 0) return lv * 3.1 + (t['roof:levels'] ? parseFloat(t['roof:levels']) * 2.5 || 0 : 1.5);
    const b = t.building;
    if (['garage', 'garages', 'shed', 'carport', 'hut', 'roof', 'kiosk'].includes(b)) return 3;
    if (['house', 'detached', 'semidetached_house', 'terrace', 'bungalow', 'cabin'].includes(b)) return 7;
    if (['apartments', 'residential', 'hotel', 'office', 'hospital'].includes(b)) return 14;
    if (['church', 'cathedral'].includes(b)) return 20;
    if (['industrial', 'warehouse', 'factory', 'retail', 'supermarket'].includes(b)) return 8;
    return 8;
  }
  function roadKind(t) {
    if (t.waterway) return 6;
    if (t.railway) return 5;
    const h = t.highway;
    if (/^(motorway|trunk)/.test(h)) return 4;
    if (/^(primary|secondary)/.test(h)) return 3;
    if (/^(tertiary|residential)/.test(h)) return 2;
    if (/^(service|living_street|unclassified|road)/.test(h)) return 1;
    if (h) return 0;
    return -1;
  }

  // ------------------------------------------------------------ pack it
  function pack(osm, lat0, lon0, size, name) {
    const H = size / 2, kx = 111320 * Math.cos(lat0 * Math.PI / 180), ky = 110540;
    const xy = g => [(g.lon - lon0) * kx, (g.lat - lat0) * ky];
    const areas = [], builds = [], roads = [], trees = [];
    const addArea = (kind, ring) => { const c = clipPoly(ring, H); if (c.length >= 3 && Math.abs(area2(c)) > 4) areas.push([kind, fit(c, true, Math.abs(area2(c)) > 20000 ? 1.5 : 0.6)]); };
    const addBuild = (t, ring) => {
      if (ring.length < 4) return;
      let cx = 0, cy = 0; ring.forEach(p => { cx += p[0]; cy += p[1]; }); cx /= ring.length; cy /= ring.length;
      if (Math.abs(cx) > H || Math.abs(cy) > H) return;
      const c = clipPoly(ring, H);
      if (c.length >= 3 && Math.abs(area2(c)) > 3) builds.push([Math.min(255, Math.round(buildHeight(t))), buildKind(t.building), fit(c, true, 0.4)]);
    };
    for (const el of osm.elements || []) {
      const t = el.tags || {};
      if (el.type === 'node') { if (t.natural === 'tree') { const p = xy(el); if (Math.abs(p[0]) < H && Math.abs(p[1]) < H) trees.push([p[0], p[1], Math.min(30, parseFloat(t.height) || 9)]); } continue; }
      let rings = [];
      if (el.type === 'way' && el.geometry) {
        const p = el.geometry.filter(Boolean).map(xy);
        const closed = p.length > 3 && Math.abs(p[0][0] - p[p.length - 1][0]) < 0.01 && Math.abs(p[0][1] - p[p.length - 1][1]) < 0.01;
        if (t.building && closed) { addBuild(t, p.slice(0, -1)); continue; }
        const rk = roadKind(t);
        if (rk >= 0 && !(t.area === 'yes')) {
          for (const part of clipLine(p, H)) roads.push([rk, fit(part, false, 0.5)]);
          continue;
        }
        if (closed) rings = [p.slice(0, -1)];
      } else if (el.type === 'relation' && el.members) {
        rings = joinRings(el.members.filter(m => m.type === 'way' && m.role !== 'inner' && m.geometry).map(m => m.geometry.filter(Boolean).map(xy)));
        if (t.building) { rings.forEach(r => addBuild(t, r)); continue; }
      }
      const k = areaKind(t);
      if (k) rings.forEach(r => addArea(k, r));
    }
    // forests and parks get trees
    let room = 2600 - trees.length;
    for (const [k, p] of areas) {
      if ((k !== 2 && k !== 3) || room <= 0) continue;
      const a = Math.abs(area2(p)), n = Math.min(room, Math.round(a / (k === 2 ? 140 : 900)));
      let minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
      p.forEach(q => { minx = Math.min(minx, q[0]); maxx = Math.max(maxx, q[0]); miny = Math.min(miny, q[1]); maxy = Math.max(maxy, q[1]); });
      for (let i = 0, tries = 0; i < n && tries < n * 6; tries++) {
        const x = minx + Math.random() * (maxx - minx), y = miny + Math.random() * (maxy - miny);
        if (inPoly(x, y, p)) { trees.push([x, y, (k === 2 ? 11 : 8) + Math.random() * 8]); i++; room--; }
      }
    }
    // the bytes
    const parts = [], u8 = a => parts.push(new Uint8Array(a));
    const dv = n => { const b = new ArrayBuffer(n); return [b, new DataView(b)]; };
    const pts = p => { const [b, d] = dv(2 + p.length * 4); d.setUint16(0, p.length, true); p.forEach((q, i) => { d.setInt16(2 + i * 4, Math.round(q[0] * 10), true); d.setInt16(4 + i * 4, Math.round(q[1] * 10), true); }); parts.push(new Uint8Array(b)); };
    const sec = (tag, n) => { const [b, d] = dv(5); d.setUint8(0, tag.charCodeAt(0)); d.setUint32(1, n, true); parts.push(new Uint8Array(b)); };
    { const [b, d] = dv(16); [67, 77, 65, 80].forEach((c, i) => d.setUint8(i, c)); d.setUint8(4, 1); d.setUint16(6, size, true); d.setFloat32(8, lat0, true); d.setFloat32(12, lon0, true); parts.push(new Uint8Array(b)); }
    const nm = new TextEncoder().encode(String(name || 'MAP').slice(0, 38));
    sec('N', nm.length); parts.push(nm);
    sec('A', areas.length); areas.forEach(([k, p]) => { u8([k]); pts(p); });
    sec('B', builds.length); builds.forEach(([h, k, p]) => { u8([h, k]); pts(p); });
    sec('R', roads.length); roads.forEach(([k, p]) => { u8([k]); pts(p); });
    sec('T', trees.length);
    { const [b, d] = dv(trees.length * 5); trees.forEach((t, i) => { d.setInt16(i * 5, Math.round(t[0] * 10), true); d.setInt16(i * 5 + 2, Math.round(t[1] * 10), true); d.setUint8(i * 5 + 4, Math.round(t[2])); }); parts.push(new Uint8Array(b)); }
    const n = parts.reduce((s, p) => s + p.length, 0), bytes = new Uint8Array(n);
    let o = 0; parts.forEach(p => { bytes.set(p, o); o += p.length; });
    return { bytes, stats: { buildings: builds.length, roads: roads.length, areas: areas.length, trees: trees.length } };
  }
  window.CubeMap = { fetch: fetchOsm, pack, bbox };
})();
