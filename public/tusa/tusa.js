// Туса У Берліні — three friends, one favourite place in Berlin: Hoongy.
'use strict';
(() => {
const A = ART;
const $ = id => document.getElementById(id);
const view = $('view'), vctx = view.getContext('2d');
const lctx = vctx;   // the world is drawn straight onto the screen canvas, sharp at any size
const stage = $('stage'), dlg = $('dlg');
let LW = 240, LH = 180, SCALE = 2, DPR = 1, CW = 480, CH = 360;

// =====================================================================
// words: every text has its own fixed language, Ukrainian or Dutch
// (never picked at random), so it always reads the same
// =====================================================================
const pick = a => a[Math.random() * a.length | 0];
const FOOD = {
  lassi: { ico: '🥭', name: 'Mango-Lassi', d: 'холодний манговий напій з йогуртом', p: '5,90' },
  coffee: { ico: '☕', name: 'Vietnamese koffie', d: 'міцна й солодка, зі згущеним молоком', p: '4,50' },
  shrimp: { ico: '🍤', name: 'Garnalen', d: 'хрусткі, з чилі та лаймом', p: '12,90' },
  fish: { ico: '🐟', name: 'Vis', d: 'ціла риба з імбиром', p: '15,50' },
};
const NAMES = {
  VITO: ['VITO'],
  VIKA: ['VIKA', 'VICA', 'VICTORIA', 'VIKTORIA', 'VIKTORIIA', 'VOCTORIA', 'VIKTORIJA', 'VIKUSYA'],
  OLEKSIY: ['OLEKSIY', 'OLEKSII', 'OLEKSIJ', 'OLEKSI', 'ALEKSEY', 'OLEXIY', 'OLEKSIJ'],
};
const HOME = {
  // gate: the exit of the camp; through it you leave for Berlin, and come home
  VITO: { title: 'CAMP LION', sub: 'România · Румунія', draw: A.campLion, door: 75, exitDir: 1, W: 540, gate: 468, sign: 'CAMP LION' },
  VIKA: { title: 'ALTENGRABOW', sub: 'Sachsen-Anhalt · Німеччина', draw: A.altengrabow, door: 79, exitDir: 1, W: 440, gate: 370, sign: 'ALTENGRABOW' },
  OLEKSIY: { title: 'LEHNIN', sub: 'Brandenburg · Німеччина', draw: A.lehnin, door: 62, exitDir: 1, W: 420, gate: 350, sign: 'LEHNIN' },
};
const ARRIVE = { VITO: 'taxi', VIKA: 'amarok', OLEKSIY: 'scooter' };
const TAG = { VITO: '#7fd3ff', VIKA: '#ff9ec0', OLEKSIY: '#b8e07a' };

// =====================================================================
// settings shared with ERROR-DOS (sound on/off)
// =====================================================================
let SET = { sound: true };
try { Object.assign(SET, JSON.parse(localStorage.getItem('errordos-settings') || '{}')); } catch (e) {}

// =====================================================================
// sound: synthesized effects
// =====================================================================
let ac = null, master = null;
function audio() {
  if (!SET.sound) return null;
  if (!ac) {
    try { ac = new (window.AudioContext || window.webkitAudioContext)(); master = ac.createGain(); master.gain.value = 0.9; master.connect(ac.destination); } catch (e) { return null; }
  }
  if (ac.state === 'suspended') ac.resume();
  return ac;
}
function tone(f, d, v, type, when, f2) {
  const a = audio(); if (!a) return;
  const t = a.currentTime + (when || 0), o = a.createOscillator(), g = a.createGain();
  o.type = type || 'square'; o.frequency.setValueAtTime(f, t); if (f2) o.frequency.exponentialRampToValueAtTime(f2, t + d);
  g.gain.setValueAtTime(v, t); g.gain.exponentialRampToValueAtTime(0.0001, t + d);
  o.connect(g).connect(master); o.start(t); o.stop(t + d + 0.03);
}
let nbuf = null;
function noise(d, v, f, q, when) {
  const a = audio(); if (!a) return;
  if (!nbuf) { nbuf = a.createBuffer(1, a.sampleRate, a.sampleRate); const x = nbuf.getChannelData(0); for (let i = 0; i < x.length; i++) x[i] = Math.random() * 2 - 1; }
  const t = a.currentTime + (when || 0), s = a.createBufferSource(), bf = a.createBiquadFilter(), g = a.createGain();
  s.buffer = nbuf; bf.type = 'bandpass'; bf.frequency.value = f; bf.Q.value = q;
  g.gain.setValueAtTime(v, t); g.gain.exponentialRampToValueAtTime(0.0001, t + d);
  s.connect(bf).connect(g).connect(master); s.start(t, Math.random() * 0.5); s.stop(t + d + 0.03);
}
const SFX = {
  click() { noise(0.02, 0.2, 3600, 0.8); },
  joke() { tone(150, 0.12, 0.12, 'sine', 0, 90); tone(120, 0.14, 0.12, 'sine', 0.16, 70); noise(0.35, 0.1, 7000, 0.6, 0.34); },
  laugh(char, lv) {
    const base = { VITO: 300, VIKA: 520, OLEKSIY: 230 }[char] || 400, n = 3 + (lv || 1) * 3;
    for (let i = 0; i < n; i++) tone(base * (1 + (i % 2) * 0.15 + (lv >= 3 ? 0.1 : 0)), 0.08, 0.03 + (lv || 1) * 0.006, 'triangle', i * (lv >= 3 ? 0.1 : 0.12), base * 0.8);
  },
  crickets() { for (let i = 0; i < 6; i++) tone(4200, 0.04, 0.012, 'sine', 0.2 + i * 0.09 + (i > 2 ? 0.5 : 0), 4000); },
  knob() { noise(0.03, 0.25, 2400, 1.5); tone(90, 0.05, 0.05, 'square'); },
  shutter() { noise(0.04, 0.3, 3000, 1.2); noise(0.06, 0.2, 1200, 1, 0.07); tone(1800, 0.05, 0.02, 'square', 0.02); },
  yum() { noise(0.05, 0.1, 1600, 1.2); tone(220, 0.4, 0.035, 'sine', 0.08, 260); tone(330, 0.3, 0.02, 'triangle', 0.25, 300); },
  heart() { tone(880, 0.09, 0.05, 'sine'); tone(1318, 0.16, 0.05, 'sine', 0.09); },
  hug() { tone(392, 0.3, 0.04, 'sine', 0, 523); tone(523, 0.35, 0.03, 'sine', 0.2, 440); },
  boop() { for (let i = 0; i < 4; i++) tone(i % 2 ? 990 : 760, 0.09, 0.045, 'sine', 0.15 + i * 0.32, i % 2 ? 760 : 1180); },
  door() { tone(1568, 0.25, 0.04, 'sine'); tone(1175, 0.4, 0.04, 'sine', 0.18); },
  bell() { tone(1200, 0.5, 0.05, 'triangle'); tone(1200, 0.5, 0.05, 'triangle', 0.28); },
  car() { noise(0.9, 0.05, 240, 0.7); },
  plane() { noise(2.2, 0.06, 500, 0.5); },
  eat() { noise(0.05, 0.12, 1800, 1.2); noise(0.05, 0.1, 1400, 1.2, 0.09); },
  pop() { tone(660, 0.06, 0.05, 'square', 0, 990); },
  step() { noise(0.03, 0.03, 700, 1); },
};

// =====================================================================
// layout
// =====================================================================
const coarse = matchMedia('(pointer: coarse)').matches || 'ontouchstart' in window;
if (!coarse) { $('pad').classList.add('hidden'); $('keys').style.display = 'block'; }
function layout() {
  const sw = stage.clientWidth, sh = stage.clientHeight;
  if (sh > sw * 0.9) {
    // portrait phone: keep about 215 pixels across and show extra height above
    SCALE = sw / 215;
    LW = Math.round(sw / SCALE);
    LH = Math.max(180, Math.min(360, Math.floor(sh / SCALE)));
  } else {
    let ch = Math.min(sh, sw * 0.78);
    let cw = Math.min(sw, ch * 430 / 180);
    if (cw < ch * 1.2) ch = Math.min(sh, cw / 1.2);
    SCALE = ch / 180;
    LW = Math.max(180, Math.round(cw / SCALE)); LH = 180;
  }
  DPR = Math.min(2, window.devicePixelRatio || 1);
  CW = Math.round(LW * SCALE); CH = Math.round(LH * SCALE);
  view.style.width = CW + 'px'; view.style.height = CH + 'px';
  view.width = Math.round(CW * DPR); view.height = Math.round(CH * DPR);
  A.setScale(SCALE * DPR);
  DM.cols = 0; LOGO.w = 0;   // the screens at the top re-measure themselves
}
addEventListener('resize', layout);

// =====================================================================
// game state
// =====================================================================
const G = {
  scene: 'login', me: null, t: 0,
  p: { x: 0, y: 150, dir: 1, phase: 0, moving: false, anim: '', animUntil: 0, seat: -1, angle: -Math.PI / 2, idle: 0 },
  order: null, menuShownFor: 0,
  camX: 0, camY: 0,
  base: null, travel: null, arrival: null, trans: null,
  npcs: [], cars: [], leaves: [], hearts: [], notes: [],
  talk: {}, jokes: {}, blush: {}, hugs: {}, toast: null, yum: {}, zoom: 1,
  others: {},
  nextPed: 0, nextCar: 0, nextTram: 8, nextSoldier: 0,
  prompt: '', closedShown: false,
};

// ---------- network ----------
const NET = { ws: null, ok: false, offset: 0, g: { clockStart: 0, radio: { ch: 0, since: 0 } }, lastSent: 0, lastKey: '', want: false, retry: null };
const snow = () => Date.now() + NET.offset;
function connect(arrive, x) {
  NET.want = true;
  clearTimeout(NET.retry);
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  const ws = new WebSocket(proto + '//' + location.host + '/api/tusa?char=' + G.me + '&arrive=' + (arrive || '') + '&x=' + Math.round(x || G.p.x));
  NET.ws = ws;
  ws.onopen = () => { NET.ok = true; NET.ping = setInterval(() => ws.readyState === 1 && ws.send('{"t":"ping"}'), 25000); };
  ws.onmessage = ev => { let m; try { m = JSON.parse(ev.data); } catch (e) { return; } onNet(m); };
  ws.onclose = () => {
    clearInterval(NET.ping);
    if (NET.ws !== ws) return;
    NET.ok = false;
    if (NET.want && (G.scene === 'street' || G.scene === 'inside' || G.scene === 'enter')) NET.retry = setTimeout(() => connect('', G.p.x), 2500);
  };
}
function disconnect() {
  NET.want = false; NET.ok = false;
  if (NET.ws) { try { NET.ws.close(1000, 'bye'); } catch (e) {} }
  NET.ws = null; G.others = {};
  radioStop(); voiceStop(true); updateMicBtn();
}
function send(obj) { if (NET.ok && NET.ws && NET.ws.readyState === 1) NET.ws.send(JSON.stringify(obj)); }
function sendState(force) {
  const now = performance.now();
  if (!force && now - NET.lastSent < 110) return;
  const s = { t: 'st', x: Math.round(G.p.x * 10) / 10, y: Math.round(G.p.y * 10) / 10, dir: G.p.dir, scene: G.scene === 'inside' ? 'inside' : 'street', anim: G.p.anim || (G.p.moving ? 'walk' : ''),
    ride: G.ride ? 1 : 0, scoot: G.scootX == null ? -1 : Math.round(G.scootX), sd: G.scootDir || 1,
    lg: myLaugh().laugh, rf: myLaugh().anim === 'rofl' ? 1 : 0, a: Math.round((G.p.angle || 0) * 100) / 100 };
  const key = JSON.stringify(s);
  if (!force && key === NET.lastKey) return;
  NET.lastKey = key; NET.lastSent = now;
  send(s);
}
function onNet(m) {
  if (m.now) NET.offset = m.now - Date.now();
  if (m.type === 'welcome') {
    G.others = {};
    for (const p of m.players) addOther(p, true);
    NET.g = m.g;
    NET.welcomed = true;
    if (VOICE.on) send({ t: 'voice', on: 1 });                  // back after a hiccup: still talking
    if (m.players.length && !VOICE.on) setTimeout(() => toast('🎙 Tik op PRATEN om met je vrienden te praten', 3.5), 3500);
    // friends already here? then nobody is being waited for: no commentary
    if (m.players.length) G.voiceDone = true;
    radioSync();
    sendState(true);
    updateRadioBtn();
  } else if (m.type === 'p') {
    if (m.p.char === G.me) { G.p.seat = m.p.seat; G.order = m.p.order; return; }
    const prev = G.others[m.p.char], pb = prev && prev.order ? prev.order.bites || 0 : 0, nb = m.p.order ? m.p.order.bites || 0 : 0;
    if (prev && nb > pb) enjoy(m.p.char, nb);
    addOther(m.p, false);
  } else if (m.type === 'gone') {
    const o = G.others[m.char];
    if (o) { hangup(m.char); delete G.others[m.char]; updateMicBtn(); }
  } else if (m.type === 'emote') {
    handleEmote(m);
  } else if (m.type === 'rtc') {
    onRTC(m);
  } else if (m.type === 'g') {
    const was = NET.g.clockStart;
    NET.g = m.g;
    if (!was && m.g.clockStart) { toast('18:00 · Вечір почався, кухня відкрита!', 3.5); SFX.bell(); }
    radioSync();
  } else if (m.type === 'taken') {
    toast('Bezet!', 2);
  } else if (m.type === 'kicked') {
    NET.want = false;
    showDialog({ title: 'ERROR', text: 'Je bent op een ander apparaat ingelogd.', opts: [{ ico: '↩', label: 'OK', act: () => location.reload() }] });
  }
}
function addOther(p, initial) {
  let o = G.others[p.char];
  const fresh = !o;
  if (!o) o = G.others[p.char] = { char: p.char, x: p.x, y: p.y, phase: 0 };
  // a friend going in or out of Hoongy: everyone sees the door open
  if (!fresh && o.scene && p.scene && o.scene !== p.scene) { G.doorUntil = G.t + 1.2; if (scenePlace() === 'street' || G.scene === 'inside') SFX.door(); }
  Object.assign(o, { tx: p.x, ty: p.y, dir: p.dir, scene: p.scene, anim: p.anim, seat: p.seat, order: p.order, arrive: p.arrive, arriveAt: p.arriveAt, ride: !!p.ride, scoot: p.scoot, sd: p.sd, lg: p.lg || 0, rf: !!p.rf, a: p.a, voice: !!p.voice });
  voicePeer(p.char);
  if (fresh) {
    o.x = p.x; o.y = p.y;
    // the first friend has arrived: the waiting is over
    if (!initial) { G.voiceDone = true; if (!VOICE.on) setTimeout(() => toast('🎙 ' + p.char + ' is er! Tik op PRATEN om te praten', 3.5), 2600); }
    updateMicBtn();
  }
}
function arrivingFor(o) { return o.arrive && snow() - o.arriveAt < 6500; }

// ---------- clock ----------
const CLOSE_MS = 60 * 60 * 1000; // 4 game hours at 4x
function clockInfo() {
  if (!NET.g.clockStart) return null;
  const ms = snow() - NET.g.clockStart;
  const gameMin = 18 * 60 + Math.floor(ms / 1000 * 4 / 60);
  return { closed: ms >= CLOSE_MS, h: Math.floor(Math.min(gameMin, 22 * 60) / 60), m: Math.min(gameMin, 22 * 60) % 60 };
}
function updateClock() {
  const c = clockInfo();
  if (!c) G.closedShown = false;
}

// =====================================================================
// the dot-matrix screen at the top: glowing green dots like an old DOS
// display. The big line says where you are and the time; the small line
// scrolls everything else (friends, radio, your order...) or shows a message.
// =====================================================================
const dmx = $('dmx'), dctx = dmx.getContext('2d');
const DM = { rows: 22, cols: 0, pitch: 0, src: null, s: null, grid: null, off: 0, last: 0, key: '' };
function dmLayout() {
  const r = dmx.getBoundingClientRect(), dpr = Math.min(2, window.devicePixelRatio || 1);
  if (!r.width) return;
  dmx.width = Math.round(r.width * dpr); dmx.height = Math.round(r.height * dpr);
  DM.pitch = dmx.height / DM.rows; DM.cols = Math.floor(dmx.width / DM.pitch);
  DM.src = document.createElement('canvas'); DM.src.width = DM.cols; DM.src.height = DM.rows;
  DM.s = DM.src.getContext('2d', { willReadFrequently: true });
  // the dark dots that are always there
  DM.grid = document.createElement('canvas'); DM.grid.width = dmx.width; DM.grid.height = dmx.height;
  const g = DM.grid.getContext('2d'), p = DM.pitch, ox = (dmx.width - DM.cols * p) / 2;
  g.fillStyle = '#020a05'; g.fillRect(0, 0, dmx.width, dmx.height);
  g.fillStyle = 'rgba(57,255,122,.07)'; g.beginPath();
  for (let y = 0; y < DM.rows; y++) for (let x = 0; x < DM.cols; x++) { g.moveTo(ox + (x + 0.5) * p + p * 0.36, (y + 0.5) * p); g.arc(ox + (x + 0.5) * p, (y + 0.5) * p, p * 0.36, 0, 7); }
  g.fill();
  DM.ox = ox;
}
const DM_FONT = '"Pixelify Sans", "VT323", monospace';
// emoji don't exist on a dot-matrix screen
const dmText = t => String(t || '').replace(/[\u{1F000}-\u{1FAFF}\u{2600}-\u{27BF}\u{2B00}-\u{2BFF}\u{FE0F}\u{200D}]/gu, '').replace(/\s{2,}/g, ' ').trim().toUpperCase();
function dmContent() {
  const sc = G.scene, c = clockInfo(), info = [];
  const time = c ? String(c.h).padStart(2, '0') + ':' + String(c.m).padStart(2, '0') : '--:--';
  let L = '', R = '', Ls = '';
  if (sc === 'login') { L = 'ERROR-DOS'; R = 'ТУСА.EXE'; info.push('ТУСА У БЕРЛІНІ', 'VIKA · VITO · OLEKSIY', 'WIE BEN JIJ?'); }
  else if ((sc === 'base' || sc === 'end') && G.base) {
    const H = G.base.H;
    L = H.title; R = sc === 'end' ? 'EINDE' : G.base.back ? 'THUIS' : 'BASIS'; Ls = H.title.replace(/^CAMP /i, '');
    info.push(H.sub, G.me, sc === 'end' ? 'КІНЕЦЬ ТУСИ' : G.base.back ? 'WELKOM THUIS · DOOR NAAR BINNEN = EINDE' : 'DOOR DE POORT → BERLIJN');
  } else if (sc === 'travel' && G.travel) {
    const st = G.travel.steps[G.travel.i];
    L = '→ BERLIN'; R = (G.travel.i + 1) + '/' + G.travel.steps.length;
    info.push(st.cap(), 'A: OVERSLAAN');
  } else {
    L = sc === 'inside' ? 'IN HOONGY' : 'HOONGY · BERLIN'; Ls = 'HOONGY'; R = (c ? time : '18:00') + (c && c.closed ? ' DICHT' : '');
    const friends = ['VIKA', 'VITO', 'OLEKSIY'].filter(x => x !== G.me).map(x => {
      const o = G.others[x];
      return x + (o ? (o.scene === 'inside' ? ' BINNEN' : ' BUITEN') + (o.voice ? ' MIC' : '') : ' ...');
    });
    info.push('VRIENDEN: ' + friends.join(' · '));
    if (!c) info.push('WACHTEN OP VRIENDEN');
    const ch = NET.g.radio ? NET.g.radio.ch : 0, rs = STATIONS[ch - 1];
    info.push(rs ? 'RADIO ' + rs.freq + ' ' + rs.name + (VOICE.on && sc === 'inside' ? ' (STIL: MIC AAN)' : '') : 'RADIO UIT');
    if (G.order) info.push(G.order.ph === 'wait' ? 'DE OBER KOMT' : FOOD[G.order.item] ? FOOD[G.order.item].name + ' ' + Math.min(5, G.order.bites || 0) + '/5' : '');
    else if (sc === 'inside' && G.p.seat >= 0 && !c) info.push('KEUKEN DICHT');
    if (voicePlaying()) info.push('COMMENTAAR SPEELT');
    if (VOICE.on) info.push('MIC AAN');
    if (noseReady()) info.push('NEUSJE-NEUSJE KAN NU');
    if (G.ride) info.push('OP DE STEP');
    if (!NET.ok) info.push('GEEN VERBINDING');
  }
  return { L: dmText(L), Ls: dmText(Ls || L), R: dmText(R), info: info.map(dmText).filter(Boolean).join('   ·   ') };
}
function dmDraw(now) {
  if (!DM.s || !DM.cols) { dmLayout(); if (!DM.s) return; }
  const s = DM.s, C = DM.cols, dt = Math.min(0.1, (now - (DM.last || now)) / 1000); DM.last = now;
  const ct = dmContent(), toast = G.toast && G.toast.until > G.t ? dmText(G.toast.text) : '';
  if (toast !== DM.toast) { DM.toast = toast; DM.toastAt = now; }
  s.clearRect(0, 0, C, DM.rows); s.fillStyle = '#fff'; s.textBaseline = 'alphabetic';
  // the big line
  s.font = '12px ' + DM_FONT;
  // place and time; on a small screen the short name, and if even that won't fit the time goes to the ticker
  const rw = ct.R ? s.measureText(ct.R).width + 5 : 0;
  let big = ct.L, info = ct.info;
  if (s.measureText(big).width + rw > C - 2) big = ct.Ls;
  if (s.measureText(big).width + rw > C - 2) { info = ct.R + '   ·   ' + info; ct.R = ''; }
  s.textAlign = 'left'; s.fillText(big, 1, 11);
  if (ct.R) { s.textAlign = 'right'; s.fillText(ct.R, C - 1, 11); }
  // the small line: a message, or everything else scrolling by
  s.font = '9px ' + DM_FONT; s.textAlign = 'left';
  const small = toast || info, w = s.measureText(small).width;
  if (w <= C - 2) { s.fillText(small, toast ? Math.round((C - w) / 2) : 1, 20); DM.off = 0; }
  else {
    if (DM.key !== small) { DM.key = small; DM.off = toast ? -8 : 0; }
    DM.off += dt * 14;
    const gap = 24, span = w + gap, x = -((DM.off % span + span) % span);
    s.fillText(small, Math.round(x), 20); s.fillText(small, Math.round(x + span), 20);
  }
  // light up the dots
  const img = s.getImageData(0, 0, C, DM.rows).data, p = DM.pitch, ox = DM.ox;
  dctx.drawImage(DM.grid, 0, 0);
  const blink = toast && now - DM.toastAt < 1000 && ((now - DM.toastAt) / 170 | 0) % 2 === 1;   // a new message blinks a few times
  const lit = new Path2D(), halo = new Path2D();
  for (let y = 0; y < DM.rows; y++) for (let x = 0; x < C; x++) {
    if (img[(y * C + x) * 4 + 3] < 110) continue;
    if (blink && y > 11) continue;
    const cx = ox + (x + 0.5) * p, cy = (y + 0.5) * p;
    lit.moveTo(cx + p * 0.38, cy); lit.arc(cx, cy, p * 0.38, 0, 7);
    halo.moveTo(cx + p * 0.75, cy); halo.arc(cx, cy, p * 0.75, 0, 7);
  }
  dctx.fillStyle = toast ? 'rgba(255,214,90,.16)' : 'rgba(57,255,122,.16)'; dctx.fill(halo);
  dctx.fillStyle = toast ? '#ffe07a' : '#5dff94'; dctx.fill(lit);
  // a faint glass sheen
  dctx.fillStyle = 'rgba(255,255,255,.035)'; dctx.fillRect(0, 0, dmx.width, dmx.height * 0.45);
}
addEventListener('resize', () => { DM.cols = 0; LOGO.w = 0; });

// =====================================================================
// the DOS button: a little 3D ERROR-DOS logo, green on black, swinging
// round and glitching now and then
// =====================================================================
const logoCv = $('dosLogo'), logoCtx = logoCv.getContext('2d');
const LOGO = { w: 0, h: 0, sets: null, glitch: 0, next: 2, t: 0, last: 0, mode: 'dos', hold: 0, to: '' };
if (document.fonts && document.fonts.ready) document.fonts.ready.then(() => { LOGO.w = 0; });   // redo the letters once the pixel font is in
function logoLayout() {
  const r = logoCv.getBoundingClientRect(), dpr = Math.min(2, window.devicePixelRatio || 1);
  if (!r.width) return;
  const w = logoCv.width = Math.round(r.width * dpr), h = logoCv.height = Math.round(r.height * dpr);
  LOGO.w = w; LOGO.h = h;
  // the letters once, then a copy in each colour: front, sides, far back.
  // two logos: ERROR DOS, and the one it sometimes glitches into, ERROR.BLUE
  const build = second => {
    const mask = document.createElement('canvas'); mask.width = w; mask.height = h;
    const m = mask.getContext('2d');
    m.fillStyle = '#fff'; m.textAlign = 'center'; m.textBaseline = 'alphabetic';
    const fit = (txt, size, y) => {
      m.font = '700 ' + size + 'px "Pixelify Sans", "VT323", monospace';
      const tw = m.measureText(txt).width, k = Math.min(1, w * 0.84 / tw);
      m.save(); m.translate(w / 2, y); m.scale(k, 1); m.fillText(txt, 0, 0); m.restore();
    };
    fit('ERROR', h * 0.4, h * 0.44);
    fit(second, h * 0.5, h * 0.88);
    const tint = col => { const c = document.createElement('canvas'); c.width = w; c.height = h; const x = c.getContext('2d'); x.drawImage(mask, 0, 0); x.globalCompositeOperation = 'source-in'; x.fillStyle = col; x.fillRect(0, 0, w, h); return c; };
    return { face: tint('#7dffa8'), side: tint('#1d9a48'), back: tint('#0a3d1c') };
  };
  LOGO.sets = { dos: build('DOS'), blue: build('.BLUE') };
}
function logoDraw(now) {
  if (!LOGO.w) { logoLayout(); if (!LOGO.w) return; }
  const dt = Math.min(0.1, (now - (LOGO.last || now)) / 1000); LOGO.last = now; LOGO.t += dt;
  const c = logoCtx, w = LOGO.w, h = LOGO.h, t = LOGO.t;
  // phosphor afterglow: fade the last picture instead of wiping it
  c.globalCompositeOperation = 'source-over'; c.globalAlpha = 1;
  c.fillStyle = 'rgba(0,0,0,.45)'; c.fillRect(0, 0, w, h);
  // now and then a glitch burst; some of them flip the logo to ERROR.BLUE for a moment, and back
  LOGO.next -= dt;
  if (LOGO.mode === 'blue') {
    LOGO.hold -= dt;
    if (LOGO.hold <= 0 && !LOGO.to) { LOGO.glitch = 0.35; LOGO.to = 'dos'; LOGO.next = 1.6 + Math.random() * 3.2; }
  } else if (LOGO.next <= 0) {
    LOGO.glitch = 0.18 + Math.random() * 0.3; LOGO.next = 1.6 + Math.random() * 3.2;
    if (Math.random() < 0.3) { LOGO.glitch = 0.35; LOGO.to = 'blue'; }
  }
  const gl = LOGO.glitch > 0; if (gl) LOGO.glitch -= dt;
  if (!gl && LOGO.to) { LOGO.mode = LOGO.to; LOGO.to = ''; if (LOGO.mode === 'blue') LOGO.hold = 1.4 + Math.random() * 1.4; }
  // while it switches, it flickers between the two
  const set = LOGO.sets[gl && LOGO.to && Math.random() < 0.5 ? LOGO.to : LOGO.mode];
  // turn round the upright axis and nod a little
  const yaw = Math.sin(t * 1.15) * 0.62 + (gl ? (Math.random() - 0.5) * 0.5 : 0), pitch = Math.sin(t * 0.8) * 0.3;
  const sx = Math.max(0.25, Math.cos(yaw)), depth = Math.round(h * 0.11);
  const ex = Math.sin(yaw) * h * 0.012, ey = -Math.sin(pitch) * h * 0.012 + h * 0.006;
  const at = (img, k) => {
    c.save(); c.translate(w / 2 + ex * k, ey * k); c.scale(sx, 1); c.drawImage(img, -w / 2, 0); c.restore();
  };
  for (let k = depth; k >= 1; k--) at(k > depth * 0.55 ? set.back : set.side, k);
  c.shadowColor = '#39ff7a'; c.shadowBlur = h * 0.08;
  c.globalAlpha = gl && Math.random() < 0.3 ? 0.35 : 1;
  at(set.face, 0);
  c.shadowBlur = 0; c.globalAlpha = 1;
  if (gl) {
    // tearing: strips of the picture slide sideways, and a green bar flashes
    for (let i = 0; i < 3; i++) {
      const y = Math.random() * h, sh = 2 + Math.random() * h * 0.18, dx = (Math.random() - 0.5) * w * 0.3;
      c.drawImage(logoCv, 0, y, w, sh, dx, y, w, sh);
    }
    if (Math.random() < 0.5) { c.globalCompositeOperation = 'difference'; c.fillStyle = '#39ff7a'; c.fillRect(0, Math.random() * h, w, 1 + Math.random() * h * 0.08); c.globalCompositeOperation = 'source-over'; }
  }
  // scan lines
  c.fillStyle = 'rgba(0,0,0,.35)';
  for (let y = 0; y < h; y += 3) c.fillRect(0, y, w, 1);
}

// =====================================================================
// login (DOS screen with the ERROR keyboard)
// =====================================================================
const term = $('term');
let typed = '', loginLines = [], loginBusy = false;
function termRender() {
  term.innerHTML = loginLines.join('\n') + (loginBusy ? '' : '\nLOGIN: ' + esc(typed) + '<span class="cur"> </span>');
  term.scrollTop = term.scrollHeight;
}
function esc(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }
function loginKey(k) {
  if (G.scene !== 'login' || loginBusy) return;
  unlock();
  if (k === 'DEL') typed = typed.slice(0, -1);
  else if (k === 'SEND') return tryLogin();
  else if (k.length >= 1 && typed.length < 16 && !/^(SHIFT|SYM|ABC)$/.test(k)) typed += k;
  termRender();
}
const kb = ErrorKbd.mount($('loginKbd'), { onKey: loginKey, sendLabel: 'ENTER', sound: false });
function whoIs(name) {
  const n = name.trim().toUpperCase().replace(/[^A-Z]/g, '');
  for (const k in NAMES) if (NAMES[k].includes(n)) return k;
  return null;
}
function tryLogin() {
  const who = whoIs(typed);
  loginLines.push('LOGIN: ' + esc(typed));
  if (!who) {
    loginLines.push('<span class="err">TOEGANG GEWEIGERD. Bad command or file name.</span>');
    loginLines.push('<span class="dim">Тільки для: VIKA · VITO · OLEKSIY</span>');
    typed = ''; SFX.pop(); termRender(); return;
  }
  try { localStorage.setItem('tusa-name', typed.trim()); } catch (e) {}
  // tell the friends who asked for it (on their phones) that you are here
  try { fetch('/api/tusa/hello', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ char: who, me: PUSH.sub ? PUSH.sub.endpoint : '' }), keepalive: true }).catch(() => {}); } catch (e) {}
  loginBusy = true;
  loginLines.push('');
  loginLines.push('Ласкаво просимо, ' + who + '!');
  termRender();
  let i = 0;
  const bar = () => {
    i++;
    loginLines[loginLines.length - 1] = 'LOADING TUSA.EXE [' + '#'.repeat(i) + '.'.repeat(Math.max(0, 16 - i)) + ']';
    termRender();
    if (i < 16) setTimeout(bar, 55); else setTimeout(() => startBase(who), 300);
  };
  loginLines.push('');
  setTimeout(bar, 300);
}
// =====================================================================
// notifications: "Vito is net in de game gestapt" on your phone, even
// when TUSA is closed. Works in Chrome/Android and desktop browsers; on an
// iPhone only when error.blue is on the home screen (iOS 16.4 and newer).
// =====================================================================
const PUSH = { reg: null, key: null, sub: null, busy: false };
function pushBytes(s) {
  s = s.replace(/-/g, '+').replace(/_/g, '/'); while (s.length % 4) s += '=';
  const b = atob(s), u = new Uint8Array(b.length); for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i); return u;
}
function pushSay(line) { if (G.scene === 'login') { loginLines.push(line); termRender(); } else toast(line.replace(/<[^>]+>/g, ''), 3.5); }
function pushShow() {
  const b = $('pushBtn'); if (!b) return;
  b.textContent = '🔔 MELDINGEN: ' + (PUSH.sub ? 'AAN' : 'UIT'); b.classList.toggle('on', !!PUSH.sub);
}
async function pushPrepare() {
  // get everything ready beforehand: on an iPhone the tap itself has to ask for permission
  try {
    if (!('serviceWorker' in navigator) || !('PushManager' in window)) return;
    PUSH.reg = await navigator.serviceWorker.register('/sw.js');
    PUSH.sub = await PUSH.reg.pushManager.getSubscription();
    pushShow();
    if (!PUSH.key) PUSH.key = pushBytes((await (await fetch('/api/push')).json()).key);
  } catch (e) {}
}
function pushToggle() {
  if (PUSH.busy) return;
  const ios = /iP(hone|ad|od)/.test(navigator.userAgent) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
  const app = matchMedia('(display-mode: standalone)').matches || matchMedia('(display-mode: fullscreen)').matches || navigator.standalone;
  if (!PUSH.reg || !('PushManager' in window) || !('Notification' in window)) {
    if (ios && !app) {
      pushSay('<span class="err">iPhone: zet TUSA eerst op je beginscherm.</span>');
      pushSay('<span class="dim">Deel-knop ⎙ → "Zet op beginscherm", open TUSA daar en tik dan op 🔔.</span>');
    } else pushSay('<span class="err">Meldingen werken niet in deze browser.</span>');
    return;
  }
  if (!PUSH.key) { pushSay('<span class="err">Even geduld… probeer het zo nog eens.</span>'); pushPrepare(); return; }
  PUSH.busy = true;
  if (PUSH.sub) {
    const old = PUSH.sub; PUSH.sub = null; pushShow();
    fetch('/api/push', { method: 'DELETE', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ endpoint: old.endpoint }) }).catch(() => {});
    old.unsubscribe().catch(() => {}).finally(() => { PUSH.busy = false; });
    pushSay('🔕 Meldingen uit.');
    return;
  }
  // straight from the tap: this is what asks "allow notifications?"
  PUSH.reg.pushManager.subscribe({ userVisibleOnly: true, applicationServerKey: PUSH.key })
    .then(sub => fetch('/api/push', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(sub) }).then(r => {
      if (!r.ok) throw new Error('server ' + r.status);
      PUSH.sub = sub; pushShow();
      pushSay('🔔 Meldingen aan! Je hoort het als VIKA, VITO of OLEKSIY de game start.');
    }))
    .catch(() => {
      pushSay(Notification.permission === 'denied'
        ? '<span class="err">Meldingen zijn geblokkeerd. Zet ze aan in de instellingen van je telefoon.</span>'
        : '<span class="err">Meldingen aanzetten lukte niet. Probeer het nog eens.</span>');
    })
    .finally(() => { PUSH.busy = false; });
}
$('pushBtn').addEventListener('click', e => { e.stopPropagation(); pushToggle(); });
pushPrepare();

