// Туса У Берліні: all the art, drawn in code in a hand-drawn Japanese cartoon
// style: soft ink outlines with a little wobble, flat colours with one shadow
// tone, watercolour skies, chibi friends with big heads and big eyes.
// Coordinates are "logical" units: side scenes are 180 units high and the
// game scales them to the screen, so everything stays sharp at any size.
'use strict';
const ART = (() => {
  let c = null, K = 2;                 // drawing context; device pixels per unit (for caches)
  const use = ctx => { c = ctx; };
  const setScale = k => { K = k; };
  const INK = '#33241b';
  const R = (x, y, w, h, col) => { c.fillStyle = col; c.fillRect(x, y, w, h); };
  function rgba(hex, a) {
    const n = parseInt(hex.slice(1), 16);
    return 'rgba(' + (n >> 16 & 255) + ',' + (n >> 8 & 255) + ',' + (n & 255) + ',' + a + ')';
  }
  function glow(x, y, r, col, a) {
    const g = c.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, rgba(col, a)); g.addColorStop(1, rgba(col, 0));
    c.fillStyle = g; c.fillRect(x - r, y - r, r * 2, r * 2);
  }
  function glowAdd(x, y, r, col, a) { c.save(); c.globalCompositeOperation = 'lighter'; glow(x, y, r, col, a); c.restore(); }
  // repeatable "random" numbers, so a drawing looks the same every frame
  function hash(n) { let x = (n | 0) ^ 0x2545f491; x = Math.imul(x ^ (x >>> 16), 0x85ebca6b); x = Math.imul(x ^ (x >>> 13), 0xc2b2ae35); x ^= x >>> 16; return (x >>> 0) / 4294967296; }
  const rnd = (i, a) => (hash(i) - 0.5) * 2 * a;

  // ---------- the pen ----------
  // a smooth path through points, with a small stable wobble like a hand-drawn line
  function path(pts, closed, seed, wob) {
    const n = pts.length; wob = wob == null ? 0.25 : wob; seed = seed || 0;
    const P = wob ? pts.map((p, i) => [p[0] + rnd(seed + i * 2, wob), p[1] + rnd(seed + i * 2 + 1, wob)]) : pts;
    c.beginPath();
    if (closed) {
      const m = (a, b) => [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2];
      const s = m(P[n - 1], P[0]); c.moveTo(s[0], s[1]);
      for (let i = 0; i < n; i++) { const q = m(P[i], P[(i + 1) % n]); c.quadraticCurveTo(P[i][0], P[i][1], q[0], q[1]); }
      c.closePath();
    } else {
      c.moveTo(P[0][0], P[0][1]);
      for (let i = 1; i < n - 1; i++) c.quadraticCurveTo(P[i][0], P[i][1], (P[i][0] + P[i + 1][0]) / 2, (P[i][1] + P[i + 1][1]) / 2);
      c.lineTo(P[n - 1][0], P[n - 1][1]);
    }
  }
  function fillInk(fill, lw, ink) {
    if (fill) { c.fillStyle = fill; c.fill(); }
    if (lw) { c.strokeStyle = ink || INK; c.lineWidth = lw; c.lineJoin = 'round'; c.lineCap = 'round'; c.stroke(); }
  }
  function shape(pts, fill, lw, seed, wob, ink) { path(pts, true, seed, wob); fillInk(fill, lw, ink); }
  // straight edges (roofs, paths): a polygon with a slightly uneven ink line
  function poly(pts, fill, lw, seed, wob) {
    seed = seed || 0; wob = wob == null ? 0.2 : wob;
    c.beginPath(); pts.forEach(([x, y], i) => { const X = x + rnd(seed + i * 2, wob), Y = y + rnd(seed + i * 2 + 1, wob); i ? c.lineTo(X, Y) : c.moveTo(X, Y); }); c.closePath();
    fillInk(fill, lw);
  }
  function stroke(pts, col, lw, seed, wob) { path(pts, false, seed, wob); c.strokeStyle = col || INK; c.lineWidth = lw || 0.8; c.lineCap = 'round'; c.lineJoin = 'round'; c.stroke(); }
  function line(x0, y0, x1, y1, col, lw) { c.beginPath(); c.moveTo(x0, y0); c.lineTo(x1, y1); c.strokeStyle = col || INK; c.lineWidth = lw || 0.8; c.lineCap = 'round'; c.stroke(); }
  function circle(x, y, r, fill, lw, ink) { c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2); fillInk(fill, lw, ink); }
  function ell(x, y, rx, ry, rot, fill, lw, ink) { c.beginPath(); c.ellipse(x, y, rx, ry, rot || 0, 0, Math.PI * 2); fillInk(fill, lw, ink); }
  // a wobbly round shape
  function blob(x, y, rx, ry, fill, lw, seed, n) {
    n = n || 14; const pts = [];
    for (let i = 0; i < n; i++) { const a = i / n * Math.PI * 2; pts.push([x + Math.cos(a) * rx, y + Math.sin(a) * ry]); }
    path(pts, true, seed || 0, Math.min(rx, ry) * 0.07); fillInk(fill, lw);
  }
  // a hand-drawn box: corners a little off, softly rounded
  function box(x, y, w, h, fill, lw, seed, r, ink) {
    r = r == null ? Math.min(1.4, w / 5, h / 5) : r;
    const s = (seed || 0) * 13, j = k => rnd(s + k, Math.min(0.4, w / 40 + 0.1, h / 40 + 0.1));
    const x0 = x + j(1), y0 = y + j(2), x1 = x + w + j(3), y1 = y + j(4), x2 = x + w + j(5), y2 = y + h + j(6), x3 = x + j(7), y3 = y + h + j(8);
    c.beginPath();
    c.moveTo(x0 + r, y0); c.lineTo(x1 - r, y1); c.quadraticCurveTo(x1, y1, x1, y1 + r);
    c.lineTo(x2, y2 - r); c.quadraticCurveTo(x2, y2, x2 - r, y2);
    c.lineTo(x3 + r, y3); c.quadraticCurveTo(x3, y3, x3, y3 - r);
    c.lineTo(x0, y0 + r); c.quadraticCurveTo(x0, y0, x0 + r, y0);
    c.closePath(); fillInk(fill, lw, ink);
  }
  // arms and legs: a round stroke with an ink edge
  function limb(pts, col, w, lw) {
    c.lineCap = 'round'; c.lineJoin = 'round';
    c.beginPath(); c.moveTo(pts[0][0], pts[0][1]);
    if (pts.length === 3) c.quadraticCurveTo(pts[1][0], pts[1][1], pts[2][0], pts[2][1]); else for (let i = 1; i < pts.length; i++) c.lineTo(pts[i][0], pts[i][1]);
    c.strokeStyle = INK; c.lineWidth = w + (lw == null ? 0.8 : lw) * 2; c.stroke();
    c.strokeStyle = col; c.lineWidth = w; c.stroke();
  }
  // shade the inside of the last path: fn draws the shadow while clipped to it
  function within(fn) { c.save(); c.clip(); fn(); c.restore(); }
  // several shapes that read as one: ink all of them first, then fill all, so
  // only the outer edge keeps its line (hair, clouds, tree tops)
  function puffs(list, fill, lw, ink, shade) {
    c.strokeStyle = ink || INK; c.lineWidth = (lw || 0.8) * 2; c.lineJoin = 'round';
    for (const [x, y, r] of list) { c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2); c.stroke(); }
    c.fillStyle = fill;
    for (const [x, y, r] of list) { c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2); c.fill(); }
    if (shade) { c.fillStyle = shade; for (const [x, y, r] of list) { c.beginPath(); c.arc(x + r * 0.25, y + r * 0.35, r * 0.72, 0, Math.PI * 2); c.fill(); } }
  }
  // watercolour: a few soft, see-through washes
  function wash(x, y, w, h, col, seed, a) {
    for (let i = 0; i < 6; i++) {
      const px = x + hash(seed + i * 3) * w, py = y + hash(seed + i * 3 + 1) * h, r = (0.25 + hash(seed + i * 3 + 2) * 0.35) * Math.max(w, h);
      glow(px, py, r, col, a == null ? 0.12 : a);
    }
  }
  // a watercolour patch: a few uneven, see-through layers whose edges dry darker
  function wcBlob(x, y, rx, ry, col, seed, a) {
    a = a == null ? 0.22 : a;
    for (let l = 0; l < 3; l++) {
      const pts = [], n = 16;
      for (let i = 0; i < n; i++) {
        const an = i / n * Math.PI * 2, k = 0.75 + hash(seed * 7 + l * 31 + i) * 0.4;
        pts.push([x + Math.cos(an) * rx * k, y + Math.sin(an) * ry * k]);
      }
      path(pts, true, seed + l * 5, 0);
      c.fillStyle = rgba(col, a * (l ? 0.6 : 1)); c.fill();
      if (!l) { c.strokeStyle = rgba(col, Math.min(1, a * 0.7)); c.lineWidth = 0.45; c.stroke(); }
    }
  }
  // watercolour texture inside a shape: blotches and a darker dried edge
  function wcFill(x, y, w, h, col, seed, a) {
    for (let i = 0; i < 5; i++) wcBlob(x + hash(seed + i) * w, y + hash(seed + i + 50) * h, w * (0.18 + hash(seed + i + 9) * 0.2), h * (0.2 + hash(seed + i + 19) * 0.25), col, seed + i, a == null ? 0.08 : a);
  }
  // Japanese ink-wash mountains: dark at the ridge, fading into mist below
  function inkRange(W, camX, par, yBase, h, col, seed, a) {
    const off = camX * par, step = 4, pts = [];
    for (let x = -20; x <= W + 20; x += step) {
      const u = (x + off) * 0.012 + seed;
      const y = yBase - h * (0.55 + 0.45 * Math.sin(u) * Math.sin(u * 2.3 + 1) + 0.25 * Math.sin(u * 5.7 + seed));
      pts.push([x, y]);
    }
    c.beginPath(); c.moveTo(-20, yBase + 8);
    for (const [x, y] of pts) c.lineTo(x, y);
    c.lineTo(W + 20, yBase + 8); c.closePath();
    const top = yBase - h * 1.25;
    c.fillStyle = grad(0, top, 0, yBase + 6, [[0, rgba(col, a == null ? 0.75 : a)], [0.55, rgba(col, (a == null ? 0.75 : a) * 0.45)], [1, rgba(col, 0)]]);
    c.fill();
    // the dry brush along the ridge
    for (let pass = 0; pass < 2; pass++) {
      c.beginPath();
      pts.forEach(([x, y], i) => { const yy = y + pass * 0.9 + rnd(seed * 3 + i, 0.25); i ? c.lineTo(x, yy) : c.moveTo(x, yy); });
      c.strokeStyle = rgba(col, pass ? 0.35 : 0.8); c.lineWidth = pass ? 2.2 : 0.9; c.lineJoin = 'round'; c.stroke();
    }
    // a few brush texture strokes down the slopes
    for (let i = 0; i < pts.length; i += 5) {
      if (hash(seed * 11 + i) > 0.55) continue;
      const [x, y] = pts[i];
      stroke([[x, y + 1], [x - 1.5 - hash(i) * 2, y + 5 + hash(i + 1) * 6]], rgba(col, 0.35), 0.7, 0, 0);
    }
  }
  // a band of white mist (between the mountain layers)
  function mist(W, y, h, a) {
    c.fillStyle = grad(0, y - h, 0, y + h, [[0, 'rgba(255,252,244,0)'], [0.5, 'rgba(255,252,244,' + (a == null ? 0.7 : a) + ')'], [1, 'rgba(255,252,244,0)']]);
    c.fillRect(-20, y - h, W + 40, h * 2);
  }
  // a few ink splatters and dry brush flecks, like on a real scroll
  function splatter(x, y, w, h, col, seed, n) {
    for (let i = 0; i < (n || 8); i++) circle(x + hash(seed + i) * w, y + hash(seed + i + 70) * h, 0.2 + hash(seed + i + 90) * 0.6, rgba(col, 0.3 + hash(seed + i + 3) * 0.3));
  }
  function grad(x0, y0, x1, y1, stops) { const g = c.createLinearGradient(x0, y0, x1, y1); stops.forEach(([o, col]) => g.addColorStop(o, col)); return g; }

  // ---------- caches: big still drawings are painted once, then copied ----------
  const caches = {};
  function layer(key, x, y, w, h, draw) {
    const kk = Math.min(K, 6, 6000 / h);
    const id = key + '@' + kk.toFixed(3);
    let tiles = caches[id];
    if (!tiles) {
      for (const k in caches) if (k.slice(0, key.length + 1) === key + '@') delete caches[k];
      const n = Math.max(1, Math.ceil(w * kk / 2048));
      tiles = [];
      for (let i = 0; i < n; i++) {
        const tx = x + w * i / n, tw = w / n + (i < n - 1 ? 1 : 0);
        const cv = document.createElement('canvas');
        cv.width = Math.ceil(tw * kk); cv.height = Math.ceil(h * kk);
        const cx = cv.getContext('2d'); cx.scale(kk, kk); cx.translate(-tx, -y);
        const prev = c; c = cx;
        try { draw(); } finally { c = prev; }
        tiles.push([cv, tx]);
      }
      caches[id] = tiles;
    }
    for (const [cv, tx] of tiles) c.drawImage(cv, tx, y, cv.width / kk, cv.height / kk);
  }

  // ---------- washi paper over the whole picture ----------
  // a warm paper with fibres and soft blotches (multiplied over the drawing),
  // a fine grain, and slightly darker edges like an old print
  let paperPat = null, grainPat = null;
  function paperTiles(ctx) {
    const S = 256, cv = document.createElement('canvas'); cv.width = cv.height = S;
    const x = cv.getContext('2d');
    x.fillStyle = '#fbf3e3'; x.fillRect(0, 0, S, S);
    // big soft blotches, tileable (drawn 9 times around the tile)
    for (let i = 0; i < 18; i++) {
      const px = hash(i + 10) * S, py = hash(i + 11) * S, r = 20 + hash(i + 12) * 50, col = i % 3 ? 'rgba(214,190,150,' : 'rgba(255,255,255,';
      for (const ox of [-S, 0, S]) for (const oy of [-S, 0, S]) {
        const g = x.createRadialGradient(px + ox, py + oy, 0, px + ox, py + oy, r);
        g.addColorStop(0, col + (i % 3 ? 0.16 : 0.3) + ')'); g.addColorStop(1, col + '0)');
        x.fillStyle = g; x.fillRect(px + ox - r, py + oy - r, r * 2, r * 2);
      }
    }
    // kozo fibres
    for (let i = 0; i < 140; i++) {
      const px = hash(i + 900) * S, py = hash(i + 901) * S, a2 = hash(i + 902) * 6.28, l = 6 + hash(i + 903) * 22;
      x.strokeStyle = i % 4 ? 'rgba(170,140,100,.22)' : 'rgba(255,255,255,.7)'; x.lineWidth = 0.4 + hash(i + 904) * 0.6;
      for (const ox of [-S, 0, S]) for (const oy of [-S, 0, S]) {
        x.beginPath(); x.moveTo(px + ox, py + oy);
        x.quadraticCurveTo(px + ox + Math.cos(a2 + 0.5) * l * 0.5, py + oy + Math.sin(a2 + 0.5) * l * 0.5, px + ox + Math.cos(a2) * l, py + oy + Math.sin(a2) * l); x.stroke();
      }
    }
    paperPat = ctx.createPattern(cv, 'repeat');
    const G = 128, gv = document.createElement('canvas'); gv.width = gv.height = G;
    const gx = gv.getContext('2d'), img = gx.createImageData(G, G);
    for (let i = 0; i < G * G; i++) { const v = 128 + (hash(i * 7 + 1) - 0.5) * 80; img.data[i * 4] = v; img.data[i * 4 + 1] = v; img.data[i * 4 + 2] = v; img.data[i * 4 + 3] = 255; }
    gx.putImageData(img, 0, 0);
    grainPat = ctx.createPattern(gv, 'repeat');
  }
  function paper(ctx, w, h, a) {
    if (!paperPat) paperTiles(ctx);
    a = a == null ? 1 : a;
    ctx.save();
    ctx.globalCompositeOperation = 'multiply'; ctx.globalAlpha = 0.55 * a;
    ctx.fillStyle = paperPat; ctx.fillRect(0, 0, w, h);
    ctx.globalCompositeOperation = 'overlay'; ctx.globalAlpha = 0.18 * a;
    ctx.fillStyle = grainPat; ctx.fillRect(0, 0, w, h);
    ctx.globalCompositeOperation = 'multiply'; ctx.globalAlpha = 0.5 * a;
    const g = ctx.createRadialGradient(w / 2, h / 2, Math.min(w, h) * 0.35, w / 2, h / 2, Math.max(w, h) * 0.75);
    g.addColorStop(0, 'rgba(255,255,255,0)'); g.addColorStop(1, 'rgba(150,120,90,0.55)');
    ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
    ctx.restore();
  }

  // =====================================================================
  // the three friends (and the waiter): chibi style, big heads, big eyes
  // =====================================================================
  const LOOK = {
    VITO:    { skin: '#efc29e', skin2: '#d9a179', hair: '#1c1714', hair2: '#3a302a', top: '#3d6a94', top2: '#2c5175', legs: '#2b3247', shoe: '#1d1d22', hood: true, style: 'vito' },
    VIKA:    { skin: '#fbdcc3', skin2: '#eebc9a', hair: '#f7dc7a', hair2: '#e0b44c', top: '#ee7fa0', top2: '#d0607f', legs: '#4a4266', shoe: '#7a3450', plump: true, style: 'vika', collar: '#fff4ec' },
    OLEKSIY: { skin: '#f0c19b', skin2: '#d89e78', hair: '#6a4428', hair2: '#4f321c', top: '#678a3c', top2: '#4d6c2a', legs: '#2f2f33', shoe: '#4a3020', hat: '#557a32', hat2: '#3e5c22', must: '#6a3d1c', coat: true, style: 'olek' },
    WAITER:  { skin: '#f3c9a3', skin2: '#d9a983', hair: '#1b1b1f', hair2: '#33333a', top: '#24242a', top2: '#17171c', legs: '#1f1f24', shoe: '#101014', apron: '#f4f1e8', style: 'bun' },
  };
  const LAUGH_TEAR = '#7fcfff';

  // side view; (x, y) is where the feet touch the ground
  // o: { dir, phase, moving, anim, laugh, blush, t, alpha, talk }
  function friend(who, x, y, o) { chibi(LOOK[who] || LOOK.VITO, x, y, o || {}); }

  function chibi(L, x, y, o) {
    const t = o.t || 0, dir = o.dir || 1, an = o.anim || '';
    c.save();
    if (o.alpha != null) c.globalAlpha = o.alpha;
    c.fillStyle = 'rgba(50,25,10,.2)'; c.beginPath(); c.ellipse(x, y + 0.6, an === 'rofl' ? 15 : (L.plump ? 9 : 7.5), 2.1, 0, 0, Math.PI * 2); c.fill();
    c.translate(x, y); c.scale(dir, 1);
    if (an === 'rofl') {
      // rolling on the floor: lying on the back, rocking, legs kicking
      c.translate(0, -5.5 + Math.abs(Math.sin(t * 9)) * -1);
      c.rotate(Math.PI / 2 + Math.sin(t * 9) * 0.28);
      body(L, { ...o, laugh: 4 }, t, true);
    } else body(L, o, t, false);
    c.restore();
  }

  function body(L, o, t, rofl) {
    const laugh = o.laugh || 0, an = o.anim || '';
    const walk = o.moving ? Math.sin((o.phase || 0) * Math.PI * 2) : 0;
    const kick = rofl ? Math.sin(t * 17) : 0;
    let bob = o.moving ? -Math.abs(walk) * 0.9 : Math.sin(t * 2.4) * 0.25;
    if (laugh >= 2 && !rofl) bob += Math.sin(t * 28) * 0.7;
    c.translate(laugh >= 2 ? Math.sin(t * 31) * 0.45 : 0, bob);
    const bw = L.plump ? 7 : 5.4;            // half the width of the body
    const sx = bw - 1.2;                     // shoulders

    // ---- arms: where are the hands? ----
    let fh, bh;                              // front and back hand
    const sw = walk * 0.55;
    if (an === 'hug') { fh = [sx + 7.2, -14.5]; bh = [-sx + 9.5, -13.5]; }
    else if (an === 'wave' || an === 'heart') { fh = [sx + 2.6, -24.5 + Math.sin(t * 14) * 0.8]; bh = [-sx - 1 + Math.sin(-sw) * 6, -15.5 + Math.cos(sw) * 6.3]; }
    else if (rofl) { fh = [sx + 4 + Math.sin(t * 15) * 2, -21 + Math.cos(t * 15) * 2]; bh = [-sx - 4 + Math.sin(t * 13) * 2, -20 + Math.cos(t * 13) * 2]; }
    else if (laugh >= 2) { fh = [bw - 0.5, -9.5]; bh = [-1.5, -9]; }
    else { fh = [sx + Math.sin(sw) * 6, -15.5 + Math.cos(sw) * 6.3]; bh = [-sx + Math.sin(-sw) * 6, -15.5 + Math.cos(-sw) * 6.3]; }
    const sleeve = L.top2 || L.top;
    const arm = (sxp, h, col) => { limb([[sxp, -15.6], [(sxp + h[0]) / 2 + (h[1] > -12 ? -0.6 : 0.4), (h[1] - 15.6) / 2 - 0.4], h], col, 2.5); circle(h[0], h[1], 1.25, L.skin, 0.6); };
    arm(-sx, bh, sleeve);

    // ---- legs ----
    const leg = (hx, a, col) => {
      const fx = hx + Math.sin(a) * 6.6, fy = -7 + Math.cos(a) * 6.6;
      limb([[hx, -7.5], [fx, fy - 0.5]], col, 3);
      ell(fx + 1.1, fy - 0.6, 2.5, 1.5, 0, L.shoe, 0.7);
    };
    leg(-1.7, -walk * 0.5 - kick * 0.9, L.legs);
    leg(1.9, walk * 0.5 + kick * 0.9, L.legs);

    // ---- body and clothes ----
    if (L.plump) {
      blob(0, -11.6, bw, 6.3, L.top, 0.9, 41);
      within(() => { R(-bw - 2, -19, 3.4, 12, L.top2); });
      shape([[-3.2, -17.6], [0, -15.4], [3.2, -17.6], [0, -16.6]], L.collar, 0.6, 3, 0.05);
    } else if (L.coat) {
      shape([[-bw + 0.6, -17.4], [bw - 0.6, -17.4], [bw + 0.8, -4.6], [-bw - 0.8, -4.6]], L.top, 0.9, 7, 0.15);
      within(() => { R(-bw - 2, -18, 3.2, 14, L.top2); });
      line(1.2, -17, 1.2, -5.2, L.top2, 0.6);
      for (const by of [-14.5, -11, -7.5]) circle(2.3, by, 0.55, '#d8c27a', 0.3);
      shape([[-1.2, -17.5], [1.4, -14.5], [3.6, -17.5]], L.top2, 0.6, 9, 0.05); // collar
      line(-bw - 0.3, -9.5, bw + 0.3, -9.5, L.top2, 1); // belt
    } else {
      shape([[-bw + 0.8, -17.4], [bw - 0.8, -17.4], [bw, -10], [bw - 0.4, -6.2], [-bw + 0.4, -6.2], [-bw, -10]], L.top, 0.9, 11, 0.12);
      within(() => { R(-bw - 2, -18, 3.1, 13, L.top2); });
      if (L.hood) { ell(-2.8, -17.2, 3.4, 1.6, -0.2, L.top2, 0.7); line(1.6, -16.5, 1.4, -12.6, '#e8e8e8', 0.5); line(3.2, -16.5, 3.3, -12.8, '#e8e8e8', 0.5); }
      if (L.apron) { shape([[-1.6, -12.6], [bw + 0.1, -12.6], [bw + 0.5, -5.4], [-1.8, -5.4]], L.apron, 0.6, 5, 0.05); line(-bw + 0.4, -12.4, -1.6, -12.4, L.apron, 0.7); }
    }

    // ---- head ----
    head(L, o, t, laugh, an, rofl);
    if (an !== 'salute') arm(sx, fh, L.top);
  }

  function head(L, o, t, laugh, an, rofl) {
    const hx = 0.7, hy = -24.6, r = L.plump ? 8.1 : 7.8;
    // hair behind the head
    if (L.style === 'vito') {
      const sway = Math.sin(t * 3.1) * 0.8;
      shape([[-5.5, -28], [-8.6, -25], [-10 + sway, -19], [-9.6 + sway, -14.5], [-7.8 + sway * 0.6, -16], [-6.6, -21], [-5, -24.5]], L.hair, 0.8, 21, 0.1);
      circle(-6.6, -26.1, 1.1, '#d8323a', 0.5);
    } else if (L.style === 'vika') {
      const ring = [];
      for (let i = 0; i < 9; i++) { const a = Math.PI * (0.62 + i * 0.2); ring.push([hx + Math.cos(a) * 7.6, hy + Math.sin(a) * 7.2 - 0.6, 3.6 + (i % 2) * 0.5]); }
      ring.push([hx - 6.4, hy + 3.2, 3.1], [hx + 7.4, hy + 2.6, 2.6]);
      puffs(ring, L.hair, 0.85);
    } else if (L.style === 'bun') {
      circle(-4.6, -31.5, 2.7, L.hair, 0.8);
    } else if (L.style === 'long') {
      shape([[-6.8, -27], [-7.8, -19], [-6.2, -15.2], [-3.5, -18], [-2, -25]], L.hair, 0.8, 23, 0.1);
    }
    // face
    circle(hx, hy, r, L.skin, 0.95);
    within(() => { ell(hx - r * 0.85, hy + 1, r * 0.55, r * 1.2, 0, L.skin2); });
    // ear
    if (L.style !== 'vika') { ell(hx - 5.9, hy + 1.2, 1.3, 1.8, 0, L.skin, 0.6); }
    // eyes
    const ey = hy + 0.5, e1 = hx + 1.6, e2 = hx + 5.4;
    const happy = laugh >= 1 || an === 'hug' || rofl;
    const blink = !happy && (t * 1000 | 0) % 3700 < 110;
    for (const ex of [e1, e2]) {
      if (happy) { c.beginPath(); c.arc(ex, ey + 1, 1.25, Math.PI * 1.12, Math.PI * 1.88); c.strokeStyle = INK; c.lineWidth = 0.8; c.lineCap = 'round'; c.stroke(); }
      else if (blink) line(ex - 1, ey + 0.3, ex + 1, ey + 0.3, INK, 0.7);
      else {
        ell(ex, ey, 1.15, 1.65, 0, '#2b1d17');
        ell(ex, ey + 0.7, 0.9, 0.7, 0, L.eye || '#6a4a35');
        circle(ex + 0.35, ey - 0.6, 0.45, '#ffffff');
      }
    }
    // cheeks
    c.globalAlpha *= (o.blush || laugh >= 2) ? 0.85 : 0.45;
    ell(hx - 0.9, hy + 3.2, 1.4, 0.8, 0, '#ff8fa6'); ell(hx + 6.8, hy + 3, 1.1, 0.75, 0, '#ff8fa6');
    c.globalAlpha /= (o.blush || laugh >= 2) ? 0.85 : 0.45;
    // mouth
    const mx = hx + 3.5, my = hy + 4.4;
    if (laugh >= 1 || rofl) {
      shape([[mx - 2, my - 0.5], [mx + 2, my - 0.6], [mx + 1.2, my + 2.2], [mx - 1.2, my + 2.2]], '#8a2a2a', 0.6, 31, 0.05);
      ell(mx, my + 1.5, 1, 0.55, 0, '#ff8a8a');
      if (L.style === 'vito') line(mx - 1.7, my - 0.2, mx + 1.7, my - 0.3, '#ffffff', 0.7);
    } else if (o.talk) {
      ell(mx, my + 0.6, 1.1, 1.2, 0, '#8a2a2a', 0.5);
    } else if (L.style === 'vito') {
      // the big smile
      shape([[mx - 2.4, my - 0.4], [mx + 2.4, my - 0.5], [mx + 1.4, my + 1.6], [mx - 1.4, my + 1.6]], '#ffffff', 0.6, 33, 0.05);
      line(mx - 2, my + 0.4, mx + 2, my + 0.4, '#d9c0b0', 0.3);
    } else {
      c.beginPath(); c.arc(mx, my - 0.6, 1.3, Math.PI * 0.2, Math.PI * 0.8); c.strokeStyle = INK; c.lineWidth = 0.7; c.stroke();
    }
    // tears of laughter and little "ha ha" marks
    if (laugh >= 3 || rofl) {
      for (let k = 0; k < 3; k++) {
        const p = ((t * 2.2 + k / 3) % 1);
        for (const [ex, side] of [[e1, -1], [e2, 1]]) {
          const tx = ex + side * (1 + p * 4), ty = ey + 0.5 - p * 2 + p * p * 7;
          c.globalAlpha *= 1 - p; ell(tx, ty, 0.6, 0.85, 0, LAUGH_TEAR, 0.3, '#3a7ab0'); c.globalAlpha /= Math.max(0.01, 1 - p);
        }
      }
    }
    if (laugh >= 2 || rofl) {
      const k = (t * 4) % 1;
      for (const [ax, ay, bx2, by2] of [[hx + 9, hy - 6, hx + 11.5, hy - 8.5], [hx + 10, hy - 2, hx + 13, hy - 2.6], [hx - 8, hy - 7, hx - 10, hy - 9.5]]) {
        c.globalAlpha *= 0.4 + 0.6 * (1 - k); line(ax, ay, bx2 + (bx2 - ax) * k * 0.3, by2 + (by2 - ay) * k * 0.3, INK, 0.7); c.globalAlpha /= (0.4 + 0.6 * (1 - k));
      }
    }
    // hair in front, hats, mustache
    if (L.style === 'vito') {
      shape([[hx - 7.8, hy + 1.5], [hx - 8.2, hy - 4], [hx - 4.5, hy - 8.4], [hx + 1, hy - 9], [hx + 5.6, hy - 7.2], [hx + 8.6, hy - 2.6], [hx + 6.4, hy - 4.6], [hx + 5.6, hy - 2.4],
        [hx + 3.8, hy - 5.2], [hx + 2.4, hy - 2.9], [hx + 0.6, hy - 5.4], [hx - 2.6, hy - 3.8], [hx - 4, hy - 0.6]], L.hair, 0.85, 51, 0.1);
      stroke([[hx - 2, hy - 7.2], [hx + 2, hy - 7.6], [hx + 4.6, hy - 6]], L.hair2, 0.6, 52, 0);
    } else if (L.style === 'vika') {
      puffs([[hx - 3.6, hy - 6.4, 3.4], [hx + 0.2, hy - 7.6, 3.5], [hx + 3.9, hy - 6.9, 3.2], [hx + 6.6, hy - 4.6, 2.5], [hx - 6.2, hy - 3.6, 2.9]], L.hair, 0.85);
      stroke([[hx - 2, hy - 8.6], [hx + 1, hy - 9.4], [hx + 3.6, hy - 8.4]], '#fff3c0', 0.8, 61, 0);
    } else if (L.style === 'olek') {
      shape([[hx - 7.6, hy + 2], [hx - 7.8, hy - 3], [hx - 5, hy - 4], [hx - 4.4, hy + 0.5]], L.hair, 0.7, 71, 0.08);
      // the hat: brim and crown with a dent
      ell(hx + 0.4, hy - 5.6, 10.2, 2.1, -0.04, L.hat2, 0.85);
      shape([[hx - 5.6, hy - 5.8], [hx - 5.2, hy - 10.8], [hx - 2, hy - 12.1], [hx + 1, hy - 11.2], [hx + 4, hy - 12.3], [hx + 6, hy - 10.6], [hx + 6.2, hy - 5.8]], L.hat, 0.85, 72, 0.08);
      R(hx - 5.5, hy - 7.9, 11.6, 1.5, L.hat2);
      // handlebar mustache with curled tips
      const mx2 = hx + 3.6, my2 = hy + 3.4;
      limb([[mx2, my2], [mx2 + 3, my2 + 0.9], [mx2 + 5.2, my2 - 1.6]], L.must, 1.25, 0.55);
      limb([[mx2, my2], [mx2 - 2.6, my2 + 0.9], [mx2 - 4.4, my2 - 1.4]], L.must, 1.25, 0.55);
      circle(mx2 + 5.1, my2 - 1.9, 0.55, L.must); circle(mx2 - 4.3, my2 - 1.7, 0.55, L.must);
    } else if (L.style === 'bun') {
      shape([[hx - 7.8, hy + 1], [hx - 7.6, hy - 4.6], [hx - 2, hy - 8.8], [hx + 4, hy - 8], [hx + 8, hy - 3.4], [hx + 3, hy - 5], [hx - 3, hy - 4.2], [hx - 4.4, hy]], L.hair, 0.85, 81, 0.1);
    } else if (L.style === 'short' || L.style === 'long') {
      shape([[hx - 7.8, hy + 1], [hx - 7.6, hy - 4.6], [hx - 2, hy - 8.8], [hx + 4, hy - 8], [hx + 8, hy - 3.4], [hx + 5, hy - 4.4], [hx + 2, hy - 3.6], [hx - 2, hy - 4.6], [hx - 4.2, hy - 0.5]], L.hair, 0.85, 83, 0.1);
    } else if (L.style === 'helmet') {
      shape([[hx - 8.4, hy - 1], [hx - 7.4, hy - 7.4], [hx, hy - 10.2], [hx + 7.4, hy - 7.4], [hx + 8.6, hy - 1.5]], L.hatc, 0.9, 85, 0.08);
      line(hx - 8.4, hy - 1.2, hx + 8.6, hy - 1.6, INK, 0.7);
      for (let i = 0; i < 4; i++) ell(hx - 4 + i * 2.8, hy - 5 + (i % 2) * 1.5, 1.4, 0.9, 0.3, L.camo || 'rgba(0,0,0,.2)');
    } else if (L.style === 'beret') {
      shape([[hx - 7.6, hy - 2.8], [hx - 6, hy - 8], [hx + 2, hy - 9.4], [hx + 7.2, hy - 7], [hx + 7.4, hy - 4], [hx, hy - 3.8]], L.hatc, 0.85, 87, 0.08);
      shape([[hx - 7.6, hy - 2.5], [hx - 8.4, hy + 2], [hx - 5.6, hy - 1]], L.hair, 0.6, 88, 0.05);
    } else if (L.style === 'cap') {
      shape([[hx - 7.8, hy - 2.4], [hx - 6.2, hy - 8.2], [hx + 1, hy - 9.6], [hx + 7, hy - 6.6], [hx + 7.6, hy - 3]], L.hatc, 0.85, 89, 0.08);
      shape([[hx + 6, hy - 3.6], [hx + 12, hy - 3.2], [hx + 11.4, hy - 2], [hx + 6, hy - 2.2]], L.hatc, 0.7, 90, 0.05);
    } else if (L.style === 'beanie') {
      shape([[hx - 8, hy - 2], [hx - 6.8, hy - 8.6], [hx + 1, hy - 10.4], [hx + 7.2, hy - 7.6], [hx + 8.2, hy - 2.6]], L.hatc, 0.85, 91, 0.08);
      R(hx - 8, hy - 3.6, 16.2, 1.6, 'rgba(0,0,0,.18)'); circle(hx, hy - 10.6, 1.6, L.hatc, 0.6);
    }
  }

  // top-down view for inside the restaurant
  function friendTop(who, x, y, o) {
    o = o || {};
    const L = LOOK[who] || LOOK.VITO, t = o.t || 0;
    const a = o.angle || 0;
    c.save();
    c.translate(x, y);
    if (o.anim === 'rofl') {
      const roll = Math.sin(t * 9), kick = Math.sin(t * 17);
      c.rotate(a);
      c.translate(0, roll * 2.2); c.rotate(roll * 0.3);
      c.fillStyle = 'rgba(50,25,10,.22)'; c.beginPath(); c.ellipse(1, 2, 13, 5.5, 0, 0, Math.PI * 2); c.fill();
      limb([[-6, -2], [-13, -3 + kick]], L.legs, 2.6); limb([[-6, 2], [-13, 3 - kick]], L.legs, 2.6);
      ell(-14, -3 + kick, 1.6, 1.4, 0, L.shoe, 0.6); ell(-14, 3 - kick, 1.6, 1.4, 0, L.shoe, 0.6);
      blob(-2, 0, 6.4, L.plump ? 5.6 : 4.8, L.top, 0.85, 3);
      limb([[0, -4], [2, -8 - kick]], L.top2, 2.2); limb([[0, 4], [2, 8 + kick]], L.top2, 2.2);
      circle(2.6, -8.6 - kick, 1.2, L.skin, 0.5); circle(2.6, 8.6 + kick, 1.2, L.skin, 0.5);
      topHead(L, 6.2, 0, 0, t, true);
      const k = (t * 3) % 1;
      ell(10 + k * 4, -3 - k * 3, 0.6, 0.9, 0, LAUGH_TEAR, 0.3); ell(10 + k * 4, 3 + k * 3, 0.6, 0.9, 0, LAUGH_TEAR, 0.3);
      c.restore();
      return;
    }
    const shake = (o.laugh || 0) >= 2 ? Math.sin(t * 30) * 0.7 : 0;
    c.translate(shake, 0); c.rotate(a);
    c.fillStyle = 'rgba(50,25,10,.22)'; c.beginPath(); c.ellipse(1, 1.6, 7.5, 7, 0, 0, Math.PI * 2); c.fill();
    const swing = o.moving ? Math.sin((o.phase || 0) * Math.PI * 2) * 2.2 : 0;
    circle(swing + 1, -7, 1.6, L.skin, 0.6); circle(-swing + 1, 7, 1.6, L.skin, 0.6);
    ell(0, 0, L.plump ? 5.4 : 4.6, L.plump ? 8 : 7.2, 0, L.top, 0.85);
    within(() => { R(-7, -9, 4, 18, L.top2); });
    if (L.apron) ell(2.4, 0, 1.8, 4.4, 0, L.apron, 0.5);
    topHead(L, 0.6, 0, 0, t, false, o);
    c.restore();
  }
  function topHead(L, x, y, a, t, rofl, o) {
    const r = 5.2;
    if (L.style === 'vito') {
      const s = Math.sin(t * 3) * 1.2;
      shape([[x - 3, y - 1.6], [x - 9.6, y - 0.6 + s], [x - 10.2, y + 0.8 + s], [x - 3, y + 1.6]], L.hair, 0.7, 5, 0.05);
      circle(x - 4.2, y, 0.9, '#d8323a', 0.4);
    }
    if (L.style === 'bun') circle(x - 4.6, y, 2.3, L.hair, 0.7);
    circle(x, y, r, L.skin, 0.85);
    if (L.style === 'vika') {
      const ring = [];
      for (let i = 0; i < 8; i++) { const an = Math.PI * 0.45 + i * Math.PI * 1.1 / 7; ring.push([x + Math.cos(an) * 4.3 - 0.8, y + Math.sin(an) * 4.3, 2.6]); }
      ring.push([x - 1.5, y, 3.4]);
      puffs(ring, L.hair, 0.75, INK, L.hair2 + '55');
    } else if (L.style === 'olek') {
      circle(x, y, 6.8, L.hat2, 0.8); circle(x - 0.4, y, 4.2, L.hat, 0.7);
      line(x - 2.6, y, x + 1.8, y, L.hat2, 0.7);
      line(x + 5.6, y - 2.6, x + 7.2, y - 3.6, L.must, 1.1); line(x + 5.6, y + 2.6, x + 7.2, y + 3.6, L.must, 1.1);
    } else {
      c.beginPath(); c.arc(x - 0.8, y, r + 0.25, Math.PI * 0.2, Math.PI * 1.8); c.closePath(); fillInk(L.hair, 0.8);
      circle(x + 4.6, y, 0.6, L.skin2);
      stroke([[x - 2.6, y - 2.4], [x + 0.4, y - 3.2]], L.hair2 || L.hair, 0.6, 7, 0);
    }
  }

  // generic people: soldiers, people on the street
  const PSTYLE = { helmet: 'helmet', beret: 'beret', cap: 'cap', beanie: 'beanie' };
  function person(x, y, p, phase, moving, dir, t) {
    if (!p._L) {
      p._L = { skin: p.skin, skin2: shadeOf(p.skin), hair: p.hair || '#2a2420', hair2: p.hair || '#2a2420', top: p.top, top2: shadeOf(p.top), legs: p.legs, shoe: p.shoe || '#1a1a1a',
        style: PSTYLE[p.hat] || (p.long ? 'long' : 'short'), hatc: p.hatc || '#4d5a32', camo: p.camo, eye: '#4a3a30' };
    }
    const L = p._L;
    chibi(L, x, y, { dir, phase, moving, t, anim: p.salute ? 'salute' : '' });
    // a soldier's camouflage spots, a bag, a salute, a dog
    c.save(); c.translate(x, y); c.scale(dir, 1);
    if (p.camo) for (let i = 0; i < 4; i++) ell(-3 + ((i * 3 + (p.seed || 0)) % 7), -15 + ((i * 5 + (p.seed || 0)) % 8), 1.5, 1, 0.4, p.camo);
    if (p.salute) { limb([[4.2, -15.6], [6.6, -21], [3.6, -27.6]], L.top2, 2.4); circle(3.6, -27.6, 1.2, L.skin, 0.55); }
    if (p.bag) { line(-3, -16.5, 3.5, -9, INK, 0.6); box(1.6, -10.4, 4.4, 4, p.bag, 0.6, 3, 0.8); }
    c.restore();
    if (p.dog) {
      const walk = moving ? Math.sin(phase * Math.PI * 2) : 0;
      const dx = x + dir * 13, dy = y;
      c.save(); c.translate(dx, dy); c.scale(dir, 1);
      limb([[-2.6, -3], [-3 + walk, -0.4]], p.dog, 1.2, 0.5); limb([[2.6, -3], [3 - walk, -0.4]], p.dog, 1.2, 0.5);
      blob(0, -4, 4, 2.4, p.dog, 0.7, 9);
      circle(4.2, -6.2, 2.2, p.dog, 0.7); ell(3.2, -7.6, 0.9, 1.6, 0.4, shadeOf(p.dog), 0.5);
      circle(5, -6.6, 0.4, INK); circle(6.3, -6, 0.45, INK);
      stroke([[-3.8, -4.6], [-5.4, -6.8 + Math.sin(t * 14) * 0.6]], p.dog, 1, 0, 0);
      c.restore();
      c.strokeStyle = 'rgba(50,35,25,.75)'; c.lineWidth = 0.4; c.beginPath(); c.moveTo(x + dir * 5, y - 10); c.quadraticCurveTo(x + dir * 9, y - 4, dx + dir * 3, dy - 6); c.stroke();
    }
  }
  function shadeOf(hex) {
    if (!hex || hex[0] !== '#') return hex;
    const n = parseInt(hex.slice(1), 16), f = 0.78;
    return '#' + [n >> 16 & 255, n >> 8 & 255, n & 255].map(v => Math.round(v * f).toString(16).padStart(2, '0')).join('');
  }

  // =====================================================================
  // skies, clouds, hills, trees: anime backgrounds
  // =====================================================================
  const TOP = -185;   // tall phones show sky above the 180-unit scene
  // a watercolour sky: a soft gradient, then uneven washes of colour that dried with darker edges
  function sky(W, groundY, stops, seed) {
    c.fillStyle = grad(0, TOP, 0, groundY, stops); c.fillRect(-10, TOP, W + 20, groundY - TOP);
    seed = seed || 5;
    for (let i = 0; i < 7; i++) {
      const x = hash(seed + i) * W, y = TOP + hash(seed + i + 40) * (groundY - TOP) * 0.8, rx = 40 + hash(seed + i + 80) * 60;
      wcBlob(x, y, rx, rx * 0.35, i % 3 ? '#ffffff' : '#5a7ab8', seed * 9 + i, i % 3 ? 0.1 : 0.05);
    }
  }
  function cloud(x, y, s, fill, shade, ink) {
    const p = [[0, 0, 7], [8, -3, 9], [18, -1, 7.5], [25, 2, 5.5], [-7, 2, 5], [11, 3, 7]].map(([a, b, r]) => [x + a * s, y + b * s, r * s]);
    c.save(); c.globalAlpha *= 0.9;
    puffs(p, fill || '#fffdf6', 0.45, ink || 'rgba(120,135,175,.5)', shade || 'rgba(150,170,215,.28)');
    c.restore();
  }
  function clouds(W, camX, t, y0, n, tint) {
    for (let i = 0; i < (n || 5); i++) {
      const span = W + 160, x = ((i * 131 + t * 2.2 - camX * 0.15) % span + span) % span - 80;
      cloud(x, y0 + (i * 17) % 34 - (i % 2) * 40, 0.55 + (i % 3) * 0.18, tint);
    }
  }
  function sun(x, y, r) { glow(x, y, r * 4, '#fff4c8', 0.45); circle(x, y, r, '#fff1b8', 0.6, '#e8b860'); }
  function hills(W, camX, par, yBase, amp, col, seed, ink) {
    const off = camX * par;
    c.beginPath(); c.moveTo(-10, 200);
    for (let x = -10; x <= W + 10; x += 6) c.lineTo(x, yBase - Math.sin((x + off) * 0.017 + seed) * amp - Math.sin((x + off) * 0.043 + seed * 2) * amp * 0.35);
    c.lineTo(W + 10, 200); c.closePath(); fillInk(col, 0.7, ink || 'rgba(60,40,30,.55)');
  }
  function mountains(W, camX, par, yBase, col, snow, seed) {
    const off = camX * par;
    for (let i = -1; i < W / 70 + 2; i++) {
      const bx = i * 70 - (off % 70), h = 34 + hash(i + seed + Math.floor(off / 70)) * 26;
      shape([[bx - 50, yBase], [bx, yBase - h], [bx + 50, yBase]], col, 0.7, i + seed, 0.6, 'rgba(60,50,70,.5)');
      shape([[bx - 11, yBase - h + h * 0.22], [bx, yBase - h], [bx + 11, yBase - h + h * 0.22], [bx + 4, yBase - h + h * 0.28], [bx - 3, yBase - h + h * 0.2]], snow, 0.5, i + seed + 9, 0.3, 'rgba(60,50,70,.4)');
    }
  }
  function pine(x, y, h, col, col2) {
    col = col || '#2f6a3f'; col2 = col2 || '#24543a';
    limb([[x, y], [x, y - h * 0.3]], '#6a4a30', 2.2, 0.6);
    for (let i = 0; i < 4; i++) {
      const ty = y - h * 0.22 - i * h * 0.19, w = (4 - i) * h * 0.1 + 2.5;
      shape([[x - w, ty], [x - w * 0.45, ty - h * 0.08], [x - w * 0.7, ty - h * 0.06], [x, ty - h * 0.3], [x + w * 0.7, ty - h * 0.06], [x + w * 0.45, ty - h * 0.08], [x + w, ty]], i % 2 ? col2 : col, 0.75, Math.round(x * 3 + i), 0.25);
      stroke([[x + w * 0.2, ty - h * 0.06], [x + w * 0.6, ty - h * 0.02]], 'rgba(255,255,255,.25)', 0.8, 0, 0);
    }
  }
  function roundTree(x, y, r, col, col2, seed) {
    limb([[x, y], [x + 0.5, y - r * 1.4]], '#6a4a30', 2.6, 0.6);
    const p = [];
    for (let i = 0; i < 7; i++) { const a = i / 7 * Math.PI * 2; p.push([x + Math.cos(a) * r * 0.7, y - r * 1.9 + Math.sin(a) * r * 0.55, r * (0.5 + hash(seed + i) * 0.2)]); }
    p.push([x, y - r * 1.9, r * 0.7]);
    puffs(p, col, 0.75, INK, col2);
  }
  function tufts(x0, x1, y, col, seed, n) {
    for (let i = 0; i < (n || 40); i++) {
      const x = x0 + hash(seed + i) * (x1 - x0), yy = y + hash(seed + i + 500) * 26;
      stroke([[x - 1.2, yy], [x, yy - 2.4], [x + 0.6, yy - 0.2], [x + 1.6, yy - 2]], col, 0.5, 0, 0);
    }
  }
  // a waving flag; horizontal stripes (Germany) or vertical ones (Romania)
  function flag(x, y, cols, t, horizontal, eu) {
    limb([[x, y], [x, y - 52]], '#d6d6d6', 1, 0.5);
    circle(x, y - 52.5, 1, '#e8c060', 0.4);
    const fw = 18, fh = 11, top = y - 51;
    const wv = (u) => Math.sin(t * 4 + u * 0.4) * 1.2 * (u / fw);
    const band = (u0, u1, v0, v1, col) => {
      c.beginPath();
      for (let k = 0; k <= 6; k++) { const u = u0 + (u1 - u0) * k / 6; c[k ? 'lineTo' : 'moveTo'](x + 0.6 + u, top + v0 + wv(u)); }
      for (let k = 6; k >= 0; k--) { const u = u0 + (u1 - u0) * k / 6; c.lineTo(x + 0.6 + u, top + v1 + wv(u)); }
      c.closePath(); c.fillStyle = col; c.fill();
    };
    if (horizontal) cols.forEach((col, i) => band(0, fw, i * fh / cols.length, (i + 1) * fh / cols.length, col));
    else cols.forEach((col, i) => band(i * fw / cols.length, (i + 1) * fw / cols.length, 0, fh, col));
    if (eu) for (let i = 0; i < 8; i++) { const a = i / 8 * 6.28, u = fw / 2 + Math.cos(a) * 3.4; circle(x + 0.6 + u, top + fh / 2 + Math.sin(a) * 3.4 + wv(u), 0.5, '#ffd84a'); }
    c.beginPath();
    for (let k = 0; k <= 6; k++) { const u = fw * k / 6; c[k ? 'lineTo' : 'moveTo'](x + 0.6 + u, top + wv(u)); }
    for (let k = 6; k >= 0; k--) { const u = fw * k / 6; c.lineTo(x + 0.6 + u, top + fh + wv(u)); }
    c.closePath(); c.strokeStyle = INK; c.lineWidth = 0.6; c.stroke();
  }
  function fence(x0, x1, y) {
    c.strokeStyle = 'rgba(90,95,100,.5)'; c.lineWidth = 0.35;
    for (let x = x0; x < x1; x += 2.5) { c.beginPath(); c.moveTo(x, y - 14); c.lineTo(x + 2.5, y); c.moveTo(x + 2.5, y - 14); c.lineTo(x, y); c.stroke(); }
    for (let x = x0; x <= x1; x += 12) limb([[x, y], [x, y - 16]], '#8b9096', 1.2, 0.5);
    line(x0, y - 14, x1, y - 14, '#8b9096', 0.7);
    for (let x = x0; x < x1; x += 3) { c.beginPath(); c.arc(x + 1.5, y - 16.5, 1.2, 0, Math.PI * 2); c.strokeStyle = '#9aa0a6'; c.lineWidth = 0.4; c.stroke(); }
  }
  function ground(x0, x1, y, col, line1, seed, dots) {
    R(x0, y, x1 - x0, 60, col);
    c.save(); c.beginPath(); c.rect(x0, y, x1 - x0, 60); c.clip(); wcFill(x0, y, x1 - x0, 32, shadeOf(col), seed, 0.12); c.restore();
    line(x0, y + 0.3, x1, y + 0.3, line1 || INK, 0.8);
    for (let i = 0; i < (dots || 120); i++) {
      const px = x0 + hash(seed + i) * (x1 - x0), py = y + 3 + hash(seed + i + 999) * 26;
      ell(px, py, 1 + hash(seed + i + 3) * 0.8, 0.5, 0, i % 3 ? 'rgba(0,0,0,.08)' : 'rgba(255,255,255,.18)');
    }
  }
  // windows with a little reflection
  function windowPane(x, y, w, h, lit, frame) {
    box(x, y, w, h, lit ? '#ffd98a' : '#7fa3c4', 0.7, x + y, 0.6);
    if (lit) glow(x + w / 2, y + h / 2, w, '#ffcf70', 0.12);
    else { c.save(); c.beginPath(); c.rect(x, y, w, h); c.clip(); stroke([[x + w * 0.2, y + h], [x + w * 0.75, y]], 'rgba(255,255,255,.45)', 1.4, 0, 0); c.restore(); }
    line(x + w / 2, y, x + w / 2, y + h, frame || '#f4f1e8', 0.6);
    if (h > 9) line(x, y + h / 2, x + w, y + h / 2, frame || '#f4f1e8', 0.5);
  }
  function signBoard(x, y, w, h, col) {
    limb([[x + 4, y + h], [x + 4, y + h + 6]], '#6a4a30', 1.4, 0.5); limb([[x + w - 4, y + h], [x + w - 4, y + h + 6]], '#6a4a30', 1.4, 0.5);
    box(x - 1, y - 1, w + 2, h + 2, '#6a4a30', 0.8, x, 1.2);
    box(x, y, w, h, col, 0.6, x + 1, 0.8);
  }

  // ----- Camp Lion, Romania: white container buildings, an aircon at every window
  const CL_BLOCKS = [[20, 4], [260, 5]];
  function campLion(W, H, camX, t) {
    sky(W, 150, [[0, '#4f8fd6'], [0.55, '#8fc0ec'], [1, '#ffe2b0']], 3);
    sun(W * 0.82 - camX * 0.05, 28, 9);
    clouds(W, camX, t, 26, 5);
    inkRange(W, camX, 0.06, 112, 46, '#7a86a8', 11, 0.55);
    mist(W, 108, 9, 0.6);
    inkRange(W, camX, 0.12, 124, 30, '#4f5f80', 4, 0.7);
    mist(W, 124, 7, 0.55);
    hills(W, camX, 0.3, 132, 5, '#a8c483', 2, 'rgba(70,90,50,.6)');
    c.save(); c.translate(-camX, 0);
    layer('campLion', -60, 60, W + 140, 125, () => {
      for (const [bx, n] of CL_BLOCKS) {
        for (let lvl = 0; lvl < 2; lvl++) for (let i = 0; i < n; i++) {
          const x = bx + i * 34, y = 150 - (lvl + 1) * 30;
          box(x, y, 34, 30, '#f4f5f2', 0.85, x + y, 0.8);
          within(() => wcFill(x, y, 34, 30, '#8aa0b8', x + y, 0.07));
          for (let k = 4; k < 33; k += 3.2) line(x + k, y + 2, x + k, y + 28, 'rgba(160,170,175,.45)', 0.4);
          R(x + 1, y + 26, 32, 3, 'rgba(160,170,175,.35)');
          windowPane(x + 5, y + 7, 13, 11, hash(x * 3 + lvl) > 0.82);
          box(x + 21, y + 9, 10, 8, '#dfe3e6', 0.7, x + 5, 0.8);
          line(x + 21.5, y + 15.6, x + 30.5, y + 15.6, '#a8adb1', 0.5);
          circle(x + 26, y + 12.6, 2.8, '#b8bfc4', 0.5);
        }
        // the walkway and stairs to the upper level
        box(bx - 2, 118, n * 34 + 4, 3, '#8c9298', 0.7, bx, 0.4);
        for (let k = 0; k <= n * 34; k += 5) line(bx + k, 118, bx + k, 112, '#8c9298', 0.6);
        line(bx - 2, 112, bx + n * 34 + 2, 112, '#8c9298', 0.8);
        for (let s = 0; s < 8; s++) line(bx + n * 34 + 2 + s * 3, 121.5 + s * 3.5, bx + n * 34 + 6 + s * 3, 121.5 + s * 3.5, '#6f767c', 1);
        line(bx + n * 34 + 2, 121, bx + n * 34 + 26, 149, '#6f767c', 0.8);
        line(bx + n * 34 + 2, 113, bx + n * 34 + 26, 141, '#8c9298', 0.6);
      }
      // Vito's door, with a little roof
      box(69, 131, 12, 19, '#cfd5d8', 0.8, 3, 0.6); box(70.5, 132.5, 9, 17, '#e6eaec', 0.6, 4, 0.5);
      circle(78, 141.5, 0.6, INK);
      box(65, 128.5, 20, 2.6, '#9aa1a6', 0.7, 5, 0.6);
      // the sign
      signBoard(150, 100, 70, 16, '#3f6a3a');
      fence(-50, 18, 150);
      ground(-60, W + 80, 150, '#cdbf9f', '#8a7a5a', 21, 160);
      for (let i = 0; i < 60; i++) circle(-60 + hash(i + 77) * (W + 140), 154 + hash(i + 78) * 24, 0.5 + hash(i + 79) * 0.7, i % 2 ? '#b0a283' : '#e2d8bf');
      tufts(-60, 18, 148, '#7a9a4a', 31, 20);
    });
    // spinning aircon fans and the odd drip
    for (const [bx, n] of CL_BLOCKS) for (let lvl = 0; lvl < 2; lvl++) for (let i = 0; i < n; i++) {
      const x = bx + i * 34, y = 150 - (lvl + 1) * 30, fx = x + 26, fy = y + 12.6, sp = t * 11 + i + lvl;
      if (fx - camX < -10 || fx - camX > W + 10) continue;
      for (let b = 0; b < 3; b++) { const a = sp + b * 2.09; line(fx, fy, fx + Math.cos(a) * 2.3, fy + Math.sin(a) * 2.3, '#6d7479', 0.7); }
      if ((t * 1.5 + i + lvl) % 3 < 0.25) ell(x + 30, y + 18 + ((t * 20) % 5), 0.4, 0.6, 0, '#7fb0e0');
    }
    flag(232, 150, ['#002b7f', '#fcd116', '#ce1126'], t);
    flag(246, 150, ['#003399'], t + 1, false, true);
    c.restore();
    return { signX: 150 - camX, signY: 100, signW: 70, signH: 16 };
  }

  // ----- Altengrabow: a long three-storey building in the pine woods
  function altengrabow(W, H, camX, t) {
    sky(W, 150, [[0, '#5a90d8'], [0.6, '#9cc6ee'], [1, '#ffd9a8']], 7);
    sun(W * 0.2 - camX * 0.05, 34, 8);
    clouds(W, camX, t, 22, 4);
    inkRange(W, camX, 0.08, 116, 26, '#6a7a8a', 21, 0.45);
    mist(W, 114, 8, 0.6);
    hills(W, camX, 0.2, 124, 5, '#9cbc84', 5, 'rgba(70,90,50,.6)');
    c.save(); c.translate(-camX * 0.5, 0);
    layer('altenPines', -20, 50, 560, 105, () => { for (let i = 0; i < 19; i++) pine(i * 29 + 6, 150, 50 + (i % 3) * 10, '#2f6a45', '#25573a'); });
    c.restore();
    c.save(); c.translate(-camX, 0);
    const bx = 20, bw = 230, by = 72;
    layer('altengrabow', -60, 60, W + 140, 125, () => {
      box(bx, by, bw, 78, '#e8dfcc', 0.9, 7, 0.6);
      within(() => { wcFill(bx, by, bw, 78, '#b09870', 17, 0.08); splatter(bx, by, bw, 78, '#6a5a40', 3, 12); });
      R(bx + 0.5, by + 0.5, 6, 77, 'rgba(160,140,110,.25)');
      box(bx - 3, by - 3.5, bw + 6, 4.5, '#8f897d', 0.8, 8, 0.6);
      for (let f = 0; f < 3; f++) {
        line(bx, by + 24 * f + 25, bx + bw, by + 24 * f + 25, 'rgba(150,130,100,.5)', 0.6);
        for (let i = 0; i < 10; i++) {
          const wx = bx + 8 + i * 22, wy = by + 7 + f * 24;
          if (f === 2 && (i === 2 || i === 3)) continue;
          windowPane(wx, wy, 12, 13, hash(f * 10 + i + 3) > 0.75);
          line(wx - 1, wy + 13.6, wx + 13, wy + 13.6, '#a8a090', 0.9);
        }
      }
      // entrance with canopy (Vika's door)
      box(bx + 51, 127, 16, 23, '#7a5236', 0.85, 9, 0.6); box(bx + 52.5, 128.5, 13, 21.5, '#946644', 0.5, 10, 0.4);
      line(bx + 59, 128.5, bx + 59, 150, '#6a4228', 0.5); circle(bx + 63, 139.5, 0.6, '#e8d8a0');
      box(bx + 46, 123.5, 26, 3.4, '#7b7f84', 0.8, 11, 0.6);
      signBoard(bx + 36, 110, 46, 11, '#2f4f78');
      // a military truck
      const tx = 300;
      box(tx, 127, 48, 17, '#5d6a44', 0.9, 12, 1.4); box(tx, 118, 17, 14, '#56623f', 0.9, 13, 1.4);
      windowPane(tx + 2.5, 120.5, 10, 6, false, '#56623f');
      poly([[tx + 19, 127], [tx + 20, 114], [tx + 47, 115], [tx + 48, 127]], '#6e7a50', 0.8, 14, 0.2);
      for (let k = 0; k < 5; k++) line(tx + 22 + k * 5.5, 115, tx + 22 + k * 5.5, 127, 'rgba(0,0,0,.18)', 0.6);
      for (const wx of [tx + 9, tx + 38]) { circle(wx, 146, 4.4, '#26262a', 0.8); circle(wx, 146, 1.8, '#77777d', 0.4); }
      ground(-60, W + 80, 150, '#8fae62', '#4f6a35', 31, 90);
      poly([[bx + 49, 150.4], [bx + 69, 150.4], [bx + 84, 182], [bx + 34, 182]], '#d2c6a8', 0.6, 15, 0.3);
      tufts(-60, W + 80, 151, '#5f8a3a', 41, 70);
    });
    flag(270, 150, ['#1a1a1a', '#dd0000', '#ffce00'], t, true);
    c.restore();
    return { signX: bx + 36 - camX, signY: 110, signW: 46, signH: 11 };
  }

  // ----- Lehnin: two-storey dark red brick houses with steep roofs, pines
  function lehnin(W, H, camX, t) {
    sky(W, 150, [[0, '#6aa0dc'], [0.6, '#a6cdee'], [1, '#ffd0a0']], 9);
    sun(W * 0.7 - camX * 0.05, 26, 8);
    clouds(W, camX, t, 30, 4);
    inkRange(W, camX, 0.08, 104, 22, '#5a6a7a', 31, 0.45);
    mist(W, 100, 8, 0.6);
    c.save(); c.translate(-camX * 0.4, 0);
    layer('lehninPines', -20, 40, 560, 115, () => { for (let i = 0; i < 21; i++) pine(i * 25 + 4, 150, 62 + (i % 4) * 9, '#2c6340', '#21502f'); });
    c.restore();
    c.save(); c.translate(-camX, 0);
    layer('lehnin', -60, 50, W + 140, 135, () => {
      const house = (hx, hw, door) => {
        const top = 104;
        box(hx, top, hw, 46, '#8a3328', 0.9, hx, 0.4);
        within(() => wcFill(hx, top, hw, 46, '#c8604a', hx, 0.1));
        for (let y = top + 4; y < 150; y += 4.5) for (let x = hx + ((y / 4.5 | 0) % 2) * 3.5; x < hx + hw - 2; x += 7) line(x, y, x + 5, y, 'rgba(60,15,10,.35)', 0.5);
        R(hx + 0.5, top + 0.5, 5, 45, 'rgba(40,10,5,.18)');
        poly([[hx - 5, top + 1], [hx + hw / 2, top - 27], [hx + hw + 5, top + 1]], '#43434c', 0.9, hx + 1, 0.3);
        within(() => { poly([[hx + hw / 2, top - 27], [hx + hw + 5, top + 1], [hx + hw / 2 + 4, top + 1]], 'rgba(0,0,0,.18)', 0, 0, 0); });
        for (let k = 1; k < 5; k++) line(hx - 5 + k * 3.6, top + 1 - k * 5.6, hx + hw + 5 - k * 3.6, top + 1 - k * 5.6, 'rgba(255,255,255,.12)', 0.6);
        box(hx + hw / 2 - 5, top - 15, 10, 9, '#efe8d8', 0.7, hx + 2, 0.5); windowPane(hx + hw / 2 - 3.5, top - 13.5, 7, 6, false);
        for (let f = 0; f < 2; f++) for (let i = 0; i < Math.floor(hw / 18); i++) {
          const wx = hx + 6 + i * 18, wy = top + 6 + f * 20;
          if (door && f === 1 && Math.abs(wx - door) < 14) continue;
          box(wx - 1.2, wy - 1.2, 12.4, 14.4, '#efe8d8', 0.7, wx, 0.4);
          windowPane(wx, wy, 10, 12, hash(wx + f) > 0.8);
        }
        if (door) { box(door - 1, 127, 14, 23, '#efe8d8', 0.8, door, 0.4); box(door + 0.5, 128.5, 11, 21.5, '#3f5236', 0.6, door + 1, 0.3); circle(door + 9, 139.5, 0.6, '#d8c27a'); box(door - 3, 124.5, 18, 2.6, '#43434c', 0.7, door + 2, 0.4); }
      };
      house(10, 120, 56); house(170, 100, 0);
      signBoard(38, 114, 48, 10, '#33402c');
      pine(150, 152, 72); pine(290, 152, 82); pine(330, 152, 66); pine(-8, 152, 62);
      ground(-60, W + 80, 150, '#7f9a52', '#4a5f30', 51, 80);
      poly([[52, 150.4], [72, 150.4], [84, 182], [40, 182]], '#c8bd9f', 0.6, 16, 0.3);
      for (let i = 0; i < 40; i++) ell(-60 + hash(i + 90) * (W + 140), 153 + hash(i + 29) * 24, 0.9, 0.5, hash(i) * 3, '#9b6a3a');
      tufts(-60, W + 80, 151, '#5a7a36', 61, 60);
    });
    flag(300, 150, ['#1a1a1a', '#dd0000', '#ffce00'], t, true);
    c.restore();
    return { signX: 38 - camX, signY: 114, signW: 48, signH: 10 };
  }

  // =====================================================================
  // Berlin street with Hoongy (evening, end of summer)
  // =====================================================================
  const STREET = { W: 760, door: 274, shop0: 290, shop1: 470, feet: 150 };
  const treesAt = [90, 560, 700];
  const FACADES = [[-100, 128, '#e9dcc0'], [128, 290, '#e2b9b0'], [290, 470, '#e6c98f'], [470, 600, '#b9c4cf'], [600, 860, '#efe3c6']];
  function facades() {
    // the sky above the roofs (only tall phones see it)
    c.fillStyle = grad(0, TOP, 0, -100, [[0, '#1b2350'], [0.6, '#3e3d73'], [1, '#8a5d86']]); c.fillRect(-100, TOP, 960, 60);
    for (let i = 0; i < 40; i++) circle(-100 + hash(i + 5) * 960, TOP + hash(i + 6) * 40, 0.4 + hash(i + 7) * 0.4, '#fff6d8');
    for (const [x0, x1, col] of FACADES) {
      const roof = -112 + hash(x0) * 14;
      box(x0, roof, x1 - x0, 140 - roof, col, 0.9, x0, 0.3);
      within(() => { wcFill(x0, roof, x1 - x0, 140 - roof, shadeOf(col), x0, 0.1); splatter(x0, roof, x1 - x0, 140 - roof, '#5a4030', x0, 10); });
      R(x0 + 0.5, roof, 6, 136 - roof, 'rgba(90,60,40,.12)');
      // cornice and the stone band above the shops
      box(x0 - 1, roof - 3, x1 - x0 + 2, 4, shadeOf(col), 0.8, x0 + 1, 0.4);
      box(x0 - 1, 24, x1 - x0 + 2, 6, shadeOf(col), 0.8, x0 + 2, 0.4);
      for (let k = x0 + 4; k < x1 - 2; k += 6) line(k, 25, k, 29, 'rgba(60,40,30,.25)', 0.5);
      // rows of tall old windows
      for (let row = 0; row < 4; row++) {
        const wy = 2 - row * 34;
        if (wy < roof + 6) continue;
        for (let x = x0 + 8; x + 18 < x1 - 4; x += 34) {
          const lit = hash(x * 7 + row) > 0.5;
          poly([[x - 2, wy - 2], [x + 9, wy - 6.5], [x + 20, wy - 2]], shadeOf(col), 0.6, x + row, 0.1);
          box(x - 1.5, wy - 1, 21, 21.5, '#f6f0e2', 0.7, x + row * 5, 0.5);
          box(x, wy, 18, 19, lit ? '#ffcf7a' : '#3c4a68', 0.6, x + row * 9, 0.4);
          if (lit) {
            glow(x + 9, wy + 10, 16, '#ffb050', 0.18);
            shape([[x, wy], [x + 5, wy], [x + 3.5, wy + 19], [x, wy + 19]], '#e6735a', 0.4, x, 0.05);   // curtains
            shape([[x + 18, wy], [x + 13, wy], [x + 14.5, wy + 19], [x + 18, wy + 19]], '#e6735a', 0.4, x + 1, 0.05);
          } else stroke([[x + 4, wy + 19], [x + 14, wy]], 'rgba(255,255,255,.18)', 2, 0, 0);
          line(x + 9, wy, x + 9, wy + 19, '#f6f0e2', 0.8); line(x, wy + 7, x + 18, wy + 7, '#f6f0e2', 0.7);
          if (hash(x + row * 3) > 0.7 && row > 0) { // a small balcony
            box(x - 4, wy + 19, 26, 2, '#6b6f78', 0.6, x, 0.3);
            for (let b = 0; b <= 26; b += 3) line(x - 4 + b, wy + 19, x - 4 + b, wy + 13, '#4a4e57', 0.5);
            line(x - 4, wy + 13, x + 22, wy + 13, '#4a4e57', 0.7);
            if (hash(x) > 0.5) puffs([[x - 1, wy + 12, 2.2], [x + 2, wy + 11, 2.4], [x + 19, wy + 12, 2]], '#e8506a', 0.5, INK);
          }
        }
      }
      line(x1, roof, x1, 136, INK, 0.9);
    }
    // ---- shops on the ground floor ----
    // left: a big old house door and a closed shop
    box(30, 52, 34, 84, '#6a4428', 0.9, 31, 1); box(33, 56, 13, 78, '#7f5434', 0.6, 32, 0.6); box(48, 56, 13, 78, '#7f5434', 0.6, 33, 0.6);
    poly([[30, 52], [47, 40], [64, 52]], '#efe3c6', 0.8, 34, 0.1);
    circle(45, 96, 0.8, '#d8c27a'); circle(49, 96, 0.8, '#d8c27a');
    box(76, 46, 46, 84, '#6f7681', 0.9, 35, 0.6);
    for (let y = 48; y < 128; y += 3) line(77, y, 121, y, 'rgba(40,40,50,.35)', 0.5);
    stroke([[82, 100], [90, 92], [98, 104], [108, 90], [114, 98]], '#ff8fb8', 1.6, 36, 0.4);
    // the Späti
    box(130, 34, 136, 102, '#3a302a', 0.9, 40, 0.6);
    box(136, 44, 92, 76, '#f0c27a', 0.8, 41, 0.6);
    glow(182, 80, 60, '#ffd890', 0.3);
    for (let sh = 0; sh < 4; sh++) {
      line(138, 58 + sh * 15, 226, 58 + sh * 15, '#7a4a24', 1.2);
      for (let b = 0; b < 21; b++) box(139.5 + b * 4.1, 50.5 + sh * 15, 3, 7.4, ['#b8443a', '#3f7aa0', '#e8d070', '#4f9a54', '#f4f0e6', '#c86ab0'][(b * 7 + sh * 3) % 6], 0.45, b + sh * 31, 0.6);
    }
    box(232, 50, 26, 86, '#2a2420', 0.9, 42, 0.6); box(234.5, 54, 21, 80, '#f7cf86', 0.6, 43, 0.4);
    glow(245, 92, 22, '#ffc870', 0.3);
    line(236, 94, 253, 94, '#8a6030', 0.6); circle(252, 96, 0.8, '#7a5a30');
    // Hoongy: the shopfront
    const X0 = STREET.shop0, X1 = STREET.shop1;
    box(X0, 30, X1 - X0, 106, '#3a2518', 1, 50, 0.6);
    box(X0 + 3, 36, X1 - X0 - 6, 25, '#1d2026', 0.7, 51, 0.4);
    box(X0, 61, X1 - X0, 5, '#4f3322', 0.8, 52, 0.3);
    // inside, seen through the big window
    const wy0 = 66, wy1 = 126;
    c.save(); c.beginPath(); c.rect(X0 + 4, wy0, X1 - X0 - 8, wy1 - wy0); c.clip();
    c.fillStyle = grad(0, wy0, 0, wy1, [[0, '#3a2f2a'], [0.55, '#6a4a32'], [1, '#4a3424']]); c.fillRect(X0 + 4, wy0, X1 - X0 - 8, wy1 - wy0);
    glow(X0 + 115, 90, 70, '#ffdc9a', 0.35);
    box(X0 + 70, 96, 92, 15, '#5a3a24', 0.7, 53, 0.6);
    for (let sh = 0; sh < 3; sh++) { line(X0 + 72, 95 - sh * 5, X0 + 160, 95 - sh * 5, '#f6e2b0', 0.8); for (let b = 0; b < 20; b++) box(X0 + 73 + b * 4.3, 90.6 - sh * 5, 2.2, 4, ['#d8a040', '#7ab0a0', '#f0ece0', '#b84a40', '#c8c060'][(b + sh) % 5], 0.3, b * 3 + sh, 0.4); }
    for (let i = 0; i < 6; i++) { const lx = X0 + 60 + i * 17; line(lx, wy0, lx, 80, 'rgba(40,30,20,.6)', 0.4); circle(lx, 82, 2.6, '#f7d79a', 0.5); glow(lx, 82, 7, '#ffcf8a', 0.4); }
    // a ginkgo in a pot, inside
    puffs([[X0 + 22, 84, 7], [X0 + 30, 82, 7.5], [X0 + 26, 76, 7], [X0 + 18, 90, 5.5], [X0 + 33, 90, 5.5]], '#e8cf4a', 0.6, INK, '#c9a830');
    limb([[X0 + 26, 118], [X0 + 26, 92]], '#6a4a30', 1.6, 0.5); box(X0 + 20, 114, 12, 10, '#a85a3a', 0.6, 54, 0.6);
    // the red paper fan decoration
    c.save(); c.translate(X0 + 118, 74);
    for (let k = 0; k < 9; k++) { const a = Math.PI * (1.08 + k * 0.105); shape([[0, 0], [Math.cos(a) * 15, Math.sin(a) * 11], [Math.cos(a + 0.1) * 15, Math.sin(a + 0.1) * 11]], k % 2 ? '#d8323a' : '#e8504a', 0.4, k, 0.05); }
    for (let k = 0; k < 9; k++) { const a = Math.PI * (-0.08 - k * 0.105); shape([[0, 0], [Math.cos(a) * 15, Math.sin(a) * 11], [Math.cos(a - 0.1) * 15, Math.sin(a - 0.1) * 11]], k % 2 ? '#d8323a' : '#e8504a', 0.4, k + 20, 0.05); }
    ell(0, 0, 4.6, 3.2, 0, '#c42a30', 0.6);
    c.restore();
    // a cosy couple at the window table
    const din = (x, col, hair, dir) => { c.save(); c.translate(x, 126); c.scale(dir, 1); shape([[-5.5, 0], [-5, -14], [5, -14], [5.5, 0]], col, 0.6, x, 0.2); circle(0.6, -18.5, 5, '#f0c8a0', 0.6); c.beginPath(); c.arc(0, -19.2, 5.3, Math.PI * 0.95, Math.PI * 2.05); c.closePath(); fillInk(hair, 0.6); circle(2.6, -18, 0.6, INK); c.restore(); };
    din(X0 + 58, '#3a4a6a', '#3a2a1e', 1); din(X0 + 134, '#6a7a3a', '#c8a060', -1);
    box(X0 + 68, 115, 56, 3, '#8a5a30', 0.6, 55, 0.4);
    box(X0 + 90, 110, 3, 5, '#ffe2a0', 0.4, 56, 0.4); box(X0 + 97, 111, 2.5, 4, '#f4f4f0', 0.4, 57, 0.4);
    // the sign plate inside the window (the neon is lit later)
    box(X0 + 48, 79, 90, 22, 'rgba(10,12,14,.88)', 0.8, 58, 1.2);
    // glass reflection
    c.fillStyle = 'rgba(255,255,255,.07)'; c.beginPath(); c.moveTo(X0 + 20, wy0); c.lineTo(X0 + 44, wy0); c.lineTo(X0 + 14, wy1); c.lineTo(X0 + 4, wy1); c.fill();
    c.restore();
    c.strokeStyle = INK; c.lineWidth = 0.9; c.strokeRect(X0 + 4, wy0, X1 - X0 - 8, wy1 - wy0);
    box(X0, wy1, X1 - X0, 10, '#3a2518', 0.9, 59, 0.4);
    // the wall to the right (the graffiti circle gets its words later)
    circle(X1 + 60, 88, 31, '#1a1a20', 1);
    circle(X1 + 60, 88, 28.5, null, 0.8, '#f0f0f0');
    stroke([[X1 + 8, 118], [X1 + 22, 116], [X1 + 30, 119]], '#ff5aa0', 1.6, 60, 0.3);
    stroke([[X1 + 92, 50], [X1 + 104, 47], [X1 + 112, 51]], '#9cff6b', 1.4, 61, 0.3);
    // far right: a bakery with a striped awning
    box(620, 46, 120, 90, '#5a3f2e', 0.9, 62, 0.6);
    box(626, 66, 70, 52, '#ffe0a8', 0.7, 63, 0.5); glow(660, 92, 40, '#ffd890', 0.3);
    for (let i = 0; i < 7; i++) puffs([[634 + i * 9.5, 112, 3.4]], '#c88a48', 0.5, INK, '#a86a30');
    for (let i = 0; i < 12; i++) shape([[616 + i * 10.6, 40], [626.6 + i * 10.6, 40], [626.6 + i * 10.6, 52], [621.3 + i * 10.6, 55], [616 + i * 10.6, 52]], i % 2 ? '#f4efe4' : '#3d7a5a', 0.6, 64 + i, 0.05);
    box(704, 66, 26, 70, '#7a5236', 0.8, 65, 0.6);
    // street lamps (old Berlin style)
    for (const lx of [200, 640]) {
      limb([[lx, 150], [lx, 52]], '#2f4a3c', 2.4, 0.7);
      box(lx - 3.5, 146, 7, 4, '#2f4a3c', 0.7, lx, 0.6);
      shape([[lx - 5, 54], [lx - 3.5, 42], [lx + 3.5, 42], [lx + 5, 54]], '#ffe6a8', 0.8, lx + 1, 0.05);
      shape([[lx - 6, 42], [lx, 36], [lx + 6, 42]], '#2f4a3c', 0.8, lx + 2, 0.05);
      line(lx, 42, lx, 54, '#2f4a3c', 0.6);
    }
    // tree trunks (the yellow tops come in front later)
    for (const tx of treesAt) { limb([[tx, 150], [tx + 1, 92], [tx - 1, 60]], '#5a4030', 4.2, 0.8); limb([[tx, 80], [tx + 9, 66]], '#5a4030', 2, 0.6); limb([[tx, 74], [tx - 8, 62]], '#5a4030', 1.8, 0.6); }
    // the pavement: a band of small cobbles, then big slabs, then the kerb
    R(-100, 136, 960, 8, '#8c8790');
    for (let i = 0; i < 300; i++) { const x = -100 + (i * 3.3) % 960, y = 137 + (i % 3) * 2.3; ell(x, y + 0.8, 1.4, 0.9, 0, ['#a39fa8', '#7d7983', '#b2aeb6'][i % 3]); }
    R(-100, 144, 960, 13, '#a9a6ad');
    for (let x = -100; x < 860; x += 24) line(x, 144, x + 0.5, 157, 'rgba(60,55,65,.45)', 0.6);
    line(-100, 150.5, 860, 150.5, 'rgba(60,55,65,.3)', 0.5);
    line(-100, 144, 860, 144, INK, 0.7);
    box(-100, 157, 960, 3.4, '#cfccd2', 0.8, 66, 0.2);
    // the road with tram rails
    R(-100, 160.4, 960, 22, '#3a3940');
    for (let i = 0; i < 80; i++) ell(-100 + hash(i + 300) * 960, 162 + hash(i + 301) * 16, 1.2, 0.4, 0, 'rgba(255,255,255,.05)');
    line(-100, 166, 860, 166, '#8a8a92', 0.8); line(-100, 170, 860, 170, '#8a8a92', 0.8);
    for (let x = -100; x < 860; x += 30) line(x, 175.5, x + 14, 175.5, '#e8e8e8', 1);
  }
  function street(W, H, camX, t, s) {
    s = s || {};
    c.save(); c.translate(-camX, 0);
    layer('street', -100, TOP, 960, 180 - TOP + 4, facades);
    const X0 = STREET.shop0, X1 = STREET.shop1, D = STREET.door;
    // light from windows and lamps
    c.save(); c.globalCompositeOperation = 'lighter';
    for (const lx of [200, 640]) glow(lx, 48, 46, '#ffcf80', 0.3);
    glow(D, 95, 34, '#ffb15a', 0.22); glow((X0 + X1) / 2, 165, 46, '#ffe23a', 0.07);
    c.restore();
    // Hoongy's door, with a noren curtain
    box(D - 14, 50, 28, 86, '#2a1a12', 0.9, 70, 0.6);
    if (s.doorOpen) { box(D - 11, 56, 22, 80, '#ffd890', 0.6, 71, 0.4); glowAdd(D, 96, 26, '#ffcf80', 0.4); box(D - 11, 56, 5, 80, '#7a5236', 0.6, 72, 0.3); }
    else { box(D - 11, 56, 22, 80, '#f2c27a', 0.6, 73, 0.4); line(D - 10, 92, D + 10, 92, '#8a6030', 0.6); circle(D + 7, 96, 0.9, '#a07830'); }
    for (let i = 0; i < 3; i++) {
      const sw = Math.sin(t * 1.5 + i) * 0.5 + (s.doorOpen ? (i - 1) * 2 : 0);
      shape([[D - 11 + i * 7.4, 56], [D - 4.4 + i * 7.4, 56], [D - 4.4 + i * 7.4 + sw, 72], [D - 11 + i * 7.4 + sw, 72]], '#24447a', 0.6, 74 + i, 0.05);
    }
    circle(D, 63, 2.4, null, 0.8, '#f4f0e6');
    // lanterns over the shopfront and the little lights
    for (let i = 0; i < 4; i++) {
      const lx = X0 + 30 + i * 40, ly = 40 + Math.sin(t * 1.2 + i) * 0.4;
      glowAdd(lx, ly + 8, 22, '#fff1d0', 0.32);
      blob(lx, ly + 8.5, 6.2, 8.4, '#fbf4e2', 0.75, i);
      for (let k = -2; k <= 2; k++) { c.beginPath(); c.ellipse(lx, ly + 8.5 + k * 3, 6 - Math.abs(k) * 0.6, 0.6, 0, 0, Math.PI * 2); c.strokeStyle = 'rgba(180,150,110,.45)'; c.lineWidth = 0.4; c.stroke(); }
      box(lx - 3.5, ly - 0.6, 7, 1.8, '#3a2518', 0.5, i, 0.4); box(lx - 3.5, ly + 16, 7, 1.8, '#3a2518', 0.5, i + 9, 0.4);
    }
    for (let k = 0; k <= 16; k++) {
      const x = X0 - 6 + k * ((X1 - X0 + 12) / 16), sag = Math.sin((k % 4) / 4 * Math.PI) * 3;
      if (k) { const px = X0 - 6 + (k - 1) * ((X1 - X0 + 12) / 16), ps = Math.sin(((k - 1) % 4) / 4 * Math.PI) * 3; line(px, 33 + ps, x, 33 + sag, 'rgba(40,30,20,.7)', 0.4); }
      const on = 0.7 + 0.3 * Math.sin(t * 3 + k * 1.7);
      glowAdd(x, 34 + sag, 6, '#ffc070', 0.5 * on); circle(x, 34 + sag, 0.9, '#fff3c0');
    }
    // neon signs and the graffiti words
    neonText('HOONGY', X0 + 56, 84, 12, '#ffe23a', t, 1.4);
    neonText('SPÄTI', 150, 37, 5.5, '#ff4d6d', t + 3, 1.1);
    tag('ТУСА', X1 + 60, 82, 11, '#ffffff', -0.08);
    tag('GEZELLIG', X1 + 60, 100, 8.5, '#7fd3ff', 0.05);
    c.restore();
  }
  function tag(txt, x, y, size, col, rot) {
    c.save(); c.translate(x, y); c.rotate(rot || 0);
    c.font = '900 ' + size + 'px "M PLUS Rounded 1c", "Pixelify Sans", sans-serif'; c.textAlign = 'center'; c.textBaseline = 'middle';
    c.lineWidth = 1.6; c.strokeStyle = '#141418'; c.strokeText(txt, 0, 0);
    c.fillStyle = col; c.fillText(txt, 0, 0);
    c.restore();
  }
  // tables outside (behind the people)
  const TABLES = [236, 336, 436, 520];
  function outdoorTables(camX, t) {
    c.save(); c.translate(-camX, 0);
    layer('tables', 200, 118, 350, 32, () => {
      for (const tx of TABLES) {
        for (const side of [-1, 1]) {
          const cx = tx + side * 17;
          shape([[cx - 5, 133], [cx + 5, 133], [cx + 5, 135], [cx - 5, 135]], '#c08848', 0.6, cx, 0.05);
          box(cx - 5, 125.5, 10, 7, '#b07a40', 0.6, cx + 1, 0.6);
          line(cx - 4, 135, cx + 4, 146, INK, 0.8); line(cx + 4, 135, cx - 4, 146, INK, 0.8);
          line(cx + side * 5, 124.5, cx + side * 5, 135, INK, 0.8);
        }
        box(tx - 12.5, 127.4, 25, 3, '#d09858', 0.7, tx, 0.6);
        line(tx - 9, 131, tx + 9, 147, INK, 0.9); line(tx + 9, 131, tx - 9, 147, INK, 0.9);
        box(tx - 2.2, 123.4, 4.4, 4.2, '#e0283a', 0.5, tx + 3, 0.6);
      }
    });
    for (const tx of TABLES) glowAdd(tx, 125, 10 + Math.sin(t * 9 + tx) * 1.2, '#ff6070', 0.45);
    c.restore();
  }
  // the yellow ginkgo tops, in front of everyone
  function streetFront(camX, t) {
    c.save(); c.translate(-camX, 0);
    for (const tx of treesAt) {
      const sw = Math.sin(t * 1.1 + tx) * 0.8, p = [];
      for (let i = 0; i < 11; i++) { const a = i / 11 * Math.PI * 2; p.push([tx + Math.cos(a) * 20 + sw * (i % 2), 50 + Math.sin(a) * 13, 8 + hash(tx + i) * 4]); }
      p.push([tx, 50, 14]);
      puffs(p, '#f2c230', 0.8, INK, '#e09a1e');
      for (let i = 0; i < 14; i++) { const a = hash(tx + i * 3) * 6.28, r = hash(tx + i * 5) * 18; ell(tx + Math.cos(a) * r, 46 + Math.sin(a) * r * 0.6, 1.3, 0.8, a, '#fff0a0'); }
    }
    c.restore();
  }

  // neon letters as glass tubes, like the real sign
  const NEON = {
    H: [[[0, 0], [0, 8]], [[5, 0], [5, 8]], [[0, 4], [5, 4]]],
    O: [[[1.5, 0], [3.5, 0], [5, 1.5], [5, 6.5], [3.5, 8], [1.5, 8], [0, 6.5], [0, 1.5], [1.5, 0]]],
    N: [[[0, 8], [0, 0], [5, 8], [5, 0]]],
    G: [[[5, 1.5], [3.5, 0], [1.5, 0], [0, 1.5], [0, 6.5], [1.5, 8], [3.5, 8], [5, 6.5], [5, 4.5], [3, 4.5]]],
    Y: [[[0, 0], [0, 2.5], [2.5, 4.5], [5, 2.5], [5, 0]], [[2.5, 4.5], [2.5, 8]]],
    S: [[[5, 1], [4, 0], [1, 0], [0, 1], [0, 3], [1, 4], [4, 4], [5, 5], [5, 7], [4, 8], [1, 8], [0, 7]]],
    P: [[[0, 8], [0, 0], [4, 0], [5, 1], [5, 3], [4, 4], [0, 4]]],
    'Ä': [[[0, 8], [0, 2], [1.5, 0], [3.5, 0], [5, 2], [5, 8]], [[0, 5], [5, 5]], [[1.5, -2], [1.5, -1.5]], [[3.5, -2], [3.5, -1.5]]],
    T: [[[0, 0], [5, 0]], [[2.5, 0], [2.5, 8]]],
    I: [[[2.5, 0], [2.5, 8]]],
  };
  function neonText(txt, x, y, size, col, t, lw) {
    const k = size / 8, flick = (Math.sin(t * 37) > 0.97) ? 0.6 : 1;
    const strokes = (width, alpha, colour) => {
      c.strokeStyle = colour; c.lineWidth = width; c.lineCap = 'round'; c.lineJoin = 'round'; c.globalAlpha = alpha * flick;
      let cx = x;
      for (const ch of txt) {
        for (const seg of NEON[ch] || []) { c.beginPath(); seg.forEach(([px, py], i) => { const X = cx + px * k, Y = y + py * k; i ? c.lineTo(X, Y) : c.moveTo(X, Y); }); c.stroke(); }
        cx += 7 * k;
      }
    };
    c.save();
    c.globalCompositeOperation = 'lighter';
    strokes(lw * 4.5, 0.14, col); strokes(lw * 2.4, 0.32, col);
    c.globalCompositeOperation = 'source-over';
    strokes(lw, 1, '#fff6c0');
    c.restore();
  }

  // =====================================================================
  // vehicles: round and friendly
  // =====================================================================
  function wheel(x, y, r, t) {
    circle(x, y, r, '#26262c', 0.8); circle(x, y, r * 0.5, '#b8bcc4', 0.5);
    for (let k = 0; k < 3; k++) { const a = t + k * 2.09; line(x, y, x + Math.cos(a) * r * 0.45, y + Math.sin(a) * r * 0.45, '#6a6e78', 0.5); }
  }
  function car(x, y, col, dir, t, night) {
    c.save(); c.translate(x, y); c.scale(dir, 1);
    shape([[-20, -4], [-20.5, -10], [-17, -12.5], [-10, -13], [-6, -19.5], [7, -19.5], [11.5, -13], [19, -12], [21, -8], [20.5, -4]], col, 0.9, 3, 0.15);
    within(() => { R(-22, -7.5, 44, 4, shadeOf(col)); });
    shape([[-8.3, -13.2], [-5, -18.2], [-0.6, -18.2], [-0.6, -13.2]], '#a8d0ee', 0.6, 4, 0.05);
    shape([[0.8, -13.2], [0.8, -18.2], [6.2, -18.2], [9.6, -13.2]], '#a8d0ee', 0.6, 5, 0.05);
    line(-4, -17, -2.5, -14.4, 'rgba(255,255,255,.7)', 0.8);
    line(0.1, -12.6, 0.1, -5, 'rgba(0,0,0,.25)', 0.5);
    ell(19.6, -9.4, 1.3, 1, 0, '#fff4c0', 0.4); ell(-19.8, -9.4, 0.9, 1.1, 0, '#e02828', 0.4);
    c.restore();
    if (night) { c.save(); c.globalCompositeOperation = 'lighter'; glow(x + dir * 26, y - 8, 16, '#fff3c0', 0.35); glow(x - dir * 22, y - 8, 6, '#ff2020', 0.4); c.restore(); }
    wheel(x - 12 * dir, y - 3, 3.6, t * 10 * dir); wheel(x + 12 * dir, y - 3, 3.6, t * 10 * dir);
  }
  function taxi(x, y, dir, t) {
    car(x, y, '#f4ecd0', dir, t, true);
    box(x - 4.5, y - 22.6, 9, 3.4, '#ffd84a', 0.6, 9, 0.6);
    c.save(); c.font = '800 2.6px "M PLUS Rounded 1c", sans-serif'; c.fillStyle = '#2a2016'; c.textAlign = 'center'; c.fillText('TAXI', x, y - 20); c.restore();
  }
  function amarok(x, y, dir, t, driver) {
    c.save(); c.translate(x, y); c.scale(dir, 1);
    const col = '#4c7a3e';
    poly([[-27.5, -4.6], [-27.5, -14.2], [-2.5, -14.2], [-0.6, -22.6], [16.6, -22.6], [20.6, -15], [27, -13.8], [28.4, -10], [28.4, -4.6]], col, 0.95, 11, 0.12);
    within(() => { R(-29, -9, 58, 5, '#3a6230'); });
    line(-27, -14, -2, -14, INK, 0.8); line(-26, -11.5, -3, -11.5, '#3a6230', 0.8);
    poly([[1.6, -21.4], [15.8, -21.4], [19.2, -15.4], [1.6, -15.4]], '#a8d0ee', 0.6, 12, 0.05);
    box(-26, -18.4, 2, 4.4, col, 0.7, 14, 0.4); box(-6, -18.4, 2, 4.4, col, 0.7, 15, 0.4); line(-25, -17.4, -5, -17.4, INK, 0.7);
    line(10, -21.5, 10, -15.4, col, 1);
    if (driver && LOOK[driver]) { circle(6, -17.6, 2.6, LOOK[driver].skin, 0.5); c.beginPath(); c.arc(6, -18.4, 2.9, Math.PI, 0); fillInk(LOOK[driver].hair, 0.5); }
    line(4, -20.6, 7, -16.4, 'rgba(255,255,255,.6)', 0.7);
    ell(27, -11, 1.4, 1.4, 0, '#fff4c0', 0.4); ell(-27, -11, 1, 1.6, 0, '#e02828', 0.4);
    box(-28, -6, 57, 2.4, '#2a2a2e', 0.6, 13, 0.6);
    c.restore();
    glowAdd(x + dir * 32, y - 10, 15, '#fff3c0', 0.28);
    wheel(x - 16 * dir, y - 3.4, 4.8, t * 9 * dir); wheel(x + 16 * dir, y - 3.4, 4.8, t * 9 * dir);
  }
  function tram(x, y, dir, t) {
    for (let s = 0; s < 3; s++) {
      const sx = x + s * 51 * dir;
      c.save(); c.translate(sx, y); c.scale(dir, 1);
      const front = s === 2;
      shape(front ? [[0, -5], [0, -40], [44, -40], [50, -34], [51, -5]] : [[0, -5], [0, -40], [50, -40], [50, -5]], '#f6c51e', 0.95, s + 21, 0.1);
      within(() => { R(-1, -9, 53, 5, '#d8a40e'); R(-1, -41, 53, 2.6, '#ffe070'); });
      for (let w = 0; w < 4; w++) box(3 + w * 11.6, -33.5, 9.6, 14, '#fff0b8', 0.6, s * 9 + w, 0.8);
      line(0, -18, 50, -18, '#3a3a3a', 1.6);
      if (front) { box(39, -38.6, 10, 5, '#1a1a1a', 0.5, 31, 0.6); c.save(); c.font = '800 3.4px "M PLUS Rounded 1c", sans-serif'; c.fillStyle = '#ffb000'; c.textAlign = 'center'; c.fillText('M10', 44, -35); c.restore(); ell(49, -10, 1.4, 1.2, 0, '#fff4c0', 0.4); }
      c.restore();
      wheel(sx + dir * 10, y - 3, 3, t * 12 * dir); wheel(sx + dir * 40, y - 3, 3, t * 12 * dir);
    }
    glowAdd(x + dir * 75, y - 25, 70, '#ffe7a8', 0.13);
    // the pantograph on the middle car
    const px = x + dir * 76;
    line(px - 6, y - 40, px, y - 50, '#333', 0.7); line(px + 6, y - 40, px, y - 50, '#333', 0.7); line(px - 6, y - 50, px + 6, y - 50, '#333', 0.9);
    return 150;
  }
  function scooter(x, y, dir, rider, t) {
    c.save(); c.translate(x, y); c.scale(dir, 1);
    box(-8.5, -3.6, 15, 2, '#33363c', 0.6, 2, 0.8);
    limb([[6, -2.6], [7.4, -19]], '#4a4e56', 1.1, 0.5);
    limb([[5, -19.6], [10, -19.6]], '#33363c', 1, 0.45);
    ell(8.6, -16.6, 0.9, 0.7, 0, '#b0ff8a', 0.3);
    c.restore();
    wheel(x - 7 * dir, y - 2.2, 2.3, t * 12 * dir); wheel(x + 7 * dir, y - 2.2, 2.3, t * 12 * dir);
  }
  function plane(x, y, s, t, tilt) {
    c.save(); c.translate(x, y); c.rotate(tilt || 0); c.scale(s, s);
    shape([[-32, -4.5], [-26, -6], [22, -6], [30, -4.6], [34, -2], [34, 0], [30, 2.6], [-26, 3.4], [-32, 1]], '#f6f7fa', 0.9, 31, 0.1);
    within(() => { R(-34, 0.8, 70, 3, '#d6dbe4'); });
    shape([[-31, -4.6], [-36, -18], [-31, -18], [-22, -5.6]], '#2f78c4', 0.8, 32, 0.05);
    shape([[-4, 1.5], [10, 1.5], [-6, 15], [-13, 15]], '#dfe3ea', 0.8, 33, 0.05);
    shape([[-4, -4.5], [8, -4.5], [-2, -11], [-7, -11]], '#c9cfd8', 0.7, 34, 0.05);
    for (let k = 0; k < 10; k++) ell(-20 + k * 4.4, -2.6, 0.9, 1.1, 0, '#3a5a7a');
    shape([[27, -4], [32, -3.6], [33.4, -1.8], [28, -1.8]], '#3a5a7a', 0.5, 35, 0.02);
    line(-30, -0.5, 30, -0.5, '#2f78c4', 0.9);
    c.restore();
  }
  function train(x, y, t, rider) {
    for (let k = 0; k < 3; k++) {
      const sx = x - k * 64;
      box(sx - 60, y - 35, 62, 30, '#d8212b', 0.95, k + 41, 1.4);
      R(sx - 59.5, y - 21, 61, 3, '#f6f2ea');
      for (let w = 0; w < 5; w++) box(sx - 56 + w * 12, y - 31.5, 9, 8.4, '#c4e0f2', 0.5, k * 7 + w, 0.8);
      box(sx - 60, y - 6, 62, 2.2, '#3a3a40', 0.6, k + 44, 0.4);
      wheel(sx - 50, y - 2.6, 3.2, t * 14); wheel(sx - 8, y - 2.6, 3.2, t * 14);
    }
    shape([[x + 2, y - 35], [x + 8, y - 34], [x + 15, y - 22], [x + 15, y - 5], [x + 2, y - 5]], '#d8212b', 0.95, 47, 0.08);
    shape([[x + 4, y - 31], [x + 8, y - 31], [x + 12, y - 23], [x + 4, y - 23]], '#c4e0f2', 0.5, 48, 0.05);
    if (rider && LOOK[rider]) {
      const L = LOOK[rider];
      circle(x - 30, y - 26, 2.8, L.skin, 0.5);
      if (L.hat) { ell(x - 30, y - 28.2, 4, 0.9, 0, L.hat2, 0.4); box(x - 32.2, y - 31.6, 4.4, 3, L.hat, 0.4, 3, 0.6); }
      if (L.must) line(x - 31.6, y - 25, x - 28.4, y - 25, L.must, 0.8);
    }
    c.save(); c.font = '800 4.4px "M PLUS Rounded 1c", sans-serif'; c.fillStyle = '#ffffff'; c.fillText('RE7', x - 55, y - 8.8); c.restore();
  }
  // =====================================================================
  // inside Hoongy, seen from above: wood, washi paper, red lanterns and two
  // big neon koi that swim under the ceiling
  // =====================================================================
  const IN = {
    W: 320, H: 236,
    door: { x: 40, y: 222 },
    bar: { x: 200, y: 52 }, kitchen: { x: 296, y: 46 },
    radio: { x: 24, y: 133 },               // the old tube radio on its cabinet, left wall
    tables: [
      { x: 80, y: 104, w: 20, h: 26 }, { x: 160, y: 104, w: 20, h: 26 }, { x: 240, y: 104, w: 20, h: 26 },
      { x: 120, y: 172, w: 20, h: 18 },
    ],
    seats: [],
    // furniture you can't walk through: [x0, y0, x1, y1]
    solid: [[10, 118, 38, 148], [8, 178, 33, 203], [280, 130, 312, 155], [276, 186, 312, 222]],
  };
  IN.tables.forEach((tb, ti) => {
    const four = ti < 3;
    const dy = four ? [-6, 6] : [0];
    for (const side of [-1, 1]) for (const d of dy) IN.seats.push({ x: tb.x + side * 17, y: tb.y + d, table: ti, face: side < 0 ? 0 : Math.PI });
  });

  // ---------- the neon koi: where their bodies are at time t ----------
  const KOI = [
    { body: '#ff7a2a', spot: '#ff3d7a', fin: '#ffd36a', speed: 0.11, off: 0, len: 66 },
    { body: '#4fd8ff', spot: '#b98cff', fin: '#d8f6ff', speed: 0.11, off: Math.PI, len: 62 },
  ];
  function koiPath(u) {
    const r = 1 + Math.sin(u * 3) * 0.07;
    return [162 + Math.cos(u) * 104 * r, 140 + Math.sin(u) * 50 * r];
  }
  function koiSpine(k, t) {
    const K = KOI[k], N = 14, seg = K.len / N;
    let u = t * K.speed + K.off;
    let [px, py] = koiPath(u);
    const pts = [[px, py]];
    let acc = 0;
    while (pts.length <= N) {
      u -= 0.004;
      const [qx, qy] = koiPath(u);
      acc += Math.hypot(qx - px, qy - py); px = qx; py = qy;
      if (acc >= seg) { pts.push([qx, qy]); acc = 0; }
    }
    // swimming: the body waves more towards the tail
    return pts.map((p, j) => {
      const a = pts[Math.max(0, j - 1)], b = pts[Math.min(N, j + 1)];
      const tx = a[0] - b[0], ty = a[1] - b[1], l = Math.hypot(tx, ty) || 1;
      const nx = -ty / l, ny = tx / l, s = j / N;
      const w = Math.sin(t * 4.2 - j * 0.55 + k) * (0.4 + 3.2 * s * s);
      return { x: p[0] + nx * w, y: p[1] + ny * w, nx, ny, tx: tx / l, ty: ty / l };
    });
  }
  function koiWidth(s) {
    // blunt round head, plump body, thin before the tail
    if (s < 0.12) return 8 * (0.62 + 0.38 * Math.sqrt(s / 0.12));
    if (s < 0.4) return 8;
    return 8 * (1 - (s - 0.4) / 0.6 * 0.8);
  }
  function neonStroke(col, draw, a) {
    c.save(); c.globalCompositeOperation = 'lighter'; c.lineCap = 'round'; c.lineJoin = 'round';
    c.strokeStyle = rgba(col, 0.13 * a); c.lineWidth = 5; draw(); c.stroke();
    c.strokeStyle = rgba(col, 0.45 * a); c.lineWidth = 2.2; draw(); c.stroke();
    c.strokeStyle = rgba('#ffffff', 0.75 * a); c.lineWidth = 0.7; draw(); c.stroke();
    c.restore();
  }
  function koiDraw(k, t) {
    const K = KOI[k], P = koiSpine(k, t), N = P.length - 1;
    // neon tubes buzz now and then
    const a = hash((t * 9 | 0) + k * 977) > 0.985 ? 0.45 : 1;
    const side = (j, sd, extra) => { const p = P[j], w = koiWidth(j / N) * sd + (extra || 0) * sd; return [p.x + p.nx * w, p.y + p.ny * w]; };
    const L = P.map((p, j) => side(j, 1)), Rt = P.map((p, j) => side(j, -1));
    const nose = [P[0].x + P[0].tx * 5, P[0].y + P[0].ty * 5];
    const end = P[N], bx = -end.tx, by = -end.ty, nx = end.nx, ny = end.ny;
    const sw = Math.sin(t * 4.2 - N * 0.55 + k) * 4;
    const at = (b, n) => [end.x + bx * b + nx * n, end.y + by * b + ny * n];
    const smooth = pts => { for (let i = 1; i < pts.length - 1; i++) c.quadraticCurveTo(pts[i][0], pts[i][1], (pts[i][0] + pts[i + 1][0]) / 2, (pts[i][1] + pts[i + 1][1]) / 2); c.lineTo(pts[pts.length - 1][0], pts[pts.length - 1][1]); };
    // body: round head, smooth sides
    neonStroke(K.body, () => {
      c.beginPath(); c.moveTo(L[N][0], L[N][1]);
      smooth(L.slice().reverse());
      c.quadraticCurveTo(nose[0] + P[0].nx * 5, nose[1] + P[0].ny * 5, nose[0], nose[1]);
      c.quadraticCurveTo(nose[0] - P[0].nx * 5, nose[1] - P[0].ny * 5, Rt[0][0], Rt[0][1]);
      smooth(Rt);
    }, a);
    // a big flowing fan tail: two soft round lobes that ripple
    neonStroke(K.fin, () => {
      const t1 = at(17, 11 + sw), t2 = at(17, -11 + sw), notch = at(11, sw * 0.6);
      c.beginPath(); c.moveTo(L[N][0], L[N][1]);
      c.bezierCurveTo(...at(4, 9), ...at(11, 15 + sw), ...t1);
      c.bezierCurveTo(...at(21, 8 + sw), ...at(16, 2 + sw), ...notch);
      c.bezierCurveTo(...at(16, -2 + sw), ...at(21, -8 + sw), ...t2);
      c.bezierCurveTo(...at(11, -15 + sw), ...at(4, -9), Rt[N][0], Rt[N][1]);
      // rays in the tail
      for (const n of [-6, 0, 6]) { c.moveTo(...at(2, n * 0.3)); c.quadraticCurveTo(...at(9, n + sw * 0.5), ...at(14, n * 1.4 + sw)); }
    }, a * 0.9);
    // round paddle fins, gently rowing
    const fin = (j, sd, size, ph) => {
      const p = P[j], f = Math.sin(t * 2.6 + ph) * 0.3;
      const [x0, y0] = side(j, sd);
      // pointing outwards and a little backwards
      const th = 0.7 + f, ux = p.nx * sd * Math.cos(th) - p.tx * Math.sin(th), uy = p.ny * sd * Math.cos(th) - p.ty * Math.sin(th);
      const ang = Math.atan2(uy, ux);
      const cx = x0 + ux * size * 0.55 - p.tx * 1.5, cy = y0 + uy * size * 0.55 - p.ty * 1.5;
      c.moveTo(cx + ux * size * 0.55, cy + uy * size * 0.55);
      c.ellipse(cx, cy, size * 0.55, size * 0.36, ang, 0, 7);
    };
    neonStroke(K.fin, () => { c.beginPath(); fin(3, 1, 10, 0); fin(3, -1, 10, 1.6); fin(8, 1, 6, 2); fin(8, -1, 6, 3.6); }, a);
    // short line along the back, whiskers at the mouth
    neonStroke(K.fin, () => {
      c.beginPath(); c.moveTo(P[4].x, P[4].y); for (let j = 5; j <= 8; j++) c.lineTo(P[j].x, P[j].y);
      for (const sd of [1, -1]) { const p = P[0]; c.moveTo(nose[0] + p.nx * sd * 2, nose[1] + p.ny * sd * 2); c.quadraticCurveTo(nose[0] + p.tx * 3 + p.nx * sd * 4, nose[1] + p.ty * 3 + p.ny * sd * 4, nose[0] + p.tx * 2 + p.nx * sd * 7 + Math.sin(t * 3) * 0.8, nose[1] + p.ty * 2 + p.ny * sd * 7); }
    }, a * 0.7);
    neonStroke(K.spot, () => {
      c.beginPath();
      for (const [j, sd, r] of [[2, 0.15, 3.2], [5, -0.35, 2.8], [7, 0.4, 2.2], [10, -0.1, 1.6]]) {
        const p = P[j], w = koiWidth(j / N) * sd;
        c.moveTo(p.x + p.nx * w + r, p.y + p.ny * w); c.ellipse(p.x + p.nx * w, p.y + p.ny * w, r, r * 0.8, Math.atan2(p.ty, p.tx), 0, 7);
      }
    }, a);
    // eyes
    for (const sd of [1, -1]) {
      const p = P[1], ex = p.x + p.nx * sd * 3.6 + p.tx * 1.5, ey = p.y + p.ny * sd * 3.6 + p.ty * 1.5;
      c.save(); c.globalCompositeOperation = 'lighter'; glow(ex, ey, 3, '#ffffff', 0.6 * a); c.restore();
      R(ex - 0.5, ey - 0.5, 1, 1, '#ffffff');
    }
  }
  function koiLight(t) {
    // the colour the neon throws on the floor below
    c.save(); c.globalCompositeOperation = 'lighter';
    KOI.forEach((K, k) => { const P = koiSpine(k, t); for (const j of [2, 7, 12]) glow(P[j].x, P[j].y, 26, K.body, 0.075); });
    c.restore();
  }

  // ---------- little helpers for the decorations ----------

  // ---------- little helpers for the decorations ----------
  function seigaiha(x0, y0, w, h, bg, fg, sz) {
    R(x0, y0, w, h, bg);
    c.save(); c.beginPath(); c.rect(x0, y0, w, h); c.clip();
    c.strokeStyle = fg; c.lineWidth = 0.6;
    for (let row = 0, y = y0 + h; y > y0 - sz; row++, y -= sz / 2) {
      for (let x = x0 - sz + (row % 2) * sz; x < x0 + w + sz; x += sz * 2) {
        for (const r of [sz, sz * 0.66, sz * 0.33]) { c.beginPath(); c.arc(x, y, r, Math.PI, 0); c.stroke(); }
      }
    }
    c.restore();
  }
  function shoji(x, y, w, h) {
    box(x, y, w, h, '#e4d3a8', 0.7, x + y, 0.4);
    for (let yy = y + 4; yy < y + h - 1; yy += 4) line(x + 0.6, yy, x + w - 0.6, yy, '#7a5434', 0.5);
    for (let xx = x + 3.3; xx < x + w - 1; xx += 3.3) line(xx, y + 0.6, xx, y + h - 0.6, '#7a5434', 0.5);
  }
  function fan(x, y, r, col) {
    c.beginPath(); c.moveTo(x, y); c.arc(x, y, r, Math.PI * 1.1, Math.PI * 1.9); c.closePath(); fillInk(col, 0.6);
    c.strokeStyle = '#e8c35a'; c.lineWidth = 0.4;
    for (let k = 1; k < 6; k++) { const a = Math.PI * (1.1 + k * 0.8 / 6); c.beginPath(); c.moveTo(x, y); c.lineTo(x + Math.cos(a) * r, y + Math.sin(a) * r); c.stroke(); }
    circle(x, y, 1, '#3a2416', 0.4);
  }

  function roomFloor() {
    const W = IN.W, H = IN.H;
    R(0, 0, W, H, '#4a3322');
    for (let y = 0; y < H; y += 7) {
      line(0, y, W, y, 'rgba(30,18,10,.55)', 0.5);
      for (let x = ((y / 7) % 2) * 22 + hash(y) * 10; x < W; x += 44) line(x, y, x, y + 7, 'rgba(30,18,10,.55)', 0.5);
      if (hash(y + 3) > 0.5) stroke([[hash(y) * W, y + 3.5], [hash(y) * W + 14, y + 3.2]], 'rgba(255,220,180,.07)', 0.7, 0, 0);
    }
    box(55, 125, 214, 30, '#d8b052', 0.9, 1, 1);
    seigaiha(56.5, 126.5, 211, 27, '#1d2d55', '#4a6aa6', 5);
    c.strokeStyle = INK; c.lineWidth = 0.6; c.strokeRect(56.5, 126.5, 211, 27);
  }
  function roomStill() {
    const W = IN.W, H = IN.H;
    // ---- walls ----
    box(-1, -1, W + 2, 16.5, '#2c1b10', 0.9, 2, 0.2);
    seigaiha(112, 1.4, 198, 11.6, '#18295a', '#3a5ca0', 4);
    line(112, 1.4, 310, 1.4, '#a0302a', 0.9); line(112, 13, 310, 13, '#a0302a', 0.9);
    fan(150, 12.4, 9, '#cc3a32'); fan(272, 12.4, 9, '#cc3a32'); fan(211, 12.4, 7, '#efe6d0');
    for (let y = 16; y < H - 14; y += 14) { shoji(0, y, 10, 13); if (y < 26 || y > 60) shoji(W - 10, y, 10, 13); }
    box(15, 1.5, 94, 12, '#121c1b', 0.8, 3, 1);
    // ---- the corner garden ----
    box(10, 16, 104, 38, '#3a3830', 0.8, 4, 0.6);
    for (let i = 0; i < 70; i++) circle(12 + hash(i + 40) * 100, 18 + hash(i + 41) * 34, 0.5 + hash(i + 42) * 0.5, ['#5a5850', '#6e6c62', '#4a4840'][i % 3]);
    blob(66, 35, 27, 15, '#8a8a80', 0.8, 5);
    blob(66, 35, 23.6, 12.2, '#16505a', 0.7, 6);
    within(() => { glow(60, 30, 18, '#5ab0b0', 0.3); });
    for (let i = 0; i < 14; i++) { const a = i / 14 * 6.28; blob(66 + Math.cos(a) * 25.5, 35 + Math.sin(a) * 13.8, 2.2 + hash(i + 7) * 1.2, 1.9 + hash(i + 8), ['#9a9a90', '#aaa89e', '#80807a'][i % 3], 0.5, i + 50, 8); }
    circle(78, 30, 3.4, '#4f8a42', 0.6); shape([[78, 30], [81.4, 28.6], [81.4, 31.2]], '#16505a', 0, 0, 0); circle(80, 39, 1.4, '#f4a8c0', 0.4);
    box(99.5, 32.5, 13, 13, '#8a887e', 0.8, 7, 1.2); box(102.5, 35.5, 7, 7, '#3a3428', 0.6, 8, 0.6); box(104, 37, 4, 4, '#ffd27a', 0, 9, 0.4);
    // ---- the bar ----
    box(119, 15.6, 170, 7, '#3a2618', 0.8, 10, 0.6);
    for (let b = 0; b < 40; b++) box(122 + b * 4, 16.6, 2.8, 4.6, ['#d8a040', '#7ab0a0', '#efeadc', '#b84a40', '#c8c060'][b % 5], 0.35, b, 0.6);
    circle(276, 19, 2.8, '#d42a2a', 0.6); ell(276, 18.6, 1.4, 1.1, 0, '#f6ead6', 0.3); // daruma
    box(119, 29.4, 170, 13, '#6b4a2e', 0.9, 11, 1.2); line(120, 31.4, 288, 31.4, '#8a6240', 1);
    for (let k = 0; k < 7; k++) { circle(132 + k * 22, 46.5, 3.2, '#8a2f26', 0.7); circle(131.4 + k * 22, 45.8, 1.4, '#b04a3c'); }
    box(227.4, 32.4, 3.6, 6, '#f2eee2', 0.5, 12, 0.8); circle(234, 35.8, 1.2, '#f2eee2', 0.4); circle(237.4, 35.8, 1.2, '#f2eee2', 0.4);
    circle(184, 36, 2.8, '#33403e', 0.6); line(186, 35, 188.4, 34.2, '#33403e', 0.9);
    // the kitchen door
    box(W - 18.5, 27.5, 9, 31, '#2c1d12', 0.8, 13, 0.4); box(W - 17, 29, 6, 28, '#7a5638', 0.5, 14, 0.4);
    // ---- the radio cabinet ----
    box(11, 119, 26, 28, '#6a4426', 0.9, 15, 1); line(12, 121.6, 36, 121.6, '#8a5a32', 0.9);
    box(15, 125, 18, 15, '#9a6634', 0.8, 16, 1.4); line(16, 126.6, 32, 126.6, '#b8803f', 0.6);
    box(16.6, 127.6, 7.4, 10.6, '#5a3a22', 0.4, 17, 0.6);
    for (let yy = 128.8; yy < 138; yy += 1.6) line(17.2, yy, 23.4, yy, 'rgba(255,220,160,.25)', 0.4);
    circle(26.5, 135.5, 1.3, '#3a2414', 0.5); circle(29.5, 135.5, 1.3, '#3a2414', 0.5);
    // ---- plants ----
    box(14.5, 184.5, 12, 12, '#7a4a30', 0.8, 18, 1.4);
    puffs([[17, 186, 7], [24, 185, 6.6], [20, 192, 7], [14, 192, 5], [26, 192, 5.4], [20, 182, 5.4]], '#e9cc46', 0.7, INK, '#c9a42e');
    box(281, 131, 30, 23, '#5a3a22', 0.8, 19, 1); box(288, 137, 16, 11, '#3f78a8', 0.7, 20, 1.6);
    stroke([[296, 143], [291, 139.6], [292, 135]], '#6a4428', 1.4, 0, 0); stroke([[296, 143], [302, 141], [303, 137]], '#6a4428', 1.4, 0, 0);
    puffs([[291, 135, 4], [303, 136, 3.6], [297, 139, 3], [287, 139, 2.8]], '#3f7a36', 0.7, INK, '#2f5a2a');
    box(287.5, 196, 13.5, 13, '#8a3a2a', 0.8, 21, 1.6);
    stroke([[294, 202], [284, 192]], '#5a3424', 1.6, 0, 0); stroke([[294, 202], [302, 190]], '#5a3424', 1.6, 0, 0); stroke([[294, 202], [292, 186]], '#5a3424', 1.6, 0, 0);
    puffs([[284, 192, 8], [302, 190, 7.5], [292, 185, 7], [298, 200, 6], [286, 202, 5.5]], '#f4b2c8', 0.75, INK, '#e48aa8');
    for (let i = 0; i < 30; i++) { const a = hash(i + 500) * 6.28, r = Math.sqrt(hash(i + 501)) * 14; circle(293 + Math.cos(a) * r, 194 + Math.sin(a) * r * 0.9, 0.6, ['#fff0f4', '#d8698c'][i % 2]); }
    // ---- tables and cushions ----
    IN.seats.forEach((st, i) => {
      box(st.x - 5, st.y - 5, 10, 10, '#3a2416', 0.7, i + 30, 1.2);
      box(st.x - 4, st.y - 4, 8, 8, i % 2 ? '#a82a3c' : '#28427a', 0.5, i + 50, 1.6);
      circle(st.x, st.y, 0.6, '#e8c35a');
    });
    IN.tables.forEach((tb, i) => {
      box(tb.x - tb.w / 2 + 1.4, tb.y - tb.h / 2 + 2.2, tb.w, tb.h, 'rgba(0,0,0,.3)', 0, i, 1.4);
      box(tb.x - tb.w / 2, tb.y - tb.h / 2, tb.w, tb.h, '#9a6a3c', 0.9, i + 70, 1.4);
      line(tb.x - tb.w / 2 + 1, tb.y - tb.h / 2 + 1.6, tb.x + tb.w / 2 - 1, tb.y - tb.h / 2 + 1.6, '#c08a50', 0.8);
      box(tb.x - tb.w / 2 + 2, tb.y - 1.2, tb.w - 4, 2.4, '#cc3a32', 0.4, i + 80, 0.4);
      circle(tb.x, tb.y, 2.2, '#f2eadc', 0.5);
      box(tb.x + 3.4, tb.y - 9.4, 2.6, 3.4, '#2f2f34', 0.4, i + 90, 0.6); circle(tb.x + 4, tb.y - 10.8, 1.1, '#f4a8c0', 0.3); line(tb.x + 5.4, tb.y - 10, tb.x + 6, tb.y - 12.6, '#5a8a3a', 0.6);
    });
    // ---- the front window along the bottom ----
    box(9, H - 12.5, W - 18, 13, '#1a1e24', 0.9, 22, 0.4);
    for (let x = 60; x < W - 20; x += 40) line(x, H - 12, x, H, '#3a2618', 1.6);
  }

  // s: { seatTaken, hiSeat, doorOpen, radioOn, hiRadio }
  function inside(t, s) {
    s = s || {};
    const W = IN.W, H = IN.H;
    layer('inFloor', 0, 0, W, H, roomFloor);
    koiLight(t);
    layer('inRoom', -2, -2, W + 4, H + 4, roomStill);
    // light through the paper walls, the bar, the lantern in the garden
    c.save(); c.globalCompositeOperation = 'lighter';
    for (let y = 30; y < H - 20; y += 40) { glow(4, y, 18, '#ffd9a0', 0.12); glow(W - 4, y + 20, 18, '#ffd9a0', 0.12); }
    glow(205, 20, 70, '#fff0c0', 0.18); glow(106, 39, 16, '#ffbe5a', 0.35);
    glow(W / 2, H, 90, '#ffcf80', 0.1);
    c.restore();
    neonText('HOONGY', 22, 3, 8, '#ffe23a', t, 1);
    // pond: ripples and three real koi
    c.strokeStyle = 'rgba(190,240,240,.35)'; c.lineWidth = 0.5;
    for (let i = 0; i < 3; i++) { const k = (t * 0.25 + i / 3) % 1; c.globalAlpha = 1 - k; c.beginPath(); c.ellipse(58 + i * 9, 33 + (i % 2) * 4, 2 + k * 8, 1 + k * 4, 0, 0, Math.PI * 2); c.stroke(); }
    c.globalAlpha = 1;
    for (let i = 0; i < 3; i++) {
      const a = t * (0.6 + i * 0.15) + i * 2.1, fx = 66 + Math.cos(a) * (13 - i * 3), fy = 35 + Math.sin(a) * (6 - i);
      const hx = Math.cos(a + 1.57), hy = Math.sin(a + 1.57);
      ell(fx, fy, 2.8, 1.1, Math.atan2(hy, hx), ['#ff8a2a', '#f8f4ec', '#ffb43a'][i], 0.4);
      shape([[fx - hx * 2.4, fy - hy * 2.4], [fx - hx * 4.4 - hy * 1.4, fy - hy * 4.4 + hx * 1.4], [fx - hx * 4.4 + hy * 1.4, fy - hy * 4.4 - hx * 1.4]], ['#ff8a2a', '#f8f4ec', '#ffb43a'][i], 0.3, i, 0);
      circle(fx + hx * 0.6, fy + hy * 0.6, 0.55, '#e8302a');
    }
    // bamboo, swaying
    for (let i = 0; i < 9; i++) {
      const bx = 14 + hash(i + 300) * 22, by = 19 + hash(i + 301) * 32, sway = Math.sin(t * 1.3 + i) * 1.4;
      circle(bx, by, 2, '#4a7a32', 0.6); circle(bx, by, 1.1, '#7ab052');
      for (let l = 0; l < 3; l++) { const a = hash(i * 3 + l + 330) * 6.28; shape([[bx, by], [bx + Math.cos(a) * 4 + sway * 0.5 - Math.sin(a) * 1.4, by + Math.sin(a) * 3 + Math.cos(a)], [bx + Math.cos(a) * 8 + sway, by + Math.sin(a) * 5.6]], '#86c45e', 0.4, i * 3 + l, 0); }
    }
    // maneki-neko waving at the guests
    const paw = Math.sin(t * 5) * 1.2;
    blob(126, 36.4, 3.6, 3.2, '#fbf7ee', 0.6, 11); circle(126, 32.4, 2.8, '#fbf7ee', 0.6);
    shape([[123.6, 31], [124, 28.4], [125.4, 30.2]], '#fbf7ee', 0.5, 1, 0); shape([[128.4, 31], [128, 28.4], [126.6, 30.2]], '#fbf7ee', 0.5, 2, 0);
    line(124, 35, 128, 35, '#d42a2a', 0.9); circle(126, 35.8, 0.6, '#ffd23a');
    circle(129.6, 32.6 + paw, 1.2, '#fbf7ee', 0.5);
    circle(125, 32.2, 0.4, INK); circle(127, 32.2, 0.4, INK);
    // the noren curtain at the kitchen door
    for (let i = 0; i < 3; i++) {
      const sw = Math.sin(t * 1.6 + i * 1.3) * 0.7;
      box(W - 21.5 + sw, 29 + i * 9.5, 5.4, 9, '#24447a', 0.5, i + 40, 0.4);
      circle(W - 18.8 + sw, 33.5 + i * 9.5, 1.3, '#f2efe6', 0.3);
    }
    // the radio: the dial lights up when it plays
    const on = !!s.radioOn;
    box(25, 128, 6, 4, on ? '#ffc35a' : '#6a5a40', 0.4, 23, 0.6);
    line(25.6 + (on ? (s.radioOn % 2 ? 1 : 4) : 2.2), 128.2, 25.6 + (on ? (s.radioOn % 2 ? 1 : 4) : 2.2), 131.8, '#c03020', 0.6);
    if (on) {
      glowAdd(28, 130, 12, '#ffb84a', 0.35);
      for (let i = 0; i < 4; i++) { const h = 1 + Math.abs(Math.sin(t * 7 + i * 1.7)) * 3; box(17 + i * 2, 141.6 - h, 1.2, h, '#ffd36a', 0, i, 0.3); }
      const k = (t * 0.7) % 1;
      c.globalAlpha = 1 - k; c.font = '6px sans-serif'; c.fillStyle = '#ffe9a8'; c.fillText(k < 0.5 ? '♪' : '♫', 30 + k * 6, 124 - k * 10); c.globalAlpha = 1;
    }
    if (s.hiRadio) { c.save(); c.setLineDash([2, 1.4]); c.strokeStyle = '#ffe23a'; c.lineWidth = 1; c.strokeRect(9.5, 117.5, 29, 31); c.restore(); }
    // petals drifting over the floor
    for (let i = 0; i < 12; i++) {
      const k = (t * 0.06 + hash(i + 600)) % 1;
      const px = 290 - k * (90 + hash(i + 601) * 120), py = 196 + Math.sin(k * 9 + i) * 6 - k * (hash(i + 602) * 60 - 20);
      c.globalAlpha = Math.min(1, (1 - k) * 2); ell(px, py, 0.9, 0.6, k * 9, '#ffc3d3'); c.globalAlpha = 1;
    }
    // seats you can take, candles on the tables
    IN.seats.forEach((st, i) => {
      const taken = s.seatTaken && s.seatTaken[i];
      if (!taken && s.hiSeat === i) { c.save(); c.setLineDash([2, 1.4]); c.strokeStyle = '#ffe23a'; c.lineWidth = 1; c.strokeRect(st.x - 6.5, st.y - 6.5, 13, 13); c.restore(); }
    });
    IN.tables.forEach((tb, i) => { circle(tb.x, tb.y, 1, '#ffcf6a'); glowAdd(tb.x, tb.y, 11 + Math.sin(t * 8 + i) * 1.2, '#ffb060', 0.35); });
    // the door in the front window
    box(IN.door.x - 12, H - 12, 24, 12, s.doorOpen ? '#f2c27a' : '#7a5236', 0.8, 24, 0.4);
    if (s.doorOpen) glowAdd(IN.door.x, H - 6, 18, '#ffd890', 0.4);
    line(IN.door.x - 12, H - 12.6, IN.door.x + 12, H - 12.6, '#d8c27a', 0.8);
    c.save(); c.globalAlpha = 0.25; neonText('HOONGY', W / 2 - 24, H - 10, 6, '#ffe23a', t, 0.8); c.restore();
  }

  // things that hang from the ceiling: red lanterns and the neon koi.
  // They are closer to the camera, so they are drawn a bit bigger and drift
  // against the floor when the view moves: that is what makes them look 3D.
  function lift(cam, k, fn) {
    const cx = cam ? cam.x + cam.w / 2 : IN.W / 2, cy = cam ? cam.y + cam.h / 2 : IN.H / 2;
    c.save(); c.translate(cx, cy); c.scale(k, k); c.translate(-cx, -cy); fn(); c.restore();
  }
  function chochin(x, y, col, rib, t, i) {
    const sx = x + Math.sin(t * 0.9 + i) * 0.6;
    glowAdd(sx, y, 26, col === '#f4ecd8' ? '#ffe6b8' : '#ff8a5a', 0.22);
    circle(sx, y, 5.6, col, 0.8);
    c.strokeStyle = rib; c.lineWidth = 0.5;
    for (const r of [4.4, 3.2, 2]) { c.beginPath(); c.arc(sx, y, r, 0, Math.PI * 2); c.stroke(); }
    circle(sx, y, 1.5, '#1a1410', 0.4);
    circle(sx - 2.2, y - 2.2, 1, 'rgba(255,255,255,.35)');
  }
  function insideTop(t, cam) {
    lift(cam, 1.1, () => {
      IN.tables.forEach((tb, i) => chochin(tb.x, tb.y, '#d0362a', '#8a1e16', t, i));
      for (const [i, x, y] of [[5, 150, 30], [6, 205, 30], [7, 260, 30]]) chochin(x, y, '#d0362a', '#8a1e16', t, i);
      for (const [i, x, y] of [[8, 44, 196], [9, 200, 168], [10, 40, 80]]) chochin(x, y, '#f4ecd8', '#b89a6a', t, i);
    });
    lift(cam, 1.24, () => { koiDraw(0, t); koiDraw(1, t); });
  }
  // food on the table (top-down)
  function dish(item, x, y, left, t) {
    left = Math.max(0, Math.min(1, left));
    if (item === 'lassi') {
      circle(x, y, 3.5, '#f4f4f0', 0.6); circle(x, y, 2.6 * Math.max(0.15, left), '#ffa620');
      line(x + 1, y - 4.4, x + 0.6, y, '#ff3a6a', 0.8);
    } else if (item === 'coffee') {
      circle(x, y, 3.5, '#f4f4f0', 0.6); circle(x, y, 2.5 * Math.max(0.1, left), '#4a2c1a');
      if (left > 0.1) circle(x, y + 0.6, 1.2 * left, '#d9b48a');
      if (left > 0) { c.globalAlpha = 0.4; stroke([[x, y - 3], [x + Math.sin(t * 3), y - 5], [x, y - 7]], '#ffffff', 0.7, 0, 0); c.globalAlpha = 1; }
    } else {
      ell(x, y, 5.2, 4.1, 0, '#f6f4ec', 0.6);
      const n = Math.ceil(left * 4);
      if (item === 'shrimp') for (let i = 0; i < n; i++) { c.beginPath(); c.arc(x - 2.5 + i * 1.8, y + (i % 2) - 0.5, 1.2, 0.3, 4.4); c.strokeStyle = '#ff7a3a'; c.lineWidth = 1.2; c.stroke(); }
      if (item === 'fish' && n) { ell(x - 0.5, y, 3.2 * left + 0.6, 1.7, 0, '#9aa9b8', 0.4); shape([[x + 2.6 * left + 0.6, y], [x + 4, y - 1.6], [x + 4, y + 1.6]], '#9aa9b8', 0.3, 0, 0); circle(x - 3 * left, y - 0.4, 0.4, INK); }
      if (item === 'fish' && !n) { c.strokeStyle = '#bbb'; c.lineWidth = 0.5; c.beginPath(); c.moveTo(x - 3, y); c.lineTo(x + 3, y); for (let k = -2; k <= 2; k++) { c.moveTo(x + k, y - 1); c.lineTo(x + k, y + 1); } c.stroke(); }
      line(x + 4.4, y - 3, x + 4.4, y + 3, '#c9c9c9', 0.6); line(x + 5.4, y - 3, x + 5.4, y + 3, '#c9c9c9', 0.6);
    }
  }
  function menuCard(x, y) { box(x - 3, y - 4, 6, 8, '#fbf4e2', 0.6, 3, 0.6); line(x - 2, y - 2.4, x + 2, y - 2.4, '#cc3a32', 0.7); line(x - 2, y - 0.4, x + 2, y - 0.4, '#8a7a6a', 0.4); line(x - 2, y + 1.2, x + 1, y + 1.2, '#8a7a6a', 0.4); }

  // =====================================================================
  // maps: ink on washi paper, sea and land in watercolour
  // =====================================================================
  const EUROPE = [
    [[-9.5, 43], [-9.3, 38.7], [-8.9, 37], [-6.3, 36.5], [-5.6, 36], [-2, 36.7], [0, 38.8], [0.2, 40], [3.2, 42], [3.1, 43.1], [4.8, 43.3], [7.5, 43.7], [8.8, 44.4], [10.2, 43.9], [12.4, 41.8], [15.6, 40], [16, 38], [17, 39], [18.5, 40.1], [16, 41.4], [13.5, 43.6], [12.3, 45.2], [13.7, 45.6], [15, 44.6], [16.4, 43.5], [18.5, 42.4], [19.5, 41.8], [19.4, 40.4], [21, 38.4], [21.6, 36.8], [22.5, 36.4], [23.2, 38], [24, 38.2], [22.9, 40.6], [24, 40.8], [26, 40.8], [28.9, 41], [28, 42], [28, 43.2], [28.6, 44.2], [29.7, 45.2], [30.7, 46.5], [31.8, 46.6], [33.5, 46], [35, 45.5], [37.5, 47], [39, 47.2], [42, 47], [42, 66], [33, 69.5], [25, 71], [15, 69], [10, 63.5], [5, 62], [5, 59], [7, 58], [10.5, 59.5], [11, 59], [11.9, 57.7], [12.8, 56], [14, 55.4], [16.5, 56.5], [18.5, 59.3], [17.5, 61], [19, 63.5], [22, 65.8], [21.5, 63], [21.3, 61], [22, 60.3], [25, 60.2], [28, 60.5], [28, 59.5], [24, 59.4], [23.5, 58.5], [24.3, 57.2], [21.5, 57.5], [21, 56], [21.1, 55.7], [19.9, 54.6], [18.6, 54.4], [16, 54.2], [14.2, 53.9], [12, 54.2], [10.9, 54.4], [9.9, 54.8], [10.6, 57.7], [8.6, 57.1], [8.1, 55.5], [8.6, 54.9], [8.9, 53.9], [7, 53.4], [4.8, 52.9], [4.6, 52.4], [4, 51.9], [3.4, 51.4], [2.5, 51.1], [1.6, 50.9], [0.2, 49.7], [-1.3, 49.6], [-4.7, 48.5], [-2.5, 47.3], [-1.2, 46.2], [-1.5, 43.5], [-4, 43.4], [-8, 43.7], [-9.5, 43]],
    [[-5.7, 50.1], [1.4, 51.1], [1.7, 52.6], [0.3, 53.4], [-0.1, 54.5], [-1.6, 55.6], [-2, 57.6], [-3.1, 58.6], [-5, 58.6], [-6.2, 57.5], [-5.6, 56.3], [-4.9, 55], [-3, 54.9], [-3.3, 53.6], [-4.6, 53.3], [-4.2, 52.3], [-5.3, 51.8], [-3, 51.5], [-5.7, 50.1]],
    [[-6, 52.2], [-6.2, 53.9], [-5.5, 54.6], [-7.3, 55.3], [-8.5, 54.4], [-10, 53.5], [-9.8, 51.6], [-8.3, 51.8], [-6, 52.2]],
    [[12.4, 38.1], [15.6, 38.3], [15.1, 36.7], [12.5, 37.6], [12.4, 38.1]],
    [[8.4, 41.2], [9.8, 40.8], [9.6, 39], [8.4, 39], [8.4, 41.2]],
    [[9.4, 43], [9.5, 41.4], [8.6, 41.7], [8.6, 42.6], [9.4, 43]],
  ];
  function projector(lon0, lat0, lon1, lat1, W, H, pad) {
    const k0 = Math.cos((lat0 + lat1) / 2 * Math.PI / 180);
    const sx = (W - pad * 2) / ((lon1 - lon0) * k0), sy = (H - pad * 2) / (lat1 - lat0), s = Math.min(sx, sy);
    const cx = W / 2, cy = H / 2, mlon = (lon0 + lon1) / 2, mlat = (lat0 + lat1) / 2;
    return (lon, lat) => [cx + (lon - mlon) * k0 * s, cy - (lat - mlat) * s];
  }
  const VERMILION = '#cf3a2a';
  function polyline(pts, P) { c.beginPath(); pts.forEach(([lo, la], i) => { const [x, y] = P(lo, la); i ? c.lineTo(x, y) : c.moveTo(x, y); }); }
  function mapPaper(W, H, P, lonR, latR, step) {
    R(0, 0, W, H, '#f4e8cc');
    // the sea: a blue watercolour wash with little waves
    for (let i = 0; i < 10; i++) wcBlob(hash(i + 3) * W, hash(i + 4) * H, W * 0.3, H * 0.3, '#6fa3c0', i + 7, 0.09);
    c.strokeStyle = 'rgba(70,120,150,.28)'; c.lineWidth = 0.5;
    for (let i = 0; i < 60; i++) { const x = hash(i + 200) * W, y = hash(i + 201) * H; for (const r of [2.4, 1.4]) { c.beginPath(); c.arc(x, y, r, Math.PI, 0); c.stroke(); } }
    // fine brown grid lines
    c.strokeStyle = 'rgba(120,80,40,.22)'; c.lineWidth = 0.4; c.setLineDash([2, 2]);
    for (let lon = lonR[0]; lon <= lonR[1]; lon += step[0]) { const a = P(lon, latR[0]), b = P(lon, latR[1]); c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke(); }
    for (let lat = latR[0]; lat <= latR[1]; lat += step[1]) { const a = P(lonR[0], lat), b = P(lonR[1], lat); c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke(); }
    c.setLineDash([]);
  }
  function compass(x, y, r) {
    circle(x, y, r, 'rgba(255,250,236,.6)', 0.6);
    shape([[x, y - r * 0.95], [x + r * 0.2, y], [x, y + r * 0.95], [x - r * 0.2, y]], VERMILION, 0.5, 1, 0);
    shape([[x - r * 0.95, y], [x, y - r * 0.2], [x + r * 0.95, y], [x, y + r * 0.2]], '#33241b', 0.5, 2, 0);
    c.save(); c.font = '800 ' + r * 0.7 + 'px "M PLUS Rounded 1c", sans-serif'; c.fillStyle = INK; c.textAlign = 'center'; c.fillText('N', x, y - r * 1.15); c.restore();
  }
  function stamp(p, on) {
    glowAdd(p[0], p[1], 9, on ? '#ff8a5a' : '#ffd080', 0.5);
    box(p[0] - 2.2, p[1] - 2.2, 4.4, 4.4, on ? VERMILION : '#a8542a', 0.5, Math.round(p[0]), 0.8);
    circle(p[0], p[1], 0.8, '#fff4e0');
  }
  function routeDots(pts, color) {
    for (let i = 0; i < pts.length; i++) if (i % 2 === 0) circle(pts[i][0], pts[i][1], 0.75, color || VERMILION);
  }
  function europeMap(W, H, progress, t) {
    const P = projector(-11, 35, 42, 66, W, H, 8);
    layer('europe' + W + 'x' + H, 0, 0, W, H, () => {
      mapPaper(W, H, P, [-20, 50], [30, 75], [5, 5]);
      for (const poly of EUROPE) {
        polyline(poly, P); c.fillStyle = '#efe0b6'; c.fill();
        within(() => { const [x0, y0] = P(-10, 66), [x1, y1] = P(42, 35); wcFill(x0, y0, x1 - x0, y1 - y0, '#8aa860', 3, 0.1); });
        polyline(poly, P); c.strokeStyle = 'rgba(51,36,27,.25)'; c.lineWidth = 2.2; c.stroke();
        polyline(poly, P); c.strokeStyle = INK; c.lineWidth = 0.8; c.lineJoin = 'round'; c.stroke();
      }
      compass(W - 16, H - 18, 7);
      splatter(0, 0, W, H, '#33241b', 5, 14);
    });
    const from = P(26.1, 44.43), to = P(13.4, 52.52);
    const pt = k => { const mx = (from[0] + to[0]) / 2, my = Math.min(from[1], to[1]) - 22; const a = 1 - k; return [a * a * from[0] + 2 * a * k * mx + k * k * to[0], a * a * from[1] + 2 * a * k * my + k * k * to[1]]; };
    const dots = []; for (let k = 0; k <= progress; k += 0.02) dots.push(pt(k));
    routeDots(dots);
    stamp(from, true); stamp(to, progress > 0.98);
    const [px, py] = pt(Math.min(1, progress)), [qx, qy] = pt(Math.min(1, progress + 0.02));
    c.save(); c.translate(px, py); c.rotate(Math.atan2(qy - py, qx - px)); c.scale(1.2, 1.2); planeIcon(); c.restore();
    return { from, to };
  }
  function planeIcon() {
    shape([[7, 0], [5, -1.2], [-5, -1.3], [-6.5, 0], [-5, 1.3], [5, 1.2]], '#ffffff', 0.6, 1, 0);
    shape([[1.5, -1], [-2, -6], [-3.6, -6], [-2, -1]], '#ffffff', 0.5, 2, 0);
    shape([[1.5, 1], [-2, 6], [-3.6, 6], [-2, 1]], '#ffffff', 0.5, 3, 0);
    shape([[-4.6, -0.6], [-6.4, -3], [-7.2, -3], [-6.2, 0]], '#ffffff', 0.4, 4, 0);
    shape([[-4.6, 0.6], [-6.4, 3], [-7.2, 3], [-6.2, 0]], '#ffffff', 0.4, 5, 0);
  }
  // little vehicles on the Brandenburg map
  function mapIcon(kind) {
    if (kind === 'train') { box(-6, -2.2, 12, 4.4, '#d8212b', 0.6, 1, 1.4); line(-5, 0, 5, 0, '#ffffff', 0.8); }
    else { box(-5, -2.4, 10, 4.8, '#4c7a3e', 0.6, 2, 1.4); box(0, -1.6, 3.6, 3.2, '#a8d0ee', 0.4, 3, 0.6); }
  }
  // Brandenburg close-up for the drive and the train
  const BERLIN = [[13.09, 52.42], [13.2, 52.37], [13.38, 52.34], [13.57, 52.37], [13.75, 52.43], [13.73, 52.53], [13.6, 52.62], [13.45, 52.67], [13.28, 52.66], [13.12, 52.6], [13.1, 52.5], [13.09, 52.42]];
  const LAKES = [[[13.17, 52.42], [13.21, 52.44], [13.19, 52.47], [13.15, 52.45]], [[13.62, 52.43], [13.68, 52.44], [13.66, 52.46], [13.61, 52.45]], [[12.48, 52.38], [12.58, 52.36], [12.62, 52.4], [12.52, 52.42]], [[12.9, 52.36], [12.98, 52.34], [12.97, 52.38]]];
  function brandenburgMap(W, H, route, progress, t) {
    const P = projector(11.95, 52.05, 13.85, 52.72, W, H, 10);
    layer('brb' + W + 'x' + H, 0, 0, W, H, () => {
      R(0, 0, W, H, '#f2e7c8');
      for (let i = 0; i < 9; i++) wcBlob(hash(i + 30) * W, hash(i + 31) * H, W * 0.25, H * 0.25, '#8aa860', i + 40, 0.08);
      // little pine woods
      for (let i = 0; i < 70; i++) { const x = hash(i + 600) * W, y = hash(i + 601) * H; shape([[x - 1.6, y + 1.6], [x, y - 2.2], [x + 1.6, y + 1.6]], 'rgba(60,100,60,.35)', 0, 0, 0); }
      c.strokeStyle = 'rgba(120,80,40,.2)'; c.lineWidth = 0.4; c.setLineDash([2, 2]);
      for (let lon = 12; lon < 14; lon += 0.25) { const a = P(lon, 52), b = P(lon, 52.8); c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke(); }
      for (let lat = 52; lat < 52.8; lat += 0.125) { const a = P(11.9, lat), b = P(14, lat); c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke(); }
      c.setLineDash([]);
      polyline(BERLIN, P); c.fillStyle = 'rgba(220,140,130,.35)'; c.fill(); c.strokeStyle = INK; c.lineWidth = 0.8; c.stroke();
      for (const lk of LAKES) { polyline(lk, P); c.fillStyle = 'rgba(90,150,190,.55)'; c.fill(); c.strokeStyle = 'rgba(40,80,110,.7)'; c.lineWidth = 0.5; c.stroke(); }
      c.strokeStyle = '#8a5a30'; c.lineWidth = 1.3; polyline([[11.95, 52.2], [12.28, 52.27], [12.55, 52.33], [12.95, 52.36], [13.1, 52.39]], P); c.stroke();
      c.strokeStyle = INK; c.lineWidth = 0.9; c.setLineDash([3, 2]); polyline([[12.76, 52.2], [12.95, 52.3], [13.06, 52.39], [13.2, 52.45], [13.37, 52.525]], P); c.stroke(); c.setLineDash([]);
      compass(W - 14, H - 16, 6);
      splatter(0, 0, W, H, '#33241b', 9, 12);
    });
    const path = route.map(([lo, la]) => P(lo, la));
    let total = 0; const seg = [];
    for (let i = 1; i < path.length; i++) { const d = Math.hypot(path[i][0] - path[i - 1][0], path[i][1] - path[i - 1][1]); seg.push(d); total += d; }
    let want = total * Math.min(1, progress), at = path[0], ang = 0;
    const dots = [];
    for (let i = 1; i < path.length; i++) {
      const d = seg[i - 1], a = path[i - 1], b = path[i], k = Math.min(1, want / d);
      for (let s = 0; s <= k; s += 1.6 / d) dots.push([a[0] + (b[0] - a[0]) * s, a[1] + (b[1] - a[1]) * s]);
      if (want <= d) { at = [a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k]; ang = Math.atan2(b[1] - a[1], b[0] - a[0]); break; }
      want -= d; at = b; ang = Math.atan2(b[1] - a[1], b[0] - a[0]);
    }
    routeDots(dots);
    for (const p of [path[0], path[path.length - 1]]) stamp(p, true);
    return { P, at, ang };
  }

  // =====================================================================
  // the trips, seen from the side
  // =====================================================================
  function scroller(W, t, speed, gap, y, fn) {
    const off = t * speed;
    for (let i = Math.floor(off / gap) - 1; i < Math.floor(off / gap) + W / gap + 2; i++) fn(i * gap - off, y, i);
  }
  function roadTrip(W, H, t, kind) {
    sky(W, 140, [[0, '#5e8ccc'], [0.7, '#a8cbe8'], [1, '#f8c88a']], 13);
    clouds(W, t * 30, t, 30, 4);
    inkRange(W, t * 40, 0.1, 112, 20, '#6a7a8a', 41, 0.5);
    mist(W, 112, 8, 0.6);
    hills(W, t * 40, 0.4, 126, 5, '#9cbc84', 9, 'rgba(70,90,50,.6)');
    scroller(W, t, 60, 22, 140, (x, y, i) => pine(x, y, 34 + hash(i) * 18, '#2f6a45', '#25573a'));
    R(-5, 140, W + 10, 45, '#55545c');
    line(-5, 140, W + 5, 140, INK, 0.9);
    scroller(W, t, 160, 24, 0, (x) => { line(x, 136, x, 142, '#c8ccd0', 1); });
    line(-5, 137.6, W + 5, 137.6, '#d8dce0', 1);
    scroller(W, t, 160, 60, 0, (x) => box(x, 157.4, 26, 1.8, '#f0f0ea', 0, Math.round(x), 0.4));
    // speed lines
    for (let i = 0; i < 6; i++) { const y = 146 + hash(i + 7) * 30, x = W - ((t * 300 + hash(i) * W * 2) % (W * 1.6)); line(x, y, x + 18 + hash(i + 3) * 20, y, 'rgba(255,255,255,.35)', 0.6); }
  }
  function skyline(W, base, t, alpha) {
    c.save(); c.globalAlpha = alpha;
    for (let i = 0; i < 26; i++) {
      const h = 14 + hash(i + 300) * 26, x = i * 17.5;
      box(x, base - h, 16, h + 2, '#3a4560', 0.6, i, 0.5);
      for (let k = 0; k < 6; k++) if (hash(i * 7 + k) > 0.5) box(x + 3 + (k % 3) * 4, base - h + 4 + Math.floor(k / 3) * 6, 2, 2.4, '#ffd27a', 0, k, 0.3);
    }
    // the TV tower
    const tx = W * 0.6;
    limb([[tx, base], [tx, base - 80]], '#4a5570', 2.4, 0.6);
    circle(tx, base - 62, 6.2, '#4a5570', 0.7); line(tx - 6, base - 62, tx + 6, base - 62, '#ffd27a', 0.8);
    line(tx, base - 80, tx, base - 97, INK, 0.9);
    if ((t * 2 | 0) % 2) circle(tx, base - 97, 0.9, '#ff3030');
    c.restore();
  }
  function trainTrip(W, H, t, rider) {
    sky(W, 130, [[0, '#6c96cf'], [0.7, '#b0d0ea'], [1, '#ffcf98']], 17);
    clouds(W, t * 25, t, 32, 4);
    inkRange(W, t * 30, 0.1, 104, 18, '#6a7a8a', 51, 0.45);
    mist(W, 104, 7, 0.6);
    R(-5, 108, W + 10, 34, '#b2c886');
    c.save(); c.beginPath(); c.rect(-5, 108, W + 10, 34); c.clip(); wcFill(0, 108, W, 34, '#7a9a50', 3, 0.12); c.restore();
    line(-5, 108, W + 5, 108, 'rgba(70,90,50,.6)', 0.6);
    scroller(W, t, 30, 30, 126, (x, y, i) => pine(x, y, 30 + hash(i) * 14));
    R(-5, 140, W + 10, 45, '#7a7262');
    line(-5, 140, W + 5, 140, INK, 0.8);
    scroller(W, t, 200, 12, 0, (x) => box(x, 145, 3.2, 10, '#5a4430', 0.4, Math.round(x), 0.4));
    line(-5, 146.5, W + 5, 146.5, '#9aa0a6', 1.4); line(-5, 152.5, W + 5, 152.5, '#9aa0a6', 1.4);
    scroller(W, t, 200, 90, 0, (x) => { limb([[x, 146], [x, 96]], '#5a5e66', 1.4, 0.5); line(x - 11, 97, x + 11, 97, '#5a5e66', 1); });
    line(-5, 100, W + 5, 100, '#3a3a40', 0.6);
    train(W * 0.62 + Math.sin(t * 6) * 0.5, 152, t, rider);
  }
  function hbf(W, H, t) {
    sky(W, 150, [[0, '#14204a'], [0.6, '#3a3d70'], [1, '#7a5a7a']], 19);
    for (let i = 0; i < 30; i++) circle(hash(i + 7) * W, TOP + hash(i + 8) * 120, 0.4, '#fff6d8');
    skyline(W, 150, t, 0.55);
    // the big glass hall
    c.save();
    c.beginPath(); c.arc(W / 2, 172, 88, Math.PI, 0); c.closePath();
    c.fillStyle = 'rgba(170,210,240,.18)'; c.fill();
    c.restore();
    for (let k = 0; k < 6; k++) { c.beginPath(); c.arc(W / 2, 172, 70 + k * 3.6, Math.PI, 0); c.strokeStyle = k % 2 ? 'rgba(200,230,250,.5)' : INK; c.lineWidth = k % 2 ? 0.5 : 0.8; c.stroke(); }
    for (let a = 0; a <= 12; a++) { const an = Math.PI + a / 12 * Math.PI; line(W / 2 + Math.cos(an) * 70, 172 + Math.sin(an) * 70, W / 2 + Math.cos(an) * 88, 172 + Math.sin(an) * 88, 'rgba(51,36,27,.6)', 0.5); }
    glowAdd(W / 2, 140, 70, '#ffe0a0', 0.18);
    box(W / 2 - 24, 100, 48, 9, '#1d3a6a', 0.7, 3, 1.4);
    c.save(); c.font = '800 5.6px "M PLUS Rounded 1c", sans-serif'; c.fillStyle = '#ffffff'; c.textAlign = 'center'; c.fillText('Berlin Hbf', W / 2, 106.4); c.restore();
    R(-5, 150, W + 10, 35, '#4a4a52'); line(-5, 150, W + 5, 150, INK, 0.9);
    for (let x = 0; x < W; x += 20) line(x, 156, x + 10, 156, '#f0e080', 0.8);
  }
  function airport(W, H, t, planeX, planeY) {
    sky(W, 150, [[0, '#1e2a5a'], [0.55, '#5a4a80'], [0.85, '#e0807a'], [1, '#ffc890']], 23);
    for (let i = 0; i < 30; i++) circle(hash(i + 7) * W, TOP + hash(i + 8) * 110, 0.4, '#fff6d8');
    inkRange(W, 0, 0, 132, 14, '#3a3050', 61, 0.6);
    mist(W, 134, 6, 0.4);
    // the terminal
    box(18, 108, 150, 42, '#3a4258', 0.9, 5, 1);
    within(() => wcFill(18, 108, 150, 42, '#7a86a8', 5, 0.1));
    for (let i = 0; i < 12; i++) box(24 + i * 12, 116, 9, 12, '#ffd27a', 0.5, i, 0.6);
    glowAdd(90, 122, 60, '#ffcf80', 0.15);
    box(70, 98, 40, 9, '#f4f0e6', 0.7, 6, 1.4);
    c.save(); c.font = '900 7px "M PLUS Rounded 1c", sans-serif'; c.fillStyle = '#c41e1e'; c.textAlign = 'center'; c.fillText('BER', 90, 105); c.restore();
    R(-5, 150, W + 10, 35, '#2e2e36'); line(-5, 150, W + 5, 150, INK, 0.9);
    for (let x = 0; x < W; x += 16) { box(x, 158, 8, 1.4, '#f0f0ea', 0, x, 0.3); circle(x + 4, 151.6, 0.8, '#7dff9a'); glowAdd(x + 4, 151.6, 3, '#7dff9a', 0.5); }
    plane(planeX, planeY, 1, t, planeY < 140 ? 0.12 : 0);
  }
  // the last bit of the trip: through Berlin at night
  function cityNight(W, H, t, who) {
    sky(W, 140, [[0, '#0e1838'], [0.7, '#2a2a5a'], [1, '#5a3a5a']], 29);
    for (let i = 0; i < 30; i++) circle(hash(i + 70) * W, TOP + hash(i + 71) * 150, 0.4, '#fff6d8');
    skyline(W, 140, t, 1);
    R(-5, 140, W + 10, 45, '#33323a'); line(-5, 140, W + 5, 140, INK, 0.9);
    scroller(W, t, 120, 40, 0, (x) => box(x, 160, 20, 1.4, '#e8e8e2', 0, Math.round(x), 0.3));
    if (who === 'VITO') taxi(W * 0.5, 160, 1, t); else amarok(W * 0.5, 160, 1, t, who);
  }

  return {
    use, setScale, paper, R, glow, rgba, hash, LOOK, friend, friendTop, person,
    campLion, altengrabow, lehnin, street, outdoorTables, streetFront, STREET, treesAt,
    car, taxi, amarok, tram, scooter, plane, train,
    inside, insideTop, IN, dish, menuCard,
    europeMap, brandenburgMap, planeIcon, mapIcon, roadTrip, skyline, trainTrip, hbf, airport, cityNight, neonText,
  };
})();
