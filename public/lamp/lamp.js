/* LAMP.EXE link: talks to the IR-LAMP (an ESP32 with an IR LED, firmware esp32/IrLamp) through
   the room /api/room/ir-lamp/<CODE>. The board shows the 6-letter LINK CODE on its own web page.

   const link = new LampLink({ code, name: 'LAMP', onChange(state, why), onStatus(online, info), onEvent(ev) });
   link.press('r')            send one key of the remote (see LAMP_KEYS)
   link.code('00F720DF', 3)   send any NEC code (3 times)
   link.button(4)             send the board's own saved button #4 (learned signal)
   link.clear()               drop what's still waiting in the board's queue
   link.state                 { on, key, level, mode } as far as we know (rebuilt from what the board sent)

   Messages, page -> board (sent "to" the board only):  {t:"ir", c, r} {t:"btn", i} {t:"clr"} {t:"hi"}
   board -> everyone:  {t:"dev", n, v, ip, rssi, b:[[name, protocol, code]], h:[last NEC codes]}
                       {t:"tx", c, f}  sent (f = who asked)   {t:"rx", c}  the real remote was pressed
                       {t:"full"}  the board's queue is full */
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
  // learned from the real remote (ir_signals.txt); the 4 effect keys are the standard codes
  const LEARNED = new Set(KEYS.filter((k) => k.kind !== 'fx').map((k) => k.c));
  const BY_ID = {}, BY_CODE = {};
  for (const k of KEYS) { BY_ID[k.id] = k; BY_CODE[k.c] = k; }
  const COLORS = KEYS.filter((k) => k.rgb);
  const LEVELS = 8;                                   // brightness steps we count (0 = dimmest)

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

  function freshState() { return { on: true, key: 'w', level: LEVELS - 1, mode: 'color', known: false }; }
  // what one code does to the lamp
  function apply(st, code) {
    const k = BY_CODE[code];
    if (!k) return false;
    st.known = true;
    if (k.kind === 'power') st.on = k.on;
    else if (k.kind === 'dim') { st.level = Math.max(0, Math.min(LEVELS - 1, st.level + k.d)); st.on = true; }
    else if (k.kind === 'fx') { st.mode = k.id; st.on = true; }
    else { st.key = k.id; st.mode = 'color'; }
    return true;
  }

  // what the lamp shows at time t (seconds): { rgb: [r, g, b], i: 0..1 }. The effects are a preview
  // of what these controllers usually do (strobe is drawn gently, at 4 flashes a second).
  const FX_SEQ = { flash: ['r', 'g', 'b', 'w'], strobe: ['w', 'r', 'g', 'b'], fade: ['r', 'g', 'b'],
    smooth: ['r', 'r1', 'r2', 'r3', 'r4', 'g', 'g1', 'g2', 'g3', 'g4', 'b', 'b1', 'b2', 'b3', 'b4'] };
  function look(st, t) {
    if (!st.on) return { rgb: [0, 0, 0], i: 0 };
    const base = 0.22 + 0.78 * st.level / (LEVELS - 1);
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
      if (this.codeStr.length >= 3) this.open();
    }
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
        this.state = st;
        const was = this.online;
        this.online = true;
        this._status();
        this.o.onChange && this.o.onChange(this.state, was ? 'sync' : 'hello');
      } else if (d.t === 'tx' || d.t === 'rx') {
        if (!this.online) { this.online = true; this._status(); }
        const changed = d.c ? apply(this.state, d.c) : false;
        const who = d.t === 'rx' ? 'remote' : d.f === 'local' ? 'board page' : d.f === this.myId ? 'you' : ((this.names || {})[d.f] || 'another page');
        this.o.onEvent && this.o.onEvent({ type: d.t, code: d.c || d.code, key: d.c ? BY_CODE[d.c] : null, from: d.f, who, mine: !!d.f && d.f === this.myId, proto: d.p, queue: d.q });
        if (changed) this.o.onChange && this.o.onChange(this.state, d.t);
      } else if (d.t === 'full') {
        this.o.onEvent && this.o.onEvent({ type: 'full' });
      }
    }
    _send(data) {
      if (!this.ws || this.ws.readyState !== 1) return false;
      this.ws.send(JSON.stringify(this.devId ? { to: this.devId, data } : data));
      return true;
    }
    get ready() { return this.online && !!this.ws && this.ws.readyState === 1; }
    code(c, r) { return this._send(r > 1 ? { t: 'ir', c, r: Math.min(10, r | 0) } : { t: 'ir', c }); }
    press(id, r) { const k = BY_ID[id]; return k ? this.code(k.c, r) : false; }
    button(i) { return this._send({ t: 'btn', i }); }
    clear() { return this._send({ t: 'clr' }); }
  }

  // the link code: ?code=XXXXXX in the address, else the last one used on this browser
  function savedCode() {
    const q = new URLSearchParams(location.search).get('code');
    if (q && cleanCode(q).length >= 3) { try { localStorage.setItem('lamp.code', cleanCode(q)); } catch (e) {} return cleanCode(q); }
    try { return localStorage.getItem('lamp.code') || ''; } catch (e) { return ''; }
  }
  function saveCode(c) { try { localStorage.setItem('lamp.code', cleanCode(c)); } catch (e) {} }

  window.LAMP_KEYS = KEYS;
  window.LAMP = { KEYS, BY_ID, BY_CODE, COLORS, LEVELS, LEARNED, nearestKey, hexRgb, apply, look, freshState, savedCode, saveCode, cleanCode };
  window.LampLink = LampLink;
})();