function showLogin() {
  G.scene = 'login';
  $('login').classList.add('show');
  document.body.classList.add('at-login'); layout(); globalThis.__LH = LH;
  try { typed = localStorage.getItem('tusa-name') || ''; } catch (e) { typed = ''; }
  loginLines = [
    '<span class="dim">ERROR-DOS 1.0 · C:\\ONLINE\\TUSA.EXE</span>',
    '',
    '<span class="big">Туса У Берліні</span>',
    '<span class="dim">(c) 2026 Hoongy &amp; vrienden · друзі</span>',
    '',
    'Троє друзів. Одне кафе в Берліні.',
    'Wie ben jij? Typ je naam en druk op ENTER.',
    '',
  ];
  loginBusy = false;
  termRender();
}

// =====================================================================
// scenes: base
// =====================================================================
// back = coming home from Berlin: you walk in from the edge, and your own door ends the game
function startBase(who, back) {
  G.me = who;
  $('login').classList.remove('show');
  if (document.body.classList.contains('at-login')) { document.body.classList.remove('at-login'); layout(); globalThis.__LH = LH; }
  G.scene = 'base';
  const H = HOME[who];
  G.base = { who, H, t0: back ? G.t - 2 : G.t, out: !!back, back: !!back, gate: back ? 1 : 0, inside: !back };
  // home again: a snapshot of the three of you pops up, like a photo taken that moment
  G.snap = back ? { t0: G.t + 0.6, k: 0, small: false, shot: false } : null;
  G.p = { x: back ? H.gate + 52 : H.door, y: 152, dir: back ? -1 : 1, phase: 0, moving: false, anim: '', animUntil: 0, seat: -1, angle: -Math.PI / 2, idle: 0 };
  G.prompt = '';
  G.npcs = [];
  for (let i = 0; i < 3; i++) spawnSoldier(true);
  G.camX = 0;
  closeDialog();
  radioStop();
}
// walking in through your own door: the end of the evening
function endGame() {
  G.scene = 'end'; G.end = { t0: G.t, shown: false }; G.prompt = '';
  G.p.dir = G.base.H.door >= G.p.x ? 1 : -1;
  SFX.door();
}
function updateEnd(dt) {
  const E = G.end, H = G.base.H, el = G.t - E.t0;
  // step into the doorway, then the picture fades and the card comes
  if (el < 0.8) { G.p.x += (H.door - G.p.x) * Math.min(1, dt * 6); G.p.moving = true; G.p.phase += dt * 2; }
  else G.p.moving = false;
  if (el > 2.2 && !E.shown) {
    E.shown = true; SFX.bell();
    showDialog({
      title: 'Кінець туси · Einde', low: true,
      text: 'Дякуємо, що були в Hoongy! Tot de volgende keer, ' + G.me + '.',
      opts: [
        { ico: '↻', label: 'Ще раз', key: 'A', act: () => location.reload() },
        { ico: '◄', label: 'Terug naar ERROR-DOS', key: 'B', act: () => { try { sessionStorage.setItem('errordos-ran', 'TUSA'); } catch (e) {} location.href = '/'; } },
      ],
    });
  }
}
function spawnSoldier(anywhere) {
  const B = G.base; if (!B) return;
  const dir = Math.random() < 0.5 ? 1 : -1, W = B.H.gate - 10;   // soldiers stay inside the camp
  const x = anywhere ? 40 + Math.random() * (W - 80) : (dir > 0 ? -20 : W + 20);
  const camo = { VITO: ['#6b6a45', '#4a4a30'], VIKA: ['#6e7350', '#3f4a2c'], OLEKSIY: ['#5d6b44', '#3a462a'] }[B.who];
  G.npcs.push({ kind: 'soldier', x, y: 152 + Math.random() * 3, dir, speed: (Math.random() < 0.25 ? 34 : 15) + Math.random() * 6, phase: Math.random(), wait: 0,
    look: { skin: pick(['#e0b090', '#c8946a', '#f0c8a0', '#8a5a3a']), top: camo[0], camo: camo[1], legs: camo[0], shoe: '#2a2018', hat: pick(['helmet', 'beret', 'cap']), hatc: pick([camo[1], '#7a1f2b', '#3a462a']), seed: Math.random() * 20 | 0 } });
}
function updateBase(dt) {
  const B = G.base, H = B.H;
  if (G.t - B.t0 < 0.9) { G.p.x = H.door + (G.t - B.t0) * 18; G.p.moving = true; G.p.phase += dt * 2; G.p.dir = 1; return; }
  if (!B.out) B.out = true;
  movePlayerSide(dt, 30, H.gate + 56);
  // home again: the door takes you inside, and that is the end of the evening
  G.prompt = B.back && Math.abs(G.p.x - H.door) < 10 ? 'A  🏠 Зайти додому' : '';
  if (G.snap) {
    if (!G.snap.shot && G.t >= G.snap.t0) { G.snap.shot = true; SFX.shutter(); }
    if (!G.snap.small && G.t - G.snap.t0 > 7) G.snap.small = true;   // then it moves to the corner
  }
  // the barrier lifts when you come close to the gate
  const nearGate = G.p.x > H.gate - 34 && G.p.x < H.gate + 64;
  B.gate += ((nearGate ? 1 : 0) - B.gate) * Math.min(1, dt * 3);
  if (G.p.x < H.gate - 8) B.inside = true;
  // walking out through the gate: off to Berlin
  if (B.inside && G.p.dir > 0 && G.p.x > H.gate + 24 && !dialogOpen()) {
    if (G.me === 'VITO') {
      showDialog({
        title: 'Летіти до Берліна?',
        text: 'Бухарест (OTP) → Берлін (BER) · ✈ 2 год 05 хв',
        opts: [
          { ico: '✈', label: 'Летимо!', key: 'A', act: () => startTravel('VITO') },
          { ico: '↩', label: 'Ще трохи побуду тут', key: 'B', act: () => { G.p.x = H.gate - 12; G.p.dir = -1; } },
        ],
      });
    } else startTravel(G.me);
  }
  // soldiers wander, sometimes salute
  G.nextSoldier -= dt;
  if (G.nextSoldier <= 0 && G.npcs.length < 5) { spawnSoldier(false); G.nextSoldier = 4 + Math.random() * 6; }
  for (const n of G.npcs) {
    if (n.wait > 0) { n.wait -= dt; n.look.salute = n.wait > 0.2 && Math.abs(n.x - G.p.x) < 26; continue; }
    n.x += n.dir * n.speed * dt; n.phase += dt * n.speed / 14;
    if (Math.random() < dt * 0.06) { n.wait = 1 + Math.random() * 2; }
  }
  G.npcs = G.npcs.filter(n => n.x > -60 && n.x < H.W + 60);
  G.camX = clamp(G.p.x - LW * 0.4, 0, Math.max(0, H.W + 30 - LW));
}
function drawBase() {
  const B = G.base, H = B.H;
  A.use(lctx);
  lctx.save(); lctx.translate(0, sideOY());
  const r = H.draw(LW, 180, G.camX, G.t, B.gate || 0, H.gate);
  B.signX = r.signX; B.signY = r.signY; B.signW = r.signW; B.signH = r.signH;
  const list = G.npcs.map(n => ({ y: n.y, d: () => A.person(n.x - G.camX, n.y, n.look, n.phase, n.wait <= 0, n.dir, G.t) }));
  list.push({ y: G.p.y, d: () => drawMe(G.p.x - G.camX, G.p.y) });
  list.sort((a, b) => a.y - b.y).forEach(o => o.d());
  // door: open while stepping out
  lctx.restore();
}

