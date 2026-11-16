// Туса У Берліні: all the pixel art, drawn in code.
// Every function draws on a small "logical" canvas (180 px high) that the
// game scales up, so 1 unit here is one fat pixel on screen.
'use strict';
const ART = (() => {
  let c = null;
  const use = ctx => { c = ctx; };
  const R = (x, y, w, h, col) => { c.fillStyle = col; c.fillRect(Math.round(x), Math.round(y), w, h); };
  function rgba(hex, a) {
    const n = parseInt(hex.slice(1), 16);
    return 'rgba(' + (n >> 16 & 255) + ',' + (n >> 8 & 255) + ',' + (n & 255) + ',' + a + ')';
  }
  function glow(x, y, r, col, a) {
    const g = c.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, rgba(col, a)); g.addColorStop(1, rgba(col, 0));
    c.fillStyle = g; c.fillRect(x - r, y - r, r * 2, r * 2);
  }
  // repeatable "random" numbers so walls and trees look the same every frame
  function hash(n) { n = (n << 13) ^ n; return ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 0x7fffffff; }

  // =====================================================================
  // the three friends
  // =====================================================================
  const LOOK = {
    VITO:    { skin: '#d9a179', skin2: '#b9825c', hair: '#141414', top: '#33506d', top2: '#26405a', legs: '#23293a', shoe: '#111111', w: 10 },
    VIKA:    { skin: '#f3cdb0', skin2: '#e0ad8e', hair: '#f6d873', hair2: '#d8b04a', top: '#e46f90', top2: '#c85878', legs: '#3d3756', shoe: '#5b2a3c', w: 13 },
    OLEKSIY: { skin: '#e6b28c', skin2: '#c9906a', hair: '#5a3a22', top: '#5a7536', top2: '#47602a', legs: '#2b2b2b', shoe: '#3a2a1a', hat: '#476a2c', hat2: '#36521f', must: '#5a3417', w: 10 },
    WAITER:  { skin: '#e9be98', skin2: '#c99a76', hair: '#1b1b1b', top: '#161616', top2: '#0e0e0e', legs: '#1d1d1d', shoe: '#0b0b0b', w: 10, apron: '#ecebe4' },
  };

  // side view; (x, y) is where the feet touch the ground
  // o: { dir, phase, moving, anim, laugh, blush, t, alpha, talk }
  function friend(who, x, y, o) {
    o = o || {};
    const L = LOOK[who] || LOOK.VITO;
    const dir = o.dir || 1, t = o.t || 0;
    if (o.anim === 'rofl') {
      c.save();
      c.translate(Math.round(x), Math.round(y - 3));
      c.rotate(-dir * Math.PI / 2 + Math.sin(t * 22) * 0.12);
      c.translate(0, Math.sin(t * 30) * 0.8 + 14);
      body(L, who, 0, 0, dir, { ...o, anim: 'laugh', laugh: 3 });
      c.restore();
      return;
    }
    let bob = 0;
    if (o.moving) bob = Math.abs(Math.sin((o.phase || 0) * Math.PI)) > 0.5 ? -1 : 0;
    else if (o.laugh >= 2) bob = Math.round(Math.sin(t * 26)) ;
    else bob = Math.sin(t * 2.2) > 0.85 ? -0 : 0;
    body(L, who, x, y + bob, dir, o);
  }

  function body(L, who, x, y, dir, o) {
    const P = (dx, dy, w, h, col) => R(dir > 0 ? x + dx : x - dx - w, y + dy, w, h, col);
    const t = o.t || 0, w = L.w, hw = Math.floor(w / 2);
    const walk = o.moving ? Math.sin((o.phase || 0) * Math.PI * 2) : 0;
    // shadow
    c.fillStyle = 'rgba(0,0,0,.28)'; c.fillRect(Math.round(x - hw - 1), Math.round(y), w + 2, 2);
    // legs
    const lA = Math.round(walk * 2), lB = -lA;
    P(-3 + lA, -8, 3, 7, L.legs); P(-3 + lA, -1, 4, 1, L.shoe);
    P(1 + lB, -8, 3, 7, L.legs); P(1 + lB, -1, 4, 1, L.shoe);
    // torso / coat
    if (who === 'OLEKSIY') {
      P(-hw, -20, w, 15, L.top); P(-hw, -20, 2, 15, L.top2);
      P(-1, -19, 1, 13, L.top2); P(1, -16, 1, 1, '#c9b46a'); P(1, -12, 1, 1, '#c9b46a'); // buttons
    } else if (who === 'VIKA') {
      P(-hw, -19, w, 11, L.top); P(-hw + 1, -20, w - 2, 1, L.top); P(-hw, -19, 2, 11, L.top2);
      P(-hw + 1, -9, w - 2, 1, L.top2);
    } else if (who === 'WAITER') {
      P(-hw, -20, w, 12, L.top); P(-hw + 2, -16, w - 3, 10, L.apron);
    } else {
      P(-hw, -20, w, 12, L.top); P(-hw, -20, 2, 12, L.top2); P(-1, -20, 3, 2, L.top2); // hoodie
    }
    // arms
    const swing = Math.round(walk * 2);
    if (o.anim === 'hug') {
      P(hw - 1, -17, 7, 3, L.top); P(hw + 6, -17, 2, 3, L.skin);
      P(-hw - 1, -17, 2, 3, L.top2);
    } else if (o.anim === 'wave' || o.anim === 'heart') {
      P(hw - 1, -25, 3, 8, L.top); P(hw - 1, -27, 3, 2, L.skin);
      P(-hw - 1, -18 - swing, 2, 8, L.top2);
    } else if (o.laugh >= 2) {
      P(hw - 2, -16, 3, 3, L.top); P(hw - 1, -14, 3, 2, L.skin);  // holding the belly
      P(-hw - 1, -18, 2, 7, L.top2); P(-hw - 1, -11, 2, 2, L.skin);
    } else {
      P(hw - 1, -18 + swing, 2, 8, L.top2); P(hw - 1, -10 + swing, 2, 2, L.skin);
      P(-hw - 1, -18 - swing, 2, 8, L.top2); P(-hw - 1, -10 - swing, 2, 2, L.skin);
    }
    // head
    const hy = who === 'VIKA' ? -28 : -28;
    P(-4, hy, 8, 8, L.skin); P(-4, hy + 6, 8, 2, L.skin); P(-4, hy, 1, 8, L.skin2);
    P(4, hy + 3, 1, 2, L.skin2); // nose
    const laugh = o.laugh || 0;
    // eyes
    if (laugh >= 1 || o.anim === 'hug') { P(0, hy + 3, 2, 1, '#1a1a1a'); P(3, hy + 3, 1, 1, '#1a1a1a'); }
    else if ((t * 1000 | 0) % 3700 < 120) { P(0, hy + 3, 2, 1, '#1a1a1a'); P(3, hy + 3, 1, 1, '#1a1a1a'); }
    else { P(1, hy + 2, 1, 2, '#1a1a1a'); P(3, hy + 2, 1, 2, '#1a1a1a'); }
    if (o.blush || laugh >= 2) { P(-1, hy + 5, 2, 1, '#ff8aa0'); P(3, hy + 5, 1, 1, '#ff8aa0'); }
    // mouth
    if (who === 'VITO') {
      if (laugh >= 1 || o.talk) { P(0, hy + 5, 4, 2, '#5a1010'); P(0, hy + 5, 4, 1, '#ffffff'); }
      else { P(-1, hy + 5, 5, 1, '#ffffff'); P(-1, hy + 6, 5, 1, '#7a2a1a'); P(-1, hy + 5, 1, 1, '#7a2a1a'); } // the big smile
    } else if (laugh >= 1 || o.talk) P(1, hy + 5, 3, 2, '#5a1010');
    else P(1, hy + 6, 3, 1, '#8a3a2a');
    // tears of laughter
    if (laugh >= 3) {
      const k = (t * 3) % 1;
      P(4 + k * 3, hy + 3 + k * 6, 1, 1, '#7fd3ff'); P(-1 - k * 2, hy + 4 + k * 5, 1, 1, '#7fd3ff');
    }
    // hair and hats
    if (who === 'VITO') {
      P(-4, hy - 1, 8, 3, L.hair); P(-5, hy, 2, 5, L.hair); P(3, hy - 1, 2, 2, L.hair);
      const sway = Math.round(Math.sin(t * 3) * 0.6 + (o.moving ? walk : 0));
      P(-6, hy + 1, 2, 3, L.hair); P(-7 + sway, hy + 3, 2, 4, L.hair); P(-7 + sway, hy + 7, 2, 4, L.hair); P(-6 + sway, hy + 11, 2, 2, L.hair); // long ponytail
      P(-6, hy + 2, 1, 1, '#c33'); // hair tie
    } else if (who === 'VIKA') {
      P(-6, hy - 3, 12, 4, L.hair); P(-7, hy - 1, 3, 7, L.hair); P(4, hy - 2, 3, 4, L.hair);
      P(-5, hy - 4, 9, 1, L.hair); P(-6, hy + 5, 3, 2, L.hair2); P(-3, hy - 3, 4, 1, '#fff2b0');
      P(-7, hy + 1, 1, 3, L.hair2);
    } else if (who === 'OLEKSIY') {
      P(-4, hy + 1, 1, 4, L.hair);
      P(-6, hy, 12, 1, L.hat2); P(-4, hy - 4, 8, 4, L.hat); P(-3, hy - 5, 6, 1, L.hat); P(-4, hy - 1, 8, 1, L.hat2);
      // handlebar mustache with curled-up tips
      P(1, hy + 5, 4, 1, L.must); P(0, hy + 4, 1, 1, L.must); P(5, hy + 4, 1, 1, L.must); P(-1, hy + 3, 1, 1, L.must);
      if (laugh >= 1 || o.talk) P(2, hy + 6, 2, 1, '#5a1010');
    } else if (who === 'WAITER') {
      P(-4, hy - 1, 8, 3, L.hair); P(-6, hy - 2, 3, 3, L.hair); // bun
    }
  }

  // top-down view for inside the restaurant
  function friendTop(who, x, y, o) {
    o = o || {};
    const L = LOOK[who] || LOOK.VITO, t = o.t || 0;
    const a = o.angle || 0, ca = Math.cos(a), sa = Math.sin(a);
    if (o.anim === 'rofl') {
      // lying on the floor, rolling from side to side, legs kicking
      const roll = Math.sin(t * 9), kick = Math.sin(t * 17);
      c.save(); c.translate(x + Math.cos(a + 1.57) * roll * 2.2, y + Math.sin(a + 1.57) * roll * 2.2); c.rotate(a + roll * 0.3);
      c.fillStyle = 'rgba(0,0,0,.3)'; c.beginPath(); c.ellipse(1, 2, 12, 5, 0, 0, 7); c.fill();
      R(-13, -3 + kick, 7, 2, L.legs); R(-13, 1 - kick, 7, 2, L.legs); R(-14, -3 + kick, 2, 2, L.shoe); R(-14, 1 - kick, 2, 2, L.shoe);
      c.fillStyle = L.top; c.beginPath(); c.ellipse(-2, 0, 6, who === 'VIKA' ? 5 : 4.2, 0, 0, 7); c.fill();
      c.fillStyle = L.top2 || L.top; c.beginPath(); c.ellipse(-2, roll * 1.5, 4, 1.6, 0, 0, 7); c.fill();
      R(0, -7 - kick, 2, 3, L.skin); R(0, 4 + kick, 2, 3, L.skin); // arms holding the belly / flailing
      c.fillStyle = who === 'OLEKSIY' ? L.hat : L.hair; c.beginPath(); c.arc(6, 0, who === 'VIKA' ? 4.8 : 4, 0, 7); c.fill();
      R(6 + roll * 1.5, -1.5 + roll * 1.5, 3, 3, L.skin);
      if (who === 'OLEKSIY') R(8 + roll * 1.5, -1 + roll * 1.5, 2, 1, L.must);
      if (who === 'VITO') { c.strokeStyle = L.hair; c.lineWidth = 1.4; c.beginPath(); c.moveTo(9, 0); c.lineTo(12, roll * 3); c.stroke(); }
      const k = (t * 3) % 1;
      R(9 + k * 4, -3 - k * 3, 1, 1, '#7fd3ff'); R(9 + k * 4, 3 + k * 3, 1, 1, '#7fd3ff'); // tears of laughter
      c.restore();
      return;
    }
    const shake = (o.laugh || 0) >= 2 ? Math.sin(t * 30) * 0.7 : 0;
    x += shake;
    c.fillStyle = 'rgba(0,0,0,.3)'; c.beginPath(); c.ellipse(x + 1, y + 2, 7, 5, 0, 0, 7); c.fill();
    // shoulders
    c.fillStyle = L.top; c.beginPath(); c.ellipse(x, y, who === 'VIKA' ? 7.5 : 6.5, 4.5, a + Math.PI / 2, 0, 7); c.fill();
    if (who === 'WAITER') { c.fillStyle = L.apron; c.beginPath(); c.ellipse(x + ca * 2, y + sa * 2, 3, 3.6, a + Math.PI / 2, 0, 7); c.fill(); }
    // arms
    const swing = o.moving ? Math.sin((o.phase || 0) * Math.PI * 2) * 2 : 0;
    const px = -sa, py = ca;
    R(x + px * 6 + ca * swing - 1, y + py * 6 + sa * swing - 1, 2, 2, L.skin);
    R(x - px * 6 - ca * swing - 1, y - py * 6 - sa * swing - 1, 2, 2, L.skin);
    // head
    if (who === 'VIKA') {
      c.fillStyle = L.hair2; c.beginPath(); c.arc(x, y, 5.2, 0, 7); c.fill();
      c.fillStyle = L.hair; for (let k = 0; k < 6; k++) { c.beginPath(); c.arc(x + Math.cos(k) * 2.8, y + Math.sin(k) * 2.8, 2.6, 0, 7); c.fill(); }
      R(x + ca * 3 - 1, y + sa * 3 - 1, 2, 2, L.skin);
    } else if (who === 'OLEKSIY') {
      c.fillStyle = L.hat2; c.beginPath(); c.arc(x, y, 5.5, 0, 7); c.fill();
      c.fillStyle = L.hat; c.beginPath(); c.arc(x, y, 3.6, 0, 7); c.fill();
      // mustache tips poke out in front
      const fx = x + ca * 4.5, fy = y + sa * 4.5;
      R(fx + px * 3 - 0.5, fy + py * 3 - 0.5, 1, 1, L.must); R(fx - px * 3 - 0.5, fy - py * 3 - 0.5, 1, 1, L.must);
      R(fx - 0.5, fy - 0.5, 1, 1, L.skin);
    } else {
      c.fillStyle = L.hair; c.beginPath(); c.arc(x, y, 4.2, 0, 7); c.fill();
      R(x + ca * 3.2 - 1, y + sa * 3.2 - 1, 2, 2, L.skin);
      if (who === 'VITO') { // ponytail behind
        c.strokeStyle = L.hair; c.lineWidth = 1.6; c.beginPath();
        c.moveTo(x - ca * 3, y - sa * 3); c.quadraticCurveTo(x - ca * 7 + px * Math.sin(t * 3), y - sa * 7 + py * Math.sin(t * 3), x - ca * 9, y - sa * 9); c.stroke();
        R(x - ca * 4 - 0.5, y - sa * 4 - 0.5, 1, 1, '#c33');
      }
      if (who === 'WAITER') { c.fillStyle = L.hair; c.beginPath(); c.arc(x - ca * 4, y - sa * 4, 2, 0, 7); c.fill(); }
    }
  }

  // generic people: soldiers, pedestrians, staff
  function person(x, y, p, phase, moving, dir, t) {
    const P = (dx, dy, w, h, col) => R(dir > 0 ? x + dx : x - dx - w, y + dy, w, h, col);
    const walk = moving ? Math.sin(phase * Math.PI * 2) : 0, lA = Math.round(walk * 2);
    c.fillStyle = 'rgba(0,0,0,.25)'; c.fillRect(Math.round(x - 5), Math.round(y), 10, 2);
    P(-3 + lA, -8, 3, 8, p.legs); P(1 - lA, -8, 3, 8, p.legs);
    P(-3 + lA, -1, 4, 1, p.shoe || '#1a1a1a'); P(1 - lA, -1, 4, 1, p.shoe || '#1a1a1a');
    P(-4, -20, p.w || 9, 12, p.top);
    if (p.camo) { for (let i = 0; i < 6; i++) P(-4 + ((i * 3 + p.seed) % 8), -19 + ((i * 5 + p.seed) % 10), 2, 2, p.camo); for (let i = 0; i < 3; i++) P(-3 + ((i * 2 + p.seed) % 6), -6 + (i % 2), 2, 2, p.camo); }
    if (p.salute) { P(3, -25, 2, 7, p.top); P(4, -27, 2, 2, p.skin); }
    else { P(4, -18 + Math.round(walk * 2), 2, 8, p.top); P(4, -10 + Math.round(walk * 2), 2, 2, p.skin); }
    if (p.bag) P(5, -11 + Math.round(walk * 2), 4, 5, p.bag);
    P(-4, -28, 8, 8, p.skin); P(1, -26, 1, 2, '#1a1a1a'); P(3, -26, 1, 2, '#1a1a1a');
    if (p.hat === 'helmet') { P(-5, -30, 10, 4, p.hatc || '#4d5a32'); P(-5, -27, 10, 1, '#3a4426'); }
    else if (p.hat === 'beret') { P(-4, -30, 8, 2, p.hatc || '#7a1f2b'); P(-5, -29, 3, 2, p.hatc || '#7a1f2b'); }
    else if (p.hat === 'cap') { P(-4, -30, 8, 3, p.hatc); P(2, -28, 4, 1, p.hatc); }
    else if (p.hat === 'beanie') { P(-4, -31, 8, 4, p.hatc); P(-1, -32, 2, 1, p.hatc); }
    else { P(-4, -29, 8, 3, p.hair); if (p.long) P(-5, -27, 2, 7, p.hair); }
    if (p.dog) { // a little dog on a lead
      const dx = dir > 0 ? x + 12 : x - 12, by = y, k = Math.round(walk);
      R(dx - 4, by - 5, 8, 3, p.dog); R(dx + (dir > 0 ? 3 : -5), by - 7, 3, 3, p.dog);
      R(dx - 4 + k, by - 2, 1, 2, p.dog); R(dx + 2 - k, by - 2, 1, 2, p.dog);
      c.strokeStyle = 'rgba(30,30,30,.7)'; c.lineWidth = 0.5; c.beginPath(); c.moveTo(x + (dir > 0 ? 5 : -5), y - 10); c.lineTo(dx, by - 5); c.stroke();
    }
  }

  // =====================================================================
  // skies and grounds
  // =====================================================================
  function sky(W, H, top, bottom, groundY) {
    const g = c.createLinearGradient(0, 0, 0, groundY);
    g.addColorStop(0, top); g.addColorStop(1, bottom);
    c.fillStyle = g; c.fillRect(0, 0, W, groundY);
  }
  function clouds(W, camX, t, col, y0) {
    for (let i = 0; i < 6; i++) {
      const x = ((i * 97 + t * 3 - camX * 0.2) % (W + 120) + W + 120) % (W + 120) - 60;
      const y = y0 + (i * 13) % 30;
      R(x, y, 34, 5, col); R(x + 6, y - 3, 18, 3, col); R(x + 20, y - 2, 10, 2, col);
    }
  }
  function pine(x, y, h, dark) {
    R(x - 1, y - h * 0.3, 2, h * 0.3, '#3d2b1c');
    for (let i = 0; i < 4; i++) {
      const ty = y - h * 0.25 - i * h * 0.19, w = (4 - i) * h * 0.11 + 2;
      c.fillStyle = i % 2 ? (dark || '#1f4a2e') : '#2a5c38';
      c.beginPath(); c.moveTo(x - w, ty); c.lineTo(x + w, ty); c.lineTo(x, ty - h * 0.3); c.closePath(); c.fill();
    }
  }
  function fence(x0, x1, y) {
    for (let x = x0; x < x1; x += 10) R(x, y - 14, 1, 14, '#8b8f94');
    R(x0, y - 13, x1 - x0, 1, '#a3a8ad'); R(x0, y - 8, x1 - x0, 1, '#a3a8ad');
    for (let x = x0; x < x1; x += 3) R(x, y - 16 + (x % 2), 1, 1, '#c0c4c8'); // barbed wire
  }
  function flag(x, y, cols, t) {
    R(x, y - 50, 1, 50, '#d0d0d0');
    for (let i = 0; i < cols.length; i++) for (let k = 0; k < 4; k++) {
      const wav = Math.round(Math.sin(t * 4 + k * 0.6 + i) * 0.8);
      R(x + 1 + i * 5 + k, y - 50 + wav + 0, 1, 9, cols[i]);
    }
  }

  // ----- Camp Lion, Romania: white container buildings with an aircon at every window
  function campLion(W, H, camX, t) {
    sky(W, H, '#5b8fd1', '#f7c98b', 150);
    c.fillStyle = '#ffe6a8'; c.beginPath(); c.arc(W * 0.8 - camX * 0.05, 40, 10, 0, 7); c.fill();
    clouds(W, camX, t, 'rgba(255,255,255,.75)', 18);
    // far hills
    c.fillStyle = '#9eaf7c';
    c.beginPath(); c.moveTo(0, 120); for (let x = 0; x <= W; x += 8) c.lineTo(x, 112 - Math.sin((x + camX * 0.3) * 0.02) * 8); c.lineTo(W, 150); c.lineTo(0, 150); c.fill();
    c.save(); c.translate(-Math.round(camX), 0);
    // container blocks: two levels
    const block = (bx, n) => {
      for (let lvl = 0; lvl < 2; lvl++) for (let i = 0; i < n; i++) {
        const x = bx + i * 34, y = 150 - (lvl + 1) * 30;
        R(x, y, 34, 30, '#eef0ee'); R(x, y, 34, 1, '#ffffff'); R(x, y + 29, 34, 1, '#c9cdcc'); R(x + 33, y, 1, 30, '#c9cdcc');
        for (let k = 3; k < 33; k += 3) R(x + k, y + 2, 1, 26, '#e0e3e1');
        // window + aircon
        R(x + 6, y + 7, 12, 10, '#5c7a94'); R(x + 6, y + 7, 12, 1, '#9fb8cc'); R(x + 11, y + 7, 1, 10, '#dfe6ea');
        R(x + 21, y + 9, 9, 7, '#d6d9dc'); R(x + 21, y + 15, 9, 1, '#a8adb1');
        c.fillStyle = '#9aa1a6'; c.beginPath(); c.arc(x + 25.5, y + 12, 2.6, 0, 7); c.fill();
        c.strokeStyle = '#6d7479'; c.lineWidth = 0.6; c.beginPath(); const sp = t * 12 + i;
        c.moveTo(x + 25.5 + Math.cos(sp) * 2.3, y + 12 + Math.sin(sp) * 2.3); c.lineTo(x + 25.5 - Math.cos(sp) * 2.3, y + 12 - Math.sin(sp) * 2.3); c.stroke();
        if ((t * 1.5 + i) % 3 < 0.2) R(x + 30, y + 16 + ((t * 20) % 4), 1, 1, '#88c'); // a drip
      }
      // walkway and stairs to the second level
      R(bx, 119, n * 34, 2, '#7c8186'); for (let k = 0; k < n * 34; k += 4) R(bx + k, 115, 1, 4, '#9ba0a5'); R(bx, 115, n * 34, 1, '#9ba0a5');
      for (let s = 0; s < 8; s++) R(bx + n * 34 + s * 3, 121 + s * 3.6, 4, 1, '#7c8186');
      R(bx + n * 34, 121, 1, 29, '#7c8186'); R(bx + n * 34 + 24, 121, 1, 29, '#7c8186');
    };
    block(20, 4); block(260, 5);
    // Vito's door
    R(70, 132, 10, 18, '#c8cdd0'); R(71, 133, 8, 16, '#dfe3e6'); R(77, 141, 1, 2, '#555');
    R(66, 130, 18, 2, '#9aa1a6'); // little roof over the door
    // sign
    R(150, 100, 70, 16, '#2f4a2a'); R(151, 101, 68, 14, '#3b5a33');
    flag(232, 150, ['#002b7f', '#fcd116', '#ce1126'], t);
    flag(244, 150, ['#003399', '#003399', '#003399'], t + 1);
    fence(-40, 18, 150);
    // gravel
    R(-200, 150, 1200, 30, '#b8ad97');
    for (let i = 0; i < 260; i++) R(-200 + hash(i) * 1200, 151 + hash(i + 7) * 28, 1, 1, hash(i + 3) > 0.5 ? '#9c917d' : '#cdc3ae');
    R(-200, 150, 1200, 1, '#a39882');
    c.restore();
    // the sign text is drawn by the game in screen space (crisper)
    return { signX: 151 - camX, signY: 101, signW: 68, signH: 14 };
  }

  // ----- Altengrabow: a flat 3-storey building
  function altengrabow(W, H, camX, t) {
    sky(W, H, '#6f9fd8', '#ffd7a0', 150);
    clouds(W, camX, t, 'rgba(255,255,255,.7)', 14);
    c.save(); c.translate(-Math.round(camX * 0.5), 0);
    for (let i = 0; i < 18; i++) pine(i * 30 + 10, 150, 46 + (i % 3) * 10, '#244f33');
    c.restore();
    c.save(); c.translate(-Math.round(camX), 0);
    const bx = 20, bw = 230, by = 150 - 78;
    R(bx, by, bw, 78, '#d9d2c3'); R(bx, by, bw, 3, '#b9b2a3'); R(bx - 2, by - 2, bw + 4, 3, '#8f897d');
    for (let f = 0; f < 3; f++) for (let i = 0; i < 10; i++) {
      const wx = bx + 8 + i * 22, wy = by + 8 + f * 24;
      if (f === 0 && i === 2) continue;
      R(wx, wy, 12, 13, '#5f7e9b'); R(wx, wy, 12, 1, '#a9c0d4'); R(wx + 6, wy, 1, 13, '#e8e8e8'); R(wx - 1, wy + 13, 14, 1, '#bdb6a8');
      if (hash(f * 10 + i) > 0.7) R(wx + 1, wy + 1, 5, 12, '#f4e3b5');
    }
    for (let f = 1; f < 3; f++) R(bx, by + f * 24 + 2, bw, 1, '#c9c2b3');
    // entrance (Vika's door) with a little canopy
    R(bx + 52, 128, 14, 22, '#6b4a33'); R(bx + 53, 129, 12, 21, '#80583c'); R(bx + 63, 139, 1, 2, '#ddd');
    R(bx + 48, 125, 22, 3, '#7b7f84');
    R(bx + 36, 112, 46, 10, '#2f4a6b'); R(bx + 37, 113, 44, 8, '#3a5a80'); // sign plate
    flag(270, 150, ['#000000', '#dd0000', '#ffce00'], t);
    // a military truck parked
    R(300, 128, 46, 16, '#56603f'); R(300, 120, 16, 12, '#4b5537'); R(302, 122, 10, 6, '#7b97ad');
    for (const wx of [308, 334]) { c.fillStyle = '#1a1a1a'; c.beginPath(); c.arc(wx, 146, 4, 0, 7); c.fill(); R(wx - 1, 145, 2, 2, '#555'); }
    R(-200, 150, 1200, 30, '#7a8a52');
    R(-200, 150, 1200, 2, '#62723f');
    for (let i = 0; i < 160; i++) R(-200 + hash(i + 50) * 1200, 152 + hash(i + 9) * 26, 1, 2, '#93a565');
    R(bx + 40, 150, 40, 30, '#9c9586');
    c.restore();
    return { signX: bx + 37 - camX, signY: 113, signW: 44, signH: 8 };
  }

  // ----- Lehnin: two-storey dark red buildings with pointed roofs, pine trees
  function lehnin(W, H, camX, t) {
    sky(W, H, '#7aa6d6', '#f9c38d', 150);
    clouds(W, camX, t, 'rgba(255,255,255,.65)', 20);
    c.save(); c.translate(-Math.round(camX * 0.4), 0);
    for (let i = 0; i < 20; i++) pine(i * 26 + 6, 150, 58 + (i % 4) * 9, '#1d4529');
    c.restore();
    c.save(); c.translate(-Math.round(camX), 0);
    const house = (hx, hw, door) => {
      const top = 150 - 46;
      R(hx, top, hw, 46, '#7a2e24');
      for (let y = top + 3; y < 150; y += 4) for (let x = hx + ((y / 4) % 2) * 3; x < hx + hw; x += 7) R(x, y, 6, 1, '#6a261e');
      c.fillStyle = '#3c3c42'; c.beginPath(); c.moveTo(hx - 4, top); c.lineTo(hx + hw / 2, top - 26); c.lineTo(hx + hw + 4, top); c.closePath(); c.fill();
      c.fillStyle = '#4c4c55'; c.beginPath(); c.moveTo(hx + 2, top); c.lineTo(hx + hw / 2, top - 22); c.lineTo(hx + hw - 2, top); c.closePath(); c.fill();
      R(hx + hw / 2 - 4, top - 14, 8, 7, '#e8e2d2'); R(hx + hw / 2 - 3, top - 13, 6, 5, '#5f7894');
      for (let f = 0; f < 2; f++) for (let i = 0; i < Math.floor(hw / 18); i++) {
        const wx = hx + 6 + i * 18, wy = top + 6 + f * 20;
        if (door && f === 1 && Math.abs(wx - door) < 14) continue;
        R(wx - 1, wy - 1, 12, 14, '#e8e2d2'); R(wx, wy, 10, 12, '#5f7894'); R(wx + 5, wy, 1, 12, '#e8e2d2'); R(wx, wy + 6, 10, 1, '#e8e2d2');
      }
      if (door) { R(door, 128, 12, 22, '#e8e2d2'); R(door + 1, 129, 10, 21, '#3b4a33'); R(door + 9, 139, 1, 2, '#c9b46a'); R(door - 2, 126, 16, 2, '#3c3c42'); }
    };
    house(10, 120, 56); house(170, 100, 0);
    R(38, 116, 48, 9, '#e8e2d2'); R(39, 117, 46, 7, '#2f3a2a'); // sign plate
    pine(150, 152, 70); pine(290, 152, 80); pine(330, 152, 64); pine(-8, 152, 60);
    flag(300, 150, ['#000000', '#dd0000', '#ffce00'], t);
    R(-200, 150, 1200, 30, '#6f7e45'); R(-200, 150, 1200, 2, '#5a6837');
    for (let i = 0; i < 140; i++) R(-200 + hash(i + 70) * 1200, 152 + hash(i + 19) * 26, 2, 1, '#4e5a2e');
    for (let i = 0; i < 40; i++) R(-200 + hash(i + 90) * 1200, 152 + hash(i + 29) * 26, 2, 1, '#9b6a3a'); // pine needles & cones
    R(50, 150, 24, 30, '#a8a08c');
    c.restore();
    return { signX: 39 - camX, signY: 117, signW: 46, signH: 7 };
  }

  // =====================================================================
  // Berlin street with Hoongy
  // =====================================================================
  const STREET = { W: 760, door: 274, shop0: 290, shop1: 470, feet: 150 };
  const treesAt = [90, 560, 700];
  const leafCols = ['#f2b400', '#ffcf3a', '#e8891c', '#d8641a', '#f5d24a', '#c9a227', '#b8761e'];
  function treeCanopy(tx, t, front) {
    const seed = tx;
    for (let i = 0; i < 150; i++) {
      const a = hash(seed + i) * Math.PI * 2, r = Math.sqrt(hash(seed + i * 3)) * 26;
      const x = tx + Math.cos(a) * r * 1.15 + Math.sin(t * 1.3 + i) * 0.6, y = 52 + Math.sin(a) * r * 0.75;
      R(x, y, 2, 2, leafCols[(i + seed) % leafCols.length]);
    }
  }
  function street(W, H, camX, t, s) {
    s = s || {};
    // night sky above the house fronts
    sky(W, H, '#07101f', '#16243a', 40);
    c.save(); c.translate(-Math.round(camX), 0);
    const SW = STREET.W;
    // old Berlin house fronts
    R(-100, 0, SW + 200, 136, '#3b3a3f');
    R(-100, 0, SW + 200, 2, '#2c2b30');
    for (let i = -2; i < 30; i++) {
      const x = i * 34 + 6;
      R(x, 2, 18, 18, '#262a33'); R(x - 2, 0, 22, 2, '#4a4950'); R(x - 2, 20, 22, 2, '#4a4950');
      if (hash(i + 3) > 0.55) { R(x + 1, 3, 16, 16, '#e8b35a'); R(x + 1, 3, 16, 16, 'rgba(255,170,60,.0)'); R(x + 8, 3, 1, 16, '#6b5030'); }
      if (hash(i + 3) > 0.55) { c.save(); c.globalCompositeOperation = 'lighter'; glow(x + 9, 11, 18, '#ffb050', 0.18); c.restore(); }
    }
    R(-100, 24, SW + 200, 4, '#55545b'); R(-100, 28, SW + 200, 2, '#2c2b30'); // cornice
    // left neighbour: a Späti
    R(130, 34, 136, 100, '#2a2420');
    R(136, 44, 92, 74, '#c9873e'); R(137, 45, 90, 72, '#e3a456');
    for (let sh = 0; sh < 4; sh++) { R(138, 58 + sh * 15, 88, 2, '#6b3f1c'); for (let b = 0; b < 22; b++) R(139 + b * 4, 50 + sh * 15, 3, 8, ['#7a2a2a', '#2a5a7a', '#d8c060', '#2a7a3a', '#e8e8e8'][(b + sh * 3) % 5]); }
    R(232, 50, 26, 84, '#1f1a17'); R(234, 54, 22, 76, '#d79a4e'); R(235, 55, 20, 74, '#efb664');
    c.save(); c.globalCompositeOperation = 'lighter'; glow(182, 80, 60, '#ffb060', 0.25); c.restore();
    neonText('SPÄTI', 152, 38, 5, '#ff4d6d', t, 1.1);
    // Hoongy door
    const D = STREET.door;
    R(D - 14, 50, 28, 86, '#1f1712'); R(D - 11, 56, 22, 78, '#e0a65a'); R(D - 10, 57, 20, 76, '#f2c27a');
    R(D - 10, 92, 20, 1, '#3a2a1a'); R(D + 6, 92, 2, 10, '#c9b46a');
    if (s.doorOpen) { R(D - 11, 56, 6, 78, '#1f1712'); }
    c.save(); c.globalCompositeOperation = 'lighter'; glow(D, 95, 34, '#ffb15a', 0.25); c.restore();
    // the shop front: dark wood frame
    const X0 = STREET.shop0, X1 = STREET.shop1;
    R(X0, 30, X1 - X0, 106, '#2a1c14'); R(X0, 30, X1 - X0, 3, '#3d2a1e');
    R(X0 + 3, 36, X1 - X0 - 6, 26, '#13161a'); // transom glass
    // four white paper lanterns
    for (let i = 0; i < 4; i++) {
      const lx = X0 + 30 + i * 40, ly = 40;
      c.save(); c.globalCompositeOperation = 'lighter'; glow(lx, ly + 8, 22, '#fff1d0', 0.35); c.restore();
      R(lx - 5, ly, 10, 18, '#f7f1e3'); R(lx - 6, ly + 3, 12, 12, '#fffaf0'); R(lx - 5, ly, 10, 1, '#b08a5a'); R(lx - 5, ly + 17, 10, 1, '#b08a5a');
      for (let k = 1; k < 5; k++) R(lx - 6, ly + 3 * k, 12, 1, 'rgba(180,150,110,.35)');
      R(lx - 1, ly + 18, 2, 3, '#f2efe6');
    }
    // string lights along the top, sagging between hooks
    for (let k = 0; k <= 16; k++) {
      const x = X0 - 6 + k * ((X1 - X0 + 12) / 16), sag = Math.sin((k % 4) / 4 * Math.PI) * 3;
      c.save(); c.globalCompositeOperation = 'lighter'; glow(x, 33 + sag, 7, '#ffc070', 0.6); c.restore();
      R(x, 33 + sag, 1, 1, '#fff2c0');
    }
    R(X0, 62, X1 - X0, 4, '#3d2a1e');
    // the big window
    const wy0 = 66, wy1 = 126;
    const g = c.createLinearGradient(0, wy0, 0, wy1);
    g.addColorStop(0, '#1a2a2a'); g.addColorStop(0.55, '#3a3324'); g.addColorStop(1, '#2a2218');
    c.fillStyle = g; c.fillRect(X0 + 4, wy0, X1 - X0 - 8, wy1 - wy0);
    // the bar at the back
    R(X0 + 70, 96, 90, 14, '#3b2a1f');
    for (let sh = 0; sh < 3; sh++) { R(X0 + 72, 94 - sh * 5, 86, 1, '#fff0c8'); for (let b = 0; b < 21; b++) R(X0 + 73 + b * 4, 90 - sh * 5, 2, 4, ['#d8a040', '#7ab0a0', '#e0e0d0', '#a04040', '#c8c060'][(b + sh) % 5]); }
    c.save(); c.globalCompositeOperation = 'lighter'; glow(X0 + 115, 88, 46, '#fff0c0', 0.25); c.restore();
    // small round paper lamps inside
    for (let i = 0; i < 6; i++) { const lx = X0 + 60 + i * 16; R(lx - 2, 82, 5, 4, '#f3c88a'); c.save(); c.globalCompositeOperation = 'lighter'; glow(lx, 84, 6, '#ffcf8a', 0.4); c.restore(); }
    // ginkgo tree inside, left
    for (let i = 0; i < 120; i++) {
      const a = hash(i + 400) * 6.28, r = Math.sqrt(hash(i + 401)) * 22;
      R(X0 + 26 + Math.cos(a) * r * 0.8, 86 + Math.sin(a) * r, 2, 2, ['#c9b23a', '#e2cc4a', '#a89a2a', '#f0dc6a'][i % 4]);
    }
    R(X0 + 25, 100, 2, 26, '#5a4630');
    // red pineapple-flower decoration at the top
    c.fillStyle = '#d8323a'; c.beginPath(); c.ellipse(X0 + 118, 72, 8, 5, 0, 0, 7); c.fill();
    for (let k = 0; k < 8; k++) R(X0 + 110 + k * 2, 68 + (k % 2) * 3, 1, 1, '#ff7070');
    for (let k = 0; k < 5; k++) { c.fillStyle = '#e8403a'; c.beginPath(); c.moveTo(X0 + 127, 72); c.lineTo(X0 + 140 + k * 3, 64 + k * 3); c.lineTo(X0 + 138 + k * 3, 68 + k * 3); c.fill(); }
    for (let k = 0; k < 5; k++) { c.fillStyle = '#c42a30'; c.beginPath(); c.moveTo(X0 + 109, 72); c.lineTo(X0 + 96 - k * 3, 64 + k * 3); c.lineTo(X0 + 98 - k * 3, 68 + k * 3); c.fill(); }
    // diners in the window (a cosy couple, like the photo)
    const din = (x, col, hair) => { R(x - 6, 110, 12, 16, col); R(x - 4, 102, 8, 8, '#e0b090'); R(x - 4, 100, 8, 4, hair); };
    din(X0 + 58, '#2a2f3a', '#3a2a1e'); din(X0 + 132, '#5a6a3a', '#c8a060');
    R(X0 + 70, 118, 50, 3, '#6b4a30'); R(X0 + 92, 114, 3, 4, '#ffd060'); R(X0 + 98, 114, 2, 4, '#e8e8e8');
    // neon HOONGY sign: hangs inside the window
    R(X0 + 50, 80, 86, 20, 'rgba(8,10,10,.82)'); R(X0 + 50, 80, 86, 1, 'rgba(255,255,255,.12)');
    neonText('HOONGY', X0 + 56, 84, 12, '#ffe23a', t, 1.4);
    // glass reflection
    c.fillStyle = 'rgba(255,255,255,.05)'; c.beginPath(); c.moveTo(X0 + 20, wy0); c.lineTo(X0 + 40, wy0); c.lineTo(X0 + 10, wy1); c.lineTo(X0 + 4, wy1); c.fill();
    R(X0, wy1, X1 - X0, 10, '#2a1c14'); R(X0, wy1, X1 - X0, 1, '#3d2a1e');
    // the wall to the right with graffiti
    R(X1, 34, 120, 102, '#3f3e44');
    c.fillStyle = '#141418'; c.beginPath(); c.arc(X1 + 60, 92, 30, 0, 7); c.fill();
    c.strokeStyle = '#e8e8e8'; c.lineWidth = 1; c.beginPath(); c.arc(X1 + 60, 92, 30, 0, 7); c.stroke();
    paintTag('ТУСА', X1 + 41, 80, '#ffffff');
    paintTag('GEZELLIG', X1 + 34, 98, '#7fd3ff');
    R(X1 + 8, 118, 22, 2, '#ff4fa0'); R(X1 + 90, 52, 18, 2, '#9cff6b');
    // street lamps (old Berlin style)
    for (const lx of [200, 640]) {
      R(lx, 52, 2, 98, '#1f3a2e'); R(lx - 2, 146, 6, 4, '#1f3a2e');
      R(lx - 4, 44, 10, 10, '#1f3a2e'); R(lx - 3, 45, 8, 8, '#ffe0a0');
      c.save(); c.globalCompositeOperation = 'lighter'; glow(lx + 1, 49, 40, '#ffcf80', 0.32); c.restore();
    }
    // back trees (trunks; canopies are drawn in front later)
    for (const tx of treesAt) { R(tx - 2, 60, 5, 90, '#3a2a1e'); R(tx - 1, 60, 2, 90, '#4a3626'); }
    // sidewalk: cobbles near the house, big slabs, the kerb
    R(-100, 136, SW + 200, 8, '#4a4a4f');
    for (let i = 0; i < 400; i++) R(-100 + (i * 5.3) % (SW + 200), 137 + (i % 3) * 2.4, 3, 2, ['#5a5a60', '#3c3c42', '#66666c'][i % 3]);
    R(-100, 144, SW + 200, 13, '#55555b');
    for (let x = -100; x < SW + 100; x += 22) R(x, 144, 1, 13, '#47474d');
    R(-100, 150, SW + 200, 1, '#4c4c52');
    R(-100, 157, SW + 200, 3, '#77777d');
    // the street with tram tracks
    R(-100, 160, SW + 200, 20, '#262629');
    R(-100, 166, SW + 200, 1, '#55555b'); R(-100, 170, SW + 200, 1, '#55555b');
    for (let x = -100; x < SW + 100; x += 30) R(x, 175, 14, 1, '#d8d8d8');
    // puddle reflections of the neon
    c.save(); c.globalCompositeOperation = 'lighter'; glow((X0 + X1) / 2, 165, 40, '#ffe23a', 0.08); c.restore();
    c.restore();
  }
  // outdoor tables (drawn in front of the shop, behind characters)
  function outdoorTables(camX, t) {
    c.save(); c.translate(-Math.round(camX), 0);
    for (const tx of [236, 336, 436, 520]) {
      // folding chairs
      for (const side of [-1, 1]) {
        const cx = tx + side * 17;
        R(cx - 5, 133, 10, 2, '#b07a40'); for (let k = 0; k < 4; k++) R(cx - 5, 127 + k * 2, 10, 1, '#a06a35');
        R(cx + side * 4, 125, 1, 10, '#1a1a1a');
        c.strokeStyle = '#1a1a1a'; c.lineWidth = 0.8; c.beginPath(); c.moveTo(cx - 4, 135); c.lineTo(cx + 4, 146); c.moveTo(cx + 4, 135); c.lineTo(cx - 4, 146); c.stroke();
      }
      R(tx - 12, 128, 24, 2, '#c08848'); R(tx - 12, 130, 24, 1, '#8a5a2a');
      for (let k = -10; k < 12; k += 3) R(tx + k, 128, 1, 2, '#a87038');
      c.strokeStyle = '#1a1a1a'; c.lineWidth = 0.9; c.beginPath(); c.moveTo(tx - 9, 131); c.lineTo(tx + 9, 147); c.moveTo(tx + 9, 131); c.lineTo(tx - 9, 147); c.stroke();
      // red candle glass
      R(tx - 2, 124, 4, 4, '#e0283a'); R(tx - 1, 124, 2, 1, '#ffb0b0');
      c.save(); c.globalCompositeOperation = 'lighter'; glow(tx, 125, 10 + Math.sin(t * 9 + tx) * 1.2, '#ff5060', 0.45); c.restore();
    }
    c.restore();
  }
  function streetFront(camX, t) {
    c.save(); c.translate(-Math.round(camX), 0);
    for (const tx of treesAt) treeCanopy(tx, t);
    c.restore();
  }
  function paintTag(txt, x, y, col) {
    c.save(); c.font = 'bold 9px "Pixelify Sans", monospace'; c.fillStyle = col; c.textBaseline = 'top';
    c.fillText(txt, x, y); c.restore();
  }

  // neon letters as tubes, like the real sign
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
        const segs = NEON[ch] || [];
        for (const seg of segs) { c.beginPath(); seg.forEach(([px, py], i) => { const X = cx + px * k, Y = y + py * k; i ? c.lineTo(X, Y) : c.moveTo(X, Y); }); c.stroke(); }
        cx += 7 * k;
      }
    };
    c.save();
    c.globalCompositeOperation = 'lighter';
    strokes(lw * 4.5, 0.12, col); strokes(lw * 2.4, 0.3, col);
    c.globalCompositeOperation = 'source-over';
    strokes(lw, 1, '#fff6b0');
    c.restore();
  }

  // =====================================================================
  // vehicles
  // =====================================================================
  function wheel(x, y, r, t) {
    c.fillStyle = '#111'; c.beginPath(); c.arc(x, y, r, 0, 7); c.fill();
    c.fillStyle = '#777'; c.beginPath(); c.arc(x, y, r * 0.45, 0, 7); c.fill();
    R(x + Math.cos(t) * r * 0.5, y + Math.sin(t) * r * 0.5, 1, 1, '#ddd');
  }
  function car(x, y, col, dir, t, night) {
    const P = (dx, dy, w, h, cl) => R(dir > 0 ? x + dx : x - dx - w, y + dy, w, h, cl);
    P(-20, -10, 40, 7, col); P(-12, -16, 22, 7, col); P(-10, -15, 8, 5, '#7c9bb4'); P(0, -15, 8, 5, '#7c9bb4');
    P(-20, -4, 40, 1, 'rgba(0,0,0,.35)');
    P(18, -9, 2, 2, '#fff6c0'); P(-20, -9, 2, 2, '#c02020');
    if (night) { c.save(); c.globalCompositeOperation = 'lighter'; glow(dir > 0 ? x + 24 : x - 24, y - 8, 14, '#fff3c0', 0.35); glow(dir > 0 ? x - 21 : x + 21, y - 8, 6, '#ff2020', 0.4); c.restore(); }
    wheel(x - 12 * dir, y - 2, 3.2, t * 10); wheel(x + 12 * dir, y - 2, 3.2, t * 10);
  }
  function taxi(x, y, dir, t) {
    car(x, y, '#f1e8c8', dir, t, true);
    R(x - 4, y - 19, 8, 3, '#f8d64a'); c.save(); c.font = '3px monospace'; c.fillStyle = '#222'; c.fillText('TAXI', x - 4, y - 16.6); c.restore();
  }
  function amarok(x, y, dir, t, driver) {
    const P = (dx, dy, w, h, cl) => R(dir > 0 ? x + dx : x - dx - w, y + dy, w, h, cl);
    const col = '#3f6b3a', col2 = '#2f5530';
    P(-26, -13, 52, 9, col); P(-26, -13, 52, 2, '#5d8a55');
    P(-2, -22, 22, 10, col); P(0, -21, 9, 7, '#88a8bf'); P(10, -21, 8, 7, '#88a8bf');
    P(-26, -15, 24, 2, col2); // the open bed
    P(24, -11, 3, 3, '#fff6c0'); P(-27, -11, 2, 3, '#c02020'); P(-26, -6, 52, 2, '#222');
    if (driver) { P(2, -20, 5, 5, LOOK[driver].skin); P(1, -22, 7, 3, LOOK[driver].hair); }
    c.save(); c.globalCompositeOperation = 'lighter'; glow(dir > 0 ? x + 30 : x - 30, y - 10, 14, '#fff3c0', 0.3); c.restore();
    wheel(x - 16 * dir, y - 3, 4.5, t * 9); wheel(x + 16 * dir, y - 3, 4.5, t * 9);
  }
  function tram(x, y, dir, t) {
    const L = 150;
    for (let s = 0; s < 3; s++) {
      const sx = x + s * 51 * dir - (dir > 0 ? 0 : 0);
      const P = (dx, dy, w, h, cl) => R(dir > 0 ? sx + dx : sx - dx - w, y + dy, w, h, cl);
      P(0, -38, 50, 34, '#f4c21a'); P(0, -38, 50, 3, '#ffdb5a'); P(0, -8, 50, 4, '#c99a0e');
      for (let w = 0; w < 4; w++) { P(3 + w * 12, -32, 10, 13, '#ffe7a8'); }
      P(0, -17, 50, 2, '#3a3a3a');
      wheel(dir > 0 ? sx + 10 : sx - 10, y - 3, 3, t * 12); wheel(dir > 0 ? sx + 40 : sx - 40, y - 3, 3, t * 12);
    }
    c.save(); c.globalCompositeOperation = 'lighter'; glow(dir > 0 ? x + 75 : x - 75, y - 25, 70, '#ffe7a8', 0.15); c.restore();
    R(dir > 0 ? x + 60 : x - 70, y - 44, 10, 1, '#333'); R(dir > 0 ? x + 64 : x - 66, y - 52, 1, 8, '#333');
    R(dir > 0 ? x + 140 : x - 150, y - 34, 10, 5, '#111'); c.save(); c.font = '4px monospace'; c.fillStyle = '#ffb000'; c.fillText('M10', dir > 0 ? x + 141 : x - 149, y - 30); c.restore();
    return L;
  }
  function scooter(x, y, dir, rider, t) {
    const P = (dx, dy, w, h, cl) => R(dir > 0 ? x + dx : x - dx - w, y + dy, w, h, cl);
    P(-8, -3, 16, 2, '#2b2b2b'); P(6, -20, 2, 18, '#3a3a3a'); P(4, -21, 6, 1, '#3a3a3a');
    wheel(x - 7 * dir, y - 2, 2.2, t * 12); wheel(x + 7 * dir, y - 2, 2.2, t * 12);
    P(7, -19, 2, 1, '#9cff6b');
  }
  function plane(x, y, s, t, tilt) {
    c.save(); c.translate(x, y); c.rotate(tilt || 0); c.scale(s, s);
    R(-30, -4, 60, 8, '#f2f4f7'); R(-30, 0, 60, 4, '#d6dbe2');
    R(26, -3, 8, 6, '#f2f4f7'); R(30, -2, 6, 3, '#3a4a5a');
    for (let k = 0; k < 9; k++) R(-18 + k * 5, -2, 2, 2, '#3a4a5a');
    c.fillStyle = '#c9d1db'; c.beginPath(); c.moveTo(-4, 2); c.lineTo(10, 2); c.lineTo(-10, 16); c.lineTo(-16, 16); c.fill();
    c.fillStyle = '#1f6fb5'; c.beginPath(); c.moveTo(-30, -4); c.lineTo(-22, -4); c.lineTo(-30, -18); c.lineTo(-35, -18); c.fill();
    R(-30, 1, 60, 1, '#1f6fb5');
    c.restore();
  }
  function train(x, y, t, rider) {
    // red regional train, three coaches
    for (let k = 0; k < 3; k++) {
      const sx = x - k * 64;
      R(sx - 60, y - 34, 62, 30, '#d8121f'); R(sx - 60, y - 34, 62, 3, '#ff4a50');
      R(sx - 60, y - 20, 62, 3, '#ffffff');
      for (let w = 0; w < 5; w++) R(sx - 56 + w * 12, y - 30, 9, 8, '#b9d4e6');
      R(sx - 60, y - 5, 62, 2, '#333');
      wheel(sx - 50, y - 2, 3, t * 14); wheel(sx - 10, y - 2, 3, t * 14);
    }
    c.fillStyle = '#d8121f'; c.beginPath(); c.moveTo(x + 2, y - 34); c.lineTo(x + 14, y - 22); c.lineTo(x + 14, y - 4); c.lineTo(x + 2, y - 4); c.fill();
    R(x + 4, y - 30, 6, 7, '#b9d4e6');
    if (rider) { R(x - 32, y - 28, 5, 5, LOOK[rider].skin); R(x - 33, y - 31, 7, 3, LOOK[rider].hat || LOOK[rider].hair); R(x - 32, y - 25, 4, 1, LOOK[rider].must || LOOK[rider].skin2); }
    c.save(); c.font = '5px monospace'; c.fillStyle = '#fff'; c.fillText('RE7', x - 54, y - 9); c.restore();
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
  function disc(x, y, r, col) { c.fillStyle = col; c.beginPath(); c.arc(x, y, r, 0, 7); c.fill(); }
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
    R(x, y, w, h, '#2a1a10');
    R(x + 1, y + 1, w - 2, h - 2, '#d8c497');
    for (let yy = y + 4; yy < y + h - 1; yy += 4) R(x + 1, yy, w - 2, 1, '#6b4a2c');
    for (let xx = x + 3; xx < x + w - 1; xx += 3) R(xx, y + 1, 1, h - 2, '#6b4a2c');
  }
  function fan(x, y, r, col) {
    c.fillStyle = col; c.beginPath(); c.moveTo(x, y); c.arc(x, y, r, Math.PI * 1.1, Math.PI * 1.9); c.closePath(); c.fill();
    c.strokeStyle = '#e8c35a'; c.lineWidth = 0.4;
    for (let k = 0; k <= 6; k++) { const a = Math.PI * (1.1 + k * 0.8 / 6); c.beginPath(); c.moveTo(x, y); c.lineTo(x + Math.cos(a) * r, y + Math.sin(a) * r); c.stroke(); }
    R(x - 1, y - 1, 2, 2, '#3a2416');
  }

  // s: { seatTaken, hiSeat, doorOpen, radioOn, hiRadio }
  function inside(t, s) {
    s = s || {};
    const W = IN.W, H = IN.H;
    // dark wooden floor
    R(0, 0, W, H, '#3a281b');
    for (let y = 0; y < H; y += 6) { R(0, y, W, 1, '#2e2015'); for (let x = (y / 6 % 2) * 20; x < W; x += 40) R(x, y, 1, 6, '#2e2015'); }
    for (let i = 0; i < 90; i++) R(hash(i + 900) * W, hash(i + 901) * H, 3, 1, 'rgba(255,220,180,.05)');
    // indigo rug with the wave pattern down the middle
    R(55, 125, 214, 30, '#c9a24a');
    seigaiha(56, 126, 212, 28, '#1b2a4e', '#3d5f97', 5);
    // colour from the neon koi above
    koiLight(t);

    // ---- walls ----
    R(0, 0, W, 15, '#24160e'); R(0, 15, W, 1, '#4a301c');
    seigaiha(112, 1, 198, 12, '#15254a', '#2f4f88', 4);
    R(112, 1, 198, 1, '#8a2a20'); R(112, 12, 198, 1, '#8a2a20');
    fan(150, 12, 9, '#c4302b'); fan(272, 12, 9, '#c4302b'); fan(211, 12, 7, '#e8e0cc');
    // washi-paper walls left and right, lit softly from behind
    for (let y = 16; y < H - 14; y += 14) { shoji(0, y, 10, 13); if (y < 26 || y > 60) shoji(W - 10, y, 10, 13); }
    c.save(); c.globalCompositeOperation = 'lighter';
    for (let y = 30; y < H - 20; y += 40) { glow(4, y, 18, '#ffd9a0', 0.12); glow(W - 4, y + 20, 18, '#ffd9a0', 0.12); }
    c.restore();
    // neon HOONGY on the back wall
    R(16, 2, 92, 11, '#0f1a19');
    neonText('HOONGY', 22, 3, 8, '#ffe23a', t, 1);

    // ---- the corner garden: bamboo, a small stone pond with real koi, a stone lantern ----
    R(10, 16, 104, 38, '#2b2a24');
    for (let i = 0; i < 70; i++) R(10 + hash(i + 40) * 104, 16 + hash(i + 41) * 38, 1, 1, ['#4a4840', '#5a5850', '#3a3830'][i % 3]); // gravel
    // the pond
    c.fillStyle = '#6a6a62'; c.beginPath(); c.ellipse(66, 35, 27, 15, 0, 0, 7); c.fill();
    c.fillStyle = '#0d3b42'; c.beginPath(); c.ellipse(66, 35, 24, 12.5, 0, 0, 7); c.fill();
    for (let i = 0; i < 14; i++) { const a = i / 14 * 6.28; disc(66 + Math.cos(a) * 25.5, 35 + Math.sin(a) * 13.8, 2 + hash(i + 7) * 1.2, ['#7a7a70', '#8a8a80', '#5e5e56'][i % 3]); }
    c.strokeStyle = 'rgba(160,230,230,.25)'; c.lineWidth = 0.5;
    for (let i = 0; i < 3; i++) { const k = (t * 0.25 + i / 3) % 1; c.beginPath(); c.ellipse(58 + i * 9, 33 + (i % 2) * 4, 2 + k * 8, 1 + k * 4, 0, 0, 7); c.globalAlpha = 1 - k; c.stroke(); c.globalAlpha = 1; }
    disc(78, 30, 3.2, '#3f7a3a'); R(78, 29, 3, 1, '#0d3b42'); disc(80, 39, 1.2, '#f4a8c0'); // lily pad and flower
    for (let i = 0; i < 3; i++) {
      const a = t * (0.6 + i * 0.15) + i * 2.1, fx = 66 + Math.cos(a) * (13 - i * 3), fy = 35 + Math.sin(a) * (6 - i);
      const hx = Math.cos(a + 1.57), hy = Math.sin(a + 1.57);
      c.strokeStyle = ['#ff8a2a', '#f4f0e8', '#ffb43a'][i]; c.lineWidth = 1.6; c.lineCap = 'round';
      c.beginPath(); c.moveTo(fx + hx * 2.5, fy + hy * 2.5); c.lineTo(fx - hx * 2.5, fy - hy * 2.5); c.stroke();
      R(fx - hx * 2, fy - hy * 2, 1, 1, '#e8302a');
    }
    c.lineCap = 'butt';
    // stone lantern, lit
    R(100, 33, 12, 12, '#77756c'); R(101, 34, 10, 10, '#8a887e'); R(103, 36, 6, 6, '#2a2620'); R(104, 37, 4, 4, '#ffcf6a');
    c.save(); c.globalCompositeOperation = 'lighter'; glow(106, 39, 16, '#ffbe5a', 0.35); c.restore();
    // bamboo in the corner, swaying
    for (let i = 0; i < 9; i++) {
      const bx = 14 + hash(i + 300) * 22, by = 19 + hash(i + 301) * 32;
      disc(bx, by, 2, '#3d6b2a'); disc(bx, by, 1.2, '#6aa04a');
      const sway = Math.sin(t * 1.3 + i) * 1.5;
      c.strokeStyle = '#7cc05a'; c.lineWidth = 1;
      for (let l = 0; l < 3; l++) { const a = hash(i * 3 + l + 330) * 6.28; c.beginPath(); c.moveTo(bx, by); c.lineTo(bx + Math.cos(a) * 7 + sway, by + Math.sin(a) * 5); c.stroke(); }
    }

    // ---- the bar ----
    R(120, 16, 168, 6, '#2b1d14');
    for (let b = 0; b < 40; b++) R(122 + b * 4, 17, 3, 4, ['#d8a040', '#7ab0a0', '#e0e0d0', '#a04040', '#c8c060'][b % 5]);
    disc(276, 19, 2.6, '#d42a2a'); R(275, 18, 2, 2, '#f4ead0'); // daruma on the shelf
    c.save(); c.globalCompositeOperation = 'lighter'; glow(205, 20, 70, '#fff0c0', 0.2); c.restore();
    R(120, 30, 168, 12, '#5a3b26'); R(120, 30, 168, 2, '#7a5236'); R(120, 41, 168, 1, '#2b1d14');
    for (let k = 0; k < 7; k++) disc(132 + k * 22, 46, 3, '#7a2a22'); // bar stools, red tops
    for (let k = 0; k < 7; k++) disc(132 + k * 22, 46, 1.5, '#9a3a2e');
    // sake set and a little teapot on the counter
    R(228, 33, 3, 5, '#f0ece0'); R(233, 35, 2, 2, '#f0ece0'); R(236, 35, 2, 2, '#f0ece0');
    disc(184, 36, 2.6, '#2f3a3a'); R(186, 35, 2, 1, '#2f3a3a');
    // maneki-neko waving at the guests
    const paw = Math.sin(t * 5) > 0 ? -1 : 0;
    disc(126, 36, 3.4, '#f6f2ea'); disc(126, 32.5, 2.6, '#f6f2ea');
    R(124, 29, 1, 2, '#f6f2ea'); R(128, 29, 1, 2, '#f6f2ea'); R(124, 30, 1, 1, '#f4a0a8'); R(128, 30, 1, 1, '#f4a0a8');
    R(124, 35, 5, 1, '#d42a2a'); R(126, 36, 1, 1, '#ffd23a');
    R(129, 33 + paw, 2, 2, '#f6f2ea');
    R(125, 32, 1, 1, '#1a1a1a'); R(127, 32, 1, 1, '#1a1a1a');
    // kitchen door with a noren curtain that moves a little
    R(W - 18, 28, 8, 30, '#2b1d14'); R(W - 17, 29, 6, 28, '#6b4a33');
    for (let i = 0; i < 3; i++) {
      const sw = Math.sin(t * 1.6 + i * 1.3) * 0.7;
      R(W - 21 + sw, 29 + i * 9.5, 5, 9, '#1f3a6e'); R(W - 21 + sw, 29 + i * 9.5, 1, 9, '#2c4f8f');
      disc(W - 18.5 + sw, 33.5 + i * 9.5, 1.3, '#f2efe6');
    }

    // ---- the radio cabinet ----
    R(11, 119, 26, 28, '#2a1a10'); R(12, 120, 24, 26, '#5e3b22'); R(12, 120, 24, 2, '#7a5030');
    R(15, 125, 18, 15, '#3a2414'); R(16, 126, 16, 13, '#8a5a2e'); R(16, 126, 16, 1, '#a8743e');
    for (let yy = 128; yy < 138; yy += 2) for (let xx = 17; xx < 24; xx += 2) R(xx, yy, 1, 1, '#4a2e18'); // speaker cloth
    const on = !!s.radioOn;
    R(25, 128, 6, 4, on ? '#ffc35a' : '#5a4a30'); R(25 + (on ? (s.radioOn % 2 ? 1 : 4) : 2), 128, 1, 4, '#c03020'); // dial
    disc(26.5, 135.5, 1.2, '#2a1a10'); disc(29.5, 135.5, 1.2, '#2a1a10');
    if (on) {
      c.save(); c.globalCompositeOperation = 'lighter'; glow(28, 130, 12, '#ffb84a', 0.35); c.restore();
      for (let i = 0; i < 4; i++) R(17 + i * 2, 141 - Math.abs(Math.sin(t * 7 + i * 1.7)) * 3, 1, 3, '#ffd36a');
    }
    if (s.hiRadio) { c.strokeStyle = '#ffe23a'; c.lineWidth = 1; c.strokeRect(9.5, 117.5, 29, 31); }

    // ---- plants: ginkgo by the door, bonsai on a stand, sakura in the far corner ----
    R(15, 185, 11, 11, '#3a2a20'); R(16, 186, 9, 9, '#4a3628');
    for (let i = 0; i < 60; i++) { const a = hash(i + 20) * 6.28, r = Math.sqrt(hash(i + 190)) * 12; R(20 + Math.cos(a) * r, 190 + Math.sin(a) * r, 2, 2, ['#c9b23a', '#e2cc4a', '#a89a2a', '#f0dc6a', '#e09a2a'][i % 5]); }
    R(281, 131, 30, 23, '#2a1a10'); R(282, 132, 28, 21, '#4a2e1c');
    R(288, 137, 16, 11, '#2c5f8a'); R(289, 138, 14, 9, '#3f78a8');
    c.strokeStyle = '#5a3a22'; c.lineWidth = 1.4; c.beginPath(); c.moveTo(296, 143); c.quadraticCurveTo(290, 139, 292, 135); c.moveTo(296, 143); c.quadraticCurveTo(302, 141, 303, 137); c.stroke();
    for (const [bx, by, r] of [[291, 135, 4], [303, 136, 3.6], [297, 139, 3], [287, 139, 2.6]]) { disc(bx, by, r, '#2f5a2a'); disc(bx - 1, by - 1, r * 0.6, '#4a8a3a'); }
    R(288, 196, 13, 13, '#5a2a20'); R(289, 197, 11, 11, '#7a3a2a');
    c.strokeStyle = '#4a2a1a'; c.lineWidth = 1.6; c.beginPath(); c.moveTo(294, 202); c.lineTo(284, 192); c.moveTo(294, 202); c.lineTo(302, 190); c.moveTo(294, 202); c.lineTo(292, 186); c.stroke();
    for (const [bx, by, r] of [[284, 192, 8], [302, 190, 7.5], [292, 185, 7], [298, 200, 6], [286, 202, 5.5]]) { disc(bx, by, r, '#e48aa8'); disc(bx - 1, by - 1, r * 0.72, '#f6b3c8'); }
    for (let i = 0; i < 46; i++) { const a = hash(i + 500) * 6.28, r = Math.sqrt(hash(i + 501)) * 15; R(293 + Math.cos(a) * r, 194 + Math.sin(a) * r * 0.9, 1, 1, ['#ffe2ea', '#fbe4ea', '#d8698c'][i % 3]); }
    // petals drifting over the floor
    for (let i = 0; i < 12; i++) {
      const k = (t * 0.06 + hash(i + 600)) % 1;
      const px = 290 - k * (90 + hash(i + 601) * 120), py = 196 + Math.sin(k * 9 + i) * 6 - k * (hash(i + 602) * 60 - 20);
      c.globalAlpha = Math.min(1, (1 - k) * 2); R(px, py, 1, 1, '#ffc3d3'); c.globalAlpha = 1;
    }

    // ---- tables with cushions, a tea light and a tiny ikebana ----
    IN.seats.forEach((st, i) => {
      const taken = s.seatTaken && s.seatTaken[i];
      R(st.x - 5, st.y - 5, 10, 10, '#2a1a10');
      R(st.x - 4, st.y - 4, 8, 8, i % 2 ? '#9b2335' : '#23396b'); R(st.x - 4, st.y - 4, 8, 1, i % 2 ? '#b8384a' : '#34508c');
      R(st.x - 0.5, st.y - 0.5, 1, 1, '#e8c35a');
      if (!taken && s.hiSeat === i) { c.strokeStyle = '#ffe23a'; c.lineWidth = 1; c.strokeRect(st.x - 6.5, st.y - 6.5, 13, 13); }
    });
    IN.tables.forEach((tb, i) => {
      R(tb.x - tb.w / 2 + 1, tb.y - tb.h / 2 + 2, tb.w, tb.h, 'rgba(0,0,0,.35)');
      R(tb.x - tb.w / 2, tb.y - tb.h / 2, tb.w, tb.h, '#8a5a32'); R(tb.x - tb.w / 2, tb.y - tb.h / 2, tb.w, 2, '#a8723f');
      R(tb.x - tb.w / 2 + 2, tb.y - 1, tb.w - 4, 2, '#c4302b'); // red runner
      R(tb.x - 2, tb.y - 2, 4, 4, '#f0e8d8'); R(tb.x - 1, tb.y - 1, 2, 2, '#ffcf6a');
      c.save(); c.globalCompositeOperation = 'lighter'; glow(tb.x, tb.y, 11 + Math.sin(t * 8 + i) * 1.2, '#ffb060', 0.35); c.restore();
      R(tb.x + 4, tb.y - 9, 2, 3, '#2a2a2a'); R(tb.x + 3, tb.y - 11, 2, 2, '#f4a8c0'); R(tb.x + 5, tb.y - 12, 1, 2, '#5a8a3a'); // ikebana
    });

    // ---- front window along the bottom, with the street glow, and the door ----
    R(10, H - 12, W - 20, 12, '#13161a');
    for (let x = 60; x < W - 20; x += 40) R(x, H - 12, 2, 12, '#2a1c14');
    c.save(); c.globalCompositeOperation = 'lighter'; glow(W / 2, H, 90, '#ffcf80', 0.12); c.restore();
    R(IN.door.x - 12, H - 12, 24, 12, s.doorOpen ? '#e0a65a' : '#6b4a33');
    R(IN.door.x - 12, H - 13, 24, 1, '#c9b46a');
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
    c.save(); c.globalCompositeOperation = 'lighter'; glow(sx, y, 26, col === '#f4ecd8' ? '#ffe6b8' : '#ff8a5a', 0.22); c.restore();
    disc(sx, y, 5.5, col);
    c.strokeStyle = rib; c.lineWidth = 0.5;
    for (const r of [4.4, 3.2, 2]) { c.beginPath(); c.arc(sx, y, r, 0, 7); c.stroke(); }
    disc(sx, y, 1.4, '#1a1410');
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
      c.fillStyle = '#e8e8e8'; c.beginPath(); c.arc(x, y, 3.4, 0, 7); c.fill();
      c.fillStyle = '#ffa620'; c.beginPath(); c.arc(x, y, 2.6 * Math.max(0.15, left), 0, 7); c.fill();
      R(x + 1, y - 4, 1, 4, '#ff3a6a');
    } else if (item === 'coffee') {
      c.fillStyle = '#e8e8e8'; c.beginPath(); c.arc(x, y, 3.4, 0, 7); c.fill();
      c.fillStyle = '#4a2c1a'; c.beginPath(); c.arc(x, y, 2.5 * Math.max(0.1, left), 0, 7); c.fill();
      c.fillStyle = '#d9b48a'; c.beginPath(); c.arc(x, y + 0.6, 1.2 * left, 0, 7); c.fill();
      if (left > 0) { c.globalAlpha = 0.4; R(x + Math.sin(t * 3), y - 6, 1, 3, '#ffffff'); c.globalAlpha = 1; }
    } else {
      c.fillStyle = '#f4f2ea'; c.beginPath(); c.ellipse(x, y, 5, 4, 0, 0, 7); c.fill();
      const n = Math.ceil(left * 4);
      if (item === 'shrimp') for (let i = 0; i < n; i++) { c.strokeStyle = '#ff7a3a'; c.lineWidth = 1.2; c.beginPath(); c.arc(x - 2.5 + i * 1.8, y + (i % 2) - 0.5, 1.2, 0.3, 4.4); c.stroke(); }
      if (item === 'fish' && n) { c.fillStyle = '#9aa9b8'; c.beginPath(); c.ellipse(x - 0.5, y, 3.2 * left + 0.6, 1.6, 0, 0, 7); c.fill(); c.beginPath(); c.moveTo(x + 2.6 * left + 0.6, y); c.lineTo(x + 4, y - 1.6); c.lineTo(x + 4, y + 1.6); c.fill(); R(x - 3 * left, y - 0.5, 1, 1, '#222'); }
      if (item === 'fish' && !n) { c.strokeStyle = '#ccc'; c.lineWidth = 0.5; c.beginPath(); c.moveTo(x - 3, y); c.lineTo(x + 3, y); for (let k = -2; k <= 2; k++) { c.moveTo(x + k, y - 1); c.lineTo(x + k, y + 1); } c.stroke(); }
      R(x + 4, y - 3, 1, 6, '#c9c9c9');
    }
  }
  function menuCard(x, y) { R(x - 3, y - 4, 6, 8, '#1f1a17'); R(x - 2, y - 3, 4, 1, '#ffe23a'); R(x - 2, y - 1, 4, 1, '#bbb'); R(x - 2, y + 1, 3, 1, '#bbb'); }

  // =====================================================================
  // maps (green phosphor, like ERROR-DOS)
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
  function mapBase(W, H, P, fg) {
    R(0, 0, W, H, '#03100a');
    c.strokeStyle = 'rgba(57,255,122,.12)'; c.lineWidth = 0.5;
    for (let lon = -20; lon <= 50; lon += 5) { const a = P(lon, 30), b = P(lon, 75); c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke(); }
    for (let lat = 30; lat <= 75; lat += 5) { const a = P(-20, lat), b = P(50, lat); c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke(); }
  }
  function polyline(pts, P) { c.beginPath(); pts.forEach(([lo, la], i) => { const [x, y] = P(lo, la); i ? c.lineTo(x, y) : c.moveTo(x, y); }); }
  function europeMap(W, H, progress, t) {
    const P = projector(-11, 35, 42, 66, W, H, 8);
    mapBase(W, H, P);
    c.fillStyle = 'rgba(57,255,122,.08)'; c.strokeStyle = '#39ff7a'; c.lineWidth = 1;
    for (const poly of EUROPE) { polyline(poly, P); c.fill(); c.stroke(); }
    const from = P(26.1, 44.43), to = P(13.4, 52.52);
    // dotted flight path, curving up
    const pt = k => { const mx = (from[0] + to[0]) / 2, my = Math.min(from[1], to[1]) - 22; const a = 1 - k; return [a * a * from[0] + 2 * a * k * mx + k * k * to[0], a * a * from[1] + 2 * a * k * my + k * k * to[1]]; };
    c.fillStyle = '#ffe23a';
    for (let k = 0; k <= progress; k += 0.03) { const [x, y] = pt(k); R(x, y, 1, 1, '#ffe23a'); }
    city(from, 'Бухарест', 'Boekarest', true); city(to, 'Берлін', 'Berlijn', progress > 0.98);
    const [px, py] = pt(Math.min(1, progress)), [qx, qy] = pt(Math.min(1, progress + 0.02));
    c.save(); c.translate(px, py); c.rotate(Math.atan2(qy - py, qx - px)); planeIcon(); c.restore();
    return { from, to };
  }
  function planeIcon() {
    c.fillStyle = '#ffffff';
    c.beginPath(); c.moveTo(6, 0); c.lineTo(-5, -1); c.lineTo(-5, 1); c.fill();
    c.beginPath(); c.moveTo(1, 0); c.lineTo(-2, -5); c.lineTo(-3, -5); c.lineTo(-2, 0); c.lineTo(-3, 5); c.lineTo(-2, 5); c.fill();
    c.beginPath(); c.moveTo(-4, 0); c.lineTo(-6, -2.5); c.lineTo(-6.5, -2.5); c.lineTo(-6, 0); c.lineTo(-6.5, 2.5); c.lineTo(-6, 2.5); c.fill();
  }
  function city(p, uk, nl, on) {
    R(p[0] - 1, p[1] - 1, 3, 3, on ? '#ffe23a' : '#39ff7a');
    c.save(); c.globalCompositeOperation = 'lighter'; glow(p[0], p[1], 8, '#39ff7a', 0.5); c.restore();
  }
  // Brandenburg close-up for the drive and the train
  const BERLIN = [[13.09, 52.42], [13.2, 52.37], [13.38, 52.34], [13.57, 52.37], [13.75, 52.43], [13.73, 52.53], [13.6, 52.62], [13.45, 52.67], [13.28, 52.66], [13.12, 52.6], [13.1, 52.5], [13.09, 52.42]];
  const LAKES = [[[13.17, 52.42], [13.21, 52.44], [13.19, 52.47], [13.15, 52.45]], [[13.62, 52.43], [13.68, 52.44], [13.66, 52.46], [13.61, 52.45]], [[12.48, 52.38], [12.58, 52.36], [12.62, 52.4], [12.52, 52.42]], [[12.9, 52.36], [12.98, 52.34], [12.97, 52.38]]];
  function brandenburgMap(W, H, route, progress, t) {
    const P = projector(11.95, 52.05, 13.85, 52.72, W, H, 10);
    R(0, 0, W, H, '#03100a');
    c.strokeStyle = 'rgba(57,255,122,.1)'; c.lineWidth = 0.5;
    for (let lon = 11.9; lon < 14; lon += 0.25) { const a = P(lon, 52), b = P(lon, 52.8); c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke(); }
    for (let lat = 52; lat < 52.8; lat += 0.125) { const a = P(11.9, lat), b = P(14, lat); c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke(); }
    c.fillStyle = 'rgba(57,255,122,.12)'; c.strokeStyle = '#39ff7a'; c.lineWidth = 1;
    polyline(BERLIN, P); c.fill(); c.stroke();
    c.fillStyle = 'rgba(80,160,255,.35)'; for (const lk of LAKES) { polyline(lk, P); c.fill(); }
    // roads and rail
    c.strokeStyle = 'rgba(57,255,122,.45)'; c.lineWidth = 1;
    polyline([[11.95, 52.2], [12.28, 52.27], [12.55, 52.33], [12.95, 52.36], [13.1, 52.39]], P); c.stroke();
    c.setLineDash([2, 2]); polyline([[12.76, 52.2], [12.95, 52.3], [13.06, 52.39], [13.2, 52.45], [13.37, 52.525]], P); c.stroke(); c.setLineDash([]);
    const path = route.map(([lo, la]) => P(lo, la));
    // walk along the route
    let total = 0; const seg = [];
    for (let i = 1; i < path.length; i++) { const d = Math.hypot(path[i][0] - path[i - 1][0], path[i][1] - path[i - 1][1]); seg.push(d); total += d; }
    let want = total * Math.min(1, progress), at = path[0], ang = 0;
    c.fillStyle = '#ffe23a';
    for (let i = 1; i < path.length; i++) {
      const d = seg[i - 1];
      const a = path[i - 1], b = path[i];
      const k = Math.min(1, want / d);
      for (let s = 0; s <= k; s += 2 / d) R(a[0] + (b[0] - a[0]) * s, a[1] + (b[1] - a[1]) * s, 1, 1, '#ffe23a');
      if (want <= d) { at = [a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k]; ang = Math.atan2(b[1] - a[1], b[0] - a[0]); break; }
      want -= d; at = b; ang = Math.atan2(b[1] - a[1], b[0] - a[0]);
    }
    return { P, at, ang };
  }
  // side-view landscapes for the trips
  function roadTrip(W, H, t, kind) {
    sky(W, H, kind === 'night' ? '#0c1630' : '#5e8ccc', kind === 'night' ? '#2a2a50' : '#f8b97a', 140);
    const far = (t * 20) % 400, mid = (t * 60) % 300, near = (t * 160) % 60;
    c.fillStyle = '#4f6a46'; c.beginPath(); c.moveTo(0, 130); for (let x = 0; x <= W; x += 10) c.lineTo(x, 118 - Math.sin((x + far) * 0.03) * 6); c.lineTo(W, 140); c.lineTo(0, 140); c.fill();
    for (let i = -1; i < W / 22 + 2; i++) pine(i * 22 - (mid % 22), 140, 34 + hash(i + Math.floor(mid / 22)) * 18, '#1f4a2e');
    R(0, 140, W, 40, '#3a3a3e'); R(0, 140, W, 2, '#8a8f94');
    for (let x = -near; x < W; x += 60) R(x, 158, 26, 2, '#e8e8e8');
    for (let x = -((t * 160) % 24); x < W; x += 24) R(x, 136, 1, 6, '#aab0b5');
    R(0, 138, W, 1, '#c0c6cb');
  }
  function skyline(W, base, t, alpha) {
    c.save(); c.globalAlpha = alpha;
    for (let i = 0; i < 24; i++) { const h = 14 + hash(i + 300) * 26; R(i * 18, base - h, 16, h, '#1e2633'); for (let k = 0; k < 6; k++) if (hash(i * 7 + k) > 0.5) R(i * 18 + 3 + (k % 3) * 4, base - h + 4 + Math.floor(k / 3) * 6, 2, 2, '#ffd27a'); }
    // the TV tower
    const tx = W * 0.6;
    R(tx - 1, base - 80, 3, 80, '#2a3342'); c.fillStyle = '#2a3342'; c.beginPath(); c.arc(tx + 0.5, base - 62, 6, 0, 7); c.fill();
    R(tx, base - 96, 1, 16, '#2a3342'); R(tx - 3, base - 62, 7, 1, '#ffd27a');
    if ((t * 2 | 0) % 2) R(tx, base - 97, 1, 1, '#ff3030');
    c.restore();
  }
  function trainTrip(W, H, t, rider) {
    sky(W, H, '#6c96cf', '#ffc28a', 130);
    const mid = (t * 30) % 300;
    c.fillStyle = '#9db36f'; c.fillRect(0, 110, W, 30);
    for (let i = -1; i < W / 30 + 2; i++) pine(i * 30 - (mid % 30), 126, 30 + hash(i + Math.floor(mid / 30)) * 14);
    R(0, 140, W, 40, '#6d6656'); R(0, 146, W, 2, '#8a8f94'); R(0, 152, W, 2, '#8a8f94');
    for (let x = -((t * 200) % 12); x < W; x += 12) R(x, 145, 3, 10, '#4a3a2a');
    for (let x = -((t * 200) % 90); x < W; x += 90) { R(x, 96, 2, 50, '#555'); R(x - 10, 96, 22, 1, '#555'); }
    R(0, 100, W, 1, '#444');
    train(W * 0.62 + Math.sin(t * 6) * 0.5, 152, t, rider);
  }
  function hbf(W, H, t) {
    sky(W, H, '#0d1a33', '#334a6a', 150);
    skyline(W, 150, t, 0.6);
    c.strokeStyle = '#9fc2d8'; c.lineWidth = 1;
    for (let k = 0; k < 6; k++) { c.beginPath(); c.arc(W / 2, 170, 70 + k * 3, Math.PI, 0); c.stroke(); }
    c.fillStyle = 'rgba(160,200,230,.15)'; c.beginPath(); c.arc(W / 2, 170, 85, Math.PI, 0); c.fill();
    R(0, 150, W, 30, '#2a2a2e');
    c.save(); c.font = '6px monospace'; c.fillStyle = '#ffffff'; c.fillText('Berlin Hbf', W / 2 - 18, 108); c.restore();
  }
  function airport(W, H, t, planeX, planeY) {
    sky(W, H, '#0b1530', '#3a3060', 150);
    for (let i = 0; i < 30; i++) R(hash(i + 7) * W, hash(i + 8) * 70, 1, 1, '#cfd8ff');
    R(20, 110, 140, 40, '#2a3142'); for (let i = 0; i < 12; i++) R(26 + i * 11, 118, 8, 10, '#ffd27a');
    c.save(); c.font = '7px monospace'; c.fillStyle = '#ffffff'; c.fillText('BER', 80, 106); c.restore();
    R(0, 150, W, 30, '#1e1e22');
    for (let x = 0; x < W; x += 16) { R(x, 158, 8, 1, '#e8e8e8'); R(x + 4, 151, 1, 1, '#4dff7a'); }
    plane(planeX, planeY, 1, t, planeY < 140 ? 0.12 : 0);
  }

  return {
    use, R, glow, rgba, hash, LOOK, friend, friendTop, person,
    campLion, altengrabow, lehnin, street, outdoorTables, streetFront, STREET, treesAt,
    car, taxi, amarok, tram, scooter, plane, train,
    inside, insideTop, IN, dish, menuCard,
    europeMap, brandenburgMap, planeIcon, roadTrip, skyline, trainTrip, hbf, airport, neonText,
  };
})();
