/* GeoLock arrows: the pointer styles of GEOLOCK.EXE, shared with its settings at error.blue/hud.
     GeoArrows.TYPES                          [{ id, name, d3 }]
     GeoArrows.draw(ctx, type, { x, y, size, rel, color, t, alpha })
       x, y   centre on the canvas     size   arrow length in px
       rel    degrees: 0 = straight ahead, + = to the right, 180 = behind you
       color  "#rrggbb"                 t      seconds (for the gentle bob and the flowing chevrons)
   3D styles float over the ground, seen from a little above and behind, like a game's mission arrow.
   2D styles turn flat on the screen: up = ahead. */
(() => {
  "use strict";
  const D2R = Math.PI / 180;
  const TYPES = [
    { id: "arrow",    name: "3D ARROW",     d3: true },
    { id: "chevron",  name: "3D CHEVRON",   d3: true },
    { id: "dart",     name: "3D DART",      d3: true },
    { id: "needle",   name: "3D NEEDLE",    d3: true },
    { id: "wire",     name: "3D WIREFRAME", d3: true },
    { id: "flat",     name: "2D ARROW",     d3: false },
    { id: "outline",  name: "2D OUTLINE",   d3: false },
    { id: "chevrons", name: "2D CHEVRONS",  d3: false },
  ];
  const rgb = (h) => { const n = parseInt(String(h).replace("#", ""), 16) || 0x33ff66; return [(n >> 16) & 255, (n >> 8) & 255, n & 255]; };
  const mix = (a, b, k) => "rgb(" + a.map((v, i) => Math.round(v + (b[i] - v) * Math.max(0, Math.min(1, k)))).join(",") + ")";
  const BLACK = [0, 0, 0], WHITE = [255, 255, 255], GREY = [150, 150, 150];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const norm = (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };
  const crs = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const LIGHT = norm([-0.45, 0.9, -0.4]);

  /* ---------- meshes: x right, y up, z forward (the way the arrow points), length about 2 ---------- */
  // a flat outline [[x, z], ...] pulled up into a slab of height h
  function extrude(out, h, tone) {
    const n = out.length, v = [], f = [];
    for (const [x, z] of out) v.push([x, h / 2, z]);
    for (const [x, z] of out) v.push([x, -h / 2, z]);
    let area = 0;
    for (let i = 0; i < n; i++) { const a = out[i], b = out[(i + 1) % n]; area += a[0] * b[1] - b[0] * a[1]; }
    const s = area > 0 ? 1 : -1;
    f.push({ i: [...Array(n).keys()], n: [0, 1, 0], top: true, tone });
    f.push({ i: [...Array(n).keys()].map((k) => n + k).reverse(), n: [0, -1, 0], tone });
    for (let i = 0; i < n; i++) {
      const j = (i + 1) % n, dx = out[j][0] - out[i][0], dz = out[j][1] - out[i][1], L = Math.hypot(dx, dz) || 1;
      f.push({ i: [i, j, n + j, n + i], n: [(s * dz) / L, 0, (-s * dx) / L], tone });
    }
    return { v, f, edges: edgesOf(f) };
  }
  function edgesOf(f) {
    const seen = new Set(), e = [];
    for (const face of f) for (let k = 0; k < face.i.length; k++) {
      const a = face.i[k], b = face.i[(k + 1) % face.i.length], key = a < b ? a + "-" + b : b + "-" + a;
      if (!seen.has(key)) { seen.add(key); e.push([a, b]); }
    }
    return e;
  }
  // a closed solid from triangles; normals point away from its middle
  function solid(v, tris, mid, tones) {
    const f = tris.map((t, k) => {
      const p = t.map((i) => v[i]);
      let n = norm(crs(sub(p[1], p[0]), sub(p[2], p[0])));
      const c = [(p[0][0] + p[1][0] + p[2][0]) / 3, (p[0][1] + p[1][1] + p[2][1]) / 3, (p[0][2] + p[1][2] + p[2][2]) / 3];
      if (dot(n, sub(c, mid)) < 0) n = [-n[0], -n[1], -n[2]];
      return { i: t, n, tone: tones ? tones[k] : "main" };
    });
    return { v, f, edges: edgesOf(f) };
  }
  const ARROW_OUT = [[0, 1.05], [0.62, 0.2], [0.24, 0.2], [0.24, -0.95], [-0.24, -0.95], [-0.24, 0.2], [-0.62, 0.2]];
  const MESH = {
    arrow: extrude(ARROW_OUT, 0.26, "main"),
    wire: extrude(ARROW_OUT, 0.26, "main"),
    chevron: extrude([[0, 0.95], [0.82, 0.05], [0.82, -0.42], [0, 0.42], [-0.82, -0.42], [-0.82, 0.05]], 0.3, "main"),
    dart: (() => {
      const v = [[0, 0, 1.1], [-0.7, 0, -0.75], [0, 0, -0.4], [0.7, 0, -0.75], [0, 0.34, -0.3]];
      return Object.assign(solid(v, [[0, 4, 1], [0, 3, 4], [1, 4, 2], [2, 4, 3], [0, 1, 2], [0, 2, 3]], [0, 0.12, -0.15]), { shadow: [0, 3, 2, 1] });
    })(),
    needle: (() => {
      const w = 0.4, h = 0.26;
      const v = [[0, 0, 1.15], [0, 0, -1.0], [w, 0, 0.05], [0, h, 0.05], [-w, 0, 0.05], [0, -h, 0.05]];
      const tris = [[0, 2, 3], [0, 3, 4], [0, 4, 5], [0, 5, 2], [1, 3, 2], [1, 4, 3], [1, 5, 4], [1, 2, 5]];
      return Object.assign(solid(v, tris, [0, 0, 0.05], ["main", "main", "main", "main", "back", "back", "back", "back"]), { shadow: [0, 2, 1, 4] });
    })(),
  };

  /* ---------- the 3D camera ---------- */
  const TILT = 52 * D2R, DIST = 5.2;
  const CAM = [0, Math.sin(TILT) * DIST, -Math.cos(TILT) * DIST];
  const FWD = [0, -Math.sin(TILT), Math.cos(TILT)], UP = [0, Math.cos(TILT), Math.sin(TILT)];

  function draw3D(ctx, type, o) {
    const m = MESH[type] || MESH.arrow;
    const r = (o.rel || 0) * D2R, c = Math.cos(r), s = Math.sin(r);
    const bob = Math.sin((o.t || 0) * 2.2) * 0.07;
    const F = (o.size * DIST) / 2.25;
    const world = m.v.map(([x, y, z]) => [x * c + z * s, y + bob + 0.1, -x * s + z * c]);
    const cam = world.map((p) => { const d = sub(p, CAM); return [d[0], dot(d, UP), dot(d, FWD)]; });
    const scr = cam.map(([x, y, z]) => [o.x + (F * x) / z, o.y - (F * y) / z, z]);
    const col = rgb(o.color), a = o.alpha === undefined ? 1 : o.alpha;
    ctx.save();
    ctx.lineJoin = "round";
    // a soft shadow on the ground
    const sh = m.v.map(([x, , z]) => { const d = sub([x * c + z * s, -0.32, -x * s + z * c], CAM); const zc = dot(d, FWD); return [o.x + (F * d[0]) / zc, o.y - (F * dot(d, UP)) / zc]; });
    if (type !== "wire") {
      const outline = m.shadow || (m.f.find((f) => f.top) || m.f[0]).i;
      ctx.globalAlpha = a * 0.22; ctx.fillStyle = mix(BLACK, col, 0.5);
      ctx.beginPath();
      outline.forEach((k, j) => (j ? ctx.lineTo(sh[k][0], sh[k][1]) : ctx.moveTo(sh[k][0], sh[k][1])));
      ctx.closePath(); ctx.fill();
    }
    if (type === "wire") {
      const zs = cam.map((p) => p[2]), z0 = Math.min(...zs), z1 = Math.max(...zs);
      for (const pass of [0, 1]) {
        for (const [i, j] of m.edges) {
          const near = 1 - ((cam[i][2] + cam[j][2]) / 2 - z0) / ((z1 - z0) || 1);
          ctx.globalAlpha = a * (pass ? 0.35 + 0.65 * near : 0.18);
          ctx.strokeStyle = pass ? mix(col, WHITE, 0.25 * near) : mix(BLACK, col, 0.8);
          ctx.lineWidth = pass ? Math.max(1, o.size * 0.012) : Math.max(3, o.size * 0.045);
          ctx.beginPath(); ctx.moveTo(scr[i][0], scr[i][1]); ctx.lineTo(scr[j][0], scr[j][1]); ctx.stroke();
        }
      }
      ctx.restore();
      return;
    }
    // faces turned to the camera, far ones first; the top of a slab always last
    const faces = [];
    for (const f of m.f) {
      const wn = [f.n[0] * c + f.n[2] * s, f.n[1], -f.n[0] * s + f.n[2] * c];
      const cen = f.i.reduce((acc, k) => [acc[0] + world[k][0], acc[1] + world[k][1], acc[2] + world[k][2]], [0, 0, 0]).map((x) => x / f.i.length);
      if (dot(wn, sub(CAM, cen)) <= 0.001) continue;
      const zc = f.i.reduce((acc, k) => acc + cam[k][2], 0) / f.i.length;
      faces.push({ f, zc, light: Math.max(0, dot(wn, LIGHT)) });
    }
    faces.sort((p, q) => (p.f.top ? 1 : 0) - (q.f.top ? 1 : 0) || q.zc - p.zc);
    for (const { f, light } of faces) {
      const fill = f.tone === "back" ? mix(BLACK, GREY, 0.3 + 0.7 * light)         // the tail of the needle
        : f.top ? mix(col, WHITE, 0.08 + 0.18 * light) : mix(BLACK, col, 0.28 + 0.62 * light);
      ctx.globalAlpha = a;
      ctx.fillStyle = fill;
      ctx.beginPath();
      f.i.forEach((k, j) => (j ? ctx.lineTo(scr[k][0], scr[k][1]) : ctx.moveTo(scr[k][0], scr[k][1])));
      ctx.closePath(); ctx.fill();
      ctx.strokeStyle = f.top ? mix(col, WHITE, 0.55) : mix(BLACK, col, 0.85);
      ctx.lineWidth = Math.max(1, o.size * (f.top ? 0.009 : 0.006));
      ctx.globalAlpha = a * (f.top ? 0.9 : 0.6);
      ctx.stroke();
    }
    ctx.restore();
  }

  /* ---------- flat styles ---------- */
  function draw2D(ctx, type, o) {
    const col = rgb(o.color), a = o.alpha === undefined ? 1 : o.alpha, R = o.size / 2;
    ctx.save();
    ctx.translate(o.x, o.y);
    ctx.rotate((o.rel || 0) * D2R);
    ctx.lineJoin = "round"; ctx.lineCap = "round";
    const path = (pts) => { ctx.beginPath(); pts.forEach(([x, y], j) => (j ? ctx.lineTo(x * R, y * R) : ctx.moveTo(x * R, y * R))); ctx.closePath(); };
    if (type === "flat") {
      path([[0, -1], [0.74, 0.82], [0, 0.4], [-0.74, 0.82]]);
      ctx.globalAlpha = a; ctx.fillStyle = mix(BLACK, col, 1); ctx.fill();
      path([[0, -1], [0, 0.4], [-0.74, 0.82]]);
      ctx.globalAlpha = a * 0.28; ctx.fillStyle = "#000"; ctx.fill();
    } else if (type === "outline") {
      path(ARROW_OUT.map(([x, z]) => [x, -z]));
      ctx.globalAlpha = a; ctx.strokeStyle = mix(BLACK, col, 1); ctx.lineWidth = Math.max(2, o.size * 0.035); ctx.stroke();
    } else {                                                  // three chevrons flowing ahead
      const t = o.t || 0;
      for (let k = 0; k < 3; k++) {
        const y = 0.62 - k * 0.62, ph = (((t * 1.4 - k * 0.33) % 1) + 1) % 1;
        ctx.globalAlpha = a * (0.3 + 0.7 * (1 - ph));
        ctx.strokeStyle = mix(BLACK, col, 1); ctx.lineWidth = Math.max(3, o.size * 0.07);
        ctx.beginPath(); ctx.moveTo(-0.62 * R, (y + 0.34) * R); ctx.lineTo(0, (y - 0.26) * R); ctx.lineTo(0.62 * R, (y + 0.34) * R); ctx.stroke();
      }
    }
    ctx.restore();
  }

  function draw(ctx, type, o) {
    const T = TYPES.find((x) => x.id === type) || TYPES[0];
    if (T.d3) draw3D(ctx, T.id, o); else draw2D(ctx, T.id, o);
  }
  window.GeoArrows = { TYPES, draw };
})();