// =====================================================================
// scenes: the trip to Berlin
// =====================================================================
const TRIPS = {
  VITO: [
    { k: 'europe', d: 5, cap: () => 'Бухарест → Берлін · Boekarest → Berlijn' },
    { k: 'airport', d: 3.6, cap: () => 'BER · Welkom in Berlijn!' },
    { k: 'city', d: 3, cap: () => 'TAXI → Hoongy' },
  ],
  VIKA: [
    { k: 'mapDrive', d: 5, cap: () => 'Altengrabow → Berlin · A2 · 🚙 Amarok' },
    { k: 'drive', d: 3.6, cap: () => 'Ще 42 км!' },
    { k: 'city', d: 2.6, cap: () => 'Berlin · Берлін' },
  ],
  OLEKSIY: [
    { k: 'mapTrain', d: 5, cap: () => 'Lehnin → Brück → Berlin Hbf · RE7' },
    { k: 'train', d: 3.6, cap: () => 'RE7 · Volgende halte: Berlin Hbf' },
    { k: 'hbf', d: 2.6, cap: () => 'Berlin Hbf · 🛴 → Hoongy' },
  ],
};
function startTravel(who) {
  closeDialog();
  G.scene = 'travel';
  G.travel = { steps: TRIPS[who], i: 0, t0: G.t };
  G.npcs = [];
  if (who === 'VITO') SFX.plane(); else SFX.car();
}
function updateTravel() {
  const T = G.travel, st = T.steps[T.i];
  if (G.t - T.t0 >= st.d) nextTravel();
}
function nextTravel() {
  const T = G.travel;
  T.i++; T.t0 = G.t;
  if (T.i >= T.steps.length) arriveBerlin();
  else if (T.steps[T.i].k === 'airport') SFX.plane();
}
function drawTravel() {
  const T = G.travel, st = T.steps[T.i], p = Math.min(1, (G.t - T.t0) / st.d), t = G.t;
  A.use(lctx);
  const side = !/^(europe|mapDrive|mapTrain)$/.test(st.k);
  G.travelShift = side ? sideOY() : 0;
  if (side) { lctx.save(); lctx.translate(0, sideOY()); }
  const LH = side ? 180 : globalThis.__LH;
  if (st.k === 'europe') { const r = A.europeMap(LW, LH, ease(p), t); G.mapPins = [['Бухарест · Boekarest', r.from], ['Берлін · Berlijn', r.to]]; }
  else if (st.k === 'mapDrive' || st.k === 'mapTrain') {
    const route = st.k === 'mapDrive'
      ? [[12.18, 52.18], [12.28, 52.27], [12.55, 52.33], [12.95, 52.36], [13.1, 52.39], [13.3, 52.48], [13.42, 52.53]]
      : [[12.74, 52.32], [12.76, 52.2], [12.95, 52.3], [13.06, 52.39], [13.2, 52.45], [13.37, 52.525]];
    const r = A.brandenburgMap(LW, LH, route, ease(p), t);
    const pins = st.k === 'mapDrive' ? [['Altengrabow', 12.18, 52.18], ['Berlin', 13.42, 52.53]] : [['Lehnin', 12.74, 52.32], ['Brück', 12.76, 52.2], ['Berlin Hbf', 13.37, 52.525]];
    G.mapPins = pins.map(([n, lo, la]) => [n, r.P(lo, la)]);
    lctx.save(); lctx.translate(r.at[0], r.at[1]); lctx.rotate(r.ang); lctx.scale(1.3, 1.3);
    A.mapIcon(st.k === 'mapTrain' ? 'train' : 'car');
    lctx.restore();
  } else if (st.k === 'airport') {
    const px = -40 + p * (LW * 0.75), py = p < 0.6 ? 70 + p / 0.6 * 70 : 140;
    A.airport(LW, LH, t, px, py);
  } else if (st.k === 'drive') {
    A.roadTrip(LW, LH, t, 'day');
    if (p > 0.6) A.skyline(LW, 140, t, (p - 0.6) / 0.4);
    A.amarok(LW * 0.45, 158, 1, t, 'VIKA');
  } else if (st.k === 'train') {
    A.trainTrip(LW, LH, t, 'OLEKSIY');
  } else if (st.k === 'hbf') {
    A.hbf(LW, LH, t);
    A.friend('OLEKSIY', LW * 0.2 + p * LW * 0.6, 160, { dir: 1, moving: true, phase: t * 2, t });
  } else if (st.k === 'city') {
    A.cityNight(LW, LH, t, G.me);
  }
  if (side) lctx.restore();
}
// side views are 180 high; on tall screens they sit at the bottom with extra sky above
function sideOY() { return LH - 180; }
function topFill(kind, cx) {
  const oy = sideOY(); if (oy <= 0) return;
  if (kind === 'street') {
    lctx.fillStyle = '#3b3a3f'; lctx.fillRect(0, 0, LW, oy + 2);
    for (let row = 1; row * 34 <= oy + 30; row++) for (let i = -2; i < 30; i++) {
      const x = i * 34 + 6 - (cx % 34), y = oy - row * 34 + 2;
      lctx.fillStyle = '#262a33'; lctx.fillRect(x, y, 18, 22);
      if (A.hash(i + row * 31 + Math.floor(cx / 34)) > 0.6) { lctx.fillStyle = '#e8b35a'; lctx.fillRect(x + 1, y + 1, 16, 20); lctx.fillStyle = '#6b5030'; lctx.fillRect(x + 8, y + 1, 1, 20); }
      lctx.fillStyle = '#4a4950'; lctx.fillRect(x - 2, y + 22, 22, 2);
    }
  } else {
    lctx.fillStyle = kind; lctx.fillRect(0, 0, LW, oy + 2);
    if (kind === '#0b1530' || kind === '#0b1324' || kind === '#0d1a33') for (let i = 0; i < 40; i++) { lctx.fillStyle = '#cfd8ff'; lctx.fillRect(A.hash(i + 70) * LW, A.hash(i + 71) * oy, 1, 1); }
  }
}
const ease = p => p < 0.5 ? 2 * p * p : 1 - Math.pow(-2 * p + 2, 2) / 2;

