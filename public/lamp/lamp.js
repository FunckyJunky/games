/* LAMP.EXE link: talks to the IR-LAMP (an ESP32 with an IR LED, firmware esp32/IrLamp) through
   the room /api/room/ir-lamp/<CODE>. The board shows the 6-letter LINK CODE on its own web page.

   const link = new LampLink({ code, name: 'LAMP', onChange(state, why), onStatus(online, info), onEvent(ev), onMsg(data) });
   link.press('r')            send one key of the remote (see LAMP_KEYS)
   link.code('00F720DF', 3)   send any NEC code (3 times)
   link.button(4)             send the board's own saved button #4 (learned signal)
   link.clear()               drop what's still waiting in the board's queue
   link.send({t: ...})        any message to the board (alarms: see esp32/IrLamp/README.md)
   link.state                 { on, key, level, mode } as far as we know (rebuilt from what the board sent)

   const drv = new LampDriver(link)   for games: say what the lamp should look like, it sends what's needed
   drv.set({ on: false })  /  drv.set({ key: 'r', level: 3 })  /  drv.set({ mode: 'smooth' })   (now: true = no waiting)
   drv.calibrate(key)      on + all the way down (so the brightness is known) + that colour
   drv.tick()              call ~10 times a second (LampKit does it)

   LampKit({ name, onOnline })  the lamp pill, SYNC button and code box that the lamp games share
   (elements #lampSw #lampTxt #syncBtn #lampLine #codeRow #codeIn #codeBtn #backLink, all optional)

   Messages, page -> board (sent "to" the board only):  {t:"ir", c, r} {t:"btn", i} {t:"clr"} {t:"hi"} + alarm ones
   board -> everyone:  {t:"dev", ...} {t:"tx", c, f} {t:"rx", c} {t:"full"} {t:"alarms", al} {t:"alarm", ...} */
