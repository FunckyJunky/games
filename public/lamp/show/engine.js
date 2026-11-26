/* SHOW.EXE engine: turns the light show settings into "what the lamp should look like at time t",
   and sends the IR keys that get the real lamp there, one at a time, as fast as the board can send.

   const plan = new ShowEngine.Plan(settings, seed, origin)   plan.want(t) -> { on, key, mode, level }
   ShowEngine.estimate(plan, t0, seconds)                      IR load: 1.0 = all the board can send
   const snd = new ShowEngine.Sender(out); snd.tick(now, plan.want(t + LEAD))
   out = ShowEngine.linkOut(link) for the real lamp, or new ShowEngine.SimOut() for the preview

   The lamp is an IR RGB lamp: 16 colours, 8 brightness steps (the dimmest is still ~30 %, darker = OFF),
   4 built-in effects. Every change is one IR key and the board sends about 3 keys a second. */
(function (root) {
  'use strict';
  const L = root.LAMP;
  const WHEEL = ['r', 'r1', 'r2', 'r3', 'r4', 'g', 'g1', 'g2', 'g3', 'g4', 'b', 'b1', 'b2', 'b3', 'b4'];   // the colour circle (b4 pink -> r)
  const ALL = WHEEL.concat('w');
  const FXS = ['flash', 'strobe', 'fade', 'smooth'];
  const CAP = 3.0;                     // IR keys a second (board: 220 ms gap + the NEC frame + Wi-Fi)
  const MAXL = L.LEVELS - 1;

  function mulberry32(a) {
    return function () { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
  }
  function hash(a, b) {
    let h = Math.imul((a | 0) ^ 0x9E3779B9, 0x85EBCA6B) ^ Math.imul((b | 0) + 0x632BE5AB, 0xC2B2AE35);
    h ^= h >>> 15; h = Math.imul(h, 0x2C1B3C6D); h ^= h >>> 12; h = Math.imul(h, 0x297A2D39); h ^= h >>> 15;
    return (h >>> 0) / 4294967296;
  }
  const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
  const W0 = () => { const w = {}; for (const k of ALL) w[k] = 0; return w; };
  const pal = (o) => Object.assign(W0(), o);

  const DEFAULTS = {
    w: pal({ r: 7, r2: 4, g: 5, g3: 3, b: 7, b4: 5 }),
    order: 'random',   // random | cycle | bounce | walk | rhythm
    bpm: 60,           // beats a minute
    hold: 1,           // a new colour every 1, 2, 4 or 8 beats
    loose: 0,          // % random timing
    fade: 'cut',       // cut | dip | decay | swell | blink
    fdepth: 3,         // brightness steps the fade goes down (more than the brightness = OFF)
    flen: 50,          // % of the colour's time the fade takes (blink: the dark part)
    mod: 'none',       // none | sine | saw | square | random | gate
    mdepth: 3,         // steps (gate: chance of a dark beat, x10 %)
    mrate: 16,         // beats per wave
    fxp: 0,            // % chance a colour becomes one of the lamp's own effects
    fx: ['smooth'],
    fxlen: 8,          // beats an effect lasts
    bright: MAXL,      // brightest step 0..7
    stop: 0,           // minutes, 0 = never (then the lamp goes OFF)
  };
  function norm(s) {
    const o = Object.assign({}, DEFAULTS, s || {});
    o.w = Object.assign(W0(), (s && s.w) || DEFAULTS.w);
    for (const k of ALL) o.w[k] = clamp(Math.round(+o.w[k] || 0), 0, 9);
    if (!['random', 'cycle', 'bounce', 'walk', 'rhythm'].includes(o.order)) o.order = 'random';
    if (!['cut', 'dip', 'decay', 'swell', 'blink'].includes(o.fade)) o.fade = 'cut';
    if (!['none', 'sine', 'saw', 'square', 'random', 'gate'].includes(o.mod)) o.mod = 'none';
    o.bpm = clamp(+o.bpm || 60, 4, 240);
    o.hold = [1, 2, 4, 8].includes(+o.hold) ? +o.hold : 1;
    o.loose = clamp(+o.loose || 0, 0, 80);
    o.fdepth = clamp(Math.round(+o.fdepth || 0), 1, 8);
    o.flen = clamp(+o.flen || 50, 10, 100);
    o.mdepth = clamp(Math.round(+o.mdepth || 0), 0, 8);
    o.mrate = clamp(+o.mrate || 16, 1, 128);
    o.fxp = clamp(+o.fxp || 0, 0, 100);
    o.fx = (Array.isArray(o.fx) ? o.fx : []).filter((f) => FXS.includes(f));
    o.fxlen = clamp(+o.fxlen || 8, 1, 64);
    o.bright = clamp(Math.round(o.bright == null ? MAXL : +o.bright), 0, MAXL);
    o.stop = clamp(+o.stop || 0, 0, 600);
    return o;
  }

  const PRESETS = [
    { n: 'RAINBOW', s: { w: pal({ r: 5, r1: 5, r2: 5, r3: 5, r4: 5, g: 5, g1: 5, g2: 5, g3: 5, g4: 5, b: 5, b1: 5, b2: 5, b3: 5, b4: 5 }), order: 'cycle', bpm: 30, fade: 'dip', fdepth: 2, flen: 60 } },
    { n: 'PARTY', s: { w: pal({ r: 9, g: 8, b: 9, b4: 7, r3: 6, g3: 6 }), order: 'random', bpm: 120, fade: 'cut', mod: 'gate', mdepth: 2, fxp: 4, fx: ['strobe', 'flash'], fxlen: 8 } },
    { n: 'CHILL', s: { w: pal({ b: 6, b1: 7, b2: 5, b3: 3, g4: 6, g3: 3 }), order: 'walk', bpm: 10, fade: 'dip', fdepth: 3, flen: 80, mod: 'sine', mdepth: 2, mrate: 8, bright: 6 } },
    { n: 'FIRE', s: { w: pal({ r: 9, r1: 8, r2: 6, r3: 3 }), order: 'random', bpm: 80, mod: 'random', mdepth: 2, mrate: 8, loose: 30 } },
    { n: 'OCEAN', s: { w: pal({ b: 7, b1: 4, g4: 8, g3: 7, g2: 5 }), order: 'walk', bpm: 16, fade: 'dip', fdepth: 2, flen: 70, mod: 'sine', mdepth: 3, mrate: 12 } },
    { n: 'SUNSET', s: { w: pal({ r3: 9, r2: 8, r1: 6, r: 5, b4: 3, b3: 2 }), order: 'bounce', bpm: 8, fade: 'dip', fdepth: 2, flen: 60, mod: 'sine', mdepth: 2, mrate: 20, bright: 6 } },
    { n: 'POLICE', s: { w: pal({ r: 3, b: 3 }), order: 'cycle', bpm: 140 } },
    { n: 'HEARTBEAT', s: { w: pal({ r: 9, b4: 2 }), order: 'random', bpm: 30, fade: 'decay', fdepth: 2, flen: 50 } },
    { n: 'THUNDER', s: { w: pal({ b1: 7, b: 5, b2: 3, w: 2 }), order: 'random', bpm: 90, mod: 'gate', mdepth: 4, fxp: 3, fx: ['flash'], fxlen: 4, loose: 40 } },
    { n: 'DISCO', s: { w: pal({ r: 6, g: 5, b: 7, r3: 3, b4: 8, g3: 4, w: 2 }), order: 'rhythm', bpm: 110 } },
    { n: 'STROBE', s: { w: pal({ w: 9, b4: 3, g3: 3 }), order: 'cycle', bpm: 50, fade: 'blink', flen: 50 } },
    { n: 'NEON', s: { w: pal({ b4: 9, g3: 8, r4: 6, b2: 5 }), order: 'random', bpm: 60, hold: 2, fade: 'decay', fdepth: 2, flen: 90 } },
    { n: 'AURORA', s: { w: pal({ g: 5, g1: 8, g2: 7, g3: 5, b1: 4, b2: 6, b3: 3 }), order: 'walk', bpm: 20, fade: 'swell', fdepth: 2, flen: 80, mod: 'sine', mdepth: 2, mrate: 32 } },
  ];

  // a random show that the lamp can keep up with
  function surprise(rnd) {
    rnd = rnd || Math.random;
    const pick = (a) => a[Math.floor(rnd() * a.length)];
    const w = W0(), kind = rnd();
    if (kind < 0.45) {                                   // a stretch of the colour circle
      const c = Math.floor(rnd() * 15), n = 2 + Math.floor(rnd() * 5);
      for (let i = 0; i < n; i++) w[WHEEL[(c + i) % 15]] = 2 + Math.floor(rnd() * 8);
    } else if (kind < 0.8) {                             // a few colours from anywhere
      const n = 2 + Math.floor(rnd() * 4);
      for (let i = 0; i < n; i++) w[pick(WHEEL)] = 2 + Math.floor(rnd() * 8);
    } else {                                             // everything, uneven
      for (const k of WHEEL) w[k] = Math.floor(rnd() * 10);
    }
    if (rnd() < 0.15) w.w = 1 + Math.floor(rnd() * 3);
    if (!ALL.some((k) => w[k])) w.r = 5;
    const s = { w, order: pick(['random', 'random', 'cycle', 'bounce', 'walk', 'walk', 'rhythm']),
      bpm: Math.round(8 * Math.pow(18, rnd())), hold: pick([1, 1, 1, 2, 2, 4]), loose: pick([0, 0, 0, 10, 25]),
      fade: pick(['cut', 'cut', 'dip', 'decay', 'swell', 'blink']), fdepth: 1 + Math.floor(rnd() * 4), flen: pick([30, 50, 70, 90]),
      mod: pick(['none', 'none', 'sine', 'saw', 'square', 'random', 'gate']), mdepth: 1 + Math.floor(rnd() * 4), mrate: pick([4, 8, 16, 32]),
      fxp: rnd() < 0.3 ? pick([3, 5, 10]) : 0, fx: FXS.filter(() => rnd() < 0.5), fxlen: pick([4, 8, 16]), bright: rnd() < 0.8 ? MAXL : 5 };
    if (!s.fx.length) s.fx = ['smooth'];
    return fit(s, 0.85);
  }
  // the board sends ~3 keys a second: less fade, less wave, then slower, until the lamp can keep up
  function fit(s, max) {
    s = norm(s);
    for (let k = 0; k < 24 && estimate(new Plan(s, 1), 0, 40) > max; k++) {
      if ((s.fade === 'dip' || s.fade === 'decay' || s.fade === 'swell') && s.fdepth > 1) s.fdepth--;
      else if (s.mod !== 'none' && s.mod !== 'gate' && s.mdepth > 1) s.mdepth--;
      else s.bpm = Math.max(4, Math.round(s.bpm * 0.85));
    }
    return s;
  }

  class Plan {
    constructor(s, seed, origin) {
      this.s = norm(s);
      this.seed = (seed >>> 0) || 1;
      this.origin = origin || 0;
      this.B = 60 / this.s.bpm;
      this.D = this.B * this.s.hold;
      this.rnd = mulberry32(this.seed ^ 0x5bd1e995);
      this.sel = ALL.filter((k) => this.s.w[k] > 0);
      if (!this.sel.length) this.sel = ['w'];
      this.slots = [];
      this.cur = null; this.ci = -1; this.dir = 1; this.rep = 0; this.fxLeft = 0; this.fxId = null;
      this.phase = {};
      for (const k of ALL) this.phase[k] = Math.floor(this.rnd() * 9);
    }
    wt(k) { return this.s.w[k] || (this.sel.length === 1 && this.sel[0] === k ? 1 : 0); }
    weighted(list, score) {
      let tot = 0;
      for (const k of list) tot += score(k);
      let r = this.rnd() * tot;
      for (const k of list) { r -= score(k); if (r < 0) return k; }
      return list[list.length - 1];
    }
    colour(i) {
      const s = this.s, sel = this.sel, n = sel.length;
      if (n === 1) return sel[0];
      if (s.order === 'cycle' || s.order === 'bounce') {
        if (this.rep > 0 && this.cur) { this.rep--; return this.cur; }
        if (s.order === 'cycle') this.ci = (this.ci + 1) % n;
        else {
          this.ci += this.dir;
          if (this.ci >= n) { this.dir = -1; this.ci = n - 2; }
          if (this.ci < 0) { this.dir = 1; this.ci = Math.min(1, n - 1); }
        }
        const k = sel[this.ci];
        this.rep = Math.floor((this.wt(k) - 1) / 3);              // slider 1-3: 1 slot, 4-6: 2, 7-9: 3
        return k;
      }
      if (s.order === 'walk' && this.cur) {
        const pos = (k) => WHEEL.indexOf(k);
        const dist = (a, b) => { if (a === 'w' || b === 'w') return 4; const d = Math.abs(pos(a) - pos(b)); return Math.min(d, 15 - d); };
        return this.weighted(sel.filter((k) => k !== this.cur), (k) => this.wt(k) / Math.pow(dist(this.cur, k), 3));
      }
      if (s.order === 'rhythm') {
        const fire = sel.filter((k) => (i + this.phase[k]) % (10 - this.wt(k)) === 0);
        if (!fire.length) return this.cur || this.weighted(sel, (k) => this.wt(k));
        const top = Math.max(...fire.map((k) => this.wt(k)));
        const best = fire.filter((k) => this.wt(k) === top);
        return best[Math.floor(this.rnd() * best.length)];
      }
      return this.weighted(sel, (k) => this.wt(k));          // random: share of the time = share of the sliders
    }
    next(i) {
      const s = this.s;
      if (this.fxLeft > 0) { this.fxLeft--; return { fx: this.fxId }; }
      if (s.fxp > 0 && s.fx.length && i > 0 && this.rnd() * 100 < s.fxp) {
        this.fxId = s.fx[Math.floor(this.rnd() * s.fx.length)];
        this.fxLeft = Math.max(1, Math.ceil(s.fxlen / s.hold)) - 1;
        return { fx: this.fxId };
      }
      const k = this.colour(i);
      this.cur = k;
      return { key: k };
    }
    extend(t) {
      while (!this.slots.length || this.slots[this.slots.length - 1].t1 <= t) {
        const prev = this.slots[this.slots.length - 1];
        const t0 = prev ? prev.t1 : this.origin, i = prev ? prev.i + 1 : 0;
        const j = this.s.loose / 100;
        const len = this.D * Math.max(0.35, 1 + j * (this.rnd() * 2 - 1));
        const c = this.next(i);
        this.slots.push({ i, t0, t1: t0 + len, key: c.key || null, fx: c.fx || null });
        if (this.slots.length > 3000) this.slots.splice(0, 1500);
      }
    }
    slotAt(t) {
      this.extend(t);
      const a = this.slots;
      if (t < a[0].t0) return a[0];
      let lo = 0, hi = a.length - 1;
      while (lo < hi) { const m = (lo + hi + 1) >> 1; if (a[m].t0 <= t) lo = m; else hi = m - 1; }
      return a[lo];
    }
    // how many steps the slow wave takes off the brightness at time t (99 = a dark beat)
    modOffset(t) {
      const s = this.s;
      if (s.mod === 'none' || !s.mdepth) return 0;
      const beat = t / this.B, ph = beat / s.mrate, f = ph - Math.floor(ph);
      switch (s.mod) {
        case 'sine': return s.mdepth * (0.5 - 0.5 * Math.cos(2 * Math.PI * ph));
        case 'saw': return s.mdepth * f;
        case 'square': return f < 0.5 ? 0 : s.mdepth;
        case 'random': return Math.round(hash(this.seed, Math.floor(beat / Math.max(1, Math.round(s.mrate / 8)))) * s.mdepth);
        case 'gate': return hash(this.seed + 7, Math.floor(beat)) < s.mdepth / 10 ? 99 : 0;
      }
      return 0;
    }
    // what the lamp should look like at show time t
    want(t) {
      const s = this.s, sl = this.slotAt(t);
      const base = s.bright - this.modOffset(t);
      if (sl.fx) return base < -0.5 ? { on: false, key: null, mode: sl.fx, level: null, fx: sl.fx } : { on: true, key: null, mode: sl.fx, level: null };
      const x = clamp((t - sl.t0) / (sl.t1 - sl.t0), 0, 1), fd = s.fdepth, len = s.flen / 100;
      let off = 0;
      switch (s.fade) {
        case 'dip': { const h = len / 2; off = x < h ? fd * (1 - x / h) : x > 1 - h ? fd * (x - (1 - h)) / h : 0; break; }
        case 'decay': off = x < len ? fd * x / len : fd; break;
        case 'swell': off = x < len ? fd * (1 - x / len) : 0; break;
        case 'blink': if (x >= 1 - len) off = 99; break;
      }
      const lv = Math.round(base - off);
      if (lv < 0) return { on: false, key: sl.key, mode: 'color', level: null };
      return { on: true, key: sl.key, mode: 'color', level: Math.min(MAXL, lv) };
    }
  }

  // IR keys a second the plan needs, as a share of what the board can send (1 = flat out)
  function estimate(plan, t0, dur) {
    const dt = 0.02;
    let cost = 0, on = null, key = null, mode = null, lvl = null;
    for (let t = t0; t < t0 + dur; t += dt) {
      const w = plan.want(t);
      if (on === null) { on = w.on; key = w.key; mode = w.mode; lvl = w.level; continue; }
      if (w.on !== on) { cost++; on = w.on; }
      if (!w.on) continue;
      if (w.mode !== 'color') { if (mode !== w.mode) { cost++; mode = w.mode; } continue; }
      if (mode !== 'color' || key !== w.key) { cost++; mode = 'color'; key = w.key; }
      if (w.level != null) { if (lvl != null) cost += Math.abs(w.level - lvl); lvl = w.level; }
    }
    return cost / dur / CAP;
  }

  // Sends the next IR key that gets the lamp closer to what the show wants. Order: on/off, effect or colour,
  // then brightness. At most 2 keys waiting on the board, so a busy show never falls far behind.
  class Sender {
    constructor(out) { this.out = out; this.pred = null; this.lastSend = -1e9; this.sent = 0; }
    resync() { this.pred = Object.assign({}, this.out.state); }
    // on + 8 x brighter (or dimmer): after that we know the brightness. Bright shows go up, dim ones down.
    calibrate(now, up) {
      const o = this.out;
      o.clear();
      o.send('on'); o.send(up ? 'up' : 'down', 8);
      this.pred = Object.assign({}, o.state, { on: true, level: up ? MAXL : 0 });
      this.lastSend = now;
    }
    tick(now, w) {
      const o = this.out;
      if (o.inflight > 0 && now - o.lastAct > 2.5) o.inflight = 0;     // a "sent" got lost
      if (o.inflight === 0 || !this.pred) this.resync();
      if (o.inflight >= 2 || now - this.lastSend < 0.1) return null;
      const p = this.pred;
      let id = null;
      if (!w.on) { if (p.on) id = 'off'; }
      else if (!p.on) id = 'on';
      else if (w.mode && w.mode !== 'color') { if (p.mode !== w.mode) id = w.mode; }
      else if (p.mode !== 'color' || p.key !== w.key) id = w.key;
      else if (w.level != null && p.level !== w.level) id = w.level > p.level ? 'up' : 'down';
      if (!id || !o.send(id)) return null;
      L.apply(p, L.BY_ID[id].c);
      this.lastSend = now; this.sent++;
      return id;
    }
  }

  // the real lamp through LAMP.EXE's link (keys go to the board, "tx" comes back when sent)
  function linkOut(link, clock) {
    const o = { inflight: 0, lastAct: 0,
      get state() { return link.state; },
      send(id, r) { if (!link.press(id, r)) return false; o.inflight += r || 1; o.lastAct = clock(); return true; },
      clear() { link.clear(); o.inflight = 0; } };
    link.listen((ev) => { if (ev.type === 'tx' && ev.mine) { o.inflight = Math.max(0, o.inflight - 1); o.lastAct = clock(); } });
    return o;
  }

  // a pretend board + lamp for the preview: same gap between keys, a little network delay
  class SimOut {
    constructor() { this.state = L.freshState(); this.state.known = true; this.q = []; this.conf = []; this.inflight = 0; this.lastAct = 0; this.free = 0; this.now = 0; this.lat = 0.06; this.gap = 1 / CAP; }   // same pace as the board
    send(id, r) { for (let i = 0; i < (r || 1); i++) this.q.push({ id, at: this.now + this.lat }); this.inflight += r || 1; this.lastAct = this.now; return true; }
    clear() { this.q.length = 0; this.conf.length = 0; this.inflight = 0; }
    step(now) {
      this.now = now;
      while (this.q.length && this.q[0].at <= now && now >= this.free) {
        const c = this.q.shift();
        L.apply(this.state, L.BY_ID[c.id].c);
        this.free = Math.max(now, c.at) + this.gap;
        this.conf.push(now + this.lat);
      }
      while (this.conf.length && this.conf[0] <= now) { this.conf.shift(); this.inflight = Math.max(0, this.inflight - 1); this.lastAct = now; }
    }
  }

  const API = { WHEEL, ALL, FXS, CAP, DEFAULTS, PRESETS, norm, surprise, fit, Plan, estimate, Sender, SimOut, linkOut, mulberry32 };
  root.ShowEngine = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : globalThis);