// =====================================================================
// scenes: Berlin street (shared)
// =====================================================================
const S = A.STREET;
const ARR = { taxi: { from: -80, stop: 318, y: 170, leave: S.W + 120 }, amarok: { from: S.W + 80, stop: 420, y: 170, leave: -120 }, scooter: { from: -40, stop: 520, y: 150 } };
function arriveBerlin() {
  G.scene = 'street';
  const kind = ARRIVE[G.me], a = ARR[kind];
  G.p = { x: a.stop + (kind === 'scooter' ? 0 : 4), y: S.feet, dir: kind === 'amarok' ? -1 : 1, phase: 0, moving: false, anim: '', animUntil: 0, seat: -1, angle: -Math.PI / 2, idle: 0 };
  G.arrival = { kind, t0: G.t };
  // Oleksiy keeps riding his step around the street until he parks it
  G.ride = kind === 'scooter'; G.scootX = null; G.scootDir = 1;
  G.order = null;
  G.npcs = []; G.cars = []; G.leaves = [];
  G.camX = clamp(G.p.x - LW / 2, 0, S.W - LW);
  connect(kind, G.p.x);
  // a new visit: the commentary starts from the beginning
  G.voiceDone = false; NET.welcomed = false;
  voiceLoad();
  try { if (voiceReady && !voiceOn) voiceEl.currentTime = 0; } catch (e) {}
}
function arrivalState(kind, t0, now) {
  // returns { vx, show (character visible), vehicle visible }
  const a = ARR[kind], el = now - t0;
  if (kind === 'scooter') {
    if (el < 3) return { vx: a.from + (a.stop - a.from) * ease(el / 3), riding: true, show: false, parked: false };
    return { vx: a.stop, riding: false, show: true, parked: true };
  }
  if (el < 2.2) return { vx: a.from + (a.stop - a.from) * ease(el / 2.2), show: false, veh: true };
  if (el < 2.8) return { vx: a.stop, show: el > 2.5, veh: true };
  if (el < 5) return { vx: a.stop + (a.leave - a.stop) * ((el - 2.8) / 2.2) ** 2, show: true, veh: true };
  return { show: true, veh: false };
}
// Street life is worked out from the shared clock, not from dice: every phone
// shows the same people, cars and trams in the same places.
const h1 = n => { let x = (n | 0) ^ 0x9e3779b9; x = Math.imul(x ^ (x >>> 16), 0x85ebca6b); x = Math.imul(x ^ (x >>> 13), 0xc2b2ae35); x ^= x >>> 16; return (x >>> 0) / 4294967296; };
const PED_TOPS = ['#5a4a7a', '#7a3a3a', '#2f5a6a', '#8a6a3a', '#3a3a3a', '#6a2a4a', '#2a4a3a', '#c86a3a'];
function pedLook(n) {
  const r = k => h1(n * 13 + k);
  const of = (list, k) => list[Math.floor(r(k) * list.length) % list.length];
  return { skin: of(['#f0c8a0', '#e0b090', '#c8946a', '#8a5a3a', '#f4d4b8'], 1), top: of(PED_TOPS, 2), legs: of(['#2a2a3a', '#3a3a2a', '#1a1a1a', '#4a3a5a'], 3), shoe: '#1a1a1a',
    hair: of(['#1a1a1a', '#5a3a1a', '#d8b860', '#8a2a1a', '#c8c8c8'], 4), long: r(5) < 0.4, hat: r(6) < 0.25 ? 'beanie' : null, hatc: of(['#c83a3a', '#e8c040', '#3a6ac8'], 7),
    bag: r(8) < 0.3 ? of(['#e8e0c8', '#3a6a3a'], 9) : null, dog: r(10) < 0.12 ? of(['#c8a070', '#3a3a3a', '#f0f0f0'], 11) : null, seed: n % 20 };
}
function streetLife() {
  const now = snow() / 1000, list = [], cars = [];
  // a pedestrian may start every 3 seconds from either end
  for (let n = Math.floor(now / 3) - 70; n <= Math.floor(now / 3); n++) {
    if (h1(n * 7 + 1) > 0.55) continue;
    const dir = h1(n * 7 + 2) < 0.5 ? 1 : -1, speed = 12 + h1(n * 7 + 3) * 14, el = now - n * 3 - h1(n * 7 + 4) * 3;
    if (el < 0) continue;
    const x = dir > 0 ? -20 + el * speed : S.W + 20 - el * speed;
    if (x < -40 || x > S.W + 40) continue;
    list.push({ kind: 'ped', id: n, x, y: 146 + h1(n * 7 + 5) * 7, dir, phase: el * speed / 14, look: pedLook(n) });
  }
  // cars every few seconds, a tram now and then
  for (let n = Math.floor(now / 4) - 12; n <= Math.floor(now / 4); n++) {
    if (h1(n * 11 + 1) > 0.6) continue;
    const dir = h1(n * 11 + 2) < 0.5 ? 1 : -1, speed = 70 + h1(n * 11 + 3) * 40, el = now - n * 4;
    const x = dir > 0 ? -50 + el * speed : S.W + 50 - el * speed;
    if (x < -260 || x > S.W + 260) continue;
    cars.push({ kind: 'car', id: n, x, y: dir > 0 ? 168 : 178, dir, col: ['#c8323a', '#2a4a8a', '#e8e8e8', '#1a1a1a', '#5a7a3a', '#d8a020', '#7a7a8a'][Math.floor(h1(n * 11 + 4) * 7) % 7] });
  }
  // a yellow bus now and then
  for (let n = Math.floor(now / 29) - 2; n <= Math.floor(now / 29); n++) {
    if (h1(n * 13 + 2) > 0.7) continue;
    const dir = h1(n * 13 + 1) < 0.5 ? 1 : -1, el = now - n * 29 - 9;
    if (el < 0) continue;
    const x = dir > 0 ? -110 + el * 48 : S.W + 110 - el * 48;
    if (x < -260 || x > S.W + 260) continue;
    cars.push({ kind: 'bus', id: 'bus' + n, x, y: dir > 0 ? 168 : 178, dir });
  }
  for (let n = Math.floor(now / 37) - 2; n <= Math.floor(now / 37); n++) {
    const dir = h1(n * 5 + 1) < 0.5 ? 1 : -1, el = now - n * 37;
    const x = dir > 0 ? -170 + el * 55 : S.W + 170 - el * 55;
    if (x < -260 || x > S.W + 260) continue;
    cars.push({ kind: 'tram', id: 'tram' + n, x: x + (dir > 0 ? -40 : 0), y: 172, dir });
  }
  return { peds: list, cars };
}
function updateStreet(dt) {
  const arr = G.arrival;
  const now = G.t;
  if (arr) {
    const st = arrivalState(arr.kind, arr.t0, now);
    if (arr.kind === 'taxi' && now - arr.t0 > 2.2 && !arr.sounded) { arr.sounded = true; SFX.door(); }
    if (st.show && now - arr.t0 > (arr.kind === 'scooter' ? 3.1 : 2.9)) G.arrival = null;
  } else if (!dialogOpen()) {
    movePlayerSide(dt, -20, S.W + 20, G.ride ? 96 : 46);
    if (G.ride && animFor(G.me).anim === 'rofl') parkScooter();   // laughing too hard to stay on
    // walked off the street: home or stay?
    if (G.p.x < -8 || G.p.x > S.W + 8) {
      showDialog({
        title: 'Назад на базу?',
        text: 'Чи ще трохи потусуєш у Берліні?',
        opts: [
          { ico: '🏠', label: 'Додому, на базу', key: 'A', act: () => { disconnect(); startBase(G.me, true); } },
          { ico: '🍜', label: 'Залишаюсь!', key: 'B', act: () => { G.p.x = clamp(G.p.x, 12, S.W - 12); G.p.dir *= -1; } },
        ],
      });
    }
  }
  // traffic and people (the same on every phone), leaves
  const life = streetLife();
  G.npcs = life.peds; G.cars = life.cars;
  G.heard = G.heard || {};
  for (const v of G.cars) {
    if (G.heard[v.id]) continue;
    if (v.kind === 'tram' && Math.abs(v.x - G.p.x) < 160) { G.heard[v.id] = 1; SFX.bell(); }
    if (v.kind === 'bus' && Math.abs(v.x - G.p.x) < 140) { G.heard[v.id] = 1; SFX.car(); }
    if (v.kind === 'car' && Math.abs(v.x - G.p.x) < 120) { G.heard[v.id] = 1; SFX.car(); }
  }
  // falling ginkgo and linden leaves, carried by the wind
  const wind = 9 + Math.sin(G.t * 0.4) * 7 + (Math.sin(G.t * 1.7) > 0.9 ? 18 : 0);
  if (Math.random() < dt * 7) {
    const tx = pick(A.treesAt);
    G.leaves.push({ x: tx + (Math.random() - 0.5) * 50, y: 40 + Math.random() * 25, vx: 0, vy: 6 + Math.random() * 6, rot: Math.random() * 6, col: pick(['#f2b400', '#ffcf3a', '#e8891c', '#d8641a', '#f5d24a']), life: 1, ground: 148 + Math.random() * 10 });
  }
  if (Math.random() < dt * 1.5) G.leaves.push({ x: G.camX - 10, y: 20 + Math.random() * 100, vx: 0, vy: 4, rot: 0, col: pick(['#f2b400', '#ffcf3a', '#e8891c']), life: 1, ground: 150 + Math.random() * 8 });
  for (const l of G.leaves) {
    if (l.y < l.ground) { l.vx += (wind - l.vx) * dt * 0.8; l.x += (l.vx + Math.sin(G.t * 3 + l.rot) * 8) * dt; l.y += (l.vy + Math.cos(G.t * 4 + l.rot) * 3) * dt; l.rot += dt * 5; }
    else { l.life -= dt * 0.15; l.x += (wind > 20 ? 6 : 0) * dt; }
  }
  G.leaves = G.leaves.filter(l => l.life > 0 && l.x < S.W + 40);
  if (G.leaves.length > 160) G.leaves.splice(0, G.leaves.length - 160);
  // the door
  G.prompt = '';
  if (!G.arrival && G.ride) G.prompt = Math.abs(G.p.x - S.door) < 14 ? 'A  🛴 Eerst de step parkeren' : 'A  🛴 Step hier parkeren';
  else if (!G.arrival && G.scootX != null && Math.abs(G.p.x - G.scootX) < 10 && Math.abs(G.p.x - S.door) >= 14) G.prompt = 'A  🛴 Op de step';
  else if (!G.arrival && Math.abs(G.p.x - S.door) < 14) {
    const c = clockInfo();
    G.prompt = c && c.closed ? '🔒 Gesloten tot morgen' : !buddyHere() ? '🔒 Wacht op een vriend' : 'A  Naar binnen';
  }
  G.camX = clamp(G.p.x - LW / 2, 0, S.W - LW);
  sendState(false);
}
function onScreen(x) { return x > G.camX - 40 && x < G.camX + LW + 40; }
// people, cars and trees are drawn on their own layer, so the evening light
// can tint them a little without touching the glowing windows behind them
let actC = null, actX = null;
function actorLayer() {
  if (!actC) { actC = document.createElement('canvas'); actX = actC.getContext('2d'); }
  if (actC.width !== view.width || actC.height !== view.height) { actC.width = view.width; actC.height = view.height; }
  return actX;
}
function drawStreet() {
  const t = G.t, cx = G.camX, oy = sideOY(), doorOpen = G.doorUntil > t;
  const m = lctx.getTransform();
  // 1. the street itself, with the room behind Hoongy's window
  A.use(lctx);
  lctx.save(); lctx.translate(0, oy);
  A.street(LW, 180, cx, t, { doorOpen });
  lctx.restore();
  // 2. late twilight: a blue wash over everything that doesn't glow
  A.dusk(cx, oy, LW, LH);
  // 3. people, cars, trees and leaves
  const g = actorLayer(), lights = [];
  g.setTransform(1, 0, 0, 1, 0, 0); g.clearRect(0, 0, actC.width, actC.height);
  g.setTransform(m); g.translate(0, oy);
  A.use(g);
  A.outdoorTables(cx, t);
  // Oleksiy's step, where he parked it
  if (G.me === 'OLEKSIY' && !G.arrival && !G.ride && G.scootX != null) A.scooter(G.scootX - cx, S.feet, G.scootDir, null, 0);
  const ol = G.others.OLEKSIY;
  if (ol && ol.scene && !ol.ride && !(arrivingFor(ol) && ol.scoot == null)) {
    const sx = typeof ol.scoot === 'number' ? ol.scoot : 528;
    if (sx >= 0) A.scooter(sx - cx, S.feet, ol.sd || -1, null, 0);
  }
  const list = [];
  for (const n of G.npcs) list.push({ y: n.y, d: () => A.person(n.x - cx, n.y, n.look, n.phase, true, n.dir, t) });
  for (const ch of presentChars()) {
    const o = ch === G.me ? null : G.others[ch];
    if (o && o.scene !== 'street') continue;
    if (ch === G.me) {
      if (G.arrival && !arrivalState(G.arrival.kind, G.arrival.t0, t).show) continue;
      list.push({ y: G.p.y + 0.5, d: () => drawMe(G.p.x - cx, G.p.y) });
    } else {
      if (arrivingFor(o) && !arrivalState(o.arrive, (o.arriveAt - snow()) / 1000 + t, t).show) continue;
      list.push({ y: o.y + 0.4, d: () => drawOther(o, o.x - cx, o.y) });
    }
  }
  list.sort((a, b) => a.y - b.y).forEach(o => o.d());
  A.streetFront(cx, t);
  const arrivals = [];
  if (G.arrival) arrivals.push([G.me, G.arrival.kind, G.arrival.t0]);
  for (const ch in G.others) { const o = G.others[ch]; if (arrivingFor(o)) arrivals.push([ch, o.arrive, (o.arriveAt - snow()) / 1000 + t]); }
  for (const v of G.cars.filter(v => v.y < 172)) drawVehicle(v, lights);
  for (const [ch, kind, t0] of arrivals) {
    const st = arrivalState(kind, t0, t), a = ARR[kind];
    if (kind === 'scooter' && st.riding) { A.scooter(st.vx - cx, a.y, 1, ch, t); A.friend(ch, st.vx - cx - 1, a.y - 3, { dir: 1, t }); }
    else if (st.veh && kind === 'taxi') { A.taxi(st.vx - cx, a.y, 1, t); lights.push({ kind: 'car', x: st.vx, y: a.y, dir: 1, s: A.CAR }); }
    else if (st.veh && kind === 'amarok') { A.amarok(st.vx - cx, a.y, -1, t, (t - t0) < 2.5 ? ch : null); lights.push({ kind: 'car', x: st.vx, y: a.y, dir: -1, s: 1.8 }); }
  }
  for (const v of G.cars.filter(v => v.y >= 172)) drawVehicle(v, lights);
  for (const l of G.leaves) {
    if (!onScreen(l.x)) continue;
    g.globalAlpha = Math.min(1, l.life * 2);
    g.fillStyle = l.col; g.beginPath(); g.moveTo(l.x - cx, l.y);
    g.arc(l.x - cx, l.y, 2.2, l.rot - 0.6, l.rot + 0.6); g.closePath(); g.fill();   // a little ginkgo fan
    g.globalAlpha = 1;
  }
  A.duskActors(g, actC.width, actC.height);
  lctx.save(); lctx.setTransform(1, 0, 0, 1, 0, 0); lctx.drawImage(actC, 0, 0); lctx.restore();
  // 4. the lights: neon, lanterns, lamps, headlights
  A.use(lctx);
  lctx.save(); lctx.translate(0, oy);
  A.streetLights(cx, t, { doorOpen }, lights);
  lctx.restore();
}
function drawVehicle(v, lights) {
  const x = v.x - G.camX;
  if (x < -260 || x > LW + 260) return;
  if (v.kind === 'tram') A.tram(x, v.y, v.dir, G.t);
  else if (v.kind === 'bus') A.bus(x, v.y, v.dir, G.t);
  else A.car(x, v.y, v.col, v.dir, G.t);
  if (lights) lights.push({ kind: v.kind, x: v.x, y: v.y, dir: v.dir });
}

// =====================================================================
// entering and leaving Hoongy
// =====================================================================
function parkScooter() {
  if (!G.ride) return;
  G.ride = false; G.scootX = G.p.x; G.scootDir = G.p.dir;
  G.p.x = clamp(G.p.x - G.p.dir * 9, -10, S.W + 10);
  SFX.knob(); sendState(true);
}
function rideScooter() {
  G.ride = true; G.p.x = G.scootX; G.p.dir = G.scootDir; G.scootX = null;
  tone(300, 0.25, 0.03, 'sawtooth', 0, 620); sendState(true);
}
// Hoongy only opens once at least one of your friends has shown up this evening
function buddyHere() { return !!NET.g.clockStart || Object.keys(G.others).length > 0; }
function enterHoongy() {
  if (!buddyHere()) { toast('Hoongy gaat open als er een vriend is', 2.5); SFX.knob(); return; }
  if (G.ride) { toast('Eerst de step parkeren: druk op A', 2.2); return; }
  const c = clockInfo();
  if (c && c.closed) { toast('Gesloten. Tot morgen!', 2.5); return; }
  G.scene = 'enter'; G.trans = { t0: G.t, dir: 'in' };
  G.doorUntil = G.t + 1.2; SFX.door();
}
function updateEnter() {
  const p = (G.t - G.trans.t0) / 1.7;
  if (p >= 1) {
    G.scene = 'inside';
    G.p.x = A.IN.door.x; G.p.y = A.IN.H - 24; G.p.angle = -Math.PI / 2; G.p.seat = -1;
    G.zoom = 1; G.focus = null;
    sendState(true);
    radioSync();
  }
}
function drawEnter() {
  const p = (G.t - G.trans.t0) / 1.7;
  if (p < 0.55) {
    // the camera slides into the front window
    const k = ease(p / 0.55), zoom = 1 + k * 3.2;
    const wx = (S.shop0 + S.shop1) / 2 - G.camX, wy = 96 + sideOY();
    lctx.save();
    lctx.translate(LW / 2, LH / 2); lctx.scale(zoom, zoom); lctx.translate(-(wx + (LW / 2 - wx) * (1 - k)), -(wy + (LH / 2 - wy) * (1 - k)));
    drawStreet();
    lctx.restore();
    lctx.fillStyle = 'rgba(255,220,140,' + (k * 0.5) + ')'; lctx.fillRect(0, 0, LW, LH);
  } else {
    // ...and rises up: the view turns to look down from the ceiling
    const k = ease((p - 0.55) / 0.45);
    const endY = A.IN.H <= LH ? (A.IN.H - LH) / 2 : clamp(A.IN.H - 24 - LH / 2, 0, A.IN.H - LH);
    G.camX = clamp(A.IN.door.x - LW / 2, Math.min(0, (A.IN.W - LW) / 2), Math.max(0, A.IN.W - LW));
    G.camY = (A.IN.H - LH) + 60 * (1 - k) - ((A.IN.H - LH) - endY) * k;
    drawInsideWorld();
    lctx.fillStyle = 'rgba(255,220,140,' + (0.5 * (1 - k)) + ')'; lctx.fillRect(0, 0, LW, LH);
  }
}
function leaveHoongy() {
  if (G.p.seat >= 0) send({ t: 'stand' });
  G.scene = 'street';
  G.p.x = S.door; G.p.y = S.feet; G.p.dir = 1; G.p.seat = -1; G.order = null;
  G.doorUntil = G.t + 1; SFX.door();
  G.fadeUntil = G.t + 0.5;
  sendState(true);
  radioSync();
}