(function () {
  'use strict';

  // The 24-key RGB remote, row by row as on the remote. rgb = what the lamp looks like.
  const KEYS = [
    { id: 'up', c: '00F700FF', name: 'Brighter', short: '☀+', kind: 'dim', d: 1 },
    { id: 'down', c: '00F7807F', name: 'Dimmer', short: '☀−', kind: 'dim', d: -1 },
    { id: 'off', c: '00F740BF', name: 'Off', short: 'OFF', kind: 'power', on: false },
    { id: 'on', c: '00F7C03F', name: 'On', short: 'ON', kind: 'power', on: true },
    { id: 'r', c: '00F720DF', name: 'Red', short: 'R', rgb: '#ff2020' },
    { id: 'g', c: '00F7A05F', name: 'Green', short: 'G', rgb: '#16ff3a' },
    { id: 'b', c: '00F7609F', name: 'Blue', short: 'B', rgb: '#2a3cff' },
    { id: 'w', c: '00F7E01F', name: 'White', short: 'W', rgb: '#f2f5ff' },
    { id: 'r1', c: '00F710EF', name: 'Orange', rgb: '#ff5414' },
    { id: 'g1', c: '00F7906F', name: 'Green 1', rgb: '#1cff8c' },
    { id: 'b1', c: '00F750AF', name: 'Blue 1', rgb: '#4f3cff' },
    { id: 'flash', c: '00F7D02F', name: 'Flash', short: 'FLASH', kind: 'fx' },
    { id: 'r2', c: '00F730CF', name: 'Light orange', rgb: '#ff8a1c' },
    { id: 'g2', c: '00F7B04F', name: 'Green 2', rgb: '#16ffc8' },
    { id: 'b2', c: '00F7708F', name: 'Blue 2', rgb: '#7c2cff' },
    { id: 'strobe', c: '00F7F00F', name: 'Strobe', short: 'STROBE', kind: 'fx' },
    { id: 'r3', c: '00F708F7', name: 'Yellow', rgb: '#ffc21c' },
    { id: 'g3', c: '00F78877', name: 'Green 3', rgb: '#16e6ff' },
    { id: 'b3', c: '00F748B7', name: 'Blue 3', rgb: '#b52cff' },
    { id: 'fade', c: '00F7C837', name: 'Fade', short: 'FADE', kind: 'fx' },
    { id: 'r4', c: '00F728D7', name: 'Yellow green', rgb: '#e6ff2c' },
    { id: 'g4', c: '00F7A857', name: 'Green 4', rgb: '#1cb0ff' },
    { id: 'b4', c: '00F76897', name: 'Blue 4', rgb: '#ff2cd2' },
    { id: 'smooth', c: '00F7E817', name: 'Smooth', short: 'SMOOTH', kind: 'fx' },
  ];
  // learned from the real remote (ir_signals.txt); the 4 effect keys are the standard codes of that remote
  const LEARNED = new Set(KEYS.filter((k) => k.kind !== 'fx').map((k) => k.c));
  const BY_ID = {}, BY_CODE = {};
  for (const k of KEYS) { BY_ID[k.id] = k; BY_CODE[k.c] = k; }
  const COLORS = KEYS.filter((k) => k.rgb);
  const LEVELS = 8;                                   // brightness steps we count (0 = dimmest, which is still ~30 %)
  const MIN_LIGHT = 0.3;                              // the lamp can't dim below about 30 %: darker = OFF

  const hexRgb = (h) => { const n = parseInt(h.slice(1), 16); return [n >> 16 & 255, n >> 8 & 255, n & 255]; };
  // the remote colour closest to any colour (rgb array or #hex)
  function nearestKey(rgb) {
    if (typeof rgb === 'string') rgb = hexRgb(rgb);
    let best = null, bd = 1e9;
    for (const k of COLORS) {
      const [r, g, b] = hexRgb(k.rgb);
      const d = (r - rgb[0]) ** 2 * 0.3 + (g - rgb[1]) ** 2 * 0.59 + (b - rgb[2]) ** 2 * 0.11;
      if (d < bd) { bd = d; best = k; }
    }
    return best;
  }
  // a light strength 0..1 -> what the lamp can do: off, or a brightness step
  function lightLevel(i) {
    if (i < 0.15) return -1;                           // dark: OFF
    return Math.max(0, Math.min(LEVELS - 1, Math.round((i - 0.15) / 0.85 * (LEVELS - 1))));
  }

  function freshState() { return { on: true, key: 'w', level: LEVELS - 1, mode: 'color', known: false }; }
  // what one code does to the lamp
  function apply(st, code) {
    const k = BY_CODE[code];
    if (!k) return false;
    st.known = true;
    if (k.kind === 'power') st.on = k.on;
    else if (k.kind === 'dim') { st.level = Math.max(0, Math.min(LEVELS - 1, st.level + k.d)); }
    else if (k.kind === 'fx') { st.mode = k.id; }
    else { st.key = k.id; st.mode = 'color'; }
    return true;
  }

  // what the lamp shows at time t (seconds): { rgb: [r, g, b], i: 0..1 }. The effects are a preview
  // of what these controllers usually do (strobe is drawn gently, at 4 flashes a second).
  const FX_SEQ = { flash: ['r', 'g', 'b', 'w'], strobe: ['w', 'r', 'g', 'b'], fade: ['r', 'g', 'b'],
    smooth: ['r', 'r1', 'r2', 'r3', 'r4', 'g', 'g1', 'g2', 'g3', 'g4', 'b', 'b1', 'b2', 'b3', 'b4'] };
  function look(st, t) {
    if (!st.on) return { rgb: [0, 0, 0], i: 0 };
    const base = MIN_LIGHT + (1 - MIN_LIGHT) * st.level / (LEVELS - 1);
    const col = (id) => hexRgb(BY_ID[id].rgb);
    if (st.mode === 'flash') return { rgb: col(FX_SEQ.flash[Math.floor(t / 0.6) % 4]), i: base };
    if (st.mode === 'strobe') return { rgb: col(FX_SEQ.strobe[Math.floor(t / 1.2) % 4]), i: (t * 4) % 1 < 0.5 ? base : base * 0.15 };
    if (st.mode === 'fade') {
      const p = t / 3, k = Math.floor(p) % 3, f = p % 1;
      return { rgb: col(FX_SEQ.fade[k]), i: base * (0.5 - 0.5 * Math.cos(f * Math.PI * 2)) };
    }
    if (st.mode === 'smooth') {
      const L = FX_SEQ.smooth, p = t / 1.2, k = Math.floor(p) % L.length, f = p % 1;
      const a = col(L[k]), b = col(L[(k + 1) % L.length]);
      return { rgb: a.map((v, j) => Math.round(v + (b[j] - v) * f)), i: base };
    }
    return { rgb: col(st.key || 'w'), i: base };
  }

  function cleanCode(c) { return String(c || '').toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 8); }

  class LampLink {
    constructor(o) {
      this.o = o || {};
      this.codeStr = cleanCode(this.o.code);
      this.state = freshState();
      this.online = false;
      this.info = null;            // the board's dev message
      this.devId = null;
      this.ws = null;
      this.tries = 0;
      this.closed = false;
      this.pingT = null;
      this.listeners = [];
      if (this.codeStr.length >= 3) this.open();
    }
    listen(fn) { this.listeners.push(fn); }
    setCode(c) {
      this.codeStr = cleanCode(c);
      this.close();
      this.closed = false;
      this.online = false; this.devId = null; this.info = null;
      this.state = freshState();
      this._status();
      if (this.codeStr.length >= 3) this.open();
    }
    open() {
      const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
      const name = encodeURIComponent(this.o.name || 'LAMP');
      let ws;
      try { ws = new WebSocket(`${proto}//${location.host}/api/room/ir-lamp/${this.codeStr}?name=${name}`); }
      catch (e) { this._retry(); return; }
      this.ws = ws;
      ws.onopen = () => {
        this.tries = 0;
        clearInterval(this.pingT);
        this.pingT = setInterval(() => { try { ws.send('{"type":"ping"}'); } catch (e) {} }, 25000);
      };
      ws.onmessage = (e) => { let m; try { m = JSON.parse(e.data); } catch (x) { return; } this._msg(m); };
      ws.onclose = () => {
        clearInterval(this.pingT);
        if (this.ws !== ws) return;
        this.ws = null;
        if (this.online) { this.online = false; this._status(); }
        if (!this.closed) this._retry();
      };
    }
    close() { this.closed = true; clearInterval(this.pingT); if (this.ws) { const w = this.ws; this.ws = null; try { w.close(); } catch (e) {} } }
    _retry() { const d = Math.min(15000, 1000 * 2 ** this.tries++); setTimeout(() => { if (!this.closed && !this.ws) this.open(); }, d); }
    _status() { this.o.onStatus && this.o.onStatus(this.online, this.info); }
    _msg(m) {
      if (m.type === 'welcome') {
        this.myId = m.id;
        this.names = {};
        for (const p of m.players || []) this.names[p.id] = p.name;
        const d = (m.players || []).find((p) => p.name === 'IR-LAMP');
        this.devId = d ? d.id : null;
        if (!d) { this.online = false; this._status(); }
        else this._send({ t: 'hi' });
        return;
      }
      if (m.type === 'join' && m.player) { (this.names = this.names || {})[m.player.id] = m.player.name; if (m.player.name === 'IR-LAMP') this.devId = m.player.id; return; }
      if (m.type === 'leave' && m.id === this.devId) { this.devId = null; this.online = false; this._status(); return; }
      if (m.type !== 'msg' || !m.data || typeof m.data !== 'object') return;
      if (this.devId && m.from !== this.devId) return;       // other pages talking to the board
      const d = m.data;
      if (d.t === 'dev') {
        this.devId = m.from;
        this.info = d;
        const st = freshState();
        for (const c of d.h || []) apply(st, c);
        if (d.h && d.h.length) st.known = true;
        this.state = st;
        const was = this.online;
        this.online = true;
        this._status();
        this.o.onChange && this.o.onChange(this.state, was ? 'sync' : 'hello');
      } else if (d.t === 'tx' || d.t === 'rx') {
        if (!this.online) { this.online = true; this._status(); }
        const changed = d.c ? apply(this.state, d.c) : false;
        const who = d.t === 'rx' ? 'remote' : d.f === 'local' ? 'board page' : d.f === 'alarm' ? 'alarm' : d.f === this.myId ? 'you' : ((this.names || {})[d.f] || 'another page');
        const ev = { type: d.t, code: d.c || d.code, key: d.c ? BY_CODE[d.c] : null, from: d.f, who, mine: !!d.f && d.f === this.myId, proto: d.p, queue: d.q };
        for (const fn of this.listeners) fn(ev);
        this.o.onEvent && this.o.onEvent(ev);
        if (changed) this.o.onChange && this.o.onChange(this.state, d.t);
      } else if (d.t === 'full') {
        this.o.onEvent && this.o.onEvent({ type: 'full' });
      } else {
        if (d.t === 'alarms' && this.info) this.info.al = d.al;
        this.o.onMsg && this.o.onMsg(d);
      }
    }
    _send(data) {
      if (!this.ws || this.ws.readyState !== 1) return false;
      this.ws.send(JSON.stringify(this.devId ? { to: this.devId, data } : data));
      return true;
    }
    send(data) { return this._send(data); }
    get ready() { return this.online && !!this.ws && this.ws.readyState === 1; }
    code(c, r) { return this._send(r > 1 ? { t: 'ir', c, r: Math.min(10, r | 0) } : { t: 'ir', c }); }
    press(id, r) { const k = BY_ID[id]; return k ? this.code(k.c, r) : false; }
    button(i) { return this._send({ t: 'btn', i }); }
    clear() { return this._send({ t: 'clr' }); }
  }

  // Makes the real lamp follow what a game wants, without flooding the IR queue:
  // one step at a time (on/off, effect, colour, brightness), only when the wish holds for a moment.
  class LampDriver {
    constructor(link, o) {
      o = o || {};
      this.link = link;
      this.stable = o.stable == null ? 350 : o.stable;          // ms a new colour / on / off must hold
      this.levelStable = o.levelStable == null ? 550 : o.levelStable;
      this.target = null;
      this.inflight = 0; this.lastTx = 0; this.busyUntil = 0;
      this.sig = ''; this.since = 0; this.lvWant = -9; this.lvSince = 0;
      link.listen((ev) => { if (ev.type === 'tx' && ev.mine) { this.inflight = Math.max(0, this.inflight - 1); this.lastTx = Date.now(); } });
    }
    set(t) { this.target = t; }
    _press(id, r) { if (this.link.press(id, r)) { this.inflight += r || 1; this.lastTx = Date.now(); } }
    // on, then all the way down (the brightness is then known), then a colour
    calibrate(key) {
      if (!this.link.ready) return false;
      this.link.clear();
      this.inflight = 0;
      this._press('on'); this._press('down', 8); this._press(key || 'w');
      this.busyUntil = Date.now() + 3300;
      return true;
    }
    tick() {
      const L = this.link, t = this.target, now = Date.now();
      if (!t || !L.ready || now < this.busyUntil) return;
      if (now - this.lastTx > 3000) this.inflight = 0;          // a confirmation got lost
      if (this.inflight > 0) return;                            // one thing at a time: wait for the board's "sent"
      const st = L.state;
      const sig = t.on === false ? 'off' : (t.mode && t.mode !== 'color' ? t.mode : t.key);
      if (sig !== this.sig) { this.sig = sig; this.since = now; }
      const ok = t.now || now - this.since >= this.stable;
      if (t.on === false) { if (st.on && ok) this._press('off'); return; }
      if (!st.on) { if (ok) this._press('on'); return; }
      if (t.mode && t.mode !== 'color') { if (st.mode !== t.mode) { if (ok) this._press(t.mode); return; } }
      else if (t.key && (st.mode !== 'color' || st.key !== t.key)) { if (ok) this._press(t.key); return; }
      if (t.level != null && t.level >= 0) {
        if (t.level !== this.lvWant) { this.lvWant = t.level; this.lvSince = now; }
        if (st.level !== t.level && (t.now || now - this.lvSince >= this.levelStable)) {
          const d = t.level - st.level;
          this._press(d > 0 ? 'up' : 'down', Math.min(4, Math.abs(d)));
        }
      }
    }
  }

  // the link code: ?code=XXXXXX in the address, else the last one used on this browser
  function savedCode() {
    const q = new URLSearchParams(location.search).get('code');
    if (q && cleanCode(q).length >= 3) { try { localStorage.setItem('lamp.code', cleanCode(q)); } catch (e) {} return cleanCode(q); }
    try { return localStorage.getItem('lamp.code') || ''; } catch (e) { return ''; }
  }
  function saveCode(c) { try { localStorage.setItem('lamp.code', cleanCode(c)); } catch (e) {} }

  // What the lamp games share: link + driver, the lamp pill in the HUD, SYNC on/off and the code box.
  function LampKit(o) {
    o = o || {};
    const $ = (id) => document.getElementById(id);
    const k = { link: null, driver: null, sync: true, wasOn: false, label: '' };
    function ui() {
      const L = k.link, on = !!(L && L.online);
      if ($('lampLine')) $('lampLine').textContent = !L ? 'LAMP: not linked — type the code from the ESP32 page, or play without' :
        on ? 'LAMP: ● online (' + L.codeStr + '). It plays along.' : 'LAMP: ' + L.codeStr + ' — offline, waiting for the ESP32…';
      if ($('codeRow')) $('codeRow').classList.toggle('hide', on);
      const st = on ? L.state : null, lk = st ? look(st, performance.now() / 1000) : null;
      if ($('lampTxt')) $('lampTxt').textContent = !L ? 'NO LAMP' : !on ? 'LAMP OFFLINE' : !k.sync ? 'SYNC OFF' :
        Date.now() < k.driver.busyUntil ? 'SYNCING…' : !st.on ? 'LAMP OFF' : (st.mode === 'color' ? BY_ID[st.key].name : st.mode).toUpperCase();
      if ($('lampSw')) $('lampSw').style.background = lk && lk.i > 0 ? `rgb(${lk.rgb.join(',')})` : '#000';
      if ($('syncBtn')) $('syncBtn').textContent = k.sync ? 'SYNC ON' : 'SYNC OFF';
      if ($('backLink')) $('backLink').href = '/lamp/' + (L ? '?code=' + L.codeStr : '');
    }
    function start() {
      const code = savedCode();
      if (k.link) k.link.close();
      k.link = null; k.driver = null; k.wasOn = false;
      if (code) {
        k.link = new LampLink({ code, name: o.name || 'GAME',
          onStatus: () => { ui(); if (k.link.online && !k.wasOn && o.onOnline) o.onOnline(); k.wasOn = k.link.online; },
          onChange: ui, onEvent: o.onEvent, onMsg: o.onMsg });
        k.driver = new LampDriver(k.link, o.driver);
      }
      ui();
    }
    k.ready = () => !!(k.link && k.link.ready && k.sync);
    k.set = (t) => { if (k.driver) k.driver.set(t); };
    k.calibrate = (key) => { if (k.ready()) k.driver.calibrate(key); ui(); };
    k.ui = ui;
    setInterval(() => { if (k.driver && k.sync) k.driver.tick(); ui(); }, 110);
    if ($('syncBtn')) $('syncBtn').onclick = () => { k.sync = !k.sync; ui(); if (k.sync && o.onSyncOn) o.onSyncOn(); };
    if ($('codeBtn')) $('codeBtn').onclick = () => { const c = cleanCode($('codeIn').value); if (c.length < 3) return; saveCode(c); start(); };
    if ($('codeIn')) $('codeIn').onkeydown = (e) => { e.stopPropagation(); if (e.key === 'Enter') $('codeBtn').click(); };
    start();
    return k;
  }

  // a browser time zone (IANA name) as a POSIX TZ string for the board (same as Home Front's)
  function posixTZ(tz) {
    let f;
    try { f = new Intl.DateTimeFormat('en-US', { timeZone: tz, hourCycle: 'h23', year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' }); } catch (e) { return ''; }
    const off = (ms) => {
      const q = {}; for (const x of f.formatToParts(new Date(ms))) q[x.type] = +x.value;
      return Math.round((Date.UTC(q.year, q.month - 1, q.day, q.hour % 24, q.minute, q.second) - Math.floor(ms / 1000) * 1000) / 60000);
    };
    const hm = (m) => { const a = Math.abs(m), s = (m < 0 ? '-' : '') + Math.floor(a / 60); return a % 60 ? s + ':' + String(a % 60).padStart(2, '0') : s; };
    const y = new Date().getUTCFullYear(), t0 = Date.UTC(y, 0, 1), t1 = Date.UTC(y + 1, 0, 1), DAY = 86400000;
    const changes = [];
    let prev = off(t0);
    for (let t = t0 + DAY; t <= t1; t += DAY) {
      const o = off(t);
      if (o === prev) continue;
      let lo = t - DAY, hi = t;
      while (hi - lo > 60000) { const mid = lo + Math.floor((hi - lo) / 120000) * 60000; if (off(mid) === prev) lo = mid; else hi = mid; }
      changes.push({ at: hi, from: prev, to: o });
      prev = o;
    }
    const offs = [off(t0), off(Date.UTC(y, 6, 1))], std = Math.min(...offs), dst = Math.max(...offs);
    if (changes.length !== 2 || std === dst) return 'STD' + hm(-off(Date.now()));
    const rule = (c) => {
      const l = new Date(c.at + c.from * 60000), day = l.getUTCDate(), dim = new Date(Date.UTC(l.getUTCFullYear(), l.getUTCMonth() + 1, 0)).getUTCDate();
      const wk = day + 7 > dim ? 5 : Math.ceil(day / 7), mins = l.getUTCHours() * 60 + l.getUTCMinutes();
      return 'M' + (l.getUTCMonth() + 1) + '.' + wk + '.' + l.getUTCDay() + '/' + hm(mins);
    };
    const on = changes.find((c) => c.to === dst), offc = changes.find((c) => c.to === std);
    return 'STD' + hm(-std) + 'DST' + hm(-dst) + ',' + rule(on) + ',' + rule(offc);
  }

  window.LAMP_KEYS = KEYS;
  window.LAMP = { KEYS, BY_ID, BY_CODE, COLORS, LEVELS, MIN_LIGHT, LEARNED, nearestKey, lightLevel, hexRgb, apply, look, freshState, savedCode, saveCode, cleanCode, posixTZ };
  window.LampLink = LampLink;
  window.LampDriver = LampDriver;
  window.LampKit = LampKit;
})();
