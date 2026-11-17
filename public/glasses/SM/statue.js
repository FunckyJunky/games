/* Святий Миколайчик: the little gift statue, as a low-poly 3D model drawn on a 2D canvas.
   Used by the gift page (/glasses/SM/) and the glasses app (/glasses/mykolai/).

     SMStatue.LOOKS[i]   { id, name, robe, cape, trim, mitre, box, ribbon }
     SMStatue.draw(ctx, { x, y, h, yaw, tilt, look, alpha, glow, flat })
       x, y   where the bottom of the pedestal goes (canvas pixels)
       h      height of the whole statue in pixels
       yaw    turn around its own axis (radians), tilt: look down on it (radians)
       look   0-5 (the colours), alpha 0-1, glow 0-1 brightens it, flat = no outlines */
(function () {
  "use strict";
  const LOOKS = [
    { id: 0, name: "Classic Red",      robe: "#d7263d", cape: "#ffc93c", trim: "#fff3d6", mitre: "#ffc93c", box: "#22c3a6", ribbon: "#ff4f79" },
    { id: 1, name: "Frosty Blue",      robe: "#2f7bff", cape: "#dff1ff", trim: "#ffffff", mitre: "#dff1ff", box: "#ffbe0b", ribbon: "#ff2e88" },
    { id: 2, name: "Pine Forest",      robe: "#25a24c", cape: "#ffc93c", trim: "#fff3d6", mitre: "#e63946", box: "#e63946", ribbon: "#ffd166" },
    { id: 3, name: "Royal Plum",       robe: "#8a3df0", cape: "#ffbe0b", trim: "#fff3d6", mitre: "#ffbe0b", box: "#2f7bff", ribbon: "#ffbe0b" },
    { id: 4, name: "Gingerbread Gold", robe: "#f2a007", cape: "#d7263d", trim: "#fff3d6", mitre: "#d7263d", box: "#8a3df0", ribbon: "#ffffff" },
    { id: 5, name: "Candy Cane",       robe: "#ff5ca8", cape: "#ffffff", trim: "#ffffff", mitre: "#7bdff2", box: "#7bdff2", ribbon: "#ff5ca8" },
  ];
  const SKIN = "#f5c9a6", BEARD = "#ffffff", STAFF = "#ffd34d", PLINTH = "#b9773a", PLINTH_TOP = "#e2a85c", EYE = "#2a1408", CHEEK = "#ff8f8f";

  const hex = (c) => [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16)];

  /* ---------- building blocks ---------- */
  function Mesh() { return { v: [], f: [] }; }
  function face(m, idx, col, two) { m.f.push({ i: idx, c: hex(col), two: !!two }); }
  // a ring-to-ring tube (frustum); rings can sit off-centre (cz) and be flattened (sz)
  function frustum(m, y0, r0, y1, r1, n, col, o = {}) {
    const b = m.v.length, sz = o.sz || 1, cz0 = o.cz0 || 0, cz1 = o.cz1 != null ? o.cz1 : cz0, cx = o.cx || 0;
    for (let i = 0; i < n; i++) {
      const a = (i / n) * Math.PI * 2 + (o.rot || 0), s = Math.sin(a), c = Math.cos(a);
      m.v.push([cx + s * r0, y0, cz0 + c * r0 * sz], [cx + s * r1, y1, cz1 + c * r1 * sz]);
    }
    for (let i = 0; i < n; i++) {
      const j = (i + 1) % n;
      face(m, [b + i * 2, b + j * 2, b + j * 2 + 1, b + i * 2 + 1], col);
    }
    if (o.bottom && r0 > 0) face(m, [...Array(n).keys()].map((i) => b + i * 2).reverse(), o.bottom === true ? col : o.bottom);
    if (o.top && r1 > 0) face(m, [...Array(n).keys()].map((i) => b + i * 2 + 1), o.top === true ? col : o.top);
  }
  function box(m, x, y, z, sx, sy, sz, col) {
    const b = m.v.length, X = sx / 2, Y = sy / 2, Z = sz / 2;
    for (const [dx, dy, dz] of [[-1, -1, -1], [1, -1, -1], [1, 1, -1], [-1, 1, -1], [-1, -1, 1], [1, -1, 1], [1, 1, 1], [-1, 1, 1]])
      m.v.push([x + dx * X, y + dy * Y, z + dz * Z]);
    const q = [[0, 3, 2, 1], [4, 5, 6, 7], [0, 4, 7, 3], [1, 2, 6, 5], [3, 7, 6, 2], [0, 1, 5, 4]];
    for (const f of q) face(m, f.map((k) => b + k), col);
  }
  function sphere(m, x, y, z, r, nLon, nLat, col, sy = 1) {
    const b = m.v.length;
    for (let j = 0; j <= nLat; j++) for (let i = 0; i < nLon; i++) {
      const th = (j / nLat) * Math.PI, ph = (i / nLon) * Math.PI * 2;
      m.v.push([x + Math.sin(th) * Math.sin(ph) * r, y + Math.cos(th) * r * sy, z + Math.sin(th) * Math.cos(ph) * r]);
    }
    for (let j = 0; j < nLat; j++) for (let i = 0; i < nLon; i++) {
      const a = b + j * nLon + i, c = b + j * nLon + ((i + 1) % nLon);
      face(m, [a, a + nLon, c + nLon, c], col);
    }
  }
  // a flat quad that faces the viewer (+z) when the statue looks at you
  function decal(m, pts, col) { const b = m.v.length; m.v.push(...pts); face(m, pts.map((_, k) => b + k), col, true); }

  /* ---------- the statue: front faces +z, pedestal bottom at y = -1, mitre tip near y = 0.93 ---------- */
  const cache = {};
  function build(look) {
    if (cache[look]) return cache[look];
    const L = LOOKS[look] || LOOKS[0], m = Mesh();
    // pedestal
    box(m, 0, -0.93, 0, 0.92, 0.14, 0.92, PLINTH);
    box(m, 0, -0.83, 0, 0.78, 0.06, 0.78, PLINTH_TOP);
    // robe, hem, cape
    const rAt = (y) => 0.42 + (0.2 - 0.42) * ((y + 0.8) / 0.98);
    frustum(m, -0.8, 0.42, 0.18, 0.2, 14, L.robe, { top: true });
    frustum(m, -0.8, 0.44, -0.69, 0.425, 14, L.trim);
    frustum(m, 0.0, 0.31, 0.27, 0.17, 14, L.cape, { bottom: true, top: true });
    // the white stole down the front, with little crosses
    const st = (y, w) => { const z = rAt(y) + 0.012; return [[-w, y, z], [w, y, z]]; };
    const a = st(-0.7, 0.06), b = st(0.02, 0.05);
    decal(m, [a[0], a[1], b[1], b[0]], L.trim);
    for (const y of [-0.52, -0.25]) {
      const z = rAt(y) + 0.02;
      decal(m, [[-0.012, y - 0.05, z], [0.012, y - 0.05, z], [0.012, y + 0.05, z], [-0.012, y + 0.05, z]], L.robe);
      decal(m, [[-0.04, y + 0.008, z], [0.04, y + 0.008, z], [0.04, y + 0.032, z], [-0.04, y + 0.032, z]], L.robe);
    }
    // head, beard, face
    sphere(m, 0, 0.42, 0, 0.155, 10, 7, SKIN, 1.06);
    frustum(m, 0.03, 0.0, 0.44, 0.17, 10, BEARD, { cz0: 0.16, cz1: 0.05 });
    box(m, 0, 0.415, 0.165, 0.16, 0.04, 0.04, BEARD);                             // moustache
    box(m, -0.055, 0.47, 0.142, 0.03, 0.035, 0.02, EYE);
    box(m, 0.055, 0.47, 0.142, 0.03, 0.035, 0.02, EYE);
    box(m, -0.09, 0.43, 0.125, 0.04, 0.03, 0.02, CHEEK);
    box(m, 0.09, 0.43, 0.125, 0.04, 0.03, 0.02, CHEEK);
    // mitre with a band and a cross
    frustum(m, 0.52, 0.152, 0.58, 0.148, 10, L.trim, { sz: 0.85 });
    frustum(m, 0.58, 0.148, 0.8, 0.118, 10, L.mitre, { sz: 0.8 });
    frustum(m, 0.8, 0.118, 0.93, 0.0, 10, L.mitre, { sz: 0.8 });
    const mz = 0.135;
    decal(m, [[-0.013, 0.62, mz], [0.013, 0.62, mz], [0.013, 0.78, mz], [-0.013, 0.78, mz]], L.trim);
    decal(m, [[-0.05, 0.69, mz], [0.05, 0.69, mz], [0.05, 0.715, mz], [-0.05, 0.715, mz]], L.trim);
    // crozier in the right hand
    box(m, 0.35, -0.1, 0.07, 0.04, 1.42, 0.04, STAFF);
    let px = 0.35, py = 0.61;
    for (let k = 0; k <= 6; k++) {
      const t = Math.PI - (k / 6) * Math.PI * 1.25, cx = 0.43 + Math.cos(t) * 0.08, cy = 0.62 + Math.sin(t) * 0.09;
      box(m, (px + cx) / 2, (py + cy) / 2, 0.07, Math.max(0.04, Math.abs(cx - px) + 0.03), Math.max(0.04, Math.abs(cy - py) + 0.03), 0.04, STAFF);
      px = cx; py = cy;
    }
    sphere(m, 0.33, 0.02, 0.1, 0.055, 6, 4, SKIN);
    // a present in the left hand
    box(m, -0.3, -0.06, 0.16, 0.22, 0.2, 0.2, L.box);
    box(m, -0.3, -0.06, 0.16, 0.05, 0.205, 0.205, L.ribbon);
    box(m, -0.3, -0.06, 0.16, 0.225, 0.205, 0.05, L.ribbon);
    box(m, -0.33, 0.06, 0.16, 0.07, 0.05, 0.04, L.ribbon);
    box(m, -0.27, 0.06, 0.16, 0.07, 0.05, 0.04, L.ribbon);
    sphere(m, -0.25, 0.0, 0.25, 0.05, 6, 4, SKIN);
    return (cache[look] = m);
  }

  /* ---------- drawing ---------- */
  const LIGHT = (() => { const l = [-0.45, 0.65, 0.62], n = Math.hypot(...l); return l.map((v) => v / n); })();
  function draw(ctx, o) {
    const m = build(o.look | 0), h = o.h, yaw = o.yaw || 0, tilt = o.tilt == null ? 0.18 : o.tilt;
    const s = h / 1.95, cy = Math.cos(yaw), sy = Math.sin(yaw), ct = Math.cos(tilt), st = Math.sin(tilt);
    const dist = 4.2, glow = o.glow || 0, alpha = o.alpha == null ? 1 : o.alpha;
    // model -> view (turn, then look down a little), perspective from in front
    const P = m.v.map(([x, y, z]) => {
      const x1 = x * cy + z * sy, z1 = -x * sy + z * cy;
      const y2 = y * ct - z1 * st, z2 = y * st + z1 * ct;
      const k = dist / (dist - z2);
      return [o.x + x1 * s * k, o.y - (y2 + 1) * s * k, z2, x1, y2];
    });
    const list = [];
    for (const f of m.f) {
      const p = f.i.map((k) => P[k]);
      // normal in view space (Newell, so faces with a point at a cone tip work too)
      let nx = 0, ny = 0, nz = 0, area = 0;
      for (let i = 0; i < p.length; i++) {
        const a = p[i], b = p[(i + 1) % p.length];
        nx += (a[4] - b[4]) * (a[2] + b[2]);
        ny += (a[2] - b[2]) * (a[3] + b[3]);
        nz += (a[3] - b[3]) * (a[4] + b[4]);
        area += a[0] * b[1] - b[0] * a[1];          // screen-space winding decides what faces us
      }
      const nl = Math.hypot(nx, ny, nz) || 1; nx /= nl; ny /= nl; nz /= nl;
      if (!f.two && area >= 0) continue;
      if (f.two && nz < 0) { nx = -nx; ny = -ny; nz = -nz; }
      const lit = 0.5 + 0.5 * Math.max(0, nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2]);
      let z = 0; for (const q of p) z += q[2]; z /= p.length;
      list.push({ p, z: z + (f.two ? 0.004 : 0), c: f.c, lit });
    }
    list.sort((a, b) => a.z - b.z);
    ctx.save();
    ctx.globalAlpha = alpha;
    ctx.lineJoin = "round";
    for (const it of list) {
      const k = it.lit * (1 + glow * 0.6), add = glow * 40;
      const col = `rgb(${Math.min(255, it.c[0] * k + add) | 0},${Math.min(255, it.c[1] * k + add) | 0},${Math.min(255, it.c[2] * k + add) | 0})`;
      ctx.beginPath();
      it.p.forEach((q, i) => (i ? ctx.lineTo(q[0], q[1]) : ctx.moveTo(q[0], q[1])));
      ctx.closePath();
      ctx.fillStyle = col;
      ctx.fill();
      if (!o.flat) { ctx.strokeStyle = col; ctx.lineWidth = Math.max(0.6, h / 160); ctx.stroke(); }   // hides the seams
    }
    ctx.restore();
  }

  window.SMStatue = { LOOKS, draw, build };
})();