// =====================================================================
// inside Hoongy (top-down)
// =====================================================================
const IN = A.IN;
function insideBlocked(x, y) {
  if (x < 18 || x > IN.W - 18 || y < 54 || y > IN.H - 16) return true;
  for (const tb of IN.tables) if (Math.abs(x - tb.x) < tb.w / 2 + 4 && Math.abs(y - tb.y) < tb.h / 2 + 4) return true;
  for (const [x0, y0, x1, y1] of IN.solid) if (x > x0 - 3 && x < x1 + 3 && y > y0 - 3 && y < y1 + 3) return true;
  return false;
}
function seatTakenMap() {
  const m = {};
  for (const ch in G.others) { const o = G.others[ch]; if (o.scene === 'inside' && o.seat >= 0) m[o.seat] = ch; }
  if (G.p.seat >= 0) m[G.p.seat] = G.me;
  return m;
}
// stand right in front of the radio cabinet to use it
const RADIO_SPOT = { x: 46, y: 133 };
function nearRadio() { return G.scene === 'inside' && G.p.seat < 0 && Math.hypot(G.p.x - RADIO_SPOT.x, G.p.y - RADIO_SPOT.y) < 16; }
function radioPrompt() {
  const cur = NET.g.radio ? NET.g.radio.ch : 0, n = STATIONS.length;
  if (!cur) return 'A  📻 Radio aan';
  return cur < n ? 'A  📻 Volgende zender (' + STATIONS[cur].freq + ')' : 'A  📻 Radio uit';
}
function nearSeat() {
  const taken = seatTakenMap();
  let best = -1, bd = 14;
  IN.seats.forEach((s, i) => { const d = Math.hypot(s.x - G.p.x, s.y - G.p.y); if (d < bd && !taken[i]) { bd = d; best = i; } });
  return best;
}
function updateInside(dt) {
  const c = clockInfo();
  if (c && c.closed && !G.closedShown) {
    G.closedShown = true;
    showDialog({ title: '22:00 · Hoongy зачиняється!', text: 'Дякуємо, до завтра!', opts: [{ ico: '🚪', label: 'OK', key: 'A', act: leaveHoongy }] });
  }
  if (G.p.seat >= 0) {
    const s = IN.seats[G.p.seat];
    G.p.x = s.x; G.p.y = s.y; G.p.angle = s.face; G.p.moving = false;
    // the waiter brings the menu
    const o = G.order;
    if (o && o.ph === 'wait' && snow() >= o.t + 3800 && G.menuShownFor !== o.t && !dialogOpen()) { G.menuShownFor = o.t; openMenu(); }
    if (!o) G.prompt = NET.g.clockStart ? 'B  Встати' : '⏳ Кухня відкриється, коли прийде друг';
    else if (o.ph === 'wait') G.prompt = snow() < o.t + 3800 ? 'Офіціант іде…' : 'A  📋 Меню';
    else if (o.ph === 'cook' && snow() < o.t + 10500) G.prompt = '🍳 Готується…';
    else if (o.ph === 'cook' && (o.bites || 0) < 5) G.prompt = 'A  Їсти     B  Встати';
    else G.prompt = 'A  Замовити ще     B  Встати';
  } else {
    movePlayerTop(dt);
    const ns = nearSeat();
    G.hiSeat = ns;
    G.prompt = ns >= 0 ? 'A  🪑 Сісти' : '';
    if (nearRadio()) G.prompt = radioPrompt();
    if (Math.abs(G.p.x - IN.door.x) < 14 && G.p.y > IN.H - 30) G.prompt = 'A  🚪 Вийти';
    if (Math.abs(G.p.x - IN.door.x) < 12 && G.p.y >= IN.H - 17 && G.inputDown) leaveHoongy();
  }
  insideCamera(dt);
  sendState(false);
}
// all three friends at the same table? then the camera moves in a little
function sharedTable() {
  const chars = presentChars();
  if (chars.length < 3) return -1;
  let table = -1;
  for (const ch of chars) {
    const seat = ch === G.me ? (G.scene === 'inside' ? G.p.seat : -1) : (G.others[ch].scene === 'inside' ? G.others[ch].seat : -1);
    if (seat < 0) return -1;
    const tb = IN.seats[seat].table;
    if (table >= 0 && tb !== table) return -1;
    table = tb;
  }
  return table;
}
function insideCamera(dt) {
  const tb = sharedTable(), want = tb >= 0 ? 1.45 : 1;
  G.zoom += (want - G.zoom) * Math.min(1, dt * 2.2);
  const fx0 = tb >= 0 ? IN.tables[tb].x : G.p.x, fy0 = tb >= 0 ? IN.tables[tb].y : G.p.y;
  G.focus = G.focus || { x: fx0, y: fy0 };
  G.focus.x += (fx0 - G.focus.x) * Math.min(1, dt * 3); G.focus.y += (fy0 - G.focus.y) * Math.min(1, dt * 3);
  const vw = LW / G.zoom, vh = LH / G.zoom;
  G.camX = IN.W <= vw ? (IN.W - vw) / 2 : clamp(G.focus.x - vw / 2, 0, IN.W - vw);
  G.camY = IN.H <= vh ? (IN.H - vh) / 2 : clamp(G.focus.y - vh / 2, 0, IN.H - vh);
}
function drawInsideWorld() {
  A.use(lctx);
  lctx.fillStyle = '#120c08'; lctx.fillRect(0, 0, LW, LH);
  const Z = G.scene === 'inside' ? G.zoom : 1;
  lctx.save(); lctx.scale(Z, Z); lctx.translate(-G.camX, -G.camY);
  const taken = seatTakenMap();
  A.inside(G.t, { seatTaken: taken, hiSeat: G.scene === 'inside' && G.p.seat < 0 ? G.hiSeat : -1, doorOpen: G.doorUntil > G.t, radioOn: NET.g.radio ? NET.g.radio.ch : 0, hiRadio: nearRadio() });
  // dishes on the tables
  const diners = [];
  if (G.p.seat >= 0) diners.push([G.p.seat, G.order]);
  for (const ch in G.others) { const o = G.others[ch]; if (o.scene === 'inside' && o.seat >= 0) diners.push([o.seat, o.order]); }
  for (const [seat, o] of diners) {
    if (!o) continue;
    const s = IN.seats[seat], tb = IN.tables[s.table];
    const dx = s.x + (tb.x - s.x) * 0.55, dy = s.y;
    if (o.ph === 'cook' && snow() >= o.t + 10500) A.dish(o.item, dx, dy, 1 - (o.bites || 0) / 5, G.t);
    if (o.ph === 'wait' && snow() >= o.t + 3800) A.menuCard(dx, dy);
  }
  // people: two regulars at the bar, the waiter, the friends
  const list = [];
  list.push({ y: 46, d: () => A.friendTop('WAITER', 142, 48, { angle: -Math.PI / 2, t: G.t }) });
  const w = waiterPos();
  list.push({ y: w.y, d: () => { A.friendTop('WAITER', w.x, w.y, { angle: w.a, moving: w.moving, phase: G.t * 2, t: G.t }); if (w.carry) A.dish(w.carry, w.x + Math.cos(w.a) * 6, w.y + Math.sin(w.a) * 6, 1, G.t); } });
  for (const ch of presentChars()) {
    if (ch === G.me) { if (G.scene === 'inside') list.push({ y: G.p.y, d: () => drawMeTop(G.p.x, G.p.y) }); continue; }
    const o = G.others[ch];
    if (o.scene !== 'inside') continue;
    list.push({ y: o.y, d: () => drawOtherTop(o) });
  }
  list.sort((a, b) => a.y - b.y).forEach(o => o.d());
  A.insideTop(G.t, { x: G.camX, y: G.camY, w: LW / Z, h: LH / Z });
  lctx.restore();
}
// the waiter: where is he right now?
function waiterPos() {
  const now = snow(), home = IN.bar;
  const tasks = [];
  const add = (seat, o) => {
    if (!o) return;
    const s = IN.seats[seat], tb = IN.tables[s.table];
    const at = { x: tb.x, y: tb.y - tb.h / 2 - 7 };
    if (o.ph === 'wait') tasks.push({ t: o.t + 600, legs: [[home, at, 3200, null], [at, at, 999999, null]] });
    if (o.ph === 'cook') tasks.push({ t: o.t, legs: [[at, IN.kitchen, 3000, null], [IN.kitchen, IN.kitchen, 4000, null], [IN.kitchen, at, 3500, o.item], [at, at, 1200, null], [at, home, 2500, null]] });
  };
  if (G.p.seat >= 0) add(G.p.seat, G.order);
  for (const ch in G.others) { const o = G.others[ch]; if (o.scene === 'inside' && o.seat >= 0) add(o.seat, o.order); }
  tasks.sort((a, b) => b.t - a.t);
  for (const tk of tasks) {
    if (now < tk.t) continue;
    let el = now - tk.t;
    for (const [from, to, d, carry] of tk.legs) {
      if (el < d) {
        const k = Math.min(1, el / Math.min(d, 3500));
        const x = from.x + (to.x - from.x) * k, y = from.y + (to.y - from.y) * k;
        const moving = (from !== to) && k < 1;
        return { x, y, a: moving ? Math.atan2(to.y - from.y, to.x - from.x) : Math.PI / 2, moving, carry };
      }
      el -= d;
    }
  }
  return { x: home.x + Math.sin(G.t * 0.3) * 30, y: home.y + 4, a: Math.PI / 2, moving: true };
}
function openMenu() {
  SFX.pop();
  showDialog({
    title: '📋 HOONGY · Меню',
    text: 'Що бажаєте?',
    opts: Object.keys(FOOD).map(k => ({
      ico: FOOD[k].ico, label: FOOD[k].name, small: FOOD[k].d + ' — ' + FOOD[k].p + ' €',
      act: () => { send({ t: 'order', item: k }); if (!NET.ok) G.order = { ph: 'cook', item: k, t: snow(), bites: 0 }; },
    })).concat([{ ico: '⏳', label: 'Ще подумаю', key: 'B', act: () => {} }]),
  });
}

// =====================================================================
// moving
// =====================================================================
const keys = { left: false, right: false, up: false, down: false };
function movePlayerSide(dt, minX, maxX, speed) {
  const dx = (keys.right ? 1 : 0) - (keys.left ? 1 : 0);
  if (holding(animFor(G.me).anim)) { G.p.moving = false; return; }
  if (dx) {
    stopLaughing(G.me);
    G.p.x = clamp(G.p.x + dx * (speed || 46) * dt, minX, maxX); G.p.dir = dx; G.p.moving = true; G.p.phase += dt * 2.4; G.p.idle = 0;
    if ((G.p.phase % 0.5) < dt * 2.4 && !G.ride) SFX.step();
  } else { G.p.moving = false; G.p.idle += dt; }
  G.inputDown = keys.down;
}
function movePlayerTop(dt) {
  const dx = (keys.right ? 1 : 0) - (keys.left ? 1 : 0), dy = (keys.down ? 1 : 0) - (keys.up ? 1 : 0);
  G.inputDown = keys.down;
  if (!dx && !dy) { G.p.moving = false; G.p.idle += dt; return; }
  if (holding(animFor(G.me).anim)) { G.p.moving = false; return; }
  stopLaughing(G.me);
  const l = Math.hypot(dx, dy), sp = 40 * dt;
  const nx = G.p.x + dx / l * sp, ny = G.p.y + dy / l * sp;
  if (!insideBlocked(nx, G.p.y)) G.p.x = nx;
  if (!insideBlocked(G.p.x, ny)) G.p.y = ny;
  G.p.angle = Math.atan2(dy, dx); G.p.moving = true; G.p.phase += dt * 2.4; G.p.idle = 0;
  if (dx) G.p.dir = dx;
}
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

// =====================================================================
// friends together: hugs, hearts and jokes
// =====================================================================
function presentChars() {
  const list = [];
  if (G.me && (G.scene === 'street' || G.scene === 'inside' || G.scene === 'enter')) list.push(G.me);
  for (const ch in G.others) list.push(ch);
  return list;
}
function posOf(ch) {
  if (ch === G.me) return { x: G.p.x, y: G.p.y, scene: G.scene === 'inside' ? 'inside' : 'street' };
  const o = G.others[ch]; return o ? { x: o.x, y: o.y, scene: o.scene } : null;
}
function nearestFriend(maxD) {
  const me = posOf(G.me); let best = null, bd = maxD || 1e9;
  for (const ch in G.others) {
    const p = posOf(ch); if (!p || p.scene !== me.scene) continue;
    const d = Math.hypot(p.x - me.x, (p.y - me.y) * (me.scene === 'inside' ? 1 : 0.3));
    if (d < bd) { bd = d; best = ch; }
  }
  return best;
}
// Vito and Oleksiy have one of their own: when they stand close, they can rub noses
const NOSE_PAIR = { VITO: 'OLEKSIY', OLEKSIY: 'VITO' };
const holding = a => a === 'hug' || a === 'nose';
function noseReady() {
  const other = NOSE_PAIR[G.me];
  if (!other || !G.others[other] || !(G.scene === 'street' || G.scene === 'inside')) return '';
  const me = posOf(G.me), p = posOf(other);
  if (!p || p.scene !== me.scene) return '';
  return Math.hypot(p.x - me.x, (p.y - me.y) * (me.scene === 'inside' ? 1 : 0.3)) < 46 ? other : '';
}
function updateNoseBtn() {
  const b = $('noseBtn'); if (!b) return;
  const show = !!noseReady();
  if ((b.style.display !== 'none') !== show) { b.style.display = show ? '' : 'none'; b.parentNode.classList.toggle('four', show); }
}
function emote(kind) {
  if (!(G.scene === 'street' || G.scene === 'inside' || G.scene === 'base')) return;
  unlock();
  if (kind === 'nose') {
    const to = noseReady(); if (!to) return;
    if (NET.ok) send({ t: 'emote', kind, to, j: 0 }); else handleEmote({ from: G.me, kind, to, j: 0, t: snow() });
    return;
  }
  const to = nearestFriend(kind === 'hug' ? 46 : 1e9) || '';
  if (NET.ok) send({ t: 'emote', kind, to, j: 0 });
  else handleEmote({ from: G.me, kind, to, j: 0, t: snow() });
}
// jokes heard in the last 25 seconds; 3 of them and you are on the floor
const ROFL_AT = 3;
function laughLevel(ch) {
  const list = (G.jokes[ch] || []).filter(t => snow() - t < 25000);
  G.jokes[ch] = list;
  return Math.min(4, list.length);
}
function lastJoke(ch) { const l = G.jokes[ch] || []; return l.length ? snow() - l[l.length - 1] : 1e9; }
function stopLaughing(ch) { if (G.jokes[ch] && G.jokes[ch].length) G.jokes[ch] = []; }
function handleEmote(e) {
  const from = e.from;
  if (e.kind === 'joke') {
    // the teller pulls a funny face and gestures; everyone else laughs harder with every joke
    G.talk[from] = G.t + 2.2;
    SFX.joke();
    const listeners = presentChars().filter(ch => ch !== from);
    for (const ch of listeners) {
      (G.jokes[ch] = G.jokes[ch] || []).push(e.t || snow());
      const lv = laughLevel(ch);
      setTimeout(() => SFX.laugh(ch, lv), 1300 + Math.random() * 700);
    }
    if (!listeners.length) SFX.crickets();
  } else if (e.kind === 'heart') {
    SFX.heart();
    const a = posOf(from), b = e.to ? posOf(e.to) : null;
    if (a) G.hearts.push({ x: a.x, y: a.y - (scenePlace() === 'inside' ? 6 : 30), tx: b ? b.x : a.x, ty: b ? b.y - (scenePlace() === 'inside' ? 6 : 26) : a.y - 70, t0: G.t, place: a.scene });
    setAnim(from, 'heart', 0.8);
    if (e.to) G.blush[e.to] = G.t + 3;
  } else if (e.kind === 'hug') {
    if (e.to) {
      SFX.hug();
      setAnim(from, 'hug', 2.2); setAnim(e.to, 'hug', 2.2);
      G.hugs[from] = { with: e.to, until: G.t + 2.2 }; G.hugs[e.to] = { with: from, until: G.t + 2.2 };
      stopLaughing(from); stopLaughing(e.to);
      const a = posOf(from), b = posOf(e.to);
      if (from === G.me && b) { G.p.dir = b.x >= G.p.x ? 1 : -1; if (G.scene === 'street') G.p.x = b.x - G.p.dir * 11; }
      if (e.to === G.me && a) { G.p.dir = a.x >= G.p.x ? 1 : -1; }
      G.blush[from] = G.t + 2.5; G.blush[e.to] = G.t + 2.5;
    } else setAnim(from, 'hug', 1.2);
  } else if (e.kind === 'nose' && e.to && NOSE_PAIR[from] === e.to) {
    // nose kiss: face each other, lean in until the noses touch, and rub them side to side
    SFX.boop();
    setAnim(from, 'nose', 2.6); setAnim(e.to, 'nose', 2.6);
    stopLaughing(from); stopLaughing(e.to);
    const a = posOf(from), b = posOf(e.to);
    if (from === G.me && b) {
      if (G.scene === 'street') { G.p.dir = b.x >= G.p.x ? 1 : -1; G.p.x = b.x - G.p.dir * 17.5; }
      else {
        const ang = Math.atan2(b.y - G.p.y, b.x - G.p.x), nx = b.x - Math.cos(ang) * 12.5, ny = b.y - Math.sin(ang) * 12.5;
        if (G.p.seat < 0 && !insideBlocked(nx, ny)) { G.p.x = nx; G.p.y = ny; }
        G.p.angle = ang; G.p.dir = Math.cos(ang) >= 0 ? 1 : -1;
      }
    }
    if (e.to === G.me && a) {
      G.p.dir = a.x >= G.p.x ? 1 : -1;
      if (G.scene === 'inside' && G.p.seat < 0) G.p.angle = Math.atan2(a.y - G.p.y, a.x - G.p.x);
    }
    G.blush[from] = G.t + 3; G.blush[e.to] = G.t + 3;
  }
}
// a bite: wiggle, sparkles, and a happy "mmm" (bigger for the last bite)
function enjoy(ch, bite) {
  G.yum[ch] = G.t + (bite >= 5 ? 2.6 : 1.7);
  G.yumBig = G.yumBig || {}; G.yumBig[ch] = bite >= 5;
  const p = posOf(ch), me = posOf(G.me);
  if (p && me && p.scene === me.scene) setTimeout(() => SFX.yum(), 180);
}
function yumFor(ch) { const u = G.yum[ch] || 0; return u > G.t ? (G.yumBig && G.yumBig[ch] ? 3 : 1) : 0; }
function scenePlace() { return G.scene === 'inside' ? 'inside' : 'street'; }
function setAnim(ch, anim, dur) {
  if (ch === G.me) { G.p.anim = anim; G.p.animUntil = G.t + dur; }
  else if (G.others[ch]) { G.others[ch].localAnim = anim; G.others[ch].localAnimUntil = G.t + dur; }
}
// a short line at the bottom of the screen (never above a character)
function toast(text, dur) { G.toast = { text, until: G.t + (dur || 2.5) }; }

// my own laughing, worked out from the jokes I heard; it is sent to everyone
function myLaugh() {
  const lv = laughLevel(G.me), since = lastJoke(G.me);
  const a = G.p.animUntil > G.t ? G.p.anim : '';
  if (lv >= ROFL_AT && since < 20000 && !holding(a)) return { anim: 'rofl', laugh: 4 };
  return { anim: a, laugh: since < 9000 ? lv : 0 };
}
function animFor(ch) {
  if (ch === G.me) return myLaugh();
  const o = G.others[ch]; if (!o) return { anim: '', laugh: 0 };
  const a = (o.localAnimUntil || 0) > G.t ? o.localAnim : '';
  if (o.rf && !holding(a)) return { anim: 'rofl', laugh: 4 };
  return { anim: a, laugh: o.lg || 0 };
}
function talking(ch) { return G.talk[ch] > G.t && (G.t * 8 | 0) % 2 === 0; }
function drawRider(ch, x, y, dir, moving, st) {
  A.scooter(x, y, dir, null, moving ? G.t : 0);
  A.friend(ch, x - dir, y - 3, { dir, t: G.t, anim: holding(st.anim) || st.anim === 'heart' ? st.anim : (G.talk[ch] > G.t ? 'wave' : ''), laugh: st.laugh, blush: G.blush[ch] > G.t, talk: mouthMoving(ch) });
}
function drawMe(x, y) {
  const st = animFor(G.me);
  if (G.ride && G.scene === 'street' && st.anim !== 'rofl') return drawRider(G.me, x, y, G.p.dir, G.p.moving, st);
  A.friend(G.me, x, y, { dir: G.p.dir, phase: G.p.phase, moving: G.p.moving, anim: st.anim || (G.talk[G.me] > G.t ? 'wave' : ''), laugh: st.laugh, blush: G.blush[G.me] > G.t, t: G.t, talk: mouthMoving(G.me) });
}
function drawOther(o, x, y) {
  const st = animFor(o.char);
  if (o.ride && o.scene === 'street' && st.anim !== 'rofl') return drawRider(o.char, x, y, o.dir, o.moving, st);
  A.friend(o.char, x, y, { dir: o.dir, phase: o.phase, moving: o.moving, anim: st.anim || (G.talk[o.char] > G.t ? 'wave' : ''), laugh: st.laugh, blush: G.blush[o.char] > G.t, t: G.t, talk: mouthMoving(o.char) });
}
// on the floor next to the chair when rolling with laughter at the table
function floorSpot(seat, x, y) { const s = IN.seats[seat]; return s ? [x + (s.face === 0 ? -9 : 9), y + 3] : [x, y]; }
function drawMeTop(x, y) {
  const st = animFor(G.me);
  if (st.anim === 'rofl' && G.p.seat >= 0) [x, y] = floorSpot(G.p.seat, x, y);
  A.friendTop(G.me, x, y, { angle: G.p.angle, moving: G.p.moving, phase: G.p.phase, laugh: st.laugh, anim: st.anim, t: G.t, yum: yumFor(G.me) });
}
function drawOtherTop(o) {
  const st = animFor(o.char);
  const ang = o.seat >= 0 ? IN.seats[o.seat].face : (o.dir > 0 ? 0 : Math.PI);
  let x = o.x, y = o.y;
  if (st.anim === 'rofl' && o.seat >= 0) [x, y] = floorSpot(o.seat, x, y);
  A.friendTop(o.char, x, y, { angle: o.seat >= 0 ? ang : (typeof o.a === 'number' ? o.a : ang), moving: o.moving, phase: o.phase, laugh: st.laugh, anim: st.anim, t: G.t, yum: yumFor(o.char) });
}

// =====================================================================
// the radio stands in Hoongy, on a cabinet by the left wall. Like a real
// radio every station is always on air: when the evening starts each song
// is at 20% and from then on it keeps playing (and looping) in the
// background, whether the radio is on or not. Switching on or changing
// channel drops you into whatever is playing right now, the same moment for
// everyone. The server keeps which channel is on (0 = off). You only hear
// it inside, louder when you stand close to it.
// =====================================================================
let STATIONS = [];
fetch('radio/stations.json', { cache: 'no-store' }).then(r => r.json()).then(j => { STATIONS = j; radioSync(); updateRadioBtn(); }).catch(() => {});
const radioEl = new Audio(); radioEl.loop = true; radioEl.preload = 'auto';
let radioNode = null, radioFilter = null, radioGain = null, radioCh = -1, radioCheck = 0;
const localEpoch = Date.now();
function radioGraph() {
  const a = audio(); if (!a || radioNode) return;
  try {
    radioNode = a.createMediaElementSource(radioEl);
    radioFilter = a.createBiquadFilter(); radioFilter.type = 'lowpass'; radioFilter.frequency.value = 12000;
    radioGain = a.createGain(); radioGain.gain.value = 0.6;
    radioNode.connect(radioFilter).connect(radioGain).connect(master);
  } catch (e) {}
}
function stationDur(ch) {
  const st = STATIONS[ch - 1]; if (!st) return 60;
  return radioEl.duration && isFinite(radioEl.duration) && radioCh === ch ? radioEl.duration : st.dur || 60;
}
function onAir(ch) {
  // where the song on channel ch is right now (seconds): 20% in at the start of the evening, then onwards
  const dur = stationDur(ch), epoch = (NET.g.radio && NET.g.radio.epoch) || localEpoch;
  return ((dur * 0.2 + (snow() - epoch) / 1000) % dur + dur) % dur;
}
function tuneNoise(len) {
  // the hiss and whistle between stations
  noise(len || 0.45, 0.07, 2600, 0.4); noise(len || 0.45, 0.05, 900, 0.6);
  tone(1800, len || 0.45, 0.012, 'sine', 0, 600);
}
// the whole song is fetched once (the host can't serve parts of a file), so we
// can jump straight to the spot that is "on air"; meanwhile you hear tuning static
const songs = {};
function songURL(file) {
  if (!songs[file]) songs[file] = fetch('radio/' + file).then(r => { if (!r.ok) throw new Error(r.status); return r.blob(); }).then(b => URL.createObjectURL(b));
  return songs[file];
}
let hiss = null;
function hissOn() {
  const a = audio(); if (!a || hiss) return;
  if (!nbuf) noise(0.01, 0.0001, 1000, 1);
  const src = a.createBufferSource(); src.buffer = nbuf; src.loop = true;
  const f = a.createBiquadFilter(); f.type = 'bandpass'; f.frequency.value = 2200; f.Q.value = 0.5;
  const g = a.createGain(); g.gain.value = 0.035;
  const lfo = a.createOscillator(), lg = a.createGain(); lfo.frequency.value = 0.7; lg.gain.value = 900; lfo.connect(lg).connect(f.frequency); lfo.start();
  src.connect(f).connect(g).connect(master); src.start();
  hiss = { src, lfo, g };
}
function hissOff() {
  if (!hiss || !ac) return;
  const h = hiss; hiss = null;
  h.g.gain.setTargetAtTime(0, ac.currentTime, 0.08);
  setTimeout(() => { try { h.src.stop(); h.lfo.stop(); } catch (e) {} }, 400);
}
function radioSync() {
  const hear = G.scene === 'inside' && !VOICE.on;   // talking? then the radio goes quiet for you
  const ch = NET.g.radio ? NET.g.radio.ch : 0;
  updateRadioBtn();
  if (!hear || !ch || !STATIONS[ch - 1]) { radioStop(); return; }
  if (!unlocked) return;            // browsers only play sound after a tap
  radioGraph();
  const st = STATIONS[ch - 1];
  if (radioCh !== ch) {
    radioCh = ch;
    try { radioEl.pause(); } catch (e) {}
    tuneNoise(); hissOn();
    songURL(st.file).then(url => {
      if (radioCh !== ch) return;   // turned to another station meanwhile
      const go = () => {
        if (radioCh !== ch) return;
        try { radioEl.currentTime = onAir(ch); } catch (e) {}
        radioEl.play().then(hissOff).catch(hissOff);
      };
      if (radioEl.src === url && radioEl.readyState >= 1) go();
      else { radioEl.onloadedmetadata = go; radioEl.src = url; radioEl.load(); }
    }).catch(() => hissOff());
  } else if (radioEl.paused && radioEl.src) {
    try { radioEl.currentTime = onAir(ch); } catch (e) {}
    radioEl.play().catch(() => {});
  }
}
function radioStop() { radioCh = -1; hissOff(); try { radioEl.pause(); } catch (e) {} }
function radioDrift() {
  // keep every phone on the same spot of the song
  if (radioCh <= 0 || radioEl.paused || !radioEl.duration) return;
  const want = onAir(radioCh), d = Math.abs(radioEl.currentTime - want);
  if (d > 2.5 && d < radioEl.duration - 2.5) { try { radioEl.currentTime = want; } catch (e) {} }
}
function radioMix() {
  if (G.t - radioCheck > 5) { radioCheck = G.t; radioDrift(); }
  if (!radioFilter || !ac || G.scene !== 'inside') return;
  // louder and brighter close to the radio
  const d = Math.hypot(G.p.x - IN.radio.x, G.p.y - IN.radio.y), k = clamp(1 - (d - 24) / 260, 0, 1);
  radioFilter.frequency.setTargetAtTime(2500 + k * k * 15500, ac.currentTime, 0.3);
  radioGain.gain.setTargetAtTime(0.22 + k * 0.6, ac.currentTime, 0.3);
}
// (the radio is only controlled at its cabinet inside Hoongy; no button on screen)
function updateRadioBtn() {
  const b = $('radioBtn'); if (!b) return;
  const ch = NET.g.radio ? NET.g.radio.ch : 0, st = STATIONS[ch - 1];
  const at = G.scene === 'street' || G.scene === 'inside' || G.scene === 'enter';
  b.textContent = at && st ? '📻 ' + st.freq : '📻 OFF';
  b.style.borderColor = at && st ? '#ffe23a' : '';
}
function radioInfo() {
  unlock();
  const at = G.scene === 'street' || G.scene === 'inside' || G.scene === 'enter';
  if (!at) return toast('De radio staat in Hoongy, in Berlijn.', 2.5);
  toast(G.scene === 'inside' ? 'De radio staat links aan de muur. Loop erheen en druk op A.' : 'De radio staat binnen in Hoongy.', 3);
}
// off → first channel → second channel → … → off
function cycleRadio() {
  unlock(); SFX.knob();
  const cur = NET.g.radio ? NET.g.radio.ch : 0;
  const next = (cur + 1) % (STATIONS.length + 1);
  if (NET.ok) send({ t: 'radio', ch: next });
  else { NET.g.radio = { ch: next, since: snow(), last: next || 1, epoch: (NET.g.radio && NET.g.radio.epoch) || localEpoch }; radioSync(); }
  if (next === 0) tone(180, 0.06, 0.05);
  toast(next ? '📻 ' + STATIONS[next - 1].freq + ' · ' + STATIONS[next - 1].name : '📻 Radio uit', 2);
}

// =====================================================================
// the Hoongy commentary: plays on the street while you wait alone,
// and stops for good when the first friend arrives
// =====================================================================
const voiceEl = new Audio(); voiceEl.preload = 'auto';
let voiceURL = null, voiceNode = null, voiceGain = null, voiceOn = false, voiceReady = false;
function voiceLoad() {
  if (voiceURL) return;
  voiceURL = fetch('voice/hoongy-commentary.mp3').then(r => { if (!r.ok) throw new Error(r.status); return r.blob(); })
    .then(b => { const u = URL.createObjectURL(b); voiceEl.src = u; voiceEl.load(); voiceReady = true; return u; }).catch(() => { voiceURL = null; });
}
function voicePlaying() { return voiceOn && !voiceEl.paused; }
function voiceWanted() {
  return unlocked && SET.sound && G.scene === 'street' && !G.arrival && NET.welcomed && !G.voiceDone && !Object.keys(G.others).length && !voiceEl.ended;
}
function voiceUpdate() {
  const want = voiceWanted();
  if (want && !voiceOn && voiceReady) {
    const a = audio(); if (!a) return;
    if (!voiceNode) { try { voiceNode = a.createMediaElementSource(voiceEl); voiceGain = a.createGain(); voiceGain.gain.value = 0; voiceNode.connect(voiceGain).connect(master); } catch (e) {} }
    voiceOn = true;
    // fade in gently (about 2 seconds)
    if (voiceGain) { voiceGain.gain.cancelScheduledValues(a.currentTime); voiceGain.gain.setValueAtTime(voiceGain.gain.value, a.currentTime); voiceGain.gain.linearRampToValueAtTime(0.95, a.currentTime + 2); }
    voiceEl.play().catch(() => { voiceOn = false; });
  } else if (!want && voiceOn) {
    voiceOn = false;
    // fade out slowly (about 2.5 seconds), then stop
    if (voiceGain && ac) { voiceGain.gain.cancelScheduledValues(ac.currentTime); voiceGain.gain.setValueAtTime(voiceGain.gain.value, ac.currentTime); voiceGain.gain.linearRampToValueAtTime(0, ac.currentTime + 2.5); }
    setTimeout(() => { if (!voiceOn) try { voiceEl.pause(); } catch (e) {} }, 2700);
  }
}


// =====================================================================
// talking: a three-way voice call between the friends (WebRTC). The game
// server only passes the set-up messages along; voices go phone to phone.
// =====================================================================
const VOICE = { on: false, stream: null, pcs: {}, els: {}, an: {}, buf: null, speaking: {}, pend: {}, hold: {} };
const ICE = [{ urls: ['stun:stun.cloudflare.com:3478', 'stun:stun.l.google.com:19302'] }];
async function voiceStart() {
  unlock();
  if (VOICE.on) return;
  if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia || !window.RTCPeerConnection) { toast('🎙 Praten werkt niet in deze browser', 3); return; }
  try {
    VOICE.stream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: true, noiseSuppression: true, autoGainControl: true } });
  } catch (e) { toast('🎙 Geen toegang tot de microfoon', 3); return; }
  VOICE.on = true;
  radioSync();
  watchLevel(G.me, VOICE.stream);
  send({ t: 'voice', on: 1 });
  toast('🎙 Microfoon aan · radio stil', 2.5);
  for (const ch in G.others) voicePeer(ch);
  updateMicBtn();
}
function voiceStop(quiet) {
  if (!VOICE.on && !VOICE.stream) return;
  VOICE.on = false;
  for (const ch of Object.keys(VOICE.pcs)) hangup(ch);
  if (VOICE.stream) VOICE.stream.getTracks().forEach(tr => tr.stop());
  VOICE.stream = null; delete VOICE.an[G.me]; VOICE.speaking = {};
  if (!quiet) { send({ t: 'voice', on: 0 }); toast('🎙 Microfoon uit', 2); radioSync(); }
  updateMicBtn();
}
// both of us have the microphone on? the one whose name comes first starts the call
function voicePeer(ch) {
  const o = G.others[ch];
  if (!VOICE.on || !o || !o.voice) { if (VOICE.pcs[ch]) hangup(ch); return; }
  if (VOICE.pcs[ch] || G.me > ch) return;
  callPeer(ch);
}
function makePeer(ch) {
  const pc = new RTCPeerConnection({ iceServers: ICE });
  VOICE.pcs[ch] = pc; VOICE.pend[ch] = VOICE.pend[ch] || [];
  VOICE.stream.getTracks().forEach(tr => pc.addTrack(tr, VOICE.stream));
  pc.onicecandidate = e => { if (e.candidate) send({ t: 'rtc', to: ch, data: { ice: e.candidate } }); };
  pc.ontrack = e => {
    let el = VOICE.els[ch];
    if (!el) { el = VOICE.els[ch] = document.createElement('audio'); el.autoplay = true; el.setAttribute('playsinline', ''); document.body.appendChild(el); }
    el.srcObject = e.streams[0]; el.play().catch(() => {});
    watchLevel(ch, e.streams[0]);
  };
  pc.onconnectionstatechange = () => { if (pc.connectionState === 'failed' && VOICE.pcs[ch] === pc) { hangup(ch); setTimeout(() => voicePeer(ch), 1500); } };
  return pc;
}
async function callPeer(ch) {
  const pc = makePeer(ch);
  try { await pc.setLocalDescription(await pc.createOffer()); send({ t: 'rtc', to: ch, data: { sdp: pc.localDescription } }); } catch (e) { hangup(ch); }
}
async function onRTC(m) {
  const ch = m.from, d = m.data || {};
  if (!VOICE.on || !G.others[ch]) return;
  try {
    if (d.sdp && d.sdp.type === 'offer') {
      if (VOICE.pcs[ch]) hangup(ch);
      const pc = makePeer(ch);
      await pc.setRemoteDescription(d.sdp);
      await pc.setLocalDescription(await pc.createAnswer());
      send({ t: 'rtc', to: ch, data: { sdp: pc.localDescription } });
      for (const c of VOICE.pend[ch].splice(0)) await pc.addIceCandidate(c).catch(() => {});
    } else if (d.sdp && d.sdp.type === 'answer' && VOICE.pcs[ch]) {
      await VOICE.pcs[ch].setRemoteDescription(d.sdp);
      for (const c of (VOICE.pend[ch] || []).splice(0)) await VOICE.pcs[ch].addIceCandidate(c).catch(() => {});
    } else if (d.ice) {
      const pc = VOICE.pcs[ch];
      if (pc && pc.remoteDescription) await pc.addIceCandidate(d.ice).catch(() => {});
      else (VOICE.pend[ch] = VOICE.pend[ch] || []).push(d.ice);
    }
  } catch (e) {}
}
function hangup(ch) {
  const pc = VOICE.pcs[ch]; delete VOICE.pcs[ch]; VOICE.pend[ch] = [];
  try { if (pc) pc.close(); } catch (e) {}
  const el = VOICE.els[ch]; if (el) { el.srcObject = null; el.remove(); delete VOICE.els[ch]; }
  delete VOICE.an[ch]; delete VOICE.speaking[ch];
}
// who is speaking right now? their mouth moves and little sound waves show
function watchLevel(ch, stream) {
  const a = audio(); if (!a) return;
  try { const src = a.createMediaStreamSource(stream), an = a.createAnalyser(); an.fftSize = 512; src.connect(an); VOICE.an[ch] = an; } catch (e) {}
}
function voiceLevels() {
  for (const ch in VOICE.an) {
    const an = VOICE.an[ch];
    if (!VOICE.buf || VOICE.buf.length !== an.fftSize) VOICE.buf = new Float32Array(an.fftSize);
    try { an.getFloatTimeDomainData(VOICE.buf); } catch (e) { continue; }
    let sum = 0; for (let i = 0; i < VOICE.buf.length; i += 4) sum += VOICE.buf[i] * VOICE.buf[i];
    if (Math.sqrt(sum / (VOICE.buf.length / 4)) > 0.025) VOICE.hold[ch] = G.t + 0.25;
    VOICE.speaking[ch] = (VOICE.hold[ch] || 0) > G.t;
  }
}
function mouthMoving(ch) { return talking(ch) || (!!VOICE.speaking[ch] && (G.t * 9 | 0) % 2 === 0); }
function updateMicBtn() {
  const b = $('micBtn'); if (!b) return;
  const inBerlin = G.scene === 'street' || G.scene === 'inside' || G.scene === 'enter';
  const show = inBerlin && (VOICE.on || Object.keys(G.others).length > 0);
  if (b.style.visibility !== (show ? 'visible' : 'hidden')) DM.cols = 0;   // the screen gets wider or narrower
  b.style.visibility = show ? 'visible' : 'hidden';
  b.style.display = show ? '' : 'none';
  b.textContent = '🎙';
  b.classList.toggle('live', VOICE.on);
}

// iPhones only let a sound element play after it was started from a tap,
// so on the first taps both players are started silently once
let primed = false;
function primeMedia() {
  if (primed) return; primed = true;
  const silent = 'data:audio/wav;base64,UklGRiQAAABXQVZFZm10IBAAAAABAAEAQB8AAIA+AAACABAAZGF0YQAAAAA=';
  for (const el of [radioEl, voiceEl]) {
    if (el.src) continue;
    try { el.src = silent; el.muted = true; el.play().then(() => { el.pause(); el.muted = false; }).catch(() => { el.muted = false; }); } catch (e) {}
  }
}
for (const ev of ['touchend', 'click', 'keydown']) addEventListener(ev, () => { primeMedia(); unlock(); }, { capture: true });

// =====================================================================
// dialogs (DOS style)
// =====================================================================
let dlgOpts = [], dlgSel = 0;
function showDialog(d) {
  dlgOpts = d.opts; dlgSel = Math.max(0, d.opts.findIndex(o => o.sel));
  dlg.innerHTML = '<h3>' + esc(d.title) + '</h3>' + (d.text ? '<p>' + esc(d.text).replace(/\n/g, '<br>') + '</p>' : '') +
    '<div class="opts">' + d.opts.map((o, i) =>
      '<button class="opt' + (i === dlgSel ? ' sel' : '') + '" data-i="' + i + '"><span class="ico">' + (o.ico || '') + '</span><span>' + esc(o.label) +
      (o.small ? '<small>' + esc(o.small) + '</small>' : '') + '</span>' + (o.key ? '<span class="k">' + o.key + '</span>' : '') + '</button>').join('') + '</div>';
  dlg.dataset.radio = d.title.indexOf('RADIO HOONGY') >= 0 ? '1' : '';
  dlg.classList.toggle('low', !!d.low);
  dlg.classList.add('show');
}
function closeDialog() { dlg.classList.remove('show'); dlgOpts = []; }
function dialogOpen() { return dlg.classList.contains('show'); }
function chooseDialog(i) {
  const o = dlgOpts[i]; SFX.click();
  if (!(o && o.keep)) closeDialog();
  if (o && o.act) o.act();
}
dlg.addEventListener('click', e => { const b = e.target.closest('.opt'); if (b) chooseDialog(+b.dataset.i); });
function dialogKey(k) {
  if (k === 'up') dlgSel = (dlgSel + dlgOpts.length - 1) % dlgOpts.length;
  else if (k === 'down') dlgSel = (dlgSel + 1) % dlgOpts.length;
  else if (k === 'a') return chooseDialog(dlgSel);
  else if (k === 'b') { const bi = dlgOpts.findIndex(o => o.key === 'B'); if (bi >= 0) return chooseDialog(bi); closeDialog(); return; }
  dlg.querySelectorAll('.opt').forEach((b, i) => b.classList.toggle('sel', i === dlgSel));
  SFX.click();
}

// =====================================================================
// input
// =====================================================================
let unlocked = false;
function unlock() { if (!unlocked) { unlocked = true; audio(); radioSync(); } }
function press(k) {
  unlock();
  if (G.scene === 'login') return;
  if (dialogOpen()) return dialogKey(k);
  if (k === 'hug' || k === 'heart' || k === 'joke' || k === 'nose') return emote(k);
  if (k === 'mic') { if ($('micBtn').style.visibility === 'visible') VOICE.on ? voiceStop() : voiceStart(); return; }
  if (G.scene === 'travel') { if (k === 'a' || k === 'b') nextTravel(); return; }
  if (G.scene === 'base' && G.snap && G.snap.shot && !G.snap.small && (k === 'a' || k === 'b')) { G.snap.small = true; return; }
  if (G.scene === 'base' && G.base.back && (k === 'a' || k === 'up') && Math.abs(G.p.x - G.base.H.door) < 10) return endGame();
  if (G.scene === 'base' && k === 'a' && G.me === 'VITO' && G.base.inside && G.p.x > G.base.H.gate + 10) return startTravel('VITO');
  if (G.scene === 'street' && !G.arrival) {
    if (k === 'a' && G.ride) return parkScooter();
    if (k === 'a' && G.scootX != null && Math.abs(G.p.x - G.scootX) < 10 && Math.abs(G.p.x - S.door) >= 14) return rideScooter();
    if ((k === 'a' || k === 'up') && Math.abs(G.p.x - S.door) < 14) return enterHoongy();
  }
  if (G.scene === 'inside') {
    if (G.p.seat >= 0) {
      const o = G.order;
      if (k === 'b' || k === 'left' || k === 'right' || k === 'up' || k === 'down') { standUp(); return; }
      if (k === 'a') {
        if (o && o.ph === 'wait' && snow() >= o.t + 3800) openMenu();
        else if (o && o.ph === 'cook' && snow() >= o.t + 10500 && (o.bites || 0) < 5) { send({ t: 'eat' }); if (!NET.ok) o.bites = (o.bites || 0) + 1; SFX.eat(); enjoy(G.me, (o.bites || 0) + 1); }
        else if (o && o.ph === 'cook' && (o.bites || 0) >= 5) { send({ t: 'more' }); if (!NET.ok) G.order = { ph: 'wait', t: snow() }; }
      }
      return;
    }
    if (k === 'a') {
      if (Math.abs(G.p.x - IN.door.x) < 14 && G.p.y > IN.H - 30) return leaveHoongy();
      if (nearRadio()) return cycleRadio();
      const s = nearSeat();
      if (s >= 0) sitDown(s);
    }
  }
}
function sitDown(s) {
  // the waiter only comes once a friend has arrived and the evening has started
  G.p.seat = s; G.order = NET.g.clockStart ? { ph: 'wait', t: snow() } : null;
  const st = IN.seats[s]; G.p.x = st.x; G.p.y = st.y; G.p.angle = st.face;
  send({ t: 'sit', seat: s }); sendState(true);
}
function standUp() {
  const s = IN.seats[G.p.seat];
  G.p.seat = -1; G.order = null; send({ t: 'stand' });
  stopLaughing(G.me);
  if (s) { G.p.x = s.x + (s.face === 0 ? -10 : 10); if (insideBlocked(G.p.x, G.p.y)) G.p.y += 14; }
  sendState(true);
}
const KEYMAP = { ArrowLeft: 'left', KeyA: 'left', ArrowRight: 'right', KeyD: 'right', ArrowUp: 'up', KeyW: 'up', ArrowDown: 'down', KeyS: 'down', Enter: 'a', Space: 'a', KeyE: 'a', Escape: 'b', Backspace: 'b', KeyQ: 'b', KeyH: 'hug', KeyL: 'heart', KeyJ: 'joke', KeyM: 'mic', KeyN: 'nose' };
addEventListener('keydown', e => {
  if (G.scene === 'login') {
    unlock();
    if (e.key === 'Enter') { e.preventDefault(); kb.flash('SEND'); return tryLogin(); }
    if (e.key === 'Backspace') { e.preventDefault(); return loginKey('DEL'); }
    if (e.key.length === 1 && !e.ctrlKey && !e.metaKey) { e.preventDefault(); kb.flash(e.key); return loginKey(e.key); }
    return;
  }
  const k = KEYMAP[e.code]; if (!k) return;
  e.preventDefault();
  if (k in keys) { if (!keys[k] && (dialogOpen() || (G.scene === 'inside' && G.p.seat >= 0) || k === 'up')) press(k); keys[k] = true; return; }
  if (!e.repeat) press(k);
});
addEventListener('keyup', e => { const k = KEYMAP[e.code]; if (k in keys) keys[k] = false; });
// touch pad
document.querySelectorAll('#pad .pb').forEach(b => {
  const k = b.dataset.k;
  const down = e => { e.preventDefault(); b.classList.add('on'); if (k in keys) { if (dialogOpen() || (G.scene === 'inside' && G.p.seat >= 0) || k === 'up') press(k); keys[k] = true; } else press(k); };
  const up = () => { b.classList.remove('on'); if (k in keys) keys[k] = false; };
  b.addEventListener('pointerdown', down); b.addEventListener('pointerup', up); b.addEventListener('pointercancel', up); b.addEventListener('pointerleave', up);
});
// tap the picture inside to walk or sit
view.addEventListener('pointerdown', e => {
  unlock();
  if (G.scene !== 'inside' || dialogOpen()) return;
  const r = view.getBoundingClientRect();
  const wx = (e.clientX - r.left) / SCALE / G.zoom + G.camX, wy = (e.clientY - r.top) / SCALE / G.zoom + G.camY;
  if (nearRadio() && Math.hypot(wx - IN.radio.x, wy - IN.radio.y) < 16) return cycleRadio();
  const taken = seatTakenMap();
  const s = IN.seats.findIndex((st, i) => !taken[i] && Math.hypot(st.x - wx, st.y - wy) < 9);
  if (s >= 0 && G.p.seat < 0 && Math.hypot(IN.seats[s].x - G.p.x, IN.seats[s].y - G.p.y) < 40) sitDown(s);
});
$('micBtn').addEventListener('click', () => { VOICE.on ? voiceStop() : voiceStart(); });
$('dosBtn').addEventListener('click', () => {
  unlock();
  showDialog({
    title: 'Terug naar ERROR-DOS?',
    opts: [
      { ico: '◄', label: 'Ja, afsluiten', key: 'A', act: () => { disconnect(); try { sessionStorage.setItem('errordos-ran', 'TUSA'); } catch (e) {} location.href = '/'; } },
      { ico: '🍜', label: 'Nee, verder spelen', key: 'B', act: () => {} },
    ],
  });
});
// game controller
const padPrev = {};
function pollPad() {
  if (!navigator.getGamepads) return;
  for (const p of navigator.getGamepads()) {
    if (!p) continue;
    const b = i => !!(p.buttons[i] && p.buttons[i].pressed), ax = p.axes || [];
    const st = { left: b(14) || ax[0] < -0.5, right: b(15) || ax[0] > 0.5, up: b(12) || ax[1] < -0.5, down: b(13) || ax[1] > 0.5, a: b(0), b: b(1), joke: b(2), heart: b(3), hug: b(4) || b(5) };
    const prev = padPrev[p.index] || {};
    for (const k in st) {
      if (k in keys) { if (st[k] && !prev[k] && (dialogOpen() || (G.scene === 'inside' && G.p.seat >= 0) || k === 'up')) press(k); keys[k] = st[k] || (prev[k] && !st[k] ? false : keys[k]); if (!st[k] && prev[k]) keys[k] = false; }
      else if (st[k] && !prev[k]) press(k);
    }
    padPrev[p.index] = st;
  }
}

// =====================================================================
// screen-space text: names, bubbles, prompts, captions
// =====================================================================
function S2(x) { return x * SCALE * DPR; }
const FONT = '"M PLUS Rounded 1c", "Pixelify Sans", sans-serif', INKC = '#33241b', PAPER = 'rgba(251,244,226,.95)';
// a cartoon heart, drawn in screen space
function heart(c, x, y, s) {
  c.save(); c.translate(x, y); c.scale(s, s);
  c.beginPath(); c.moveTo(0, 3); c.bezierCurveTo(-5, -0.5, -3.6, -5, 0, -2.6); c.bezierCurveTo(3.6, -5, 5, -0.5, 0, 3); c.closePath();
  c.fillStyle = '#ff5a7a'; c.fill(); c.lineWidth = 0.8; c.strokeStyle = INKC; c.lineJoin = 'round'; c.stroke();
  c.beginPath(); c.arc(-1.6, -1.6, 0.7, 0, 7); c.fillStyle = 'rgba(255,255,255,.75)'; c.fill();
  c.restore();
}
function drawOverlay() {
  const c = vctx, k = SCALE * DPR;
  const oy = (G.scene === 'base' || G.scene === 'street' || G.scene === 'end') ? sideOY() : 0;
  c.textBaseline = 'alphabetic';
  // captions
  if (G.scene === 'base' || G.scene === 'end') {
    const H = G.base.H;
    if (G.base.signX != null) {
      c.font = '800 ' + (5.6 * k) + 'px ' + FONT; c.fillStyle = '#fbf4e2'; c.textAlign = 'center';
      c.fillText(H.sign, S2(G.base.signX + G.base.signW / 2), S2(G.base.signY + G.base.signH * 0.72 + oy));
      c.textAlign = 'left';
    }
    if (G.base.inside && G.p.x > H.gate - 70 && G.p.x < H.gate + 20 && !dialogOpen() && !G.prompt) hint(G.me === 'VITO' ? 'Через ворота ► до аеропорту' : 'Через ворота ► до Берліна');
  }
  if (G.scene === 'travel') {
    const st = G.travel.steps[G.travel.i];
    if (G.mapPins && (st.k === 'mapDrive' || st.k === 'mapTrain' || st.k === 'europe')) {
      c.font = '800 ' + (6 * k) + 'px ' + FONT; c.lineJoin = 'round';
      for (const [n, [x, y]] of G.mapPins) {
        const w = c.measureText(n).width, tx = Math.min(S2(x + 4), view.width - w - 4), ty = S2(y - 3);
        c.lineWidth = k * 2; c.strokeStyle = 'rgba(251,244,226,.95)'; c.strokeText(n, tx, ty);
        c.fillStyle = INKC; c.fillText(n, tx, ty);
      }
    }
  }
  if (G.scene === 'street' || G.scene === 'inside') {
    const placed = [];
    for (const ch of presentChars()) {
      const p = posOf(ch); if (!p) continue;
      if ((G.scene === 'inside') !== (p.scene === 'inside')) continue;
      const Z = G.scene === 'inside' ? G.zoom : 1;
      const sx = (p.x - G.camX) * Z, sy = G.scene === 'inside' ? (p.y - G.camY) * Z : p.y + oy;
      if (ch !== G.me && G.others[ch] && arrivingFor(G.others[ch]) && G.scene === 'street') {
        const o = G.others[ch], t0 = (o.arriveAt - snow()) / 1000 + G.t;
        if (!arrivalState(o.arrive, t0, G.t).show) continue;
      }
      if (ch === G.me && G.arrival && !arrivalState(G.arrival.kind, G.arrival.t0, G.t).show) continue;
      let top = G.scene === 'inside' ? sy - 10 * Z : sy - 40;
      // friends standing close together: stack the names instead of overlapping them
      while (placed.some(([px, py]) => Math.abs(px - sx) < 30 && Math.abs(py - top) < 6)) top -= 7;
      placed.push([sx, top]);
      nameTag(ch, sx, top);
      if (VOICE.speaking[ch]) waves(c, S2(sx) + c.measureText(ch).width / 2 + 3 * k, S2(top) - 2 * k, k);
    }
  }
  if (G.scene === 'base') nameTag(G.me, G.p.x - G.camX, G.p.y - 40 + oy);
  // hearts
  for (const h of G.hearts) {
    const el = G.t - h.t0; if (el < 0) continue;
    const kk = Math.min(1, el / 0.9);
    const hy = G.scene === 'inside' ? G.camY : -oy;
    const Zh = G.scene === 'inside' ? G.zoom : 1;
    const x = (h.x + (h.tx - h.x) * kk - G.camX) * Zh, y = (h.y + (h.ty - h.y) * kk - Math.sin(kk * Math.PI) * 18 - hy) * Zh;
    c.globalAlpha = Math.max(0, 1 - Math.max(0, el - 1.1) * 2);
    heart(c, S2(x), S2(y), k * (h.small ? 0.8 : 1.15));
    c.globalAlpha = 1;
  }
  G.hearts = G.hearts.filter(h => G.t - h.t0 < 1.8);
  // chill notes when idle
  if ((G.scene === 'street' || G.scene === 'base') && G.p.idle > 6 && !G.arrival) {
    c.font = (7 * k) + 'px sans-serif'; c.fillStyle = '#5a4030'; c.textAlign = 'center';
    const ph = (G.t * 0.7) % 1;
    c.globalAlpha = 1 - ph; c.fillText(ph < 0.5 ? '♪' : '♫', S2(G.p.x - G.camX + 6 + ph * 6), S2(G.p.y - 32 - ph * 14 + oy));
    c.globalAlpha = 1; c.textAlign = 'left';
  }
  if (G.scene === 'base') drawSnapshot();
  if (G.scene === 'end') drawEnd();
  if (G.prompt && !dialogOpen()) hint(G.prompt, 0);
  // fade after leaving
  if (G.fadeUntil > G.t) { c.fillStyle = 'rgba(0,0,0,' + ((G.fadeUntil - G.t) * 2) + ')'; c.fillRect(0, 0, view.width, view.height); }
}
// ---------- the snapshot: the photo stands still, the three of you breathe, sway and blink ----------
const SNAP = { ok: false };
fetch('snap/snap.json').then(r => r.json()).then(meta => {
  const load = src => new Promise(res => { const im = new Image(); im.onload = () => res(im); im.onerror = () => res(null); im.src = src; });
  return Promise.all(['bg', 'vito', 'vika', 'olek'].map(n => load('snap/snap-' + n + '.png'))).then(([bg, vito, vika, olek]) => {
    if (!bg) return;
    Object.assign(SNAP, { meta, bg, people: { vito, vika, olek } });
    SNAP.small = document.createElement('canvas'); SNAP.small.width = meta.w; SNAP.small.height = meta.h;
    SNAP.g = SNAP.small.getContext('2d'); SNAP.ok = true;
  });
}).catch(() => {});
function snapFrame(t) {
  const g = SNAP.g, M = SNAP.meta;
  g.imageSmoothingEnabled = false;
  g.drawImage(SNAP.bg, 0, 0);
  ['vito', 'vika', 'olek'].forEach((n, i) => {
    const p = M.people[n], im = SNAP.people[n]; if (!im) return;
    const bob = Math.round(Math.sin(t * 1.7 + i * 2.1) * 0.75), sway = Math.round(Math.sin(t * 0.8 + i * 1.7) * 0.7);
    g.drawImage(im, p.x + sway, p.y + bob);
    if ((t * 1000 + i * 1300) % (3300 + i * 650) < 150) p.eyes.forEach(([ex, ey], k) => { g.fillStyle = p.skin[k]; g.fillRect(ex - 1 + sway, ey + bob, 3, 2); });
  });
}
// a polaroid: white frame, a strip of tape, a handwritten line
function polaroid(c, cx, cy, pw, rot, alpha, k) {
  const M = SNAP.meta, ph = pw * M.h / M.w, pad = pw * 0.06, fw = pw + pad * 2, fh = ph + pad * 2 + pw * 0.2;
  c.save(); c.globalAlpha = alpha; c.translate(cx, cy); c.rotate(rot);
  c.shadowColor = 'rgba(30,15,5,.45)'; c.shadowBlur = 8 * k; c.shadowOffsetY = 3 * k;
  c.fillStyle = '#fbf8f0'; c.fillRect(-fw / 2, -fh / 2, fw, fh);
  c.shadowColor = 'transparent';
  c.strokeStyle = 'rgba(51,36,27,.5)'; c.lineWidth = Math.max(1, k * 0.4); c.strokeRect(-fw / 2, -fh / 2, fw, fh);
  c.imageSmoothingEnabled = false;
  c.drawImage(SNAP.small, -fw / 2 + pad, -fh / 2 + pad, pw, ph);
  c.imageSmoothingEnabled = true;
  c.strokeStyle = 'rgba(51,36,27,.35)'; c.strokeRect(-fw / 2 + pad, -fh / 2 + pad, pw, ph);
  c.fillStyle = 'rgba(255,170,190,.55)'; c.save(); c.translate(0, -fh / 2); c.rotate(0.05); c.fillRect(-pw * 0.16, -pw * 0.035, pw * 0.32, pw * 0.07); c.restore();
  c.fillStyle = INKC; c.textAlign = 'center'; c.font = '800 ' + Math.max(9, pw * 0.07) + 'px ' + FONT;
  c.fillText('Туса У Берліні · Hoongy', 0, fh / 2 - pw * 0.075);
  c.restore();
}
function drawSnapshot() {
  const S = G.snap; if (!S || !SNAP.ok || G.t < S.t0) return;
  const c = vctx, w = view.width, h = view.height, k = SCALE * DPR, el = G.t - S.t0;
  const dt = Math.min(0.1, el - (S.last == null ? el : S.last)); S.last = el;
  S.k += ((S.small ? 0 : 1) - S.k) * Math.min(1, dt * 5);
  snapFrame(G.t);
  // camera flash
  if (el < 0.4) { c.fillStyle = 'rgba(255,255,255,' + (1 - el / 0.4) + ')'; c.fillRect(0, 0, w, h); }
  const pop = Math.min(1, el / 0.35), ease2 = 1 - Math.pow(1 - pop, 3);
  const bigW = Math.min(w * 0.68, h * 0.62 * SNAP.meta.w / SNAP.meta.h), smallW = Math.min(w, h) * 0.2;
  const pw = smallW + (bigW - smallW) * S.k;
  const bx = w / 2, by = h * 0.47, sx = smallW * 0.75 + 6 * k, sy = 15 * k + smallW * 0.75;
  const cx = sx + (bx - sx) * S.k, cy = sy + (by - sy) * S.k;
  if (S.k > 0.02) { c.fillStyle = 'rgba(20,12,6,' + (0.35 * S.k) + ')'; c.fillRect(0, 0, w, h); }
  polaroid(c, cx, cy, pw * (0.7 + 0.3 * ease2), -0.035 - (1 - S.k) * 0.05, 1, k);
  if (!S.small && el > 1.2) hint('A  ✓', 0);
}
function drawEnd() {
  const c = vctx, el = G.t - G.end.t0, w = view.width, h = view.height, k = SCALE * DPR;
  const f = Math.min(1, Math.max(0, (el - 0.6) / 1.4));
  c.fillStyle = 'rgba(251,244,226,' + (f * 0.92) + ')'; c.fillRect(0, 0, w, h);
  if (f <= 0) return;
  A.paper(c, w, h, f);
  let r = Math.min(w, h) * 0.13, cx = w / 2, cy = h * 0.2 + r;
  if (SNAP.ok) {
    snapFrame(G.t);
    const pw = Math.min(w * 0.6, h * 0.4 * SNAP.meta.w / SNAP.meta.h);
    polaroid(c, w / 2, h * 0.32, pw, -0.03, f, k);
    r = pw * 0.15; cx = w / 2 + pw * 0.48; cy = h * 0.32 + pw * 0.22;
  }
  c.save(); c.globalAlpha = f;
  c.translate(cx, cy); c.rotate(-0.08);
  c.beginPath(); c.arc(0, 0, r, 0, 7); c.fillStyle = '#cf3a2a'; c.fill();
  c.lineWidth = r * 0.06; c.strokeStyle = '#fbf4e2'; c.beginPath(); c.arc(0, 0, r * 0.84, 0, 7); c.stroke();
  c.fillStyle = '#fbf4e2'; c.font = '900 ' + r * 1.05 + 'px "M PLUS Rounded 1c", serif'; c.textAlign = 'center'; c.textBaseline = 'middle';
  c.fillText('終', 0, r * 0.06);
  c.restore();
  if (SNAP.ok) return;
  c.save(); c.globalAlpha = f; c.textAlign = 'center'; c.fillStyle = INKC;
  c.font = '900 ' + 9 * k + 'px ' + FONT; c.fillText('Туса У Берліні', cx, cy + r + 12 * k);
  c.restore();
}
function capText(a, b) {
  const c = vctx, k = SCALE * DPR;
  c.fillStyle = PAPER; c.fillRect(0, 0, view.width, 15 * k);
  c.fillStyle = '#cf3a2a'; c.fillRect(0, 15 * k - k * 0.5, view.width, k * 0.5);
  c.font = '800 ' + (8 * k) + 'px ' + FONT; c.fillStyle = INKC; c.textAlign = 'left';
  c.fillText(a, 5 * k, 10.5 * k);
  const aw = c.measureText(a).width;
  if (b) { c.font = '700 ' + (6 * k) + 'px ' + FONT; if (aw + c.measureText(b).width + 16 * k < view.width) { c.fillStyle = '#8a6a50'; c.textAlign = 'right'; c.fillText(b, view.width - 5 * k, 10.5 * k); c.textAlign = 'left'; } }
}
// one line at the bottom of the picture, on a paper label; row 1 sits just above row 0
function hint(txt, row, info) {
  const c = vctx, k = SCALE * DPR;
  let fs = 7 * k;
  c.font = '800 ' + fs + 'px ' + FONT;
  let w = c.measureText(txt).width + 12 * k;
  if (w > view.width - 6 * k) { fs *= (view.width - 18 * k) / (w - 12 * k); c.font = '800 ' + fs + 'px ' + FONT; w = c.measureText(txt).width + 12 * k; }
  const x = Math.round((view.width - w) / 2), y = Math.round(view.height - 16 * k - (row || 0) * 15 * k), h = 12 * k;
  c.save();
  c.shadowColor = 'rgba(40,20,10,.35)'; c.shadowOffsetY = k; c.shadowBlur = 2 * k;
  roundRect(c, x, y, w, h, 3 * k); c.fillStyle = info ? '#fff1c8' : PAPER; c.fill();
  c.restore();
  roundRect(c, x, y, w, h, 3 * k); c.lineWidth = k * 0.8; c.strokeStyle = info ? '#cf3a2a' : INKC; c.stroke();
  c.fillStyle = INKC; c.textAlign = 'left'; c.fillText(txt, x + 6 * k, y + 6 * k + fs * 0.36);
}
function waves(c, x, y, k) {
  c.save(); c.strokeStyle = '#fbf4e2'; c.lineCap = 'round';
  for (let i = 0; i < 3; i++) {
    const a = 0.4 + 0.6 * Math.abs(Math.sin(G.t * 8 - i * 0.9));
    c.globalAlpha = a; c.lineWidth = k * 0.9;
    c.beginPath(); c.arc(x, y, (2 + i * 2.2) * k, -0.7, 0.7); c.stroke();
  }
  c.restore();
}
function nameTag(ch, x, y) {
  const c = vctx, k = SCALE * DPR;
  c.font = '800 ' + (5.6 * k) + 'px ' + FONT; c.textAlign = 'center'; c.lineJoin = 'round';
  c.lineWidth = k * 1.6; c.strokeStyle = INKC; c.strokeText(ch, S2(x), S2(y));
  c.fillStyle = TAG[ch] || '#fff'; c.fillText(ch, S2(x), S2(y));
  c.textAlign = 'left';
}
function roundRect(c, x, y, w, h, r) { c.beginPath(); c.moveTo(x + r, y); c.arcTo(x + w, y, x + w, y + h, r); c.arcTo(x + w, y + h, x, y + h, r); c.arcTo(x, y + h, x, y, r); c.arcTo(x, y, x + w, y, r); c.closePath(); }

// =====================================================================
// main loop
// =====================================================================
let last = performance.now(), lastClock = 0;
function frame(now) {
  const dt = Math.min(0.05, (now - last) / 1000);
  last = now; G.t += dt;
  pollPad();
  // smooth other players towards their last known spot
  for (const ch in G.others) {
    const o = G.others[ch];
    const dx = o.tx - o.x, dy = o.ty - o.y, d = Math.hypot(dx, dy);
    if (d > 80) { o.x = o.tx; o.y = o.ty; }
    else { o.x += dx * Math.min(1, dt * 10); o.y += dy * Math.min(1, dt * 10); }
    o.moving = d > 0.6 || o.anim === 'walk';
    if (o.moving) { o.phase += dt * 2.4; if (Math.abs(dx) + Math.abs(dy) > 0.3) o.angleLive = Math.atan2(dy, dx); }
  }
  if (G.scene === 'base') updateBase(dt);
  else if (G.scene === 'travel') updateTravel();
  else if (G.scene === 'street') updateStreet(dt);
  else if (G.scene === 'enter') { updateStreet(0); updateEnter(); }
  else if (G.scene === 'inside') updateInside(dt);
  else if (G.scene === 'end') updateEnd(dt);
  radioMix();
  voiceUpdate();
  voiceLevels();
  // draw
  const k = SCALE * DPR;
  vctx.setTransform(k, 0, 0, k, 0, 0);
  vctx.imageSmoothingEnabled = true;
  vctx.fillStyle = '#000'; vctx.fillRect(0, 0, LW, LH);
  if (G.scene === 'base' || G.scene === 'end') drawBase();
  else if (G.scene === 'travel') drawTravel();
  else if (G.scene === 'street') drawStreet();
  else if (G.scene === 'enter') drawEnter();
  else if (G.scene === 'inside') drawInsideWorld();
  vctx.setTransform(1, 0, 0, 1, 0, 0);
  if (G.scene !== 'login') A.paper(vctx, view.width, view.height);
  drawOverlay();
  if (now - lastClock > 250) { updateClock(); updateMicBtn(); updateNoseBtn(); lastClock = now; }
  dmDraw(now);
  logoDraw(now);
  requestAnimationFrame(frame);
}

// =====================================================================
// start
// =====================================================================
layout();
globalThis.__LH = LH;
addEventListener('resize', () => { globalThis.__LH = LH; });
showLogin();
requestAnimationFrame(frame);
if ('serviceWorker' in navigator) addEventListener('load', () => navigator.serviceWorker.register('/sw.js').catch(() => {}));
addEventListener('beforeunload', () => { NET.want = false; });
// test hook: ?as=VITO&skip=1 jumps straight to Berlin (used for quick checks)
const q = new URLSearchParams(location.search);
if (q.get('as') && whoIs(q.get('as'))) {
  const who = whoIs(q.get('as'));
  setTimeout(() => { startBase(who); if (q.get('skip')) { startTravel(who); G.travel.i = G.travel.steps.length - 1; nextTravel(); } }, 50);
}
window.__tusa = G;
window.__tusaDbg = { LOGO, NET, radioEl, voiceEl, voiceOn: () => voiceOn, onAir, press, startTravel, STATIONS: () => STATIONS, VOICE };
})();
